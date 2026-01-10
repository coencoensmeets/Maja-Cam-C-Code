import MapElement from './MapElement.js';
import Camera from './Camera.js';
import Grid from './Grid.js';
import DebugPanelController from './DebugPanelController.js';
import HomePopup from './HomePopup.js';
import HomeScreenReceipt from './HomeScreenReceipt.js';
import LoremReceipt from './LoremReceipt.js';
import CorkboardTexture from './CorkboardTexture.js';

export default class Map extends MapElement {
    constructor(mapEl, options = {}) {
        if (!mapEl) throw new Error('map element required');
        super(options);
        this.map = mapEl;
        // Initialize camera with grid size (default 100 if not specified)
        this.camera = new Camera({ gridSize: options.gridSize || 100 });
        this._applyStyles();
        this._updateCenter();

        // Map #world element first
        this.mapElements();

        // Register setBG to automatically update when camera changes
        this.camera.appendCoordinateUpdateCallback(() => this.setBG());
        this.setBG();

        // Now safe to add listeners
        this._addListeners();

        // Initialize corkboard texture (must be after mapElements)
        this.corkboard = new CorkboardTexture(this.map, this.camera, {
            tileSize: 400,
            noiseIntensity: 0.03
        });

        // Initialize receipts array and add receipts
        this.receipts = [];
        this._resizeDebounceTimer = null;
        if (this.world) {
            this._initializeReceipts();
        }

        // Initialize grid
        this.grid = new Grid(this.map, this.camera);

        // Attach DebugPanelController as a property
        this.debugPanel = new DebugPanelController(this);

        // Optionally attach home panel as a property if present
        const homeEl = document.querySelector('.home-popup');
        if (homeEl) {
            this.home = new HomePopup(homeEl, this, { thresholdCells: 10 });
            window._home = this.home;
        }
    }

    _bindMethods() {
        this._onPointerDown = this._onPointerDown.bind(this);
        this._onPointerMove = this._onPointerMove.bind(this);
        this._onPointerUp = this._onPointerUp.bind(this);
        this._onWheel = this._onWheel.bind(this);
        this._onResize = this._onResize.bind(this);
    }

    setGridVisible(visible) {
        if (this.grid) {
            this.grid.setVisible(visible);
        }
    }

    mapElements() {
        this.world = this.map.querySelector('#world');
    }

    _applyStyles() {
        this.map.style.touchAction = 'none';
    }

    _initializeReceipts() {
        // Initialize placement tracking
        this.placed = [];
        this.padding = 30;
        
        // Add center receipt at origin
        const homeReceipt = new HomeScreenReceipt(this.world);
        this._placeReceiptAt(homeReceipt, 0, 0);
        
        // Add remaining receipts with calculated positions
        for (let i = 0; i < 210; i++) {
            const receipt = new LoremReceipt(this.world);
            this._findAndPlaceReceipt(receipt);
        }
        
        // Update minimum zoom to fit all receipts
        this._updateMinZoom();
    }

    _placeReceiptAt(receipt, x, y) {
        // Set position using receipt's setPosition method
        receipt.setPosition(x, y);
        
        // Add to receipts array
        this.receipts.push(receipt);
        
        // Track bounds for collision detection using world coordinates
        const width = receipt.el.offsetWidth;
        const height = receipt.el.offsetHeight;
        this.placed.push({
            x: x - width / 2,
            y: y - height / 2,
            width: width,
            height: height
        });
        this._updateMinZoom();
    }

    _findAndPlaceReceipt(receipt) {
        // Temporarily set position to origin to get dimensions
        receipt.setPosition(0, 0);
        
        const width = receipt.el.offsetWidth;
        const height = receipt.el.offsetHeight;
        let positioned = false;
        let searchRadius = 150; // Start close to center
        const radiusIncrement = 30;

        // Go in circles around the center, expanding radius each loop
        while (!positioned) {
            // Calculate number of angle steps based on radius (more steps for larger circles)
            const angleSteps = Math.max(16, Math.floor((searchRadius * 2 * Math.PI) / 100));
            
            // Try each position around the circle at this radius
            for (let angleIndex = 0; angleIndex < angleSteps; angleIndex++) {
                const angle = (angleIndex / angleSteps) * Math.PI * 2;
                const x = Math.cos(angle) * searchRadius;
                const y = Math.sin(angle) * searchRadius;
                
                const bounds = {
                    x: x - width / 2,
                    y: y - height / 2,
                    width: width,
                    height: height
                };

                // Check if this position overlaps with any placed receipts
                const overlaps = this.placed.some(other => 
                    this._rectsOverlap(bounds, other, this.padding)
                );

                if (!overlaps) {
                    this._placeReceiptAt(receipt, x, y);
                    positioned = true;
                    break;
                }
            }

            // No space found at this radius, expand to next circle
            if (!positioned) {
                searchRadius += radiusIncrement;
            }
        }
    }

