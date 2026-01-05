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
        // Create top edge canvas
        this.topCanvas = document.createElement('canvas');
        this.topCanvas.className = 'map-receipt-edge-top';
        this.topCanvas.style.position = 'absolute';
        this.topCanvas.style.top = '0';
        this.topCanvas.style.left = '0';
        this.topCanvas.style.zIndex = '-1';
        this.backgroundDiv.appendChild(this.topCanvas);

        // Create bottom edge canvas
        this.bottomCanvas = document.createElement('canvas');
        this.bottomCanvas.className = 'map-receipt-edge-bottom';
        this.bottomCanvas.style.position = 'absolute';
        this.bottomCanvas.style.bottom = '0';
        this.bottomCanvas.style.left = '0';
        this.bottomCanvas.style.zIndex = '-1';
        this.backgroundDiv.appendChild(this.bottomCanvas);
        
        this.middleCanvas = document.createElement('canvas');
        this.middleCanvas.className = 'map-receipt-edge-middle';
        this.middleCanvas.style.position = 'absolute';
        this.middleCanvas.style.zIndex = '-1';
        this.backgroundDiv.appendChild(this.middleCanvas);

        // Draw the serrated edges and middle section after layout
        const drawAll = () => {
            this._drawSerratedEdges();
            this._drawMiddleSection();
        };
        window.requestAnimationFrame(drawAll);

        // Redraw on window resize
        this._resizeHandler = drawAll;
        window.addEventListener('resize', this._resizeHandler);
    }

    _drawMiddleSection() {
        // Get the bounding rectangles
        const completeRect = this.el.getBoundingClientRect();

        const topOffset = this.topCanvas.height;
        const bottomOffset = this.bottomCanvas.height;
        const height = completeRect.height - topOffset - bottomOffset+1;
        // Set up the middle canvas
        const width = this.el.offsetWidth;
        this.middleCanvas.width = width;
        this.middleCanvas.height = height;
        this.middleCanvas.style.top = `${topOffset}px`;
        this.middleCanvas.style.left = '0';
        
        const midCtx = this.middleCanvas.getContext('2d');
        midCtx.clearRect(0, 0, width, height);
        midCtx.fillStyle = '#ffffff';
        midCtx.beginPath();
        // 6-point path: top-left, top-right, mid-right, bottom-right, bottom-left, mid-left
        midCtx.moveTo(0, 0); // top-left
        midCtx.lineTo(width, 0); // top-right
        midCtx.lineTo(width, height / 2); // mid-right
        midCtx.lineTo(width, height); // bottom-right
        midCtx.lineTo(0, height); // bottom-left
        midCtx.lineTo(0, height / 2); // mid-left
        midCtx.closePath();
        midCtx.fill();
    }

    _drawSerratedEdges() {
        const width = this.el.offsetWidth;
        const height = TOOTH_HEIGHT;

        // Setup top canvas
        this.topCanvas.width = width;
        this.topCanvas.height = height;
        this.topCanvas.style.height = `${height}px`;
        const topCtx = this.topCanvas.getContext('2d');

        // Setup bottom canvas
        this.bottomCanvas.width = width;
        this.bottomCanvas.height = height;
        this.bottomCanvas.style.height = `${height}px`;
        const bottomCtx = this.bottomCanvas.getContext('2d');

        // Draw top serrated edge (triangles pointing down)
        topCtx.fillStyle = '#ffffff';
        topCtx.beginPath();
        for (let x = 0; x < width; x += TOOTH_WIDTH) {
            if (x === 0) {
                topCtx.moveTo(x, height);
            }
            topCtx.lineTo(x + TOOTH_WIDTH / 2, 0); // Peak
            topCtx.lineTo(x + TOOTH_WIDTH, height); // Valley
        }
        topCtx.lineTo(width, height);
        topCtx.lineTo(width, height);
        topCtx.lineTo(0, height);
        topCtx.closePath();
        topCtx.fill();

        // Draw bottom serrated edge (triangles pointing down, inverted)
        bottomCtx.fillStyle = '#ffffff';
        bottomCtx.beginPath();
        for (let x = 0; x < width; x += TOOTH_WIDTH) {
            if (x === 0) {
                bottomCtx.moveTo(x, 0);
            }
            bottomCtx.lineTo(x + TOOTH_WIDTH / 2, height); // Peak
            bottomCtx.lineTo(x + TOOTH_WIDTH, 0); // Valley
        }
        bottomCtx.lineTo(width, height);
        bottomCtx.lineTo(width, 0);
        bottomCtx.lineTo(0, 0);
        bottomCtx.closePath();
        bottomCtx.fill();
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
