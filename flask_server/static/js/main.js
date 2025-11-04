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
            gallery.innerHTML = data.images.map((image, index) => {
                const hasPoems = image.poems && image.poems.length > 0;
                const poemCount = image.poem_count || 0;
                
                // Store poems data in a global cache for easy access
                if (hasPoems) {
                    window.poemsCache = window.poemsCache || {};
                    window.poemsCache[image.filename] = image.poems;
                }
                
                let poemBadge = '';
                let poemButton = '';
                
                if (hasPoems) {
                    poemBadge = `<div class="saved-poem-indicator">
                        <span class="poem-badge">📜 ${poemCount} Poem${poemCount > 1 ? 's' : ''}</span>
                    </div>`;
                    poemButton = `<button onclick="viewPoemsList('${image.filename}')" class="btn btn-small btn-poem">📜 View Poems</button>
                                  <button onclick="generatePoem('${image.filename}')" class="btn btn-small btn-poem">✨ Add Poem</button>`;
                } else {
                    poemButton = `<button onclick="generatePoem('${image.filename}')" class="btn btn-small btn-poem">✨ Generate Poem</button>`;
                }
                
                return `
                <div class="gallery-item" data-filename="${image.filename}">
                    <img src="${image.url}" alt="${image.filename}" loading="lazy">
                    <div class="image-info">
                        <div class="image-name">${image.filename}</div>
                        <div class="image-meta">
                            <span>${(image.size / 1024).toFixed(1)} KB</span>
                            <span>${image.modified.substring(0, 10)}</span>
                        </div>
                        ${poemBadge}
                        <div class="image-actions">
                            <a href="${image.url}" target="_blank" class="btn btn-small">View</a>
                            ${poemButton}
                            <button onclick="downloadImage('${image.url}', '${image.filename}')" class="btn btn-small">Download</button>
                            <button onclick="deleteImage('${image.filename}')" class="btn btn-small btn-danger">Delete</button>
                        </div>
                    </div>
                </div>
                `;
            }).join('');
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
    refreshGallery(); // Load gallery on page load
    checkQueueStatus();
    checkPoemStatus();
    getUserLocation(); // Get user location on page load
    console.log('Poem Camera Web Interface loaded');
    
    // Setup context input character counter
    const contextInput = document.getElementById('poem-context-input');
    if (contextInput) {
        contextInput.addEventListener('input', updateContextCharCount);
    }
    
    // Auto-refresh gallery every 5 seconds
    setInterval(refreshGallery, 5000);
    
    // Check queue status every 1 second
    setInterval(checkQueueStatus, 1000);
    
    // Check poem generation status every 1 second
    setInterval(checkPoemStatus, 1000);
});

// Check poem generation status
async function checkPoemStatus() {
    try {
        const response = await fetch('/api/poem-status');
        const data = await response.json();
        
        const statusElement = document.getElementById('poem-status');
        if (data.is_generating) {
            statusElement.textContent = '🎨 Generating...';
            statusElement.style.color = '#ff6b6b';
        } else {
            statusElement.textContent = 'Idle';
            statusElement.style.color = '#51cf66';
        }
    } catch (error) {
        console.error('Error checking poem status:', error);
    }
}

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
    const modalTitle = document.getElementById('poem-modal-title');
    const loading = document.getElementById('poem-loading');
    const poemText = document.getElementById('poem-text');
    const poemTitle = document.getElementById('poem-title');
    const poetSelect = document.getElementById('poet-style');
    const generateBtn = document.getElementById('generate-poem-btn');
    
    // Remove poems-list-container class
    poemText.classList.remove('poems-list-container');
    
    // Show poet selector for new poems
    document.querySelector('.poet-selector').style.display = 'block';
    
    // Show the generate button
    if (generateBtn) generateBtn.style.display = 'inline-block';
    
    // Show modal WITHOUT auto-generating
    modal.style.display = 'block';
    modalTitle.textContent = '✨ Generate Poem';
    loading.style.display = 'none';
    poemText.style.display = 'none';
    poemText.textContent = '';
    poemTitle.style.display = 'none';
    
    // Reset to general style
    poetSelect.value = 'general';
    
    // Hide poem actions until poem is generated
    document.querySelector('.poem-actions').style.display = 'none';
}

