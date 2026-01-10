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
        this.toggleCrosshair = document.getElementById('toggleCrosshair');
        this.toggleHomeMarker = document.getElementById('toggleHomeMarker');
        this.toggleGrid = document.getElementById('toggleGrid');
        this.addReceiptBtn = document.getElementById('addReceiptBtn');
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
                showCrosshair: true,
                showHomeMarker: true,
                showGrid: true
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

        if (this.toggleHomeMarker) {
            this.toggleHomeMarker.checked = !!debug.showHomeMarker;
            if (this.centerHomeMarker) {
                this.centerHomeMarker.style.display = debug.showHomeMarker ? 'block' : 'none';
            }
        }

        if (this.toggleCrosshair) {
            this.toggleCrosshair.checked = !!debug.showCrosshair;
        }
        // Ensure crosshair reflects setting immediately if element exists
        if (this.crosshair && typeof debug.showCrosshair !== 'undefined') {
            this.crosshair.style.display = debug.showCrosshair ? 'block' : 'none';
        }

        if (this.toggleGrid) {
            this.toggleGrid.checked = !!debug.showGrid;
            if (this.map && typeof this.map.setGridVisible === 'function') {
                this.map.setGridVisible(debug.showGrid);
            }
        }
    }

    _bindEvents() {
        if (this.debugPanel && this.debugHeader) {
            this.debugHeader.addEventListener('click', () => {
                this.debugPanel.classList.toggle('open');
            });
        }
        if (this.toggleHomeMarker && this.centerHomeMarker) {
            this.toggleHomeMarker.addEventListener('change', (e) => {
                this.centerHomeMarker.style.display = e.target.checked ? 'block' : 'none';
            });
        }
        if (this.toggleGrid && this.map) {
            this.toggleGrid.addEventListener('change', (e) => {
                if (typeof this.map.setGridVisible === 'function') {
                    this.map.setGridVisible(e.target.checked);
                }
            });
        }
        if (this.addReceiptBtn && this.map) {
            this.addReceiptBtn.addEventListener('click', () => {
                this._addLoremReceipt();
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
    updateAllCoords = (x, y, z) => {
        // Update debug coordinates
        if (this.debugCoords) {
            // Coordinates are already in grid units (Cartesian system where top-right is ++)
            // z is the zoom level
            this.debugCoords.textContent = `(${x.toFixed(2)}, ${y.toFixed(2)}, ${z.toFixed(2)})`;
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
        this.updateAllCoords(this.map.camera.x, this.map.camera.y, this.map.camera.z);
    }

    async _addLoremReceipt() {
        if (!this.map || !this.map.world) return;
        
        // Dynamically import LoremReceipt
        const { default: LoremReceipt } = await import('./LoremReceipt.js');
        const receipt = new LoremReceipt(this.map.world);
        this.map._findAndPlaceReceipt(receipt);
    }
}
