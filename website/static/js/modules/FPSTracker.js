// FPSTracker.js
// Tracks and reports frames per second

export default class FPSTracker {
    constructor(callback) {
        this.callback = callback;
        this.fps = 0;
        this.frameCount = 0;
        this.lastUpdateTime = performance.now();
        this._start();
    }

    _start() {
        const update = () => {
            this.frameCount++;
            const now = performance.now();
            const elapsed = now - this.lastUpdateTime;
            
            if (elapsed >= 1000) {
                this.fps = Math.round((this.frameCount * 1000) / elapsed);
                this.frameCount = 0;
                this.lastUpdateTime = now;
                
                if (this.callback) {
                    this.callback(this.fps);
                }
            }
            
            requestAnimationFrame(update);
        };
        requestAnimationFrame(update);
    }

    getFPS() {
        return this.fps;
    }
}
