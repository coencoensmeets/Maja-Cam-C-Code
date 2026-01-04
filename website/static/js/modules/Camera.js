import MapElement from './MapElement.js';

export default class Camera {
    constructor() {
        this.offsetX = 0;
        this.offsetY = 0;
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
    getGridCoords(gridSize) {
        return [
            -this.offsetX / gridSize,
            -this.offsetY / gridSize
        ];
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
        this._coordinateUpdateCallback = (x, y) => {
            if (typeof prevCb === 'function') prevCb(x, y);
            if (typeof cb === 'function') cb(x, y);
        };
    }
    _triggerCoordinateUpdate() {
        if (typeof this._coordinateUpdateCallback === 'function') {
            this._coordinateUpdateCallback(this.offsetX, this.offsetY);
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
        this.x += dx;
        this.y -= dy;
        return true;
    }
    onPointerUp(e) {
        this.isDown = false;
        try { e.target.releasePointerCapture(e.pointerId); } catch (err) { }
    }
}
