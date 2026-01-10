import Map from './modules/Map.js';

// Entry point for the app

document.addEventListener('DOMContentLoaded', () => {
    const mapEl = document.getElementById('map');
    if (!mapEl) return;
    window._map = new Map(mapEl, { gridSize: 40, lineColor: 'rgba(0,0,0,0.12)', gridSize:64});
    // Ensure camera is at 0,0 on load
    window._map.camera.x = 0;
    window._map.camera.y = 0;
    window._map.updateMapView();
    // Ensure crosshair is visible/hidden according to checkbox on load
    const crosshair = document.getElementById('crosshair');
    const toggleCrosshair = document.getElementById('toggleCrosshair');
    if (crosshair && toggleCrosshair) {
        crosshair.classList.toggle('hidden', !toggleCrosshair.checked);
    }
});
