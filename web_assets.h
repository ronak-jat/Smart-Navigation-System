const char* index_html = R"raw(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Smart Navigation System </title>
    <link rel="stylesheet" href="style.css">
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800&display=swap" rel="stylesheet">
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
</head>
<body>
    <header>
        <div class="header-content">
            <h1>Smart Navigation System</h1>
            <div id="connectionStatus" class="status-badge">
                <i class="fas fa-wifi"></i> Connected
            </div>
        </div>
    </header>

    <div class="container">
        <!-- Debug Console -->
        <div id="debugConsole" style="grid-column: 1 / -1; background: #334155; color: #f8fafc; padding: 10px; border-radius: 8px; font-family: monospace; font-size: 12px; max-height: 100px; overflow-y: auto; margin-bottom: 20px; display: none;">
            <div><strong>Debug Log:</strong></div>
        </div>

        <main class="card map-container" id="mapContainer">
            <button id="fsBtn" class="fs-btn" title="Toggle Fullscreen">
                <i class="fas fa-expand"></i>
            </button>
            <div id="map"></div>
            <div id="mapLoading" class="loading">
                <div class="spinner"></div>
            </div>
        </main>

        <aside class="controls">
            <div class="card">
                <h2 class="section-title"><i class="fas fa-location-dot"></i> Set Destination</h2>
                <div class="input-group" style="margin-top: 1rem;">
                    <input type="text" id="destination" placeholder="Enter place name...">
                    <button id="routeBtn" class="btn btn-primary" style="margin-top: 0.75rem; width: 100%;">
                        <i class="fas fa-search-location"></i> Navigate
                    </button>
                </div>
            </div>

            <div class="card">
                <h2 class="section-title"><i class="fas fa-sliders"></i> Controls</h2>
                <div class="btn-grid" style="margin-top: 1rem;">
                    <button id="pseudoBtn" class="btn btn-secondary">
                        <i class="fas fa-bicycle"></i> Test Mode
                    </button>
                    <button id="laptopBtn" class="btn btn-secondary">
                        <i class="fas fa-laptop"></i> Laptop GPS
                    </button>
                </div>
                
                <div class="stats-grid">
                    <div class="stat-box">
                        <div class="stat-label">Distance</div>
                        <div class="stat-value" id="distValue">--</div>
                    </div>
                    <div class="stat-box">
                        <div class="stat-label">ETA</div>
                        <div class="stat-value" id="etaValue">--</div>
                    </div>
                </div>
            </div>

            <div class="card" style="flex: 1; display: flex; flex-direction: column;">
                <h2 class="section-title"><i class="fas fa-route"></i> Route</h2>
                <ul id="directions" class="directions-list">
                    <li class="direction-item">Enter a destination to start navigation.</li>
                </ul>
            </div>
        </aside>
    </div>

    <script id="gmapScript"></script>
    
    <script>
        let map, realMarker, pseudoMarker, routePolyline;
        let pseudoEnabled = false;
        let laptopEnabled = false;
        let googleApiKey = ""; 
        let lastPolyline = "";

        const els = {
            destInput: document.getElementById('destination'),
            routeBtn: document.getElementById('routeBtn'),
            pseudoBtn: document.getElementById('pseudoBtn'),
            laptopBtn: document.getElementById('laptopBtn'),
            directions: document.getElementById('directions'),
            loading: document.getElementById('mapLoading'),
            status: document.getElementById('connectionStatus'),
            mapContainer: document.getElementById('mapContainer'),
            fsBtn: document.getElementById('fsBtn'),
            debug: document.getElementById('debugConsole')
        };

        function log(msg) {
            console.log(msg);
            els.debug.style.display = 'block';
            const div = document.createElement('div');
            div.textContent = `> ${msg}`;
            els.debug.appendChild(div);
            els.debug.scrollTop = els.debug.scrollHeight;
        }

        window.gm_authFailure = function() {
            log("CRITICAL: Google Maps Authentication Failed. Check API Key.");
            alert("Google Maps API Key Error. Check Debug Log.");
        };

        async function init() {
            try {
                log("Fetching config...");
                const res = await fetch('/config.json');
                if (!res.ok) throw new Error(`Config fetch failed: ${res.status}`);
                const config = await res.json();
                googleApiKey = config.apiKey;
                log(`API Key loaded (length: ${googleApiKey.length})`);
                
                const script = document.createElement('script');
                script.src = `https://maps.googleapis.com/maps/api/js?key=${googleApiKey}&libraries=geometry&callback=initMap`;
                script.onerror = () => log("Failed to load Google Maps script (Network Error?)");
                document.body.appendChild(script);

                setInterval(updateGPS, 2000);
                setInterval(updatePseudo, 1000);
                setInterval(updateRoute, 5000);
                
            } catch (e) {
                log(`Error: ${e.message}`);
                console.error("Failed to load config", e);
                alert("Failed to load configuration. See debug log.");
            }
        }

        window.initMap = function() {
            log("Initializing Map...");
            try {
                // Default to user's preferred location (Jaipur/Agra area)
                const defaultPos = { lat: 27.1761744, lng: 75.9568270 };
                
                map = new google.maps.Map(document.getElementById('map'), {
                    center: defaultPos, 
                    zoom: 16,
                    disableDefaultUI: false,
                });

                realMarker = new google.maps.Marker({
                    position: defaultPos,
                    map: map,
                    title: 'Current Location',
                    icon: {
                        url: "https://maps.google.com/mapfiles/kml/paddle/blu-circle.png",
                        scaledSize: new google.maps.Size(40,40),
                        anchor: new google.maps.Point(20,20)
                    }
                });

                pseudoMarker = new google.maps.Marker({
                    position: defaultPos,
                    map: null,
                    title: 'Test Traveler',
                    icon: {
                        url: 'https://maps.gstatic.com/mapfiles/ms2/micons/cycling.png',
                        scaledSize: new google.maps.Size(32, 32),
                        anchor: new google.maps.Point(16, 32)
                    }
                });

                routePolyline = new google.maps.Polyline({
                    path: [],
                    strokeColor: '#3b82f6',
                    strokeWeight: 5,
                    strokeOpacity: 0.8,
                    map: map
                });
                log("Map initialized successfully.");
            } catch (e) {
                log(`Map init error: ${e.message}`);
            }
        };

        // Fullscreen Toggle
        els.fsBtn.addEventListener('click', () => {
            els.mapContainer.classList.toggle('fullscreen');
            const icon = els.fsBtn.querySelector('i');
            if (els.mapContainer.classList.contains('fullscreen')) {
                icon.classList.remove('fa-expand');
                icon.classList.add('fa-compress');
            } else {
                icon.classList.remove('fa-compress');
                icon.classList.add('fa-expand');
            }
            // Trigger resize so map fills new area
            setTimeout(() => google.maps.event.trigger(map, 'resize'), 300);
        });

        els.routeBtn.addEventListener('click', async () => {
            const dest = els.destInput.value;
            if (!dest) return;

            els.loading.classList.add('visible');
            try {
                log(`Setting destination: ${dest}`);
                await fetch('/setdest?place=' + encodeURIComponent(dest));
                // Reset last polyline to force update
                lastPolyline = "";
                setTimeout(updateRoute, 2000);
            } catch (e) {
                log(`SetDest Error: ${e.message}`);
                console.error(e);
            } finally {
                setTimeout(() => els.loading.classList.remove('visible'), 1000);
            }
        });

        els.pseudoBtn.addEventListener('click', async () => {
            pseudoEnabled = !pseudoEnabled;
            updateBtnState(els.pseudoBtn, pseudoEnabled, 'Test Mode');
            pseudoMarker.setMap(pseudoEnabled ? map : null);
            await fetch('/togglepseudo');
            updateStatus();
        });

        els.laptopBtn.addEventListener('click', async () => {
            laptopEnabled = !laptopEnabled;
            updateBtnState(els.laptopBtn, laptopEnabled, 'Laptop GPS');
            if (laptopEnabled) {
                if (navigator.geolocation) {
                    navigator.geolocation.watchPosition(pos => {
                        const { latitude, longitude } = pos.coords;
                        fetch(`/uplocation?lat=${latitude}&lon=${longitude}`);
                    }, err => console.error(err));
                } else {
                    alert("Geolocation not supported");
                }
            }
            await fetch('/toggleLaptop');
            updateStatus();
        });

        function updateBtnState(btn, active, text) {
            if (active) {
                btn.classList.add('active');
                btn.innerHTML = `<i class="fas fa-check"></i> ${text} ON`;
            } else {
                btn.classList.remove('active');
                btn.innerHTML = `<i class="fas fa-power-off"></i> ${text}`;
            }
        }

        function updateStatus() {
            if (pseudoEnabled) {
                els.status.className = 'status-badge pseudo';
                els.status.innerHTML = '<i class="fas fa-bicycle"></i> Test Mode';
            } else if (laptopEnabled) {
                els.status.className = 'status-badge';
                els.status.innerHTML = '<i class="fas fa-laptop"></i> Laptop GPS';
            } else {
                els.status.className = 'status-badge';
                els.status.innerHTML = '<i class="fas fa-satellite-dish"></i> Real GPS';
            }
        }

        async function updateGPS() {
            try {
                const res = await fetch('/gps.json');
                const data = await res.json();
                
                // IGNORE INVALID GPS (0,0) - PREVENTS "WATER" LOCATION
                if (Math.abs(data.lat) < 0.0001 && Math.abs(data.lon) < 0.0001) {
                    return; 
                }

                const latlng = { lat: data.lat, lng: data.lon };
                realMarker.setPosition(latlng);
                // Only pan if we are not in test mode. 
                if (!pseudoEnabled) map.panTo(latlng);
            } catch (e) {}
        }

        async function updatePseudo() {
            if (!pseudoEnabled) return;
            try {
                const res = await fetch('/pseudo.json');
                const data = await res.json();
                
                if (Math.abs(data.lat) < 0.0001 && Math.abs(data.lon) < 0.0001) return;

                const latlng = { lat: data.lat, lng: data.lon };
                pseudoMarker.setPosition(latlng);
                map.panTo(latlng);
            } catch (e) {}
        }

        async function updateRoute() {
            try {
                const res = await fetch('/route.json');
                const data = await res.json();
                
                if (!data.routes || data.routes.length === 0) return;

                const currentPolyline = data.routes[0].overview_polyline.points;
                
                // STABILITY FIX: Only update map if route changed
                if (currentPolyline !== lastPolyline) {
                    lastPolyline = currentPolyline;
                    
                    const points = google.maps.geometry.encoding.decodePath(currentPolyline);
                    routePolyline.setPath(points);
                    
                    const bounds = new google.maps.LatLngBounds();
                    points.forEach(p => bounds.extend(p));
                    map.fitBounds(bounds);

                    const steps = data.routes[0].legs[0].steps;
                    els.directions.innerHTML = steps.map(step => `
                        <li class="direction-item">
                            ${step.html_instructions}
                            <div style="font-size: 0.8em; color: #94a3b8; margin-top: 0.25rem;">
                                ${step.distance.text} • ${step.duration.text}
                            </div>
                        </li>
                    `).join('');

                    const leg = data.routes[0].legs[0];
                    document.getElementById('distValue').textContent = leg.distance.text;
                    document.getElementById('etaValue').textContent = leg.duration.text;
                }

            } catch (e) {}
        }

        init();
    </script>