// Start poem generation when user clicks the button
async function startPoemGeneration() {
    const poetSelect = document.getElementById('poet-style');
    const selectedStyle = poetSelect.value;
    const modalTitle = document.getElementById('poem-modal-title');
    const generateBtn = document.getElementById('generate-poem-btn');
    
    // Hide the generate button during generation
    if (generateBtn) generateBtn.style.display = 'none';
    
    // Update modal title
    modalTitle.textContent = '✨ Generated Poem';
    
    // Show poem actions
    document.querySelector('.poem-actions').style.display = 'flex';
    
    await fetchPoem(currentPoemFilename, selectedStyle);
}

// View a saved poem (already generated)
function viewSavedPoem(poemData) {
    const modal = document.getElementById('poem-modal');
    const modalTitle = document.getElementById('poem-modal-title');
    const loading = document.getElementById('poem-loading');
    const poemText = document.getElementById('poem-text');
    const poemTitle = document.getElementById('poem-title');
    const poetSelect = document.getElementById('poet-style');
    const generateBtn = document.getElementById('generate-poem-btn');
    
    // Remove poems-list-container class
    poemText.classList.remove('poems-list-container');
    
    // Hide poet selector and generate button for saved poems
    document.querySelector('.poet-selector').style.display = 'none';
    if (generateBtn) generateBtn.style.display = 'none';
    
    // Show modal with poem
    modal.style.display = 'block';
    modalTitle.textContent = '✨ Generated Poem';
    loading.style.display = 'none';
    poemText.style.display = 'block';
    
    // Show poem actions
    document.querySelector('.poem-actions').style.display = 'flex';
    
    // Display the saved poem
    poemTitle.textContent = poemData.title || 'Untitled';
    poemTitle.style.display = 'block';
    poemText.innerHTML = `<p><em>Style: ${poemData.poet_style || 'General'}</em></p>` +
                         '<p>' + (poemData.poem_text || '').replace(/\n/g, '<br>') + '</p>';
}

// View list of poems for an image
let currentImageFilename = null;
let currentPoemsList = [];

function viewPoemsList(filename) {
    // Get poems from cache
    const poems = window.poemsCache && window.poemsCache[filename];
    
    if (!poems || poems.length === 0) {
        showMessage('No poems found for this image', 'error');
        return;
    }
    
    currentImageFilename = filename;
    currentPoemsList = poems;
    
    const modal = document.getElementById('poem-modal');
    const modalTitle = document.getElementById('poem-modal-title');
    const loading = document.getElementById('poem-loading');
    const poemText = document.getElementById('poem-text');
    const poemTitle = document.getElementById('poem-title');
    const generateBtn = document.getElementById('generate-poem-btn');
    
    // Hide poet selector, generate button, and loading
    document.querySelector('.poet-selector').style.display = 'none';
    if (generateBtn) generateBtn.style.display = 'none';
    loading.style.display = 'none';
    
    // Show modal
    modal.style.display = 'block';
    modalTitle.textContent = '✨ Your Poems';
    poemText.style.display = 'block';
    
    // Add class to prevent double scrollbar
    poemText.classList.add('poems-list-container');
    
    // Show poem actions
    document.querySelector('.poem-actions').style.display = 'flex';
    
    // Display list of poems
    poemTitle.textContent = `${poems.length} Poem${poems.length > 1 ? 's' : ''}`;
    poemTitle.style.display = 'block';
    
    let poemsHTML = '<div class="poems-list">';
    poems.forEach((poem, index) => {
        poemsHTML += `
            <div class="poem-item" id="poem-${poem.id}">
                <div class="poem-header">
                    <h3>${poem.title || 'Untitled'}</h3>
                    <span class="poem-style-badge">${poem.poet_style || 'General'}</span>
                </div>
                <div class="poem-content">
                    <p>${(poem.poem_text || '').replace(/\n/g, '<br>')}</p>
                </div>
                <div class="poem-actions">
                    <button onclick="printSinglePoem(${index})" class="btn btn-small">🖨️ Print</button>
                    <button onclick="deletePoem('${filename}', '${poem.id}')" class="btn btn-small btn-danger">🗑️ Delete</button>
                </div>
            </div>
        `;
    });
    poemsHTML += '</div>';
    
    poemText.innerHTML = poemsHTML;
}

