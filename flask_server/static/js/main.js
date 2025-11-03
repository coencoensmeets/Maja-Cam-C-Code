// Main JavaScript for Poem Camera Web Interface

// Take picture - queues command for ESP32 to poll
async function takePicture(event) {
    try {
        // Show loading state
        const button = event ? event.target : document.querySelector('.btn-primary');
        const originalText = button.textContent;
        button.textContent = '📸 Queuing command...';
        button.disabled = true;
        
        const response = await fetch('/api/trigger-capture', {
            method: 'POST'
        });
        
        const data = await response.json();
        
        // Restore button
        button.textContent = originalText;
        button.disabled = false;
        
        if (response.ok) {
            showMessage('Capture command queued! ESP32 will capture within 1 second...', 'success');
            // Refresh gallery after 3 seconds to show new image
            setTimeout(refreshGallery, 3000);
            // Update queue status immediately
            checkQueueStatus();
        } else {
            showMessage(data.error || 'Failed to queue capture', 'error');
        }
    } catch (error) {
        showMessage('Error: ' + error.message, 'error');
        console.error('Capture error:', error);
    }
}

// Show selected filename
document.getElementById('image-input').addEventListener('change', function(e) {
    const fileName = e.target.files[0]?.name || 'Choose an image...';
    document.getElementById('file-name').textContent = fileName;
});

// Handle form submission
document.getElementById('upload-form').addEventListener('submit', async function(e) {
    e.preventDefault();
    
    const formData = new FormData();
    const fileInput = document.getElementById('image-input');
    const messageDiv = document.getElementById('upload-message');
    
    if (!fileInput.files[0]) {
        showMessage('Please select an image first', 'error');
        return;
    }
    
    formData.append('image', fileInput.files[0]);
    
    try {
        // Show loading state
        const submitBtn = e.target.querySelector('button[type="submit"]');
        const originalText = submitBtn.textContent;
        submitBtn.textContent = 'Uploading...';
        submitBtn.disabled = true;
        
        const response = await fetch('/upload', {
            method: 'POST',
            body: formData
        });
        
        const data = await response.json();
        
        if (response.ok) {
            showMessage(data.message || 'Image uploaded successfully!', 'success');
            fileInput.value = '';
            document.getElementById('file-name').textContent = 'Choose an image...';
            
            // Refresh gallery after 1 second
            setTimeout(refreshGallery, 1000);
        } else {
            showMessage(data.error || 'Upload failed', 'error');
        }
        
        // Restore button state
        submitBtn.textContent = originalText;
        submitBtn.disabled = false;
        
    } catch (error) {
        showMessage('Network error: ' + error.message, 'error');
        console.error('Upload error:', error);
    }
});

// Show message function
function showMessage(message, type) {
    const messageDiv = document.getElementById('upload-message');
    messageDiv.textContent = message;
    messageDiv.className = `message ${type}`;
    
    // Auto-hide after 5 seconds
    setTimeout(() => {
        messageDiv.className = 'message';
    }, 5000);
}

// Refresh gallery
async function refreshGallery() {
    try {
        const response = await fetch('/images');
        const data = await response.json();
        
        const gallery = document.getElementById('gallery');
        
        if (data.images.length === 0) {
            gallery.innerHTML = `
                <div class="empty-state">
                    <p>📭 No images yet</p>
                    <p class="empty-subtitle">Upload your first image or capture one with your ESP32 camera</p>
                </div>
            `;
        } else {
            gallery.innerHTML = data.images.map(image => `
                <div class="gallery-item" data-filename="${image.filename}">
                    <img src="${image.url}" alt="${image.filename}" loading="lazy">
                    <div class="image-info">
                        <div class="image-name">${image.filename}</div>
                        <div class="image-meta">
                            <span>${(image.size / 1024).toFixed(1)} KB</span>
                            <span>${image.modified.substring(0, 10)}</span>
                        </div>
                        <div class="image-actions">
                            <a href="${image.url}" target="_blank" class="btn btn-small">View</a>
                            <button onclick="generatePoem('${image.filename}')" class="btn btn-small btn-poem">✨ Poem</button>
                            <button onclick="downloadImage('${image.url}', '${image.filename}')" class="btn btn-small">Download</button>
                            <button onclick="deleteImage('${image.filename}')" class="btn btn-small btn-danger">Delete</button>
                        </div>
                    </div>
                </div>
            `).join('');
        }
        
        // Update image count
        document.getElementById('image-count').textContent = data.images.length;
        
    } catch (error) {
        console.error('Error refreshing gallery:', error);
        showMessage('Failed to refresh gallery', 'error');
    }
}