</body>
</html>
)raw";

const char* style_css = R"raw(:root {
  --bg-color: #0f172a;
  --card-bg: rgba(30, 41, 59, 0.7);
  --text-primary: #f1f5f9;
  --text-secondary: #94a3b8;
  --accent-color: #3b82f6;
  --accent-hover: #2563eb;
  --success-color: #10b981;
  --danger-color: #ef4444;
  --glass-border: rgba(255, 255, 255, 0.1);
  --shadow-lg: 0 10px 15px -3px rgba(0, 0, 0, 0.1), 0 4px 6px -2px rgba(0, 0, 0, 0.05);
}

* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

body {
  font-family: 'Inter', system-ui, -apple-system, sans-serif;
  background-color: var(--bg-color);
  background-image: 
    radial-gradient(at 0% 0%, rgba(59, 130, 246, 0.15) 0px, transparent 50%),
    radial-gradient(at 100% 100%, rgba(16, 185, 129, 0.15) 0px, transparent 50%);
  color: var(--text-primary);
  min-height: 100vh;
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 2rem 1rem;
}

.container {
  width: 100%;
  max-width: 1200px;
  display: grid;
  grid-template-columns: 1fr 350px;
  gap: 2rem;
}

@media (max-width: 900px) {
  .container {
    grid-template-columns: 1fr;
  }
}

