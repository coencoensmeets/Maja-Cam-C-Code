
import ReceiptBase from '/static/js/receipt/ReceiptBase.js';


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

export default class HomeScreenReceipt extends ReceiptBase {
    constructor(world, options = {}) {
        super(world, options);
        console.log('HomeScreenReceipt constructed');
        // render a poem immediately from the fallback list so text is visible
        const immediatePoem = FALLBACK_POEMS[Math.floor(Math.random() * FALLBACK_POEMS.length)];
        const immediateEl = this.el.querySelector('.map-receipt-poem');
        if (immediateEl && immediatePoem) {
            immediateEl.innerHTML = immediatePoem.split('\n').map((line) => this._escapeHTML(line)).join('<br>');
        }
    }

    _getContentHTML() {
        return `
                    <div>
                        <h2 class="map-receipt-title">Maja poetry cam</h2>
                        <div class="map-receipt-sub">by Coen Smeets</div>
                    </div>
                    <div class="map-receipt-divider" aria-hidden="true">-_-_-_-_-_-</div>
                    <div class="map-receipt-buttons">
                        <button class="map-receipt-btn" data-action="settings" aria-label="Settings" style="text-decoration: underline;">Settings</button>
                        <button class="map-receipt-btn" data-action="log" aria-label="Log" style="text-decoration: underline;">log</button>
                        <button class="map-receipt-btn" data-action="filters" aria-label="Filters" style="text-decoration: underline;">filters</button>
                    </div>
                    <div class="map-receipt-poem" aria-live="polite"></div>
                `;
    }

    _escapeHTML(str) {
        return String(str)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#39;');
    }

    _onAction(action) {
        console.log('HomeScreenReceipt _onAction called with action:', action);
        // Handle button clicks with default implementations
        switch (action) {
            case 'settings':
                if (typeof this.options.onSettingsClick === 'function') {
                    this.options.onSettingsClick();
                } else {
                    this._onSettingsClick();
                }
                break;
            case 'log':
                if (typeof this.options.onLogClick === 'function') {
                    this.options.onLogClick();
                } else {
                    this._onLogClick();
                }
                break;
            case 'filters':
                if (typeof this.options.onFiltersClick === 'function') {
                    this.options.onFiltersClick();
                } else {
                    this._onFiltersClick();
                }
                break;
            default:
                if (typeof this.options.onAction === 'function') this.options.onAction(action);
        }
    }

    // Default click handlers
    _onSettingsClick() {
        console.log('Home receipt: settings clicked');
        // future implementation: open settings menu
    }

    _onLogClick() {
        console.log('Home receipt: log clicked');
        // future implementation: show logs
    }

    _onFiltersClick() {
        console.log('Home receipt: filters clicked');
        // future implementation: show filter options
    }
}
