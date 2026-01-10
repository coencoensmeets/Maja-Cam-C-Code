// ReceiptLayoutManager.js
// Manages receipt placement and layout with collision detection

export default class ReceiptLayoutManager {
    constructor(options = {}) {
        this.padding = options.padding || 30;
        this.startRadius = options.startRadius || 150;
        this.radiusIncrement = options.radiusIncrement || 30;
        this.placedBounds = [];
        this.receipts = [];
    }

    placeReceipt(receipt, x, y) {
        receipt.setPosition(x, y);
        
        // Cache dimensions on receipt object
        const width = receipt.receipt.offsetWidth;
        const height = receipt.receipt.offsetHeight;
        receipt._cachedWidth = width;
        receipt._cachedHeight = height;
        
        // Add to receipts array
        this.receipts.push(receipt);
        
        // Track bounds for collision detection
        this.placedBounds.push({
            x: x - width / 2,
            y: y - height / 2,
            width: width,
            height: height
        });
    }

    findPlacementPosition(receipt) {
        // Temporarily position to get dimensions
        receipt.setPosition(0, 0);
        
        const width = receipt.receipt.offsetWidth;
        const height = receipt.receipt.offsetHeight;
        let searchRadius = this.startRadius;

        // Search in expanding circles until we find space
        while (true) {
            const angleSteps = Math.max(16, Math.floor((searchRadius * 2 * Math.PI) / 100));
            
            for (let angleIndex = 0; angleIndex < angleSteps; angleIndex++) {
                const angle = (angleIndex / angleSteps) * Math.PI * 2;
                const x = Math.cos(angle) * searchRadius;
                const y = Math.sin(angle) * searchRadius;
                
                if (!this._hasCollision(x, y, width, height)) {
                    return { x, y };
                }
            }
            
            searchRadius += this.radiusIncrement;
        }
    }

    _hasCollision(x, y, width, height) {
        const bounds = {
            x: x - width / 2,
            y: y - height / 2,
            width: width,
            height: height
        };

        return this.placedBounds.some(other => 
            this._boundsOverlap(bounds, other, this.padding)
        );
    }

    _boundsOverlap(rect1, rect2, margin = 0) {
        return !(
            rect1.x + rect1.width + margin < rect2.x ||
            rect2.x + rect2.width + margin < rect1.x ||
            rect1.y + rect1.height + margin < rect2.y ||
            rect2.y + rect2.height + margin < rect1.y
        );
    }

    clear() {
        this.placedBounds = [];
        this.receipts = [];
    }

    getPlacedBounds() {
        return this.placedBounds;
    }

    getReceipts() {
        return this.receipts;
    }

    getReceiptCount() {
        return this.receipts.length;
    }

    /**
     * Remove a receipt from tracking, freeing up its space for new receipts
     * @param {ReceiptBase} receipt - The receipt to remove
     */
    removeReceipt(receipt) {
        // Find and remove from receipts array
        const receiptIndex = this.receipts.indexOf(receipt);
        if (receiptIndex !== -1) {
            this.receipts.splice(receiptIndex, 1);
            
            // Also remove the corresponding bounds
            if (receiptIndex < this.placedBounds.length) {
                this.placedBounds.splice(receiptIndex, 1);
            }
        }
    }

    /**
     * Check if there's space for a new receipt next to a placed receipt
     * Samples positions around the entire perimeter to find space closest to (0,0)
     * @param {ReceiptBase} newReceipt - The unplaced receipt to find space for
     * @param {ReceiptBase} placedReceipt - The already placed receipt to check around
     * @returns {Object|null} - {x, y} center position if space available, null otherwise
     */
    findSpaceNextTo(newReceipt, placedReceipt) {
        const placedRect = placedReceipt.receipt.getBoundingClientRect();
        const newRect = newReceipt.receipt.getBoundingClientRect();

        const validPositions = [];
        
        // Calculate how many sample points to check based on receipt dimensions
        const samplesPerSide = Math.ceil(Math.max(placedRect.width, placedRect.height) / newRect.width);

        // Check all four edges
        this._checkEdgePositions(placedReceipt, placedRect, newRect, samplesPerSide, validPositions);

        // Return the position closest to (0,0), or null if no valid positions
        if (validPositions.length === 0) return null;
        
        validPositions.sort((a, b) => a.distance - b.distance);
        return { x: validPositions[0].x, y: validPositions[0].y };
    }

    _checkEdgePositions(placedReceipt, placedRect, newRect, samplesPerSide, validPositions) {
        const edges = [
            // Right edge
            (i) => ({
                x: placedReceipt.x + placedRect.width / 2 + this.padding + newRect.width / 2,
                y: placedReceipt.y - placedRect.height / 2 + (i / samplesPerSide) * placedRect.height
            }),
            // Left edge
            (i) => ({
                x: placedReceipt.x - placedRect.width / 2 - this.padding - newRect.width / 2,
                y: placedReceipt.y - placedRect.height / 2 + (i / samplesPerSide) * placedRect.height
            }),
            // Bottom edge
            (i) => ({
                x: placedReceipt.x - placedRect.width / 2 + (i / samplesPerSide) * placedRect.width,
                y: placedReceipt.y + placedRect.height / 2 + this.padding + newRect.height / 2
            }),
            // Top edge
            (i) => ({
                x: placedReceipt.x - placedRect.width / 2 + (i / samplesPerSide) * placedRect.width,
                y: placedReceipt.y - placedRect.height / 2 - this.padding - newRect.height / 2
            })
        ];

        for (const edgeFunc of edges) {
            for (let i = 0; i <= samplesPerSide; i++) {
                const pos = edgeFunc(i);
                this._checkAndAddPosition(pos.x, pos.y, newRect, validPositions);
            }
        }
    }

    _checkAndAddPosition(x, y, newRect, validPositions) {
        const targetBounds = {
            x: x - newRect.width / 2,
            y: y - newRect.height / 2,
            width: newRect.width,
            height: newRect.height
        };

        const hasOverlap = this.placedBounds.some(other => 
            this._boundsOverlap(targetBounds, other, this.padding)
        );

        if (!hasOverlap) {
            const distance = Math.sqrt(x * x + y * y);
            validPositions.push({ x, y, distance });
        }
    }
}
