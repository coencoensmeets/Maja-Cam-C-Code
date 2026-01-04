import MapElement from './MapElement.js';

export default class DebugPanelController extends MapElement {
    constructor(mapInstance, options = {}) {
        super(options);
        this.map = mapInstance;
        this.mapEl = mapInstance?.map;
        this.debugPanel = document.querySelector('.debug-panel');
        this.debugDropdownBtn = document.getElementById('debugDropdownBtn');
        this.debugHeader = document.getElementById('debugHeader');
        this.debugDropdown = document.getElementById('debugDropdown');
        this.toggleGrid = document.getElementById('toggleGrid');
        this.toggleCenterLines = document.getElementById('toggleCenterLines');
        this.toggleCrosshair = document.getElementById('toggleCrosshair');
        this.toggleHomeMarker = document.getElementById('toggleHomeMarker');
        this.centerHomeMarker = this.mapEl?.querySelector('#center-homeMarker');
        this.crosshair = document.getElementById('crosshair');
        this.debugCoords = document.getElementById('debugCoords');
        // Bind event handlers first, then load defaults and initialize state
        this._bindEvents();
        this._applyDefaultSettings().then(() => {
            this._initCoordsUpdater();
            this._initCrosshairToggle();
        });
    }

    async _applyDefaultSettings() {
        const fallback = {
            debug: {
                showGrid: true,
                showCenterLines: true,
                showCrosshair: true,
                showHomeMarker: true
            }
        };

        let settings = null;
        const paths = [
            '/static/data/defaultSettings.json',
            '/data/defaultSettings.json',
            '/defaultSettings.json'
        ];

        for (const p of paths) {
            try {
                const res = await fetch(p, { cache: 'no-cache' });
                if (!res.ok) continue;
                settings = await res.json();
                break;
            } catch (err) {
                // try next
            }
        }

        if (!settings) settings = fallback;

        this._applyDebugSettings(settings.debug || fallback.debug);
    }

    _applyDebugSettings(debug) {
        if (!debug) return;

        if (this.toggleGrid) {
            this.toggleGrid.checked = !!debug.showGrid;
            this.toggleGrid.dispatchEvent(new Event('change', { bubbles: true }));
        }

        if (this.toggleCenterLines) {
            this.toggleCenterLines.checked = !!debug.showCenterLines;
            this.toggleCenterLines.dispatchEvent(new Event('change', { bubbles: true }));
        }

        if (this.toggleHomeMarker) {
            this.toggleHomeMarker.checked = !!debug.showHomeMarker;
            this.toggleHomeMarker.dispatchEvent(new Event('change', { bubbles: true }));
        }

        if (this.toggleCrosshair) {
            this.toggleCrosshair.checked = !!debug.showCrosshair;
        }
        // Ensure crosshair reflects setting immediately if element exists
        if (this.crosshair && typeof debug.showCrosshair !== 'undefined') {
            this.crosshair.classList.toggle('hidden', !debug.showCrosshair);
        }
    }

    _bindEvents() {
        if (this.debugPanel && this.debugHeader) {
            this.debugHeader.addEventListener('click', () => {
                this.debugPanel.classList.toggle('open');
            });
        }
        if (this.toggleGrid && this.mapEl) {
            this.toggleGrid.addEventListener('change', (e) => {
                if (e.target.checked) {
                    this.mapEl.style.backgroundImage = 'linear-gradient(var(--grid-color) 1px, transparent 1px), linear-gradient(90deg, var(--grid-color) 1px, transparent 1px)';
                    this.mapEl.style.backgroundSize = 'var(--grid-size) var(--grid-size), var(--grid-size) var(--grid-size)';
                    this.mapEl.style.backgroundRepeat = 'repeat';
                } else {
                    this.mapEl.style.backgroundImage = 'none';
                }
            });
        }
        if (this.toggleCenterLines && this.mapEl) {
            this.toggleCenterLines.addEventListener('change', (e) => {
                const xLine = this.mapEl.querySelector('#center-x-line');
                const yLine = this.mapEl.querySelector('#center-y-line');
                if (xLine) xLine.style.display = e.target.checked ? 'block' : 'none';
                if (yLine) yLine.style.display = e.target.checked ? 'block' : 'none';
            });
        }
        if (this.toggleHomeMarker && this.centerHomeMarker) {
            this.toggleHomeMarker.addEventListener('change', (e) => {
                this.centerHomeMarker.style.display = e.target.checked ? 'block' : 'none';
            });
        }
    }

    _initCrosshairToggle() {
        if (!this.toggleCrosshair || !this.crosshair) return;
        // Set initial state using inline style to override CSS defaults
        this.crosshair.style.display = this.toggleCrosshair.checked ? 'block' : 'none';
        this.toggleCrosshair.addEventListener('change', (e) => {
            this.crosshair.style.display = e.target.checked ? 'block' : 'none';
        });
    }

    // Unified coordinate update function
    updateAllCoords = (x, y) => {
        // Update debug coordinates
        if (this.debugCoords && this.map) {
            const gridSize = this.map.grid?.gridSize || 40;
            const [gx, gy] = this.map.camera.getGridCoords(gridSize);
            this.debugCoords.textContent = `(${gx.toFixed(2)}, ${gy.toFixed(2)})`;
        }
        // Add other coordinate-dependent updates here
        // Example: update info panel, overlays, etc.
        if (window._info && typeof window._info._updateCoords === 'function') {
            window._info._updateCoords(this.map.camera.x, this.map.camera.y);
        }
    }

    _initCoordsUpdater() {
        if (!this.map) return;
        // Register the callback with the camera
        if (this.map.camera && typeof this.map.camera.appendCoordinateUpdateCallback === 'function') {
            this.map.camera.appendCoordinateUpdateCallback(this.updateAllCoords);
        }
        // Initial update
        this.updateAllCoords(this.map.camera.x, this.map.camera.y);
    }
}
