// ViewportCuller.js
// Handles viewport-based culling of receipts for performance

export default class ViewportCuller {
    constructor(map, camera, options = {}) {
        this.map = map;
        this.camera = camera;
        this.enabled = options.enabled !== false;
        this.throttleMs = options.throttleMs || 10;
        this.viewportPadding = options.viewportPadding || 200;
        
        this.visibleReceipts = new Set();
        this.lastCullTime = 0;
    }

    setEnabled(enabled) {
        this.enabled = enabled;
    }

    cull(receipts) {
        if (!this.enabled || !receipts || receipts.length === 0) return;
        
        // Throttle culling checks
        const now = performance.now();
        if (now - this.lastCullTime < this.throttleMs) return;
        this.lastCullTime = now;
        
        const viewportBounds = this._calculateViewportBounds();
        const changes = this._determineVisibilityChanges(receipts, viewportBounds);
        this._applyVisibilityChanges(changes);
    }

    _calculateViewportBounds() {
        const viewportWidth = this.map.clientWidth;
        const viewportHeight = this.map.clientHeight;
        
        // Get viewport bounds in world coordinates
        const centerX = this.camera.cx;
        const centerY = this.camera.cy;
        const pixelX = this.camera.toPixels(-this.camera.x);
        const pixelY = this.camera.toPixels(-this.camera.y);
        const offsetX = centerX + pixelX * this.camera.z;
        const offsetY = centerY - pixelY * this.camera.z;
        
        return {
            left: (-offsetX - this.viewportPadding) / this.camera.z,
            right: (viewportWidth - offsetX + this.viewportPadding) / this.camera.z,
            top: (-offsetY - this.viewportPadding) / this.camera.z,
            bottom: (viewportHeight - offsetY + this.viewportPadding) / this.camera.z
        };
    }

    _determineVisibilityChanges(receipts, viewportBounds) {
        const toShow = [];
        const toHide = [];
        
        for (const receipt of receipts) {
            const isVisible = this._isReceiptInBounds(receipt, viewportBounds);
            
            if (isVisible && !this.visibleReceipts.has(receipt)) {
                toShow.push(receipt);
            } else if (!isVisible && this.visibleReceipts.has(receipt)) {
                toHide.push(receipt);
            }
        }
        
        return { toShow, toHide };
    }

    _isReceiptInBounds(receipt, bounds) {
        // Use cached dimensions to avoid layout thrashing
        const receiptWidth = receipt._cachedWidth || receipt.el.offsetWidth;
        const receiptHeight = receipt._cachedHeight || receipt.el.offsetHeight;
        const receiptLeft = receipt.x - receiptWidth / 2;
        const receiptRight = receipt.x + receiptWidth / 2;
        const receiptTop = receipt.y - receiptHeight / 2;
        const receiptBottom = receipt.y + receiptHeight / 2;
        
        return !(receiptRight < bounds.left || 
                receiptLeft > bounds.right || 
                receiptBottom < bounds.top || 
                receiptTop > bounds.bottom);
    }

    _applyVisibilityChanges({ toShow, toHide }) {
        if (toShow.length === 0 && toHide.length === 0) return;
        
        // Batch DOM updates in requestAnimationFrame
        requestAnimationFrame(() => {
            for (const receipt of toShow) {
                receipt.el.classList.remove('culled');
                this.visibleReceipts.add(receipt);
            }
            for (const receipt of toHide) {
                receipt.el.classList.add('culled');
                this.visibleReceipts.delete(receipt);
            }
        });
    }
}
