import ReceiptDivider from '/static/js/receipt/ReceiptDivider.js';

// poems embedded in JS so a poem is available instantly (no network request)
const FALLBACK_POEMS = [
    "I take a picture from the light,\nThen let its fragile shape dissolve.\nWhat once was seen becomes a poem—\nA softer way the moment’s told.",
    "I steal an image from the dark,\nAnd watch it quietly decay.\nThe sight returns to you as verse,\nThe photograph has slipped away.",
    "I take thy image as it stands,\nYet bind it not in lifeless frame.\nI turn the sight to measured words,\nAnd give thee verse in place of name.",
    "I take the picture.\nI let it go.\nWhat comes back\nIs a poem of what I saw.",
    "I take the image for a breath,\nThen free it from the need to stay.\nIt learns to speak as quiet verse—\nThe moment, answered in this way.",
    "I borrow the light and give it back\nAs language on a curling page.\nThe picture fades, the poem stays,\nA different record of the same.",
    "This camera captures a photograph.\nThe image is not preserved.\nIt is translated into a poem\nAnd returned as text.",
    "I take your picture, then erase it.\nWhat survives is how it felt.\nYou hold a poem in your hand—\nThe image, undone.",
    "I take the light, I lose the form,\nI keep the echo of the view.\nThe picture turns itself to verse\nBefore returning home to you.",
    "I take a picture, not to keep.\nI let it change to something said.\nWhat you receive is not the image—\nIt is the poem it became."
];

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
            <div>
                <h2 class="map-receipt-title">Maja poetry cam</h2>
                <div class="map-receipt-sub">by Coen Smeets</div>
            </div>
            <div class="map-receipt-divider" aria-hidden="true">-_-_-_-_-_-</div>

            <div class="map-receipt-buttons">
                <button class="map-receipt-btn" data-action="Settings" aria-label="Settings">Settings</button>
                <button class="map-receipt-btn" data-action="log" aria-label="Log">log</button>
                <button class="map-receipt-btn" data-action="filters" aria-label="Filters">filters</button>
            </div>
            <div class="map-receipt-poem" aria-live="polite"></div>
        `;

        // Position at world origin (0,0)
        this.el.style.position = 'absolute';
        this.el.style.left = '0px';
        this.el.style.top = '0px';

        this.world.appendChild(this.el);
        this._bindEvents();

        // attach a divider controller to manage filling and resizing (random style)
        this.divider = new ReceiptDivider(this.el, { selector: '.map-receipt-divider', ratio: 0.95 });

        // No JS button layout logic needed; CSS handles responsive stacking

        // render a poem immediately from the fallback list so text is visible
        const immediatePoem = FALLBACK_POEMS[Math.floor(Math.random() * FALLBACK_POEMS.length)];
        const immediateEl = this.el.querySelector('.map-receipt-poem');
        if (immediateEl && immediatePoem) {
            immediateEl.innerHTML = immediatePoem.split('\n').map((line) => this._escapeHTML(line)).join('<br>');
        }

        // poems are embedded in JS; no fetch needed — nothing else to do here
    }

    _escapeHTML(str) {
        return String(str)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#39;');
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
