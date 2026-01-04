import MapElement from './MapElement.js';

export default class HomePopup extends MapElement {
    constructor(homeEl, mapInstance, options = {}) {
        if (!homeEl) throw new Error('info element required');
        if (!mapInstance) throw new Error('mapInstance required');
        super(options);
        this.el = homeEl;
        this.map = mapInstance;
        this.camera = mapInstance.camera;
        this.grid = mapInstance.grid;
        this.threshold = options.thresholdCells || 5;
        // Register for camera coordinate updates
        if (this.camera && typeof this.camera.appendCoordinateUpdateCallback === 'function') {
            this.camera.appendCoordinateUpdateCallback((x, y) => {
                this._updateVisibility(x, y);
            });
        }
        // Initial update
        this._updateVisibility(this.camera.x, this.camera.y);
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
        if (this.map && typeof this.map.center === 'function') {
            this.map.center();
        }
    }

    _updateVisibility(offsetX, offsetY) {
        // Ellipse is centered at (0,0), width: 80vw, height: 80vh (in px)
        // Get viewport size
        const vw = window.innerWidth;
        const vh = window.innerHeight;
        const a = 0.4 * vw; // semi-major axis (horizontal radius)
        const b = 0.4 * vh; // semi-minor axis (vertical radius)
        // Camera offset is in px, relative to center (0,0)
        // Check if point (offsetX, offsetY) is outside the ellipse
        const inside = ((offsetX * offsetX) / (a * a)) + ((offsetY * offsetY) / (b * b)) <= 1;
        if (!inside) {
            this.el.classList.add('visible');
        } else {
            this.el.classList.remove('visible');
        }
    }
}
