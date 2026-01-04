// MapElement: base class for all map-related UI elements (flexible for future extensions)
export default class MapElement {
	constructor(options = {}) {
		this.options = options || {};
		this.elements = {};
		this._bindMethods();
		// Do not call mapElements() or _addListeners() here; let child call after required properties are set
	}

	// Override in child to bind methods to 'this'
	_bindMethods() {}

	// Override in child to map DOM elements
	mapElements() {}

	// Override in child to add event listeners
	_addListeners() {}

	// Common show/hide for all elements
	show() {
		if (this.el) this.el.style.display = '';
	}
	hide() {
		if (this.el) this.el.style.display = 'none';
	}
	// Common update method (optional, for future use)
	update() {}
}
