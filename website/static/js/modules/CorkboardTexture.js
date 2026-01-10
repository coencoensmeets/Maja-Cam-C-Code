// Generates an infinite noise texture using canvas
import MapElement from './MapElement.js';

export default class CorkboardTexture extends MapElement {
    constructor(mapEl, camera, options = {}) {
        super(options);
        this.map = mapEl;
        this.camera = camera;
        this.tileSize = options.tileSize || 400;
        this.noiseIntensity = options.noiseIntensity || 0.01;
        this.baseColor = options.baseColor || 'hsl(35, 61%, 88%)';
        this._lastDrawTime = 0;
        this._drawThrottleMs = 1; // Throttle draws to every 1ms
        this._lastZoom = 1;
                // Initialize Perlin noise
        this._initPerlin();
                // Generate tile pattern once
        this.tileCanvas = this._generateTile();
        
        this._initCanvas();
        this._resizeCanvas();
    }

    // Perlin noise implementation
    _initPerlin() {
        this.permutation = [];
        for (let i = 0; i < 256; i++) {
            this.permutation[i] = i;
        }
        // Shuffle using Fisher-Yates
        for (let i = 255; i > 0; i--) {
            const j = Math.floor(Math.random() * (i + 1));
            [this.permutation[i], this.permutation[j]] = [this.permutation[j], this.permutation[i]];
        }
        // Duplicate for wrapping
        this.permutation = [...this.permutation, ...this.permutation];
    }

    _fade(t) {
        return t * t * t * (t * (t * 6 - 15) + 10);
    }

    _lerp(t, a, b) {
        return a + t * (b - a);
    }

    _grad(hash, x, y) {
        const h = hash & 3;
        const u = h < 2 ? x : y;
        const v = h < 2 ? y : x;
        return ((h & 1) === 0 ? u : -u) + ((h & 2) === 0 ? v : -v);
    }

    _perlin(x, y) {
        const X = Math.floor(x) & 255;
        const Y = Math.floor(y) & 255;
        
        x -= Math.floor(x);
        y -= Math.floor(y);
        
        const u = this._fade(x);
        const v = this._fade(y);
        
        const a = this.permutation[X] + Y;
        const aa = this.permutation[a];
        const ab = this.permutation[a + 1];
        const b = this.permutation[X + 1] + Y;
        const ba = this.permutation[b];
        const bb = this.permutation[b + 1];
        
        return this._lerp(v,
            this._lerp(u, this._grad(this.permutation[aa], x, y), this._grad(this.permutation[ba], x - 1, y)),
            this._lerp(u, this._grad(this.permutation[ab], x, y - 1), this._grad(this.permutation[bb], x - 1, y - 1))
        );
    }

    // Seeded random for tileable noise
    _seededRandom(x, y, seed = 0) {
        // Hash function for position-based random
        const n = Math.sin(x * 12.9898 + y * 78.233 + seed * 43758.5453) * 43758.5453;
        return n - Math.floor(n);
    }

    // Gaussian random using seeded values for tiling
    _seededGaussian(x, y, seed, mean = 0, stdev = 1) {
        const u = 1 - this._seededRandom(x, y, seed);
        const v = this._seededRandom(x, y, seed + 1);
        const z = Math.sqrt(-2.0 * Math.log(u)) * Math.cos(2.0 * Math.PI * v);
        return z * stdev + mean;
    }

    // Fast seeded random using simpler hash
    _fastRandom(x, y) {
        let h = (x * 374761393 + y * 668265263) | 0;
        h = (h ^ (h >>> 13)) * 1274126177 | 0;
        return ((h ^ (h >>> 16)) >>> 0) / 4294967296;
    }

    _generateTile() {
        const canvas = document.createElement('canvas');
        canvas.width = this.tileSize;
        canvas.height = this.tileSize;
        const ctx = canvas.getContext('2d');

        // Fill base color
        ctx.fillStyle = this.baseColor;
        ctx.fillRect(0, 0, this.tileSize, this.tileSize);

        // Generate pixel-by-pixel noise using fast method
        const imageData = ctx.getImageData(0, 0, this.tileSize, this.tileSize);
        const data = imageData.data;
        const intensity = this.noiseIntensity * 255;
        
        for (let i = 0; i < data.length; i += 4) {
            const pixelIndex = i / 4;
            const x = pixelIndex % this.tileSize;
            const y = (pixelIndex / this.tileSize) | 0;
            
            // Use fast random based on position for perfect tiling
            const noise = (this._fastRandom(x, y) - 0.5) * 2 * intensity;
            
            data[i] += noise;     // R
            data[i + 1] += noise; // G
            data[i + 2] += noise; // B
            // Alpha stays at 255
        }
        
        ctx.putImageData(imageData, 0, 0);

        return canvas;
    }

    _initCanvas() {
        this.canvas = document.createElement('canvas');
        this.canvas.id = 'corkboard-canvas';
        this.canvas.style.cssText = `
            position: absolute;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            pointer-events: none;
            z-index: -1;
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
        if (!this.canvas) return;

        const zoom = this.camera.z;
        
        // Throttle draw calls
        const now = performance.now();
        if (now - this._lastDrawTime < this._drawThrottleMs && Math.abs(zoom - this._lastZoom) < 0.01) {
            return;
        }
        this._lastDrawTime = now;
        this._lastZoom = zoom;

        const ctx = this.ctx;
        const width = this.map.clientWidth;
        const height = this.map.clientHeight;

        ctx.clearRect(0, 0, width, height);

        // Skip texture rendering at very low zoom levels for performance
        if (zoom < 0.1) {
            // Just fill with base color
            ctx.fillStyle = this.baseColor;
            ctx.fillRect(0, 0, width, height);
            return;
        }

        // Calculate tile size in screen space
        const screenTileSize = this.tileSize * zoom;

        // Calculate texture origin position in screen space
        const pixelX = -this.camera.x * this.camera.GRID_SIZE;
        const pixelY = -this.camera.y * this.camera.GRID_SIZE;
        const originX = this.camera.cx + pixelX * zoom;
        const originY = this.camera.cy - pixelY * zoom;

        // Calculate tile offset
        const offsetX = originX % screenTileSize;
        const offsetY = originY % screenTileSize;

        // Calculate how many tiles we need to cover the viewport
        const tilesX = Math.ceil(width / screenTileSize) + 2;
        const tilesY = Math.ceil(height / screenTileSize) + 2;

        // Draw tiles
        const startX = offsetX - screenTileSize;
        const startY = offsetY - screenTileSize;

        for (let ty = 0; ty < tilesY; ty++) {
            for (let tx = 0; tx < tilesX; tx++) {
                const x = startX + tx * screenTileSize;
                const y = startY + ty * screenTileSize;
                ctx.drawImage(this.tileCanvas, x, y, screenTileSize, screenTileSize);
            }
        }
    }
}
