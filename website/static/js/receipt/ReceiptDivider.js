export default class ReceiptDivider {
    constructor(parentEl, options = {}) {
        if (!parentEl) throw new Error('parent element required');
        this.parent = parentEl;
        this.selector = options.selector || '.map-receipt-divider';
        this.pattern = options.pattern || '-_-';
        this.ratio = typeof options.ratio === 'number' ? options.ratio : 0.9;
        this.maxIterations = options.maxIterations || 200;

        // find or create divider element inside parent
        this.el = (typeof this.selector === 'string') ? this.parent.querySelector(this.selector) : null;
        if (!this.el) {
            this.el = document.createElement('div');
            this.el.className = (this.selector && this.selector.replace(/^\./, '')) || 'divider';
            this.parent.insertBefore(this.el, this.parent.querySelector('.map-receipt-buttons') || null);
        }

        this._onResize = this._onResize.bind(this);
        window.addEventListener('resize', this._onResize);
        // initial fill after layout
        requestAnimationFrame(() => this.fill());
    }

    _onResize() {
        // debounce slightly by rAF
        if (this._raf) cancelAnimationFrame(this._raf);
        this._raf = requestAnimationFrame(() => this.fill());
    }

    fill() {
        if (!this.el || !this.parent) return;
        const divider = this.el;
        const containerWidth = Math.max(0, this.parent.clientWidth);
        const targetWidth = Math.floor(containerWidth * this.ratio);

        const computed = getComputedStyle(divider);
        const measurer = document.createElement('span');
        measurer.style.visibility = 'hidden';
        measurer.style.whiteSpace = 'nowrap';
        measurer.style.font = computed.font;
        if (computed.letterSpacing) measurer.style.letterSpacing = computed.letterSpacing;
        document.body.appendChild(measurer);

        // measure pattern
        measurer.textContent = this.pattern;
        const patternWidth = Math.ceil(measurer.getBoundingClientRect().width);
        if (patternWidth > targetWidth) {
            divider.style.display = 'none';
            document.body.removeChild(measurer);
            return;
        }

        let repeats = Math.max(1, Math.floor(targetWidth / patternWidth));
        let text = this.pattern.repeat(repeats);
        measurer.textContent = text;

        while (repeats > 1 && Math.ceil(measurer.getBoundingClientRect().width) > targetWidth) {
            repeats--;
            text = this.pattern.repeat(repeats);
            measurer.textContent = text;
        }

        document.body.removeChild(measurer);

        divider.style.display = '';
        divider.style.whiteSpace = 'nowrap';
        divider.style.overflow = 'hidden';
        divider.style.textAlign = 'center';
        divider.textContent = text;
    }

    destroy() {
        if (this._raf) cancelAnimationFrame(this._raf);
        window.removeEventListener('resize', this._onResize);
        this._raf = null;
    }
}
