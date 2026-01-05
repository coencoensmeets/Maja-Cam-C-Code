import MapElement from './MapElement.js';
import Grid from './Grid.js';
import Camera from './Camera.js';
import DebugPanelController from './DebugPanelController.js';
import HomePopup from './HomePopup.js';
import HomeScreenReceipt from './HomeScreenReceipt.js';

export default class Map extends MapElement {
    constructor(mapEl, options = {}) {
        if (!mapEl) throw new Error('map element required');
        super(options);
        this.map = mapEl;
        this.grid = new Grid(mapEl, options);
        this.camera = new Camera();
        this._applyStyles();
        this._updateCenter();
        this.setBG();

        // Now safe to map elements and add listeners
        this.mapElements();
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

    mapElements() {
        this.world = this.map.querySelector('#world');
    }

    _applyStyles() {
        this.map.style.touchAction = 'none';
    }

    setBG() {
        if (this.world) {
            // include zoom scale
            this.world.style.transform = `translate3d(${this.camera.x + this.camera.cx}px, ${-this.camera.y + this.camera.cy}px, 0) scale(${this.camera.z})`;
        }
        // keep grid offset behavior, pass scale if grid supports it
        if (typeof this.grid.setOffset === 'function') {
            try {
                this.grid.setOffset(this.camera.x, -this.camera.y, this.camera.cx, this.camera.cy, this.camera.z);
            } catch (err) {
                // fallback to previous signature
                this.grid.setOffset(this.camera.x, -this.camera.y, this.camera.cx, this.camera.cy);
            }
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
        this.setBG();
    }

    _onPointerDown(e) {
        this.camera.onPointerDown(e);
        this.map.classList.add('dragging');
    }

    _onPointerMove(e) {
        const moved = this.camera.onPointerMove(e);
        if (moved) {
            this.setBG();
        }
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
        this.camera.zoomBy(factor);
        this.setBG();
    }

    center() {
        // Animate camera to (0,0)
        const startX = this.camera.x;
        const startY = this.camera.y;
        console.log(`Centering from (${startX}, ${startY}) to (0, 0)`);
        const endX = 0;
        const endY = 0;
        const duration = 400;
        const start = performance.now();
        if (this._centerAnim) cancelAnimationFrame(this._centerAnim);
        const ease = (t) => 1 - Math.pow(1 - t, 3);
        const step = (now) => {
            const t = Math.min(1, (now - start) / duration);
            const v = ease(t);
            this.camera.x = startX + (endX - startX) * v;
            this.camera.y = startY + (endY - startY) * v;
            this.setBG();
            if (t < 1) {
                this._centerAnim = requestAnimationFrame(step);
            } else {
                this._centerAnim = null;
            }
        };
        this._centerAnim = requestAnimationFrame(step);
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
