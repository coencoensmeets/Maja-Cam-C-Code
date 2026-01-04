import ReceiptDivider from '/static/js/receipt/ReceiptDivider.js';

export default class HomeScreenReceipt {
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
        this.el.setAttribute('aria-label', 'Welcome receipt');

        this.el.innerHTML = `
            <h2 class="map-receipt-title">Maja poetry cam</h2>
            <div class="map-receipt-sub">by Coen Smeets</div>
            <div class="map-receipt-divider" aria-hidden="true">-_-_-_-_-_-</div>

            <div class="map-receipt-buttons">
                <button class="map-receipt-btn" data-action="Settings" aria-label="Settings">Settings</button>
                <button class="map-receipt-btn" data-action="log" aria-label="Log">log</button>
                <button class="map-receipt-btn" data-action="filters" aria-label="Filters">filters</button>
            </div>
        `;

        // Position at world origin (0,0)
        this.el.style.position = 'absolute';
        this.el.style.left = '0px';
        this.el.style.top = '0px';

        this.world.appendChild(this.el);
        this._bindEvents();

        // attach a divider controller to manage filling and resizing
        this.divider = new ReceiptDivider(this.el, { selector: '.map-receipt-divider', pattern: '-_-', ratio: 0.9 });

        // init responsive button layout observer
        this._initButtonsLayout();
    }

    _initButtonsLayout() {
        this._buttonsEl = this.el.querySelector('.map-receipt-buttons');
        if (!this._buttonsEl) return;
        this._updateButtonsLayout = this._updateButtonsLayout.bind(this);
        // use ResizeObserver to react to size changes
        try {
            this._ro = new ResizeObserver(this._updateButtonsLayout);
            this._ro.observe(this.el);
        } catch (err) {
            // fallback to window resize
            window.addEventListener('resize', this._updateButtonsLayout);
        }
        // initial check
        this._updateButtonsLayout();
    }

    _updateButtonsLayout() {
        if (!this._buttonsEl) return;
        // If the buttons' scrollWidth fits within their visible width, keep inline
        const fitsInline = this._buttonsEl.scrollWidth <= this._buttonsEl.clientWidth + 1; // 1px tolerance
        this.el.classList.toggle('buttons-inline', fitsInline);
        this.el.classList.toggle('buttons-stack', !fitsInline);
    }

    _bindEvents() {
        this.el.addEventListener('click', (e) => {
            const btn = e.target.closest('.map-receipt-btn');
            if (!btn) return;
            const action = btn.dataset.action;
            // animate receipt then call callback
            this._animateThen(action);
        });
    }

    _animateThen(action) {
        if (this._animating) return; // prevent re-entry
        this._animating = true;
        const onEnd = () => {
            this.el.removeEventListener('animationend', onEnd);
            this.el.classList.remove('map-receipt-animate');
            this._animating = false;
            // call the actual callback after animation
            switch (action) {
                case 'settings':
                    if (typeof this.options.onSettingsClick === 'function') this.options.onSettingsClick();
                    break;
                case 'log':
                    if (typeof this.options.onLogClick === 'function') this.options.onLogClick();
                    break;
                case 'filters':
                    if (typeof this.options.onFiltersClick === 'function') this.options.onFiltersClick();
                    break;
            }
        };
        // trigger animation
        this.el.classList.add('map-receipt-animate');
        this.el.addEventListener('animationend', onEnd);
    }

    // divider logic handled by HomeScreenDivider

    show() {
        this.el.style.display = '';
    }

    hide() {
        this.el.style.display = 'none';
    }

    destroy() {
        if (this.el && this.el.parentNode) this.el.parentNode.removeChild(this.el);
        this.el = null;
        if (this._onResizeBound) {
            window.removeEventListener('resize', this._onResizeBound);
            this._onResizeBound = null;
        }
        if (this.divider && typeof this.divider.destroy === 'function') {
            this.divider.destroy();
            this.divider = null;
        }
    }
}