// Download image
function downloadImage(url, filename) {
    const link = document.createElement('a');
    link.href = url;
    link.download = filename;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
}

// Delete image
async function deleteImage(filename) {
    if (!confirm(`Are you sure you want to delete ${filename}?`)) {
        return;
    }
    
    try {
        const response = await fetch(`/delete/${filename}`, {
            method: 'POST'
        });
        
        const data = await response.json();
        
        if (response.ok) {
            showMessage(data.message || 'Image deleted successfully', 'success');
            
            // Remove the image from gallery with animation
            const item = document.querySelector(`[data-filename="${filename}"]`);
            if (item) {
                item.style.opacity = '0';
                item.style.transform = 'scale(0.8)';
                setTimeout(() => {
                    item.remove();
                    // Update count
                    const count = document.getElementById('image-count');
                    count.textContent = parseInt(count.textContent) - 1;
                    
                    // Show empty state if no images left
                    const gallery = document.getElementById('gallery');
                    if (gallery.children.length === 0) {
                        gallery.innerHTML = `
                            <div class="empty-state">
                                <p>📭 No images yet</p>
                                <p class="empty-subtitle">Upload your first image or capture one with your ESP32 camera</p>
                            </div>
                        `;
                    }
                }, 300);
            }
        } else {
            showMessage(data.error || 'Failed to delete image', 'error');
        }
    } catch (error) {
        showMessage('Network error: ' + error.message, 'error');
        console.error('Delete error:', error);
    }
}

// Check server health on load
async function checkHealth() {
    try {
        const response = await fetch('/health');
        if (response.ok) {
            console.log('Server health check: OK');
        }
    } catch (error) {
        console.warn('Server health check failed:', error);
    }
}

// Check queue status
async function checkQueueStatus() {
    try {
        const response = await fetch('/api/queue-status');
        if (response.ok) {
            const data = await response.json();
            updateQueueIndicator(data.queue_count);
        }
    } catch (error) {
        console.warn('Queue status check failed:', error);
    }
}

// Update queue indicator display
function updateQueueIndicator(count) {
    const indicator = document.getElementById('queue-indicator');
    const status = document.getElementById('queue-status');
    
    status.textContent = `Queue: ${count}`;
    
    if (count > 0) {
        indicator.classList.add('has-queue');
    } else {
        indicator.classList.remove('has-queue');
    }
}

// Initialize
document.addEventListener('DOMContentLoaded', function() {
    checkHealth();
    checkQueueStatus();
    getUserLocation(); // Get user location on page load
    console.log('Poem Camera Web Interface loaded');
    
    // Auto-refresh gallery every 5 seconds
    setInterval(refreshGallery, 5000);
    
    // Check queue status every 1 second
    setInterval(checkQueueStatus, 1000);
});

// Generate poem for an image
let currentPoemFilename = null;
let userLocation = null;

// Get user location on page load
async function getUserLocation() {
    if (navigator.geolocation) {
        try {
            const position = await new Promise((resolve, reject) => {
                navigator.geolocation.getCurrentPosition(resolve, reject);
            });
            
            userLocation = {
                latitude: position.coords.latitude,
                longitude: position.coords.longitude,
                accuracy: position.coords.accuracy
            };
            
            console.log('User location obtained:', userLocation);
        } catch (error) {
            console.log('Location access denied or unavailable:', error.message);
            userLocation = null;
        }
    }
}

