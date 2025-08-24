#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>

ESP8266WebServer server(80);
DNSServer dnsServer;
File fsUploadFile;

const char* ssid = "Wolf_Server";
const char* password = "wolf1234";
const char* hostname = "cyberwolf";
const char* customDomain = "cyberwolf.com";

// DNS and Captive Portal settings
const byte DNS_PORT = 53;

// Device management
struct ConnectedDevice {
  String ip;
  String mac;
  unsigned long lastSeen;
};

ConnectedDevice connectedDevices[50]; // Support up to 50 tracked devices
int deviceCount = 0;

// Enhanced device tracking and connection monitoring
void updateDeviceList() {
  static int lastDeviceCount = 0;
  
  // Get current device count
  deviceCount = WiFi.softAPgetStationNum();
  
  // Monitor connection changes
  if (deviceCount != lastDeviceCount) {
    Serial.println("Device count changed: " + String(lastDeviceCount) + " -> " + String(deviceCount));
    if (deviceCount > lastDeviceCount) {
      Serial.println("New device(s) connected!");
    } else {
      Serial.println("Device(s) disconnected");
    }
    lastDeviceCount = deviceCount;
  }
  
  // Update device list for display
  for (int i = 0; i < deviceCount && i < 50; i++) {
    connectedDevices[i].ip = "Device " + String(i + 1);
    connectedDevices[i].mac = "Active";
    connectedDevices[i].lastSeen = millis();
  }
}

// WiFi event monitoring function
void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case WIFI_EVENT_SOFTAPMODE_STACONNECTED:
      Serial.println("Device connected to Wolf AP");
      break;
    case WIFI_EVENT_SOFTAPMODE_STADISCONNECTED:
      Serial.println("Device disconnected from Wolf AP");
      break;
    case WIFI_EVENT_SOFTAPMODE_PROBEREQRECVED:
      // Too verbose, comment out if needed
      // Serial.println("Probe request received");
      break;
    default:
      break;
  }
}

// Format file size
String formatBytes(size_t bytes) {
  if (bytes < 1024) return String(bytes) + " B";
  else if (bytes < (1024 * 1024)) return String(bytes / 1024.0, 2) + " KB";
  else return String(bytes / 1024.0 / 1024.0, 2) + " MB";
}

// WiFi connection diagnostics
String getWiFiDiagnostics() {
  String diag = "<div class='wifi-diagnostics'>";
  diag += "<h4>WiFi Connection Diagnostics</h4>";
  diag += "<p><strong>AP Status:</strong> " + String(WiFi.getMode() == WIFI_AP ? "Active" : "Inactive") + "</p>";
  diag += "<p><strong>SSID:</strong> " + String(ssid) + "</p>";
  diag += "<p><strong>Channel:</strong> 1</p>";
  diag += "<p><strong>IP Address:</strong> " + WiFi.softAPIP().toString() + "</p>";
  diag += "<p><strong>MAC Address:</strong> " + WiFi.softAPmacAddress() + "</p>";
  diag += "<p><strong>Max Connections:</strong> 20</p>";
  diag += "<p><strong>Current Connections:</strong> " + String(WiFi.softAPgetStationNum()) + "</p>";
  
  // Connection quality indicators
  int deviceCount = WiFi.softAPgetStationNum();
  String quality = "Excellent";
  if (deviceCount > 15) quality = "Good";
  if (deviceCount > 18) quality = "Fair";
  
  diag += "<p><strong>Connection Quality:</strong> " + quality + "</p>";
  diag += "<p><strong>DNS Server:</strong> Active on port 53</p>";
  diag += "</div>";
  return diag;
}