.card {
  background: var(--card-bg);
  backdrop-filter: blur(12px);
  -webkit-backdrop-filter: blur(12px);
  border: 1px solid var(--glass-border);
  border-radius: 1.5rem;
  padding: 1.5rem;
  box-shadow: var(--shadow-lg);
}

header {
  width: 100%;
  max-width: 1200px;
  margin-bottom: 2rem;
  display: flex;
  justify-content: space-between;
  align-items: center;
}

h1 {
  font-size: 2rem;
  font-weight: 800;
  letter-spacing: -0.025em;
  background: linear-gradient(to right, #fff, #94a3b8);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
}

.status-badge {
  display: inline-flex;
  align-items: center;
  gap: 0.5rem;
  padding: 0.5rem 1rem;
  border-radius: 9999px;
  font-size: 0.875rem;
  font-weight: 600;
  background: rgba(59, 130, 246, 0.1);
  color: var(--accent-color);
  border: 1px solid rgba(59, 130, 246, 0.2);
}

.status-badge.pseudo {
  background: rgba(239, 68, 68, 0.1);
  color: var(--danger-color);
  border-color: rgba(239, 68, 68, 0.2);
}

.map-container {
  height: 600px;
  border-radius: 1rem;
  overflow: hidden;
  position: relative;
  transition: all 0.3s ease;
}

/* Fullscreen Mode */
.map-container.fullscreen {
  position: fixed;
  top: 0;
  left: 0;
  width: 100vw;
  height: 100vh;
  z-index: 1000;
  border-radius: 0;
  margin: 0;
}

.fs-btn {
  position: absolute;
  top: 1rem;
  right: 1rem;
  z-index: 5;
  background: rgba(15, 23, 42, 0.8);
  color: white;
  border: 1px solid rgba(255,255,255,0.2);
  padding: 0.75rem;
  border-radius: 0.5rem;
  cursor: pointer;
  transition: all 0.2s;
  font-size: 1.2rem;
}

.fs-btn:hover {
  background: rgba(15, 23, 42, 1);
  transform: scale(1.1);
}

#map {
  width: 100%;
  height: 100%;
}

