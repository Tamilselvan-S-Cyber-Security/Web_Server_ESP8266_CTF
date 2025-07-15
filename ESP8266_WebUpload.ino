#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>

ESP8266WebServer server(80);
File fsUploadFile;

const char* ssid = "Wolf_Server";
const char* password = "wolf1234";

// Format file size
String formatBytes(size_t bytes) {
  if (bytes < 1024) return String(bytes) + " B";
  else if (bytes < (1024 * 1024)) return String(bytes / 1024.0, 2) + " KB";
  else return String(bytes / 1024.0 / 1024.0, 2) + " MB";
}

// List all files in SPIFFS
String listFiles() {
  String output = "<h2>Files:</h2><ul>";
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    output += "<li>";
    output += "<a href=\"" + fileName + "\" target=\"_blank\">&#128196; " + fileName.substring(1) + "</a> ";
    output += "(" + formatBytes(fileSize) + ") ";
    output += "<a href=\"" + fileName + "\" download><button>Download</button></a> ";
    output += "<a href=\"/delete?file=" + fileName + "\" onclick=\"return confirm('Delete " + fileName.substring(1) + "?');\"><button>Delete</button></a>";
    output += "</li>";
  }
  output += "</ul>";
  return output;
}

// Serve main page with upload form and file list
void handleRoot() {
  String msg = server.hasArg("msg") ? "<p style='color:green;'>" + server.arg("msg") + "</p>" : "";
  String html = "<!DOCTYPE html><html><head><title>Wolf Server </title><style>";
  html += "body{font-family:sans-serif;max-width:600px;margin:auto;background:#f9f9f9;}h1{color:#333;}ul{list-style:none;padding:0;}li{margin:8px 0;}";
  html += "form{margin-bottom:20px;}button{margin-left:8px;}";
  html += "</style></head><body>";
  html += "<h1>Wolf Server</h1>";
  html += "<h1 style='color:lime; text-align:center; font-family:monospace;'>Developed by <span style='color:cyan;'>S. Tamilselvan</span></h1>";
  html += msg;
  html += "<form method='POST' action='/upload' enctype='multipart/form-data'>";
  html += "<input type='file' name='data' required><input type='submit' value='Upload'></form>";
  html += listFiles();
  html += "</body></html>";
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
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  WiFi.softAP(ssid, password, 1, 0, 4); // 4 client limit
  Serial.println("AP started. IP: " + WiFi.softAPIP().toString());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/upload", HTTP_POST, []() {
    server.send(200, "text/plain", "Upload done");
  }, handleUpload);
  server.on("/delete", HTTP_GET, handleDelete);

  server.onNotFound([]() {
    if (!handleFileRead(server.uri()))
      server.send(404, "text/plain", "Not Found");
  });

  server.begin();
  Serial.println("Web server running...");
}

void loop() {
  server.handleClient();
}