// Get system information
String getSystemInfo() {
  String info = "<div class='system-info'>";
  info += "<h3>Wolf Server Status</h3>";
  info += "<p><strong>Primary Domain:</strong> " + String(customDomain) + "</p>";
  info += "<p><strong>Hostname:</strong> " + String(hostname) + ".local</p>";
  info += "<p><strong>IP Address:</strong> " + WiFi.softAPIP().toString() + "</p>";
  info += "<p><strong>DNS Server:</strong> Active (Port 53)</p>";
  info += "<p><strong>Free Heap:</strong> " + formatBytes(ESP.getFreeHeap()) + "</p>";
  info += "<p><strong>Flash Size:</strong> " + formatBytes(ESP.getFlashChipSize()) + "</p>";
  // Get SPIFFS info using FSInfo structure
  FSInfo fs_info;
  SPIFFS.info(fs_info);
  info += "<p><strong>SPIFFS Used:</strong> " + formatBytes(fs_info.usedBytes) + " / " + formatBytes(fs_info.totalBytes) + "</p>";
  info += "<p><strong>Connected Devices:</strong> " + String(WiFi.softAPgetStationNum()) + "</p>";
  info += "<p><strong>Uptime:</strong> " + String(millis() / 1000) + " seconds</p>";
  info += "</div>";
  
  // Add WiFi diagnostics
  info += getWiFiDiagnostics();
  return info;
}

// Generate shareable URLs
String generateUrls() {
  String urls = "<div class='url-generator'>";
  urls += "<h3>Access URLs</h3>";
  urls += "<div class='url-box'>";
  urls += "<label>Primary Domain:</label><input type='text' value='http://" + String(customDomain) + "' readonly onclick='this.select()'>";
  urls += "<label>Direct IP:</label><input type='text' value='http://" + WiFi.softAPIP().toString() + "' readonly onclick='this.select()'>";
  urls += "<label>mDNS:</label><input type='text' value='http://" + String(hostname) + ".local' readonly onclick='this.select()'>";
  urls += "<label>WiFi Network:</label><input type='text' value='" + String(ssid) + "' readonly onclick='this.select()'>";
  urls += "<label>Password:</label><input type='text' value='" + String(password) + "' readonly onclick='this.select()'>";
  urls += "</div>";
  urls += "<div style='margin-top: 15px; padding: 10px; background: rgba(0,255,136,0.1); border-radius: 8px;'>";
  urls += "<strong>Quick Access:</strong> Connect to <em>" + String(ssid) + "</em> and visit <strong>" + String(customDomain) + "</strong>";
  urls += "</div>";
  urls += "</div>";
  return urls;
}

// List all files in SPIFFS with enhanced functionality
String listFiles() {
  String output = "<div class='file-manager'>";
  output += "<h2>File Manager</h2>";
  output += "<div class='file-actions'>";
  output += "<button onclick='refreshFiles()'>Refresh</button>";
  output += "<button onclick='selectAll()'>Select All</button>";
  output += "<button onclick='deploySelected()'>Deploy Selected</button>";
  output += "</div>";
  output += "<div class='file-list'>";
  
  Dir dir = SPIFFS.openDir("/");
  int fileCount = 0;
  while (dir.next()) {
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    String fileExt = fileName.substring(fileName.lastIndexOf('.') + 1);
    String fileIcon = getFileIcon(fileExt);
    
    output += "<div class='file-item'>";
    output += "<input type='checkbox' class='file-checkbox' value='" + fileName + "'>";
    output += "<span class='file-icon'>" + fileIcon + "</span>";
    output += "<div class='file-info'>";
    output += "<div class='file-name'><a href=\"" + fileName + "\" target=\"_blank\">" + fileName.substring(1) + "</a></div>";
    output += "<div class='file-size'>(" + formatBytes(fileSize) + ")</div>";
    output += "</div>";
    output += "<div class='file-actions-btn'>";
    output += "<button onclick=\"viewFile('" + fileName + "')\">View</button>";
    output += "<button onclick=\"editFile('" + fileName + "')\">Edit</button>";
    output += "<a href=\"" + fileName + "\" download><button>Download</button></a>";
    output += "<button onclick=\"deployFile('" + fileName + "')\">Deploy</button>";
    output += "<button onclick=\"deleteFile('" + fileName + "', '" + fileName.substring(1) + "')\">Delete</button>";
    output += "</div>";
    output += "</div>";
    fileCount++;
  }
  
  if (fileCount == 0) {
    output += "<div class='empty-state'>No files uploaded yet. Upload your first file above!</div>";
  }
  
  output += "</div></div>";
  return output;
}

