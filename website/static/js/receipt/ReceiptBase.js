// ReceiptBase.js
// Base class for receipt-style overlays. Extend this for custom receipts.


import ReceiptDivider from '/static/js/receipt/ReceiptDivider.js';

// Serrated edge tooth size constants
const TOOTH_WIDTH = 12; // Width of each triangle tooth in px
const TOOTH_HEIGHT = 8; // Height of each triangle tooth in px

export default class ReceiptBase {
    constructor(world, options = {}) {
        if (!world) throw new Error('world element required');
        this.world = world;
        this.options = options;
        this._createElement();
    }

    _createElement() {
        this.el = document.createElement('div');
        this.el.className = 'map-receipt';
        this.el.setAttribute('role', 'region');
        this.el.setAttribute('aria-label', this.options.ariaLabel || 'Receipt');

        // Position at world origin (0,0)
        this.el.style.position = 'relative';
        this.el.style.left = '0px';
        this.el.style.top = '0px';

        // Create background div for canvases
        this.backgroundDiv = document.createElement('div');
        this.backgroundDiv.className = 'receipt-background';
        this.el.appendChild(this.backgroundDiv);

        // Create data div for content
        this.dataDiv = document.createElement('div');
        this.dataDiv.className = 'receipt-data';
        this.dataDiv.innerHTML = this._getContentHTML();
        this.el.appendChild(this.dataDiv);

        this.world.appendChild(this.el);
        
        this._createCanvas();
        
        this._bindEvents();

        // Divider logic (optional, can be disabled)
        if (this.options.divider !== false) {
            this.divider = new ReceiptDivider(this.dataDiv, { selector: '.map-receipt-divider', ratio: 0.95 });
        }
    }

    // Subclasses or options should override this to provide content
    _getContentHTML() {
        return `
            <div>
                <h2 class="map-receipt-title">Receipt</h2>
            </div>
            <div class="map-receipt-divider" aria-hidden="true">-_-_-_-_-_-</div>
            <div class="map-receipt-buttons"></div>
            <div class="map-receipt-content"></div>
        `;
    }

    _createCanvas() {
        // Create single canvas for entire receipt shape (top edge + middle + bottom edge)
        this.canvas = document.createElement('canvas');
        this.canvas.className = 'map-receipt-background';
        this.canvas.style.position = 'absolute';
        this.canvas.style.top = '0';
        this.canvas.style.left = '0';
        this.canvas.style.zIndex = '-1';
        this.backgroundDiv.appendChild(this.canvas);

        // Draw the complete receipt shape after layout
        const drawAll = () => {
            this._drawReceiptShape();
        };
        window.requestAnimationFrame(drawAll);

        // Redraw on window resize
        this._resizeHandler = drawAll;
        window.addEventListener('resize', this._resizeHandler);
    }

    _drawReceiptShape() {
        const width = this.el.offsetWidth;
        const height = this.el.offsetHeight;

        // Setup canvas to match full receipt size
        this.canvas.width = width;
        this.canvas.height = height;
        this.canvas.style.width = `${width}px`;
        this.canvas.style.height = `${height}px`;

        const ctx = this.canvas.getContext('2d');
        ctx.clearRect(0, 0, width, height);
        ctx.fillStyle = '#ffffff';
        ctx.beginPath();

        // Start at top-left, just below the first tooth valley
        ctx.moveTo(0, TOOTH_HEIGHT);

        // Draw top serrated edge (triangles pointing up)
        for (let x = 0; x < width; x += TOOTH_WIDTH) {
            ctx.lineTo(x + TOOTH_WIDTH / 2, 0); // Peak
            ctx.lineTo(Math.min(x + TOOTH_WIDTH, width), TOOTH_HEIGHT); // Valley
        }

        // Right edge down to bottom serrated area
        ctx.lineTo(width, height - TOOTH_HEIGHT);

        // Draw bottom serrated edge (triangles pointing down) - right to left
        for (let x = width; x > 0; x -= TOOTH_WIDTH) {
            ctx.lineTo(x - TOOTH_WIDTH / 2, height); // Peak
            ctx.lineTo(Math.max(x - TOOTH_WIDTH, 0), height - TOOTH_HEIGHT); // Valley
        }

        // Left edge back up to start
        ctx.lineTo(0, TOOTH_HEIGHT);
        ctx.closePath();
        ctx.fill();
    }

    _bindEvents() {
        this.el.addEventListener('click', (e) => {
            const btn = e.target.closest('.map-receipt-btn');
            if (!btn) return;
            const action = btn.dataset.action;
            this._animateThen(action);
        });
    }

    _animateThen(action) {
        if (this._animating) return;
        this._animating = true;
        const onEnd = () => {
            this.el.removeEventListener('animationend', onEnd);
            this.el.classList.remove('map-receipt-animate');
            this._animating = false;
            this._onAction(action);
        };
        this.el.classList.add('map-receipt-animate');
        this.el.addEventListener('animationend', onEnd);
    }

    // Subclasses or options can override this
    _onAction(action) {
        if (this.options.onAction) {
            this.options.onAction(action);
        }
    }

    show() {
        this.el.style.display = '';
    }

    hide() {
        this.el.style.display = 'none';
    }

    destroy() {
        if (this.el && this.el.parentNode) this.el.parentNode.removeChild(this.el);
        this.el = null;
        if (this.divider && typeof this.divider.destroy === 'function') {
            this.divider.destroy();
            this.divider = null;
        }
    }
}