// Delete a specific poem
async function deletePoem(filename, poemId) {
    if (!confirm('Are you sure you want to delete this poem?')) {
        return;
    }
    
    try {
        const response = await fetch(`/api/delete-poem/${filename}/${poemId}`, {
            method: 'POST'
        });
        
        const data = await response.json();
        
        if (response.ok) {
            showMessage('Poem deleted successfully', 'success');
            
            // Remove poem from UI
            const poemElement = document.getElementById(`poem-${poemId}`);
            if (poemElement) {
                poemElement.style.opacity = '0';
                poemElement.style.transform = 'scale(0.8)';
                setTimeout(() => {
                    poemElement.remove();
                    
                    // Update the list
                    currentPoemsList = currentPoemsList.filter(p => p.id !== poemId);
                    
                    // If no poems left, close modal and refresh
                    if (currentPoemsList.length === 0) {
                        closePoemModal();
                        refreshGallery();
                    } else {
                        // Update the title
                        document.getElementById('poem-title').textContent = 
                            `${currentPoemsList.length} Poem${currentPoemsList.length > 1 ? 's' : ''}`;
                    }
                }, 300);
            }
            
            // Refresh gallery to update poem count
            setTimeout(refreshGallery, 500);
        } else {
            showMessage(data.error || 'Failed to delete poem', 'error');
        }
    } catch (error) {
        showMessage('Error deleting poem: ' + error.message, 'error');
        console.error('Delete poem error:', error);
    }
}

// Print a single poem from the list
async function printSinglePoem(poemIndex) {
    const poem = currentPoemsList[poemIndex];
    
    if (!poem) {
        showMessage('❌ Poem not found', 'error');
        return;
    }
    
    try {
        const printData = {
            title: poem.title,
            poet_style: poem.poet_style,
            poem_text: poem.poem_text,
            filename: currentImageFilename
        };
        
        const response = await fetch('/api/print-poem', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(printData)
        });
        
        const result = await response.json();
        
        if (response.ok) {
            showMessage('✅ Print queued! ESP32 will print within 1 second.', 'success');
        } else {
            showMessage(`❌ Print failed: ${result.error || 'Unknown error'}`, 'error');
        }
    } catch (error) {
        showMessage('❌ Failed to queue print: ' + error.message, 'error');
    }
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
        
        // Add custom context if available
        if (currentPoemContext) {
            requestBody.context = currentPoemContext;
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
            
            // Show title
            const titleElement = document.getElementById('poem-title');
            if (data.title) {
                titleElement.textContent = data.title;
                titleElement.style.display = 'block';
            } else {
                titleElement.style.display = 'none';
            }
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
    const poemText = document.getElementById('poem-text');
    
    modal.style.display = 'none';
    currentPoemFilename = null;
    
    // Remove poems-list-container class if it was added
    poemText.classList.remove('poems-list-container');
}

// Print poem on thermal printer
async function printPoem() {
    console.log('Print button clicked!');
    
    const poemText = document.getElementById('poem-text').textContent;
    const poemTitle = document.getElementById('poem-title').textContent || 'Untitled';
    const poetSelect = document.getElementById('poet-style');
    const poetStyle = poetSelect.options[poetSelect.selectedIndex].text;
    
    console.log('Poem title:', poemTitle);
    console.log('Poem text:', poemText);
    console.log('Poet style:', poetStyle);
    console.log('Current filename:', currentPoemFilename);
    
    if (!poemText || poemText.trim() === '') {
        showMessage('No poem to print!', 'error');
        return;
    }
    
    try {
        // Prepare print data
        const printData = {
            title: poemTitle,
            poet_style: poetStyle,
            poem_text: poemText
        };
        
        console.log('Sending print data:', printData);
        
        // Show printing status
        showMessage('🖨️ Queuing print command...', 'info');
        
        // Send to Flask server to queue for ESP32
        const response = await fetch('/api/print-poem', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(printData)
        });
        
        console.log('Response status:', response.status);
        
        const result = await response.json();
        console.log('Response data:', result);
        
        if (response.ok) {
            showMessage('✅ Print queued! ESP32 will print within 1 second.', 'success');
        } else {
            showMessage(`❌ Print failed: ${result.error || 'Unknown error'}`, 'error');
        }
    } catch (error) {
        console.error('Print error:', error);
        showMessage('❌ Failed to queue print: ' + error.message, 'error');
    }
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

