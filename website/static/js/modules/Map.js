import MapElement from './MapElement.js';
import Camera from './Camera.js';
import Grid from './Grid.js';
import DebugPanelController from './DebugPanelController.js';
import HomePopup from './HomePopup.js';
import HomeScreenReceipt from './HomeScreenReceipt.js';

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

        // Add home-screen receipt element at world origin (center of logical map)
        if (this.world) {
            this.homeReceipt = new HomeScreenReceipt(this.world, {
                onMenuClick: () => this._onHomeReceiptMenu(),
                onLogClick: () => this._onHomeReceiptLog(),
                onFiltersClick: () => this._onHomeReceiptFilters()
            });
            window._homeReceipt = this.homeReceipt;
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

    // Placeholder callbacks for home-screen-receipt buttons
    _onHomeReceiptMenu() {
        console.log('Home receipt: menu clicked');
        // future implementation: open menu
    }

    _onHomeReceiptLog() {
        console.log('Home receipt: log clicked');
        // future implementation: show logs
    }

    _onHomeReceiptFilters() {
        console.log('Home receipt: filters clicked');
        // future implementation: show filter options
    }
}