    _rectsOverlap(rect1, rect2, margin = 0) {
        return !(
            rect1.x + rect1.width + margin < rect2.x ||
            rect2.x + rect2.width + margin < rect1.x ||
            rect1.y + rect1.height + margin < rect2.y ||
            rect2.y + rect2.height + margin < rect1.y
        );
    }

    /**
     * Check if there's space for a new receipt next to a placed receipt
     * Samples positions around the entire perimeter to find space closest to (0,0)
     * @param {ReceiptBase} newReceipt - The unplaced receipt to find space for
     * @param {ReceiptBase} placedReceipt - The already placed receipt to check around
     * @returns {Object|null} - {x, y} center position if space available, null otherwise
     */
    hasSpaceNextTo(newReceipt, placedReceipt) {
        const placedRect = placedReceipt.el.getBoundingClientRect();
        const newRect = newReceipt.el.getBoundingClientRect();
        const spacing = this.padding;

        const validPositions = [];
        
        // Calculate how many sample points to check based on receipt dimensions
        const samplesPerSide = Math.ceil(Math.max(placedRect.width, placedRect.height) / newRect.width);

        // Check along right edge (varying Y positions)
        for (let i = 0; i <= samplesPerSide; i++) {
            const t = i / samplesPerSide;
            const x = placedReceipt.x + placedRect.width / 2 + spacing + newRect.width / 2;
            const y = placedReceipt.y - placedRect.height / 2 + t * placedRect.height;
            this._checkAndAddPosition(x, y, newRect, validPositions);
        }

        // Check along left edge (varying Y positions)
        for (let i = 0; i <= samplesPerSide; i++) {
            const t = i / samplesPerSide;
            const x = placedReceipt.x - placedRect.width / 2 - spacing - newRect.width / 2;
            const y = placedReceipt.y - placedRect.height / 2 + t * placedRect.height;
            this._checkAndAddPosition(x, y, newRect, validPositions);
        }

        // Check along bottom edge (varying X positions)
        for (let i = 0; i <= samplesPerSide; i++) {
            const t = i / samplesPerSide;
            const x = placedReceipt.x - placedRect.width / 2 + t * placedRect.width;
            const y = placedReceipt.y + placedRect.height / 2 + spacing + newRect.height / 2;
            this._checkAndAddPosition(x, y, newRect, validPositions);
        }

        // Check along top edge (varying X positions)
        for (let i = 0; i <= samplesPerSide; i++) {
            const t = i / samplesPerSide;
            const x = placedReceipt.x - placedRect.width / 2 + t * placedRect.width;
            const y = placedReceipt.y - placedRect.height / 2 - spacing - newRect.height / 2;
            this._checkAndAddPosition(x, y, newRect, validPositions);
        }

        // Return the position closest to (0,0), or null if no valid positions
        if (validPositions.length === 0) return null;
        
        validPositions.sort((a, b) => a.distance - b.distance);
        return { x: validPositions[0].x, y: validPositions[0].y };
    }

    _checkAndAddPosition(x, y, newRect, validPositions) {
        const targetBounds = {
            x: x - newRect.width / 2,
            y: y - newRect.height / 2,
            width: newRect.width,
            height: newRect.height
        };

        const hasOverlap = this.placed.some(other => 
            this._rectsOverlap(targetBounds, other, this.padding)
        );

        if (!hasOverlap) {
            const distance = Math.sqrt(x * x + y * y);
            validPositions.push({ x, y, distance });
        }
    }



    repositionAllReceipts() {
        if (this.receipts.length === 0) return;

        // Clear placement tracking
        this.placed = [];

        // Store all receipts except the center one
        const receiptsToReposition = this.receipts.slice(1);
        this.receipts = [this.receipts[0]];

        // Re-place the center receipt
        const centerReceipt = this.receipts[0];
        centerReceipt.setPosition(0, 0);
        const centerWidth = centerReceipt.el.offsetWidth;
        const centerHeight = centerReceipt.el.offsetHeight;
        this.placed.push({
            x: -centerWidth / 2,
            y: -centerHeight / 2,
            width: centerWidth,
            height: centerHeight
        });

        // Re-place all other receipts
        for (const receipt of receiptsToReposition) {
            this._findAndPlaceReceipt(receipt);
        }
        
        // Update minimum zoom to fit all receipts
        this._updateMinZoom();
    }