// Global variable to store current poem context
let currentPoemContext = null;

// Character counter for context input
function updateContextCharCount() {
    const input = document.getElementById('poem-context-input');
    const counter = document.getElementById('context-char-count');
    if (input && counter) {
        counter.textContent = input.value.length;
    }
}

// Add GPS location to context
async function addGPSLocation() {
    const input = document.getElementById('poem-context-input');
    const statusDiv = document.getElementById('context-status');
    
    if (!navigator.geolocation) {
        statusDiv.className = 'context-status error';
        statusDiv.textContent = '❌ GPS not supported by your browser';
        setTimeout(() => statusDiv.style.display = 'none', 3000);
        return;
    }
    
    statusDiv.className = 'context-status';
    statusDiv.style.display = 'block';
    statusDiv.textContent = '📍 Getting your location...';
    
    try {
        const position = await new Promise((resolve, reject) => {
            navigator.geolocation.getCurrentPosition(resolve, reject, {
                enableHighAccuracy: true,
                timeout: 10000
            });
        });
        
        const lat = position.coords.latitude.toFixed(6);
        const lon = position.coords.longitude.toFixed(6);
        const locationText = `\n📍 Location: ${lat}, ${lon}`;
        
        // Add location to existing text
        if (input.value && !input.value.includes('📍 Location:')) {
            input.value += locationText;
        } else if (!input.value) {
            input.value = `Taken at coordinates: ${lat}, ${lon}`;
        }
        
        updateContextCharCount();
        
        statusDiv.className = 'context-status success';
        statusDiv.textContent = '✅ Location added to context';
        setTimeout(() => statusDiv.style.display = 'none', 3000);
        
    } catch (error) {
        statusDiv.className = 'context-status error';
        statusDiv.textContent = '❌ Could not get location: ' + error.message;
        setTimeout(() => statusDiv.style.display = 'none', 3000);
    }
}

// Submit context
async function submitContext() {
    const input = document.getElementById('poem-context-input');
    const statusDiv = document.getElementById('context-status');
    const displayDiv = document.getElementById('current-context-display');
    const contentDiv = document.getElementById('context-content-text');
    
    const contextText = input.value.trim();
    
    if (!contextText) {
        statusDiv.className = 'context-status error';
        statusDiv.style.display = 'block';
        statusDiv.textContent = '❌ Please enter some context first';
        setTimeout(() => statusDiv.style.display = 'none', 3000);
        return;
    }
    
    // Store context globally
    currentPoemContext = contextText;
    
    // Show success message
    statusDiv.className = 'context-status success';
    statusDiv.style.display = 'block';
    statusDiv.textContent = '✅ Context saved! It will be used for all new poems.';
    setTimeout(() => statusDiv.style.display = 'none', 3000);
    
    // Display current context
    displayDiv.style.display = 'block';
    contentDiv.textContent = contextText;
    
    console.log('Poem context set:', currentPoemContext);
}

// Clear context
function clearContext() {
    const input = document.getElementById('poem-context-input');
    const statusDiv = document.getElementById('context-status');
    const displayDiv = document.getElementById('current-context-display');
    
    // Clear global context
    currentPoemContext = null;
    
    // Clear input
    input.value = '';
    updateContextCharCount();
    
    // Hide display
    displayDiv.style.display = 'none';
    
    // Show message
    statusDiv.className = 'context-status success';
    statusDiv.style.display = 'block';
    statusDiv.textContent = '✅ Context cleared';
    setTimeout(() => statusDiv.style.display = 'none', 2000);
    
    console.log('Poem context cleared');
}