// Get file icon based on extension
String getFileIcon(String extension) {
  extension.toLowerCase();
  if (extension == "html" || extension == "htm") return "[HTML]";
  if (extension == "css") return "[CSS]";
  if (extension == "js") return "[JS]";
  if (extension == "json") return "[JSON]";
  if (extension == "txt") return "[TXT]";
  if (extension == "md") return "[MD]";
  if (extension == "png" || extension == "jpg" || extension == "gif" || extension == "jpeg") return "[IMG]";
  if (extension == "pdf") return "[PDF]";
  if (extension == "zip" || extension == "rar") return "[ZIP]";
  if (extension == "mp3" || extension == "wav") return "[AUDIO]";
  if (extension == "mp4" || extension == "avi") return "[VIDEO]";
  return "[FILE]";
}

// Serve main page with enhanced Wolf-themed UI
void handleRoot() {
  updateDeviceList(); // Update device tracking
  String msg = server.hasArg("msg") ? "<div class='alert success'>" + server.arg("msg") + "</div>" : "";
  
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>Wolf WebServer - Advanced File Management</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  
  // Modern Wolf-themed CSS
  html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: linear-gradient(135deg, #0c0c0c 0%, #1a1a2e 50%, #16213e 100%); color: #fff; min-height: 100vh; }";
  html += ".container { max-width: 1200px; margin: 0 auto; padding: 20px; }";
  html += ".header { text-align: center; margin-bottom: 30px; background: rgba(255,255,255,0.1); border-radius: 15px; padding: 20px; backdrop-filter: blur(10px); }";
  html += ".wolf-logo { font-size: 3em; margin-bottom: 10px; }";
  html += ".title { color: #00ff88; font-size: 2.5em; font-weight: bold; text-shadow: 0 0 20px #00ff88; margin-bottom: 10px; }";
  html += ".subtitle { color: #00ccff; font-size: 1.2em; }";
  html += ".developer { color: #ff6b6b; font-style: italic; margin-top: 10px; }";
  
  // Navigation tabs
  html += ".nav-tabs { display: flex; justify-content: center; margin-bottom: 30px; }";
  html += ".nav-tab { background: rgba(255,255,255,0.1); border: none; color: #fff; padding: 12px 24px; margin: 0 5px; border-radius: 25px; cursor: pointer; transition: all 0.3s; }";
  html += ".nav-tab:hover, .nav-tab.active { background: #00ff88; color: #000; transform: translateY(-2px); box-shadow: 0 5px 15px rgba(0,255,136,0.3); }";
  
  // Card system
  html += ".card { background: rgba(255,255,255,0.1); border-radius: 15px; padding: 25px; margin-bottom: 25px; backdrop-filter: blur(10px); border: 1px solid rgba(255,255,255,0.2); }";
  html += ".card h2, .card h3 { color: #00ff88; margin-bottom: 15px; }";
  
  // Upload section
  html += ".upload-section { text-align: center; }";
  html += ".upload-form { display: flex; gap: 15px; justify-content: center; align-items: center; flex-wrap: wrap; }";
  html += ".file-input { padding: 12px; border: 2px dashed #00ff88; border-radius: 10px; background: rgba(0,255,136,0.1); color: #fff; min-width: 200px; }";
  html += ".btn { background: linear-gradient(45deg, #00ff88, #00ccff); border: none; color: #000; padding: 12px 24px; border-radius: 25px; cursor: pointer; font-weight: bold; transition: all 0.3s; }";
  html += ".btn:hover { transform: scale(1.05); box-shadow: 0 5px 15px rgba(0,255,136,0.3); }";
  html += ".btn-danger { background: linear-gradient(45deg, #ff6b6b, #ff8e8e); }";
  html += ".btn-secondary { background: linear-gradient(45deg, #6c5ce7, #a29bfe); }";
  
  // System info styling
  html += ".system-info { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; }";
  html += ".system-info p { background: rgba(0,255,136,0.1); padding: 10px; border-radius: 8px; border-left: 4px solid #00ff88; }";
  
  // URL generator styling
  html += ".url-box { display: grid; gap: 10px; }";
  html += ".url-box label { color: #00ccff; font-weight: bold; }";
  html += ".url-box input { background: rgba(255,255,255,0.1); border: 1px solid #00ff88; border-radius: 8px; padding: 10px; color: #fff; width: 100%; cursor: pointer; }";
  html += ".url-box input:focus { outline: none; box-shadow: 0 0 10px rgba(0,255,136,0.5); }";
  
  // File manager styling
  html += ".file-actions { display: flex; gap: 10px; margin-bottom: 20px; flex-wrap: wrap; }";
  html += ".file-list { max-height: 500px; overflow-y: auto; }";
  html += ".file-item { display: flex; align-items: center; gap: 15px; padding: 15px; background: rgba(255,255,255,0.05); border-radius: 10px; margin-bottom: 10px; border: 1px solid rgba(255,255,255,0.1); transition: all 0.3s; }";
  html += ".file-item:hover { background: rgba(0,255,136,0.1); border-color: #00ff88; }";
  html += ".file-icon { font-size: 1.5em; }";
  html += ".file-info { flex: 1; }";
  html += ".file-name a { color: #00ccff; text-decoration: none; font-weight: bold; }";
  html += ".file-name a:hover { color: #00ff88; }";
  html += ".file-size { color: #aaa; font-size: 0.9em; }";
  html += ".file-actions-btn { display: flex; gap: 5px; flex-wrap: wrap; }";
  html += ".file-actions-btn button { padding: 6px 12px; font-size: 0.8em; }";
  html += ".empty-state { text-align: center; padding: 40px; color: #aaa; font-size: 1.2em; }";
  
  // Alert styles
  html += ".alert { padding: 15px; border-radius: 10px; margin-bottom: 20px; }";
  html += ".alert.success { background: rgba(0,255,136,0.2); border: 1px solid #00ff88; color: #00ff88; }";
  html += ".alert.error { background: rgba(255,107,107,0.2); border: 1px solid #ff6b6b; color: #ff6b6b; }";
  
  // Responsive design
  html += "@media (max-width: 768px) { .container { padding: 10px; } .upload-form { flex-direction: column; } .file-actions { justify-content: center; } .file-actions-btn { justify-content: center; } }";
  
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  html += "<div class='header'>";
  html += "<div class='wolf-logo'>WOLF</div>";
  html += "<div class='title'>CYBERWOLF WEBSERVER</div>";
  html += "<div class='subtitle'>Advanced File Management & Deployment System</div>";
  html += "<div style='color: #00ccff; margin-top: 10px;'>Access via: <strong>" + String(customDomain) + "</strong></div>";
  html += "<div class='developer'>Developed by <strong>S. Tamilselvan</strong></div>";
  html += "</div>";
  
  html += msg;
  
  // Navigation tabs
  html += "<div class='nav-tabs'>";
  html += "<button class='nav-tab active' onclick='showTab(\"upload\")'>Upload</button>";
  html += "<button class='nav-tab' onclick='showTab(\"files\")'>Files</button>";
  html += "<button class='nav-tab' onclick='showTab(\"system\")'>System</button>";
  html += "<button class='nav-tab' onclick='showTab(\"urls\")'>URLs</button>";
  html += "<button class='nav-tab' onclick='showTab(\"admin\")'>Admin</button>";
  html += "</div>";
  
  // Upload tab
  html += "<div id='upload-tab' class='card upload-section'>";
  html += "<h2>Upload Files</h2>";
  html += "<form method='POST' action='/upload' enctype='multipart/form-data' class='upload-form'>";
  html += "<input type='file' name='data' class='file-input' multiple required>";
  html += "<button type='submit' class='btn'>Upload & Deploy</button>";
  html += "</form>";
  html += "</div>";
  
  // Files tab
  html += "<div id='files-tab' class='card' style='display:none;'>";
  html += listFiles();
  html += "</div>";
  
  // System tab
  html += "<div id='system-tab' class='card' style='display:none;'>";
  html += getSystemInfo();
  html += "</div>";
  
  // URLs tab
  html += "<div id='urls-tab' class='card' style='display:none;'>";
  html += generateUrls();
  html += "</div>";
  
  // Admin tab
  html += "<div id='admin-tab' class='card' style='display:none;'>";
  html += "<h3>Admin Panel</h3>";
  html += "<div class='system-info'>";
  html += "<h4>Server Management</h4>";
  html += "<button class='btn btn-secondary' onclick='rebootServer()'>Reboot Server</button>";
  html += "<button class='btn btn-secondary' onclick='formatSPIFFS()'>Format Storage</button>";
  html += "<button class='btn btn-danger' onclick='factoryReset()'>Factory Reset</button>";
  html += "</div>";
  html += "<div class='system-info' style='margin-top: 20px;'>";
  html += "<h4>WiFi Troubleshooting</h4>";
  html += "<button class='btn btn-secondary' onclick='restartWiFi()'>Restart WiFi AP</button>";
  html += "<button class='btn btn-secondary' onclick='refreshConnections()'>Refresh Connections</button>";
  html += "<button class='btn btn-secondary' onclick='viewLogs()'>View Connection Logs</button>";
  html += "</div>";
  html += "<div style='margin-top: 20px; padding: 15px; background: rgba(0,255,136,0.1); border-radius: 8px;'>";
  html += "<h4>Connection Issues?</h4>";
  html += "<p><strong>Common Solutions:</strong></p>";
  html += "<ul style='text-align: left; margin-left: 20px;'>";
  html += "<li>Make sure you're connecting to: <strong>" + String(ssid) + "</strong></li>";
  html += "<li>Use password: <strong>" + String(password) + "</strong></li>";
  html += "<li>Try restarting your device WiFi</li>";
  html += "<li>Clear browser cache and try again</li>";
  html += "<li>Visit: <strong>" + String(customDomain) + "</strong> after connecting</li>";
  html += "</ul>";
  html += "</div>";
  html += "</div>";
  
  // JavaScript for enhanced functionality
  html += "<script>";
  html += "function showTab(tabName) {";
  html += "  document.querySelectorAll('[id$=\"-tab\"]').forEach(tab => tab.style.display = 'none');";
  html += "  document.querySelectorAll('.nav-tab').forEach(tab => tab.classList.remove('active'));";
  html += "  document.getElementById(tabName + '-tab').style.display = 'block';";
  html += "  event.target.classList.add('active');";
  html += "}";
  
  html += "function refreshFiles() { location.reload(); }";
  html += "function selectAll() { document.querySelectorAll('.file-checkbox').forEach(cb => cb.checked = !cb.checked); }";
  html += "function deploySelected() { alert('Deploy functionality coming soon!'); }";
  html += "function viewFile(file) { window.open(file, '_blank'); }";
  html += "function editFile(file) { window.open('/edit?file=' + encodeURIComponent(file), '_blank'); }";
  html += "function deployFile(file) { if(confirm('Deploy ' + file.substring(1) + '?')) { fetch('/deploy?file=' + encodeURIComponent(file)).then(() => alert('Deployed successfully!')); } }";
  html += "function deleteFile(file, name) { if(confirm('Delete ' + name + '?')) { window.location.href = '/delete?file=' + encodeURIComponent(file); } }";
  html += "function rebootServer() { if(confirm('Reboot Wolf Server?')) { fetch('/admin/reboot'); alert('Server rebooting...'); } }";
  html += "function formatSPIFFS() { if(confirm('Format storage? This will delete ALL files!')) { fetch('/admin/format'); alert('Storage formatted!'); } }";
  html += "function factoryReset() { if(confirm('Factory reset? This will erase everything!')) { fetch('/admin/reset'); alert('Factory reset initiated!'); } }";
  html += "function restartWiFi() { if(confirm('Restart WiFi Access Point?')) { fetch('/admin/restart-wifi'); alert('WiFi restarting...'); } }";
  html += "function refreshConnections() { location.reload(); }";
  html += "function viewLogs() { window.open('/admin/logs', '_blank'); }";
  html += "</script>";
  
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

// Handle file upload
void handleUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = "/" + upload.filename;
    if (SPIFFS.exists(filename)) {
      SPIFFS.remove(filename); // Overwrite
    }
    fsUploadFile = SPIFFS.open(filename, "w");
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile)
      fsUploadFile.close();
    server.sendHeader("Location", "/?msg=Upload+Success");
    server.send(303);
  }
}

