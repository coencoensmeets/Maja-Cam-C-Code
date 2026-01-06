import MapElement from './MapElement.js';

export default class HomePopup extends MapElement {
    constructor(homeEl, mapInstance, options = {}) {
        if (!homeEl) throw new Error('info element required');
        if (!mapInstance) throw new Error('mapInstance required');
        super(options);
        this.el = homeEl;
        this.map = mapInstance;
        this.camera = mapInstance.camera;
        this.threshold = options.thresholdCells || 5;
        // Register for camera coordinate updates
        if (this.camera && typeof this.camera.appendCoordinateUpdateCallback === 'function') {
            this.camera.appendCoordinateUpdateCallback((x, y, z) => {
                this._updateVisibility(x, y, z);
            });
        }
        // Initial update
        this._updateVisibility(this.camera.x, this.camera.y, this.camera.z);
        // Ensure button is mapped and event is registered
        this.mapElements();
        this._addListeners();
    }

    _bindMethods() {
        this._onReset = this._onReset.bind(this);
        this._onInfoBtnClick = this._onInfoBtnClick.bind(this);
    }

    mapElements() {
        // The home-popup itself is now the button
        this.btn = this.el;
    }

    _addListeners() {
        if (this.btn) {
            this.btn.removeEventListener('click', this._onInfoBtnClick); // Prevent duplicate
            this.btn.addEventListener('click', this._onInfoBtnClick);
        }
    }

    _onInfoBtnClick(e) {
        e.preventDefault();
        e.stopPropagation();
        this._onReset(e);
    }

    _onReset() {
        if (this.map && this.map.camera) {
            this.map.camera.resetXY();
            this.map.camera.resetZ();
        }
    }

    _updateVisibility(offsetX, offsetY, z) {
        // Ellipse is centered at (0,0), width: 80vw, height: 80vh (in px)
        // Get viewport size
        const vw = window.innerWidth;
        const vh = window.innerHeight;
        const a = 0.4 * vw; // semi-major axis (horizontal radius) in pixels
        const b = 0.4 * vh; // semi-minor axis (vertical radius) in pixels
        // Camera offset is in grid units, convert to pixels for comparison
        const offsetXPx = this.camera.toPixels(offsetX);
        const offsetYPx = this.camera.toPixels(offsetY);
        // Check if point (offsetXPx, offsetYPx) is outside the ellipse
        const inside = ((offsetXPx * offsetXPx) / (a * a)) + ((offsetYPx * offsetYPx) / (b * b)) <= 1;
        if (!inside || z < 0.75) {
            this.el.classList.add('visible');
        } else {
            this.el.classList.remove('visible');
        }
    }
}