.controls {
  display: flex;
  flex-direction: column;
  gap: 1.5rem;
}

.input-group {
  position: relative;
}

.input-group input {
  width: 100%;
  padding: 1rem;
  padding-right: 3rem;
  background: rgba(15, 23, 42, 0.6);
  border: 1px solid var(--glass-border);
  border-radius: 0.75rem;
  color: white;
  font-size: 1rem;
  transition: all 0.2s;
}

.input-group input:focus {
  outline: none;
  border-color: var(--accent-color);
  box-shadow: 0 0 0 2px rgba(59, 130, 246, 0.2);
}

.btn-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 0.75rem;
}

.btn {
  padding: 0.75rem 1rem;
  border-radius: 0.75rem;
  border: none;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.2s;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 0.5rem;
  font-size: 0.9rem;
}

.btn-primary {
  background: var(--accent-color);
  color: white;
}

.btn-primary:hover {
  background: var(--accent-hover);
  transform: translateY(-1px);
}

.btn-secondary {
  background: rgba(255, 255, 255, 0.1);
  color: white;
}

.btn-secondary:hover {
  background: rgba(255, 255, 255, 0.15);
}

.btn-danger {
  background: rgba(239, 68, 68, 0.2);
  color: #fca5a5;
  border: 1px solid rgba(239, 68, 68, 0.2);
}

.btn-danger:hover {
  background: rgba(239, 68, 68, 0.3);
}

.btn.active {
  background: var(--success-color);
  color: white;
}

.directions-list {
  list-style: none;
  margin-top: 1rem;
  max-height: 300px;
  overflow-y: auto;
  padding-right: 0.5rem;
}

.directions-list::-webkit-scrollbar {
  width: 6px;
}

.directions-list::-webkit-scrollbar-track {
  background: transparent;
}

.directions-list::-webkit-scrollbar-thumb {
  background: rgba(255, 255, 255, 0.1);
  border-radius: 3px;
}

.direction-item {
  padding: 1rem;
  background: rgba(255, 255, 255, 0.03);
  border-radius: 0.75rem;
  margin-bottom: 0.75rem;
  font-size: 0.9rem;
  line-height: 1.5;
  border-left: 3px solid transparent;
  transition: all 0.2s;
}

.direction-item:hover {
  background: rgba(255, 255, 255, 0.05);
}

.direction-item.active {
  border-left-color: var(--accent-color);
  background: rgba(59, 130, 246, 0.1);
}

.stats-grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 1rem;
  margin-top: 1rem;
}

.stat-box {
  background: rgba(255, 255, 255, 0.03);
  padding: 1rem;
  border-radius: 0.75rem;
  text-align: center;
}

.stat-label {
  font-size: 0.75rem;
  color: var(--text-secondary);
  text-transform: uppercase;
  letter-spacing: 0.05em;
}

.stat-value {
  font-size: 1.25rem;
  font-weight: 700;
  margin-top: 0.25rem;
}

.loading {
  position: absolute;
  inset: 0;
  background: rgba(15, 23, 42, 0.8);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 10;
  backdrop-filter: blur(4px);
  opacity: 0;
  pointer-events: none;
  transition: opacity 0.3s;
}

.loading.visible {
  opacity: 1;
  pointer-events: all;
}

.spinner {
  width: 40px;
  height: 40px;
  border: 3px solid rgba(255, 255, 255, 0.1);
  border-radius: 50%;
  border-top-color: var(--accent-color);
  animation: spin 1s linear infinite;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}
)raw";
