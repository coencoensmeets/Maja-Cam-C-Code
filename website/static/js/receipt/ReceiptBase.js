// ReceiptBase.js
// Base class for receipt-style overlays. Extend this for custom receipts.

import ReceiptDivider from '/static/js/receipt/ReceiptDivider.js';

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

        // Content is provided by subclass or options
        this.el.innerHTML = this._getContentHTML();

        // Position at world origin (0,0)
        this.el.style.position = 'absolute';
        this.el.style.left = '0px';
        this.el.style.top = '0px';

        this.world.appendChild(this.el);
        this._bindEvents();

        // Divider logic (optional, can be disabled)
        if (this.options.divider !== false) {
            this.divider = new ReceiptDivider(this.el, { selector: '.map-receipt-divider', ratio: 0.95 });
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
