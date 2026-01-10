import MapElement from './MapElement.js';
import Camera from './Camera.js';
import Grid from './Grid.js';
import DebugPanelController from './DebugPanelController.js';
import HomePopup from './HomePopup.js';
import HomeScreenReceipt from './HomeScreenReceipt.js';
import LoremReceipt from './LoremReceipt.js';
import CorkboardTexture from './CorkboardTexture.js';
import FPSTracker from './FPSTracker.js';
import ViewportCuller from './ViewportCuller.js';
import ReceiptLayoutManager from './ReceiptLayoutManager.js';

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
        
        // Initialize subsystems early (before setBG is called)
        this._resizeDebounceTimer = null;
        
        // Initialize layout manager
        this.layoutManager = new ReceiptLayoutManager({
            padding: 30,
            startRadius: 150,
            radiusIncrement: 30
        });
        
        // Initialize viewport culler
        this.culler = new ViewportCuller(this.map, this.camera, {
            enabled: true,
            throttleMs: 10,
            viewportPadding: 200
        });

        // Register updateMapView to automatically update when camera changes
        this.camera.appendCoordinateUpdateCallback(() => this.updateMapView());
        this.updateMapView();

        // Now safe to add listeners
        this._addListeners();

        // Initialize corkboard texture (must be after mapElements)
        this.corkboard = new CorkboardTexture(this.map, this.camera, {
            tileSize: 400,
            noiseIntensity: 0.05
        });

        if (this.world) {
            this._initializeReceipts();
        }

        // Initialize grid
        this.grid = new Grid(this.map, this.camera);

        // Attach DebugPanelController as a property
        this.debugPanel = new DebugPanelController(this);
        
        // Initialize FPS tracker
        this.fpsTracker = new FPSTracker((fps) => {
            if (this.debugPanel) {
                this.debugPanel.updateFps(fps);
            }
        });

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
        // Add center receipt at origin
        const homeReceipt = new HomeScreenReceipt(this.world);
        this._placeReceiptAt(homeReceipt, 0, 0);
        
        // Add remaining receipts with calculated positions
        for (let i = 0; i < 0; i++) {
            const receipt = new LoremReceipt(this.world);
            this._findAndPlaceReceipt(receipt);
        }
        
        // Update minimum zoom to fit all receipts
        this._updateMinZoom();
    }

    _placeReceiptAt(receipt, x, y) {
        this.layoutManager.placeReceipt(receipt, x, y);
        this._updateMinZoom();
    }

    _findAndPlaceReceipt(receipt) {
        const position = this.layoutManager.findPlacementPosition(receipt);
        this._placeReceiptAt(receipt, position.x, position.y);
    }

    repositionAllReceipts() {
        const receipts = this.layoutManager.getReceipts();
        if (receipts.length === 0) return;

        // Store all receipts except the center one
        const receiptsToReposition = receipts.slice(1);
        const centerReceipt = receipts[0];

        // Clear layout manager
        this.layoutManager.clear();

        // Re-place the center receipt
        this._placeReceiptAt(centerReceipt, 0, 0);

        // Re-place all other receipts
        for (const receipt of receiptsToReposition) {
            this._findAndPlaceReceipt(receipt);
        }
        
        // Update minimum zoom to fit all receipts
        this._updateMinZoom();
    }

    _updateMinZoom() {
        if (!this.camera) return;
        const receipts = this.layoutManager.getReceipts();
        this.camera.updateMinZoom(receipts, this.map.clientWidth, this.map.clientHeight);
    }

    updateMapView() {
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
        
        // Cull receipts based on viewport
        this.culler.cull(this.layoutManager.getReceipts());
        
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
        
        this.updateMapView();
    }

    _onPointerDown(e) {
        this.camera.onPointerDown(e);
        this.map.classList.add('dragging');
    }

    _onPointerMove(e) {
        const moved = this.camera.onPointerMove(e);
        // updateMapView is called automatically via camera callback
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
        // updateMapView is called automatically via camera callback
    }
}
