import ReceiptBase from '/static/js/receipt/ReceiptBase.js';

export default class LoremReceipt extends ReceiptBase {
    constructor(world, options = {}) {
        super(world, options);
    }

    _getContentHTML() {

        const titles = [
            'Sample Receipt',
            'Demo Document',
            'Lorem Ipsum Note',
            'Test Paper',
            'Fictional Invoice'
        ];
        const randomTitle = titles[Math.floor(Math.random() * titles.length)];

        const randomLengthIpsum = () => {
            const lines = [
                'Lorem ipsum dolor sit amet, consectetur adipiscing elit.',
                'Sed do eiusmod tempor incididunt ut labore et dolore.',
                'Ut enim ad minim veniam, quis nostrud exercitation.',
                'Duis aute irure dolor in reprehenderit in voluptate.',
                'Excepteur sint occaecat cupidatat non proident.',
                'Sunt in culpa qui officia deserunt mollit anim id est laborum.',
                'Curabitur pretium tincidunt lacus. Nulla gravida orci a odio.',
                'Nullam varius, turpis et commodo pharetra, est eros bibendum elit.',
            ];
            const lineCount = Math.floor(Math.random() * lines.length) + 2;
            return lines.slice(0, lineCount).join('<br>\n');
        }
        return `
            <div>
                <h2 class="map-receipt-title">${randomTitle}</h2>
            </div>
            <div class="map-receipt-divider" aria-hidden="true">-_-_-_-_-_-</div>
            <div class="map-receipt-poem">
                ${randomLengthIpsum()}
            </div>
        `;
    }
}
