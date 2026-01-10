export default class ReceiptOption {
    constructor(receiptBase, options = {}) {
        this.receiptBase = receiptBase;
        this.options = options;

        // Create the option button
        this.el = document.createElement('div');
        this.el.className = `map-receipt-option receipt-option-${this.options.name || 'default'}`;
        this.el.textContent = this.options.name || 'Option';
        
        // Set gradient as CSS variable on the element
        const fadedColour = this.options.colour.replace(/0\.80\)/, '0.65)');
        const gradient = `linear-gradient(
            to right,
            ${this.options.colour} 0%,
            ${this.options.colour} 30%,
            ${fadedColour} 30%,
            ${fadedColour} 100%
        )`;
        this.el.style.setProperty('--gradient-bg', gradient);

        // Add random rotation and translation for hand-written feel
        const randomRotation = (Math.random() - 0.5) * 8; // -4 to +4 degrees
        const randomTranslateX = (Math.random() - 0.5) * 6; // -3 to +3 pixels
        const randomTranslateY = (Math.random() - 0.5) * 4; // -2 to +2 pixels
        this.el.style.transform = `rotate(${randomRotation}deg) translate(${randomTranslateX}px, ${randomTranslateY}px)`;

        // Add click handler
        this.el.addEventListener('click', (e) => {
            e.stopPropagation();
            if (typeof this.options.onClick === 'function') {
                this.options.onClick(this.receiptBase);
            } else {
                // Default: remove the receipt from DOM
                this.receiptBase.destroy();
            }
        });

        // Append to the receipt element
        this.receiptBase.receipt_options.appendChild(this.el);
    }
}