import MapElement from './MapElement.js';

export default class Grid extends MapElement {
    constructor(mapEl, camera, options = {}) {
        super(options);
        this.map = mapEl;
        this.camera = camera;
        this.visible = true; // Default to visible
        // Use the same GRID_SIZE as the camera to keep coordinates synchronized
        this.GRID_SIZE = camera.GRID_SIZE; // pixels per grid cell
        this.gridSpacing = 1; // 1 grid cell
        this._initCanvas();
        this._resizeCanvas();
    }

    _initCanvas() {
        // Create canvas for grid
        this.canvas = document.createElement('canvas');
        this.canvas.id = 'grid-canvas';
        this.canvas.style.cssText = `
            position: absolute;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            pointer-events: none;
            z-index: 0;
        `;
        this.map.insertBefore(this.canvas, this.map.firstChild);
        this.ctx = this.canvas.getContext('2d');
    }

    _resizeCanvas() {
        if (!this.canvas) return;
        const dpr = window.devicePixelRatio || 1;
        this.canvas.width = this.map.clientWidth * dpr;
        this.canvas.height = this.map.clientHeight * dpr;
        this.ctx.scale(dpr, dpr);
        this.draw();
    }

    resize() {
        this._resizeCanvas();
    }

    draw() {
        if (!this.canvas || !this.visible) {
            if (this.ctx) {
                this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
            }
            return;
        }

        const ctx = this.ctx;
        const width = this.map.clientWidth;
        const height = this.map.clientHeight;
        const zoom = this.camera.z;

        ctx.clearRect(0, 0, width, height);

        // Grid spacing: 1 grid cell in world coordinates
        // Camera coordinates are already in grid units
        // Convert to screen space: grid cells * pixels per cell * zoom
        const screenSpacing = this.gridSpacing * this.GRID_SIZE * zoom;

        // Only draw if grid lines are reasonably spaced (not too dense or too sparse)
        if (screenSpacing < 5 || screenSpacing > 500) return;

        // Calculate grid origin position in screen space
        // Camera x,y are in grid units, convert to pixels
        // Invert signs so top-right is positive-positive
        const pixelX = -this.camera.x * this.GRID_SIZE;
        const pixelY = -this.camera.y * this.GRID_SIZE;
        const originX = this.camera.cx + pixelX * zoom;
        const originY = this.camera.cy - pixelY * zoom;

        // Calculate grid offset in screen space
        const offsetX = originX % screenSpacing;
        const offsetY = originY % screenSpacing;

        // Style
        ctx.strokeStyle = 'rgba(165, 105, 45, 0.11)';
        ctx.lineWidth = 1;

        ctx.beginPath();

        // Vertical lines
        for (let x = offsetX; x < width; x += screenSpacing) {
            ctx.moveTo(x, 0);
            ctx.lineTo(x, height);
        }
        for (let x = offsetX - screenSpacing; x >= 0; x -= screenSpacing) {
            ctx.moveTo(x, 0);
            ctx.lineTo(x, height);
        }

        // Horizontal lines
        for (let y = offsetY; y < height; y += screenSpacing) {
            ctx.moveTo(0, y);
            ctx.lineTo(width, y);
        }
        for (let y = offsetY - screenSpacing; y >= 0; y -= screenSpacing) {
            ctx.moveTo(0, y);
            ctx.lineTo(width, y);
        }

        ctx.stroke();

        // Draw origin axes in different color
        if (originX >= 0 && originX <= width) {
            ctx.strokeStyle = 'rgba(255, 100, 100, 0.4)';
            ctx.lineWidth = 2;
            ctx.beginPath();
            ctx.moveTo(originX, 0);
            ctx.lineTo(originX, height);
            ctx.stroke();
        }
        if (originY >= 0 && originY <= height) {
            ctx.strokeStyle = 'rgba(104, 104, 202, 0.4)';
            ctx.lineWidth = 2;
            ctx.beginPath();
            ctx.moveTo(0, originY);
            ctx.lineTo(width, originY);
            ctx.stroke();
        }
    }

    setVisible(visible) {
        this.visible = visible;
        this.draw();
    }
}
