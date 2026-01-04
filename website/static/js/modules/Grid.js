import MapElement from './MapElement.js';

export default class Grid extends MapElement {
    constructor(mapEl, options = {}) {
        super(options);
        if (!mapEl) throw new Error('map element required');
        this.map = mapEl;
        this.gridSize = options.gridSize || 40;
        this.lineColor = options.lineColor || 'rgba(0,0,0,0.12)';
        this.offsetX = 0;
        this.offsetY = 0;
        this.cx = 0;
        this.cy = 0;
        this._applyStyles();
    }

    setOffset(offsetX, offsetY, cx, cy) {
        this.offsetX = offsetX;
        this.offsetY = offsetY;
        this.cx = cx;
        this.cy = cy;
        this.setBG();
    }

    _applyStyles() {
        document.documentElement.style.setProperty('--grid-size', `${this.gridSize}px`);
        document.documentElement.style.setProperty('--grid-color', this.lineColor);
        this.map.style.backgroundSize = `${this.gridSize}px ${this.gridSize}px, ${this.gridSize}px ${this.gridSize}px`;
    }

    setBG() {
        const gs = this.gridSize;
        const mod = (v) => ((v % gs) + gs) % gs;
        const bgX = mod(this.offsetX + this.cx);
        const bgY = mod(this.offsetY + this.cy);
        this.map.style.backgroundPosition = `${bgX}px ${bgY}px`;

        // Center lines (infinite)
        if (this.map.querySelector('#world')) {
            const world = this.map.querySelector('#world');
            const centerXLine = world.querySelector('#center-x-line');
            const centerYLine = world.querySelector('#center-y-line');
            if (centerXLine && centerYLine) {
                const lineLength = 20000;
                // Only set layout-related styles, not background color (handled by CSS)
                centerXLine.style.position = 'absolute';
                centerXLine.style.left = `${-lineLength/2}px`;
                centerXLine.style.top = `-1px`;
                centerXLine.style.width = `${lineLength}px`;
                centerXLine.style.height = '2px';
                centerXLine.style.pointerEvents = 'none';
                centerXLine.style.zIndex = '10';

                centerYLine.style.position = 'absolute';
                centerYLine.style.left = `-1px`;
                centerYLine.style.top = `${-lineLength/2}px`;
                centerYLine.style.width = '2px';
                centerYLine.style.height = `${lineLength}px`;
                centerYLine.style.pointerEvents = 'none';
                centerYLine.style.zIndex = '10';
            }
        }
    }
}