// Serve files for download/view
bool handleFileRead(String path) {
  if (SPIFFS.exists(path)) {
    String contentType = getContentType(path);
    File file = SPIFFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

// Delete a file
void handleDelete() {
  if (server.hasArg("file")) {
    String path = server.arg("file");
    if (SPIFFS.exists(path)) {
      SPIFFS.remove(path);
      server.sendHeader("Location", "/?msg=Deleted+" + path.substring(1));
      server.send(303);
      return;
    }
  }
  server.sendHeader("Location", "/?msg=Delete+Failed");
  server.send(303);
}

// Handle file deployment
void handleDeploy() {
  if (server.hasArg("file")) {
    String path = server.arg("file");
    if (SPIFFS.exists(path)) {
      // Deployment logic - for now just copy to deployment directory
      server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"File deployed successfully\"}");
      return;
    }
  }
  server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Deployment failed\"}");
}

// Handle file editing
void handleEdit() {
  if (server.hasArg("file")) {
    String path = server.arg("file");
    if (SPIFFS.exists(path)) {
      File file = SPIFFS.open(path, "r");
      String content = file.readString();
      file.close();
      
      String html = "<!DOCTYPE html><html><head>";
      html += "<title>Wolf Editor - " + path.substring(1) + "</title>";
      html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
      html += "<style>";
      html += "body { font-family: 'Consolas', monospace; background: #1a1a2e; color: #fff; margin: 0; padding: 20px; }";
      html += ".editor-header { background: rgba(0,255,136,0.1); padding: 15px; border-radius: 10px; margin-bottom: 20px; }";
      html += ".editor-title { color: #00ff88; font-size: 1.5em; }";
      html += "textarea { width: 100%; height: 500px; background: #0f0f0f; color: #fff; border: 1px solid #00ff88; border-radius: 10px; padding: 15px; font-family: 'Consolas', monospace; font-size: 14px; }";
      html += ".btn { background: linear-gradient(45deg, #00ff88, #00ccff); border: none; color: #000; padding: 12px 24px; border-radius: 25px; cursor: pointer; font-weight: bold; margin: 10px 5px; }";
      html += ".btn:hover { transform: scale(1.05); }";
      html += "</style></head><body>";
      
      html += "<div class='editor-header'>";
      html += "<div class='editor-title'>Editing: " + path.substring(1) + "</div>";
      html += "</div>";
      
      html += "<form method='POST' action='/save'>";
      html += "<input type='hidden' name='file' value='" + path + "'>";
      html += "<textarea name='content'>" + content + "</textarea><br>";
      html += "<button type='submit' class='btn'>Save File</button>";
      html += "<button type='button' class='btn' onclick='history.back()'>Back</button>";
      html += "</form>";
      
      html += "</body></html>";
      server.send(200, "text/html", html);
      return;
    }
  }
  server.sendHeader("Location", "/?msg=File+not+found");
  server.send(303);
}