async function generatePoem(filename) {
    currentPoemFilename = filename;
    const modal = document.getElementById('poem-modal');
    const loading = document.getElementById('poem-loading');
    const poemText = document.getElementById('poem-text');
    const poetSelect = document.getElementById('poet-style');
    
    // Show modal with loading state
    modal.style.display = 'block';
    loading.style.display = 'block';
    poemText.style.display = 'none';
    poemText.textContent = '';
    
    // Reset to general style
    poetSelect.value = 'general';
    
    await fetchPoem(filename, 'general');
}

async function regeneratePoem() {
    if (!currentPoemFilename) return;
    
    const poetSelect = document.getElementById('poet-style');
    const poetStyle = poetSelect.value;
    const loading = document.getElementById('poem-loading');
    const poemText = document.getElementById('poem-text');
    
    // Show loading immediately when changing poet
    loading.style.display = 'block';
    poemText.style.display = 'none';
    
    await fetchPoem(currentPoemFilename, poetStyle);
}

async function fetchPoem(filename, poetStyle) {
    const loading = document.getElementById('poem-loading');
    const poemText = document.getElementById('poem-text');
    
    // Show loading
    loading.style.display = 'block';
    poemText.style.display = 'none';
    
    try {
        // Build request body with poet style and location if available
        const requestBody = { poet_style: poetStyle };
        
        if (userLocation) {
            requestBody.location = userLocation;
        }
        
        const response = await fetch(`/api/generate-poem/${filename}`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(requestBody)
        });
        
        const data = await response.json();
        
        if (response.ok) {
            // Hide loading, show poem
            loading.style.display = 'none';
            poemText.textContent = data.poem;
            poemText.style.display = 'block';
        } else {
            loading.style.display = 'none';
            poemText.innerHTML = `<p class="error">❌ ${data.error || 'Failed to generate poem'}</p>`;
            poemText.style.display = 'block';
        }
    } catch (error) {
        loading.style.display = 'none';
        poemText.innerHTML = `<p class="error">❌ Network error: ${error.message}</p>`;
        poemText.style.display = 'block';
        console.error('Poem generation error:', error);
    }
}

// Close poem modal
function closePoemModal() {
    const modal = document.getElementById('poem-modal');
    modal.style.display = 'none';
    currentPoemFilename = null;
}

// Copy poem to clipboard
async function copyPoem() {
    const poemText = document.getElementById('poem-text').textContent;
    
    // Try modern clipboard API first
    if (navigator.clipboard && navigator.clipboard.writeText) {
        try {
            await navigator.clipboard.writeText(poemText);
            showMessage('Poem copied to clipboard!', 'success');
            return;
        } catch (error) {
            console.warn('Clipboard API failed, trying fallback:', error);
        }
    }
    
    // Fallback method for older browsers or non-secure contexts
    try {
        const textArea = document.createElement('textarea');
        textArea.value = poemText;
        textArea.style.position = 'fixed';
        textArea.style.left = '-999999px';
        textArea.style.top = '-999999px';
        document.body.appendChild(textArea);
        textArea.focus();
        textArea.select();
        
        const successful = document.execCommand('copy');
        document.body.removeChild(textArea);
        
        if (successful) {
            showMessage('Poem copied to clipboard!', 'success');
        } else {
            throw new Error('Copy command failed');
        }
    } catch (error) {
        console.error('Copy failed:', error);
        showMessage('Failed to copy poem. Please select and copy manually.', 'error');
    }
}

// Close modal when clicking outside
window.onclick = function(event) {
    const modal = document.getElementById('poem-modal');
    if (event.target === modal) {
        closePoemModal();
    }
}
