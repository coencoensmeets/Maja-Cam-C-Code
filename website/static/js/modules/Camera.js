import MapElement from './MapElement.js';

export default class Camera {
    constructor(options = {}) {
        // Grid coordinate system: 1 unit = 1 grid cell
        this.GRID_SIZE = options.gridSize || 100; // pixels per grid cell
        // Store coordinates in grid units
        this.offsetX = 0;
        this.offsetY = 0;
        this.z = 1; // zoom level (1 = 100%)
        this._minZ = 0.25;
        this._maxZ = 1;
        this.cx = 0;
        this.cy = 0;
        this._coordinateUpdateCallback = null;
        this.isDown = false;
        this.startX = 0;
        this.startY = 0;
        // Crosshair overlay always in center
        this.crosshair = document.getElementById('crosshair');
        this._initCrosshair();
    }

    _initCrosshair() {
        if (!this.crosshair) return;
        // Style to always be centered in viewport
        this.crosshair.style.position = 'fixed';
        this.crosshair.style.left = '50%';
        this.crosshair.style.top = '50%';
        this.crosshair.style.transform = 'translate(-50%, -50%)';
        this.crosshair.style.pointerEvents = 'none';
        this.crosshair.style.zIndex = '200';
        // Initial visibility (handled by DebugPanelController)
    }

    setOffset(x, y) {
        this.offsetX = x;
        this.offsetY = y;
        this._triggerCoordinateUpdate();
    }

    // Zoom methods

    setZoom(zoom) {
        const clamped = Math.max(this._minZ, Math.min(this._maxZ, zoom));
        if (clamped === this.z) return;
        this.z = clamped;
        this._triggerCoordinateUpdate();
    }

    zoomBy(factor) {
        this.setZoom(this.z * factor);
    }

    getGridCoords() {
        // Coordinates are already in grid units
        return [this.offsetX, this.offsetY];
    }

    // Convert grid coordinates to pixel coordinates
    toPixels(gridValue) {
        return gridValue * this.GRID_SIZE;
    }

    // Convert pixel coordinates to grid coordinates
    fromPixels(pixelValue) {
        return pixelValue / this.GRID_SIZE;
    }

    setCenter(cx, cy) {
        this.cx = cx;
        this.cy = cy;
        this._triggerCoordinateUpdate();
    }

    get x() { return this.offsetX; }
    get y() { return this.offsetY; }
    get centerX() { return this.cx; }
    get centerY() { return this.cy; }
    set x(val) {
        this.offsetX = val;
        this._triggerCoordinateUpdate();
    }
    set y(val) {
        this.offsetY = val;
        this._triggerCoordinateUpdate();
    }

    setCoordinateUpdateCallback(cb) {
        this._coordinateUpdateCallback = cb;
    }

    appendCoordinateUpdateCallback(cb) {
        const prevCb = this._coordinateUpdateCallback;
        this._coordinateUpdateCallback = (x, y, z) => {
            if (typeof prevCb === 'function') prevCb(x, y, z);
            if (typeof cb === 'function') cb(x, y, z);
        };
    }

    _triggerCoordinateUpdate() {
        if (typeof this._coordinateUpdateCallback === 'function') {
            this._coordinateUpdateCallback(this.offsetX, this.offsetY, this.z);
        }
    }

    // Movement logic
    onPointerDown(e) {
        try { e.target.setPointerCapture(e.pointerId); } catch (err) { }
        this.isDown = true;
        this.startX = e.clientX;
        this.startY = e.clientY;
    }

    onPointerMove(e) {
        if (!this.isDown) return false;
        const dx = e.clientX - this.startX;
        const dy = e.clientY - this.startY;
        this.startX = e.clientX;
        this.startY = e.clientY;
        // Convert pixel delta to grid units
        // Invert signs so top-right is positive-positive
        this.x -= this.fromPixels(dx) * 1/this.z;
        this.y += this.fromPixels(dy) * 1/this.z;
        return true;
    }

    onPointerUp(e) {
        this.isDown = false;
        try { e.target.releasePointerCapture(e.pointerId); } catch (err) { }
    }

    // Camera height methods
    setHeight(z) {
        const clamped = Math.max(this._minZ, Math.min(this._maxZ, z));
        if (clamped === this.z) return;
        this.z = clamped;
        this._triggerCoordinateUpdate();
    }

    moveHeightBy(delta) {
        this.setHeight(this.z + delta);
    }

    // Reset methods
    resetXY(onUpdate = null) {
        // Animate camera to (0,0)
        const startX = this.x;
        const startY = this.y;
        const endX = 0;
        const endY = 0;
        const duration = 400;
        const start = performance.now();
        if (this._resetXYAnim) cancelAnimationFrame(this._resetXYAnim);
        const ease = (t) => 1 - Math.pow(1 - t, 3);
        const step = (now) => {
            const t = Math.min(1, (now - start) / duration);
            const v = ease(t);
            this.x = startX + (endX - startX) * v;
            this.y = startY + (endY - startY) * v;
            if (typeof onUpdate === 'function') onUpdate();
            if (t < 1) {
                this._resetXYAnim = requestAnimationFrame(step);
            } else {
                this._resetXYAnim = null;
            }
        };
        this._resetXYAnim = requestAnimationFrame(step);
    }

    resetZ(onUpdate = null) {
        // Animate zoom back to 1
        const startZ = this.z;
        const endZ = 1;
        const duration = 400;
        const start = performance.now();
        if (this._resetZAnim) cancelAnimationFrame(this._resetZAnim);
        const ease = (t) => 1 - Math.pow(1 - t, 3);
        const step = (now) => {
            const t = Math.min(1, (now - start) / duration);
            const v = ease(t);
            this.setZoom(startZ + (endZ - startZ) * v);
            if (typeof onUpdate === 'function') onUpdate();
            if (t < 1) {
                this._resetZAnim = requestAnimationFrame(step);
            } else {
                this._resetZAnim = null;
            }
        };
        this._resetZAnim = requestAnimationFrame(step);
    }

    updateMinZoom(receipts, viewportWidth, viewportHeight) {
        if (!receipts || receipts.length === 0) return;

        // Calculate bounding box of all receipts
        let minX = Infinity, maxX = -Infinity;
        let minY = Infinity, maxY = -Infinity;

        for (const receipt of receipts) {
            const width = receipt.receipt.offsetWidth;
            const height = receipt.receipt.offsetHeight;
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

        // Calculate zoom needed for width and height, take the smaller one
        const zoomForWidth = viewportWidth / boundsWidth;
        const zoomForHeight = viewportHeight / boundsHeight;
        const requiredZoom = Math.min(zoomForWidth, zoomForHeight);

        // Set minimum zoom (with a lower bound to prevent too small zoom)
        this._minZ = Math.max(0.01, Math.min(0.9, requiredZoom));
    }
}