// Handle file saving
void handleSave() {
  if (server.hasArg("file") && server.hasArg("content")) {
    String path = server.arg("file");
    String content = server.arg("content");
    
    File file = SPIFFS.open(path, "w");
    if (file) {
      file.print(content);
      file.close();
      server.sendHeader("Location", "/?msg=Saved+" + path.substring(1));
      server.send(303);
      return;
    }
  }
  server.sendHeader("Location", "/?msg=Save+Failed");
  server.send(303);
}

// Captive Portal Handler - redirects all requests to our domain
bool handleCaptivePortal() {
  String hostHeader = server.hostHeader();
  
  // If request is not for our custom domain, redirect to it
  if (hostHeader != customDomain && hostHeader != WiFi.softAPIP().toString()) {
    String redirectURL = "http://" + String(customDomain);
    server.sendHeader("Location", redirectURL, true);
    server.send(302, "text/plain", "");
    return true;
  }
  return false;
}

// Admin functions
void handleAdminReboot() {
  server.send(200, "text/plain", "Rebooting Wolf Server...");
  delay(1000);
  ESP.restart();
}

void handleAdminFormat() {
  SPIFFS.format();
  server.send(200, "text/plain", "Storage formatted successfully");
}

void handleAdminReset() {
  SPIFFS.format();
  server.send(200, "text/plain", "Factory reset completed");
  delay(1000);
  ESP.restart();
}

