// ReceiptBase.js
// Base class for receipt-style overlays. Extend this for custom receipts.


import ReceiptDivider from '/static/js/receipt/ReceiptDivider.js';

// Serrated edge tooth size constants
const TOOTH_WIDTH = 12; // Width of each triangle tooth in px
const TOOTH_HEIGHT = 8; // Height of each triangle tooth in px
const SIDE_BEND = 20; // Margin for red background

export default class ReceiptBase {
    constructor(world, options = {}) {
        if (!world) throw new Error('world element required');
        this.world = world;
        this.options = options;
        // Initialize position properties
        this.x = 0;
        this.y = 0;
        this._createElement();
    }

    _createElement() {
        this.el = document.createElement('div');
        this.el.className = 'map-receipt';
        this.el.setAttribute('role', 'region');
        this.el.setAttribute('aria-label', this.options.ariaLabel || 'Receipt');

        // Position absolutely within world container
        this.el.style.position = 'absolute';
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

        // Canvas is larger to allow for margin
        const canvasWidth = width + SIDE_BEND * 2;
        const canvasHeight = height;

        this.canvas.width = canvasWidth;
        this.canvas.height = canvasHeight;
        this.canvas.style.width = `${canvasWidth}px`;
        this.canvas.style.height = `${canvasHeight}px`;

        // Position canvas so the margin is visible around the receipt
        this.canvas.style.left = `-${SIDE_BEND}px`;

        const ctx = this.canvas.getContext('2d');
        ctx.clearRect(0, 0, canvasWidth, canvasHeight);

        // Draw shadow
        const grad = ctx.createLinearGradient(
            0 , canvasHeight/2,
            canvasWidth, canvasHeight/2
        );
        grad.addColorStop(0.01, 'rgba(0,0,0,0.0)');      // Start transparent
        grad.addColorStop(0.2, 'rgb(0, 0, 0,0.3)'); // Middle black
        grad.addColorStop(0.8, 'rgb(0, 0, 0,0.3)'); // Middle black
        grad.addColorStop(0.99, 'rgba(0,0,0,0.0)');      // End transparent

        ctx.translate(0, TOOTH_HEIGHT);
        ctx.beginPath();
        // Top edge
        ctx.moveTo(SIDE_BEND, 0);
        ctx.lineTo(canvasWidth - SIDE_BEND, 0);
        // Right edge with rounded bulge
        ctx.quadraticCurveTo(
            canvasWidth, 
            canvasHeight / 2-TOOTH_HEIGHT*2, 
            canvasWidth - SIDE_BEND, 
            canvasHeight-TOOTH_HEIGHT*2
        );
        // Bottom edge
        ctx.lineTo(SIDE_BEND, canvasHeight-TOOTH_HEIGHT*2);
        // Left edge with rounded bulge
        ctx.quadraticCurveTo(
            0, 
            canvasHeight / 2, 
            SIDE_BEND, 
            0
        );
        ctx.closePath();
        ctx.fillStyle = grad;
        ctx.fill();

        // Draw white receipt shape inset by margin
        ctx.save();
        ctx.translate(SIDE_BEND, -TOOTH_HEIGHT);

        ctx.fillStyle = '#ffffff';
        ctx.beginPath();

        // Start at top-left, just below the first tooth valley
        ctx.moveTo(0, TOOTH_HEIGHT);

        
        const halfTooth = TOOTH_WIDTH / 2;

        // Draw top serrated edge (triangles pointing up)
        let x = 0;
        let isValley = true;
        while (x < width) {
            ctx.lineTo(x, isValley ? TOOTH_HEIGHT : 0);
            isValley = !isValley;
            x += halfTooth;
        }
        if (!isValley){
            ctx.lineTo(width, TOOTH_HEIGHT-(width-(x-(TOOTH_WIDTH/2)))*(TOOTH_HEIGHT/(TOOTH_WIDTH/2))); // Adjust last peak if needed
        }
        else {
            ctx.lineTo(width, TOOTH_HEIGHT); // Last valley
        }

        // Right edge down to bottom serrated area
        ctx.lineTo(width, height - TOOTH_HEIGHT);

        // Draw bottom serrated edge (triangles pointing down) - right to left
        x = width;
        isValley = true;
        while (x > 0) {
            ctx.lineTo(x, isValley ? height - TOOTH_HEIGHT : height);
            isValley = !isValley;
            x -= halfTooth;
        }
        if (!isValley){
            ctx.lineTo(0, height - TOOTH_HEIGHT + (x+(TOOTH_WIDTH/2))*(TOOTH_HEIGHT/(TOOTH_WIDTH/2))); // Adjust last peak if needed
        }
        else {
            ctx.lineTo(0, height - TOOTH_HEIGHT);
        }

        // Left edge back up to start
        ctx.lineTo(0, TOOTH_HEIGHT);
        ctx.closePath();
        ctx.fill();

        ctx.restore();
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
            if (timeoutId) clearTimeout(timeoutId);
            this.el.removeEventListener('animationend', onEnd);
            this.el.classList.remove('map-receipt-animate');
            this._animating = false;
            this._onAction(action);
        };
        this.el.classList.add('map-receipt-animate');
        this.el.addEventListener('animationend', onEnd);
        // Fallback: if no animation fires within 50ms, call onEnd anyway
        const timeoutId = setTimeout(onEnd, 50);
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

    get centreCoords() {
        if (!this.el) return { x: 0, y: 0 };
        const rect = this.el.getBoundingClientRect();
        return {
            x: rect.left + rect.width / 2,
            y: rect.top + rect.height / 2
        };
    }

    setPosition(x, y) {
        this.x = x;
        this.y = y;
        if (this.el) {
            this.el.style.left = `${x}px`;
            this.el.style.top = `${y}px`;
        }
    }

    get position() {
        return { x: this.x, y: this.y };
    }
}