    _updateMinZoom() {
        if (!this.receipts || this.receipts.length === 0 || !this.camera) return;

        // Calculate bounding box of all receipts
        let minX = Infinity, maxX = -Infinity;
        let minY = Infinity, maxY = -Infinity;

        for (const receipt of this.receipts) {
            const width = receipt.el.offsetWidth;
            const height = receipt.el.offsetHeight;
            const left = receipt.x - width / 2;
            const right = receipt.x + width / 2;
            const top = receipt.y - height / 2;
            const bottom = receipt.y + height / 2;

            minX = Math.min(minX, left);
            maxX = Math.max(maxX, right);
            minY = Math.min(minY, top);
            maxY = Math.max(maxY, bottom);
        }

        // Add padding to bounding box
        const padding = 50;
        minX -= padding;
        maxX += padding;
        minY -= padding;
        maxY += padding;

        // Calculate required zoom to fit in viewport
        const boundsWidth = maxX - minX;
        const boundsHeight = maxY - minY;
        const viewportWidth = this.map.clientWidth;
        const viewportHeight = this.map.clientHeight;

        // Calculate zoom needed for width and height, take the smaller one
        const zoomForWidth = viewportWidth / boundsWidth;
        const zoomForHeight = viewportHeight / boundsHeight;
        const requiredZoom = Math.min(zoomForWidth, zoomForHeight);

        // Set minimum zoom (with a lower bound to prevent too small zoom)
        this.camera._minZ = Math.max(0.1, Math.min(0.9, requiredZoom));
        console.log('Updated min zoom to:', this.camera._minZ);
    }

    setBG() {
        if (this.world) {
            // Always map world origin (0,0) to viewport center (cx, cy)
            // Camera offset is in grid units, convert to pixels for rendering
            const centerX = this.camera.cx;
            const centerY = this.camera.cy;
            // Convert grid coordinates to pixel coordinates
            // Invert signs so top-right is positive-positive
            const pixelX = this.camera.toPixels(-this.camera.x);
            const pixelY = this.camera.toPixels(-this.camera.y);
            // The translation puts (0,0) at (centerX, centerY), then applies pan and zoom
            const offsetX = centerX + pixelX * this.camera.z;
            const offsetY = centerY - pixelY * this.camera.z;
            this.world.style.transformOrigin = '0 0';
            this.world.style.transform = `translate3d(${offsetX}px, ${offsetY}px, 0) scale(${this.camera.z})`;
        }
        // Redraw grid when map transforms
        if (this.grid) {
            this.grid.draw();
        }
        // Redraw corkboard when map transforms
        if (this.corkboard) {
            this.corkboard.draw();
        }
    }

    _addListeners() {
        this.map.addEventListener('pointerdown', this._onPointerDown);
        this.map.addEventListener('pointermove', this._onPointerMove);
        this.map.addEventListener('pointerup', this._onPointerUp);
        this.map.addEventListener('pointercancel', this._onPointerUp);
        this.map.addEventListener('wheel', this._onWheel, { passive: false });
        window.addEventListener('resize', this._onResize);
    }

    _updateCenter() {
        this.camera.setCenter(
            Math.round(this.map.clientWidth / 2) || 0,
            Math.round(this.map.clientHeight / 2) || 0
        );
    }

    _onResize() {
        this._updateCenter();
        if (this.grid) {
            this.grid.resize();
        }
        if (this.corkboard) {
            this.corkboard.resize();
        }
        
        // Debounce receipt repositioning to prevent multiple rapid calls
        if (this._resizeDebounceTimer) {
            clearTimeout(this._resizeDebounceTimer);
        }
        this._resizeDebounceTimer = setTimeout(() => {
            this.repositionAllReceipts();
        }, 300);
        
        this.setBG();
    }

    _onPointerDown(e) {
        this.camera.onPointerDown(e);
        this.map.classList.add('dragging');
    }

    _onPointerMove(e) {
        const moved = this.camera.onPointerMove(e);
        // setBG is called automatically via camera callback
    }

    _onPointerUp(e) {
        this.camera.onPointerUp(e);
        this.map.classList.remove('dragging');
    }

    _onWheel(e) {
        e.preventDefault();
        // Use wheel to zoom (no modifier required). deltaY > 0 => zoom out, < 0 => zoom in
        const delta = e.deltaY;
        // Exponential zoom factor for smoothness. Adjust sensitivity multiplier as needed.
        const sensitivity = 0.0012; // smaller = less sensitive
        const factor = Math.exp(-delta * sensitivity);
        this.camera.setZoom(this.camera.z * factor);
        // setBG is called automatically via camera callback
    }
}