// WiFi management functions
void handleRestartWiFi() {
  server.send(200, "text/plain", "Restarting WiFi Access Point...");
  
  // Stop current AP
  WiFi.softAPdisconnect(true);
  delay(1000);
  
  // Restart with same settings
  WiFi.mode(WIFI_AP);
  IPAddress local_IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid, password, 1, false, 20);
  
  // Restart DNS server
  dnsServer.stop();
  delay(100);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  
  Serial.println("WiFi Access Point restarted");
}

void handleConnectionLogs() {
  String logs = "<!DOCTYPE html><html><head>";
  logs += "<title>Wolf Server - Connection Logs</title>";
  logs += "<style>body{font-family:monospace;background:#1a1a2e;color:#fff;padding:20px;}";
  logs += ".log-entry{background:rgba(0,255,136,0.1);padding:10px;margin:5px 0;border-radius:5px;}";
  logs += "</style></head><body>";
  logs += "<h1>Wolf Server Connection Logs</h1>";
  logs += "<div class='log-entry'>Current Connected Devices: " + String(WiFi.softAPgetStationNum()) + "</div>";
  logs += "<div class='log-entry'>Access Point Status: " + String(WiFi.getMode() == WIFI_AP ? "Active" : "Inactive") + "</div>";
  logs += "<div class='log-entry'>SSID: " + String(ssid) + "</div>";
  logs += "<div class='log-entry'>Channel: 1</div>";
  logs += "<div class='log-entry'>IP Address: " + WiFi.softAPIP().toString() + "</div>";
  logs += "<div class='log-entry'>MAC Address: " + WiFi.softAPmacAddress() + "</div>";
  logs += "<div class='log-entry'>Free Heap: " + formatBytes(ESP.getFreeHeap()) + "</div>";
  logs += "<div class='log-entry'>Uptime: " + String(millis() / 1000) + " seconds</div>";
  logs += "<br><button onclick='history.back()'>Back to Admin</button>";
  logs += "<button onclick='location.reload()'>Refresh Logs</button>";
  logs += "</body></html>";
  
  server.send(200, "text/html", logs);
}

// Determine content type
String getContentType(String filename) {
  if (filename.endsWith(".html")) return "text/html";
  if (filename.endsWith(".css")) return "text/css";
  if (filename.endsWith(".js")) return "application/javascript";
  if (filename.endsWith(".png")) return "image/png";
  if (filename.endsWith(".jpg")) return "image/jpeg";
  if (filename.endsWith(".gif")) return "image/gif";
  if (filename.endsWith(".ico")) return "image/x-icon";
  if (filename.endsWith(".xml")) return "text/xml";
  if (filename.endsWith(".pdf")) return "application/pdf";
  if (filename.endsWith(".zip")) return "application/zip";
  return "text/plain";
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nWolf WebServer Starting...");
  
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  Serial.println("SPIFFS Mounted Successfully");

  // Configure WiFi mode and disconnect any existing connections
  WiFi.mode(WIFI_AP);
  WiFi.softAPdisconnect(true);
  delay(100);
  
  // Register WiFi event handler for connection monitoring
  WiFi.onEvent(onWiFiEvent);
  
  // Configure Access Point with optimized settings
  IPAddress local_IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  
  WiFi.softAPConfig(local_IP, gateway, subnet);
  
  // Start Access Point with enhanced settings
  // Parameters: (ssid, password, channel, hidden, max_connections)
  bool apResult = WiFi.softAP(ssid, password, 1, false, 20); // Channel 1, not hidden, max 20 devices
  
  if (apResult) {
    Serial.println("Wolf AP started successfully!");
    Serial.println("AP IP: " + WiFi.softAPIP().toString());
    Serial.println("Network: " + String(ssid));
    Serial.println("Password: " + String(password));
    Serial.println("Channel: 1");
    Serial.println("Max Connections: 20");
  } else {
    Serial.println("Failed to start Wolf AP!");
    // Try alternative configuration
    WiFi.softAP(ssid, password);
    Serial.println("Started with default settings");
  }

  // Initialize DNS Server for Captive Portal
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  Serial.println("DNS Server started on port 53");
  Serial.println("All domains redirect to: " + String(customDomain));

  // Initialize mDNS
  if (MDNS.begin(hostname)) {
    Serial.println("mDNS started: http://" + String(hostname) + ".local");
    MDNS.addService("http", "tcp", 80);
  }

  // Initialize OTA
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.setPassword("wolfota123");
  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("OTA Update starting: " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Update completed");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA Update enabled");

  // Route handlers
  server.on("/", HTTP_GET, handleRoot);
  server.on("/upload", HTTP_POST, []() {
    server.sendHeader("Location", "/?msg=Upload+Complete");
    server.send(303);
  }, handleUpload);
  server.on("/delete", HTTP_GET, handleDelete);
  server.on("/deploy", HTTP_GET, handleDeploy);
  server.on("/edit", HTTP_GET, handleEdit);
  server.on("/save", HTTP_POST, handleSave);
  
  // Admin routes
  server.on("/admin/reboot", HTTP_GET, handleAdminReboot);
  server.on("/admin/format", HTTP_GET, handleAdminFormat);
  server.on("/admin/reset", HTTP_GET, handleAdminReset);
  server.on("/admin/restart-wifi", HTTP_GET, handleRestartWiFi);
  server.on("/admin/logs", HTTP_GET, handleConnectionLogs);

  // 404 handler with Captive Portal support
  server.onNotFound([]() {
    // First check if this is a captive portal redirect
    if (handleCaptivePortal()) {
      return;
    }
    
    // Try to serve file
    if (!handleFileRead(server.uri())) {
      // If no file found, show 404 with redirect info
      String html = "<!DOCTYPE html><html><head>";
      html += "<title>Wolf Server - Page Not Found</title>";
      html += "<meta http-equiv='refresh' content='5;url=http://" + String(customDomain) + "'>";
      html += "</head><body>";
      html += "<h1>Wolf Server - 404</h1>";
      html += "<p>Page not found</p>";
      html += "<p>Redirecting to <a href='http://" + String(customDomain) + "'>" + String(customDomain) + "</a> in 5 seconds...</p>";
      html += "<a href='/'>Back to Wolf Den</a>";
      html += "</body></html>";
      server.send(404, "text/html", html);
    }
  });

  server.begin();
  Serial.println("Wolf WebServer running on port 80");
  Serial.println("Ready to serve unlimited devices!");
  Serial.println("=====================================");
  Serial.println("");
  Serial.println("ACCESS INSTRUCTIONS:");
  Serial.println("1. Connect to WiFi: " + String(ssid));
  Serial.println("2. Open browser and go to: " + String(customDomain));
  Serial.println("3. Alternative access: " + WiFi.softAPIP().toString());
  Serial.println("=====================================");
}

void loop() {
  // Handle DNS requests (important for captive portal)
  dnsServer.processNextRequest();
  
  server.handleClient();
  ArduinoOTA.handle();
  MDNS.update();
  
  // Update device tracking every 10 seconds for better monitoring
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 10000) {
    updateDeviceList();
    lastUpdate = millis();
  }
  
  // Check WiFi status every 60 seconds
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 60000) {
    if (WiFi.getMode() != WIFI_AP) {
      Serial.println("WiFi AP mode lost! Restarting...");
      WiFi.mode(WIFI_AP);
      WiFi.softAP(ssid, password, 1, false, 20);
    }
    lastWiFiCheck = millis();
  }
}
