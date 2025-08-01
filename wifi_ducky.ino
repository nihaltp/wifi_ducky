#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <vector>
#include <DNSServer.h>

ESP8266WebServer server(80);
std::vector<String> targetSSIDs;
std::vector<String> targetBSSIDs;
const char* targets = "/targets.txt";

bool toggle = true; // state of auto refresh
bool matched = false; // if the target is found

const byte DNS_PORT = 53;
DNSServer dnsServer;

const int ledPin = 2;

// MARK: saveTargets
void saveTargets() {
    File file = SPIFFS.open(targets, "w");
    if (!file) {
        Serial.println("❌ Failed to open file for writing");
        return;
    }
    
    for (size_t i = 0; i < targetSSIDs.size(); ++i) {
        file.println(targetSSIDs[i] + "," + targetBSSIDs[i]);
    }
    file.close();
    Serial.println("✅ Target list saved.");
}

// MARK: loadTargets
void loadTargets() {
    File file = SPIFFS.open(targets, "r");
    if (!file) {
        Serial.println("⚠️ No existing target list found.");
        return;
    }
    
    while (file.available()) {
        String line = file.readStringUntil('\n');
        int commaIndex = line.indexOf(',');
        if (commaIndex > 0) {
            String ssid = line.substring(0, commaIndex);
            String bssid = line.substring(commaIndex + 1);
            targetSSIDs.push_back(ssid);
            targetBSSIDs.push_back(bssid);
        }
    }
    file.close();
    Serial.println("✅ Loaded target list from SPIFFS.");
}

String strengthColor(int rssi) {
    if (rssi >= -65) return "green";
    else if (rssi >= -75) return "orange";
    else return "red";
}

String encryptionTypeName(uint8_t type) {
    switch (type) {
        case ENC_TYPE_NONE: return "Open &#128275;";
        case ENC_TYPE_WEP: return "WEP &#128274;";
        case ENC_TYPE_TKIP: return "WPA &#128274;";
        case ENC_TYPE_CCMP: return "WPA2 &#128274;";
        case ENC_TYPE_AUTO: return "Auto &#128274;";
        default: return "Unknown";
    }
}

// MARK: handleRoot
void handleRoot() {
    String html = "<!DOCTYPE html><html><head><title>WiFi Ducky</title>";
    if (toggle) html += "<meta http-equiv='refresh' content='10'>"; // refreshes every 10 seconds
    html += "<style>body{font-family:sans-serif;}table{border-collapse:collapse;width:100%;}th,td{border:1px solid #ccc;padding:8px;}</style></head><body>";
    html += "<h2>Tracked SSIDs / BSSIDs</h2><table><tr><th>SSID</th><th>BSSID</th><th>Remove</th></tr>";
    
    for (size_t i = 0; i < targetSSIDs.size(); ++i) {
        html += "<tr><td>" + targetSSIDs[i] + "</td><td>" + targetBSSIDs[i] + "</td>";
        html += "<td><a href='/delete?i=" + String(i) + "'>&#10060;</a></td></tr>";  // ❌
    }
    
    html += "</table><br><h3>Add New Target</h3>";
    html += "<form action='/add' method='GET'>";
    html += "SSID: <input name='ssid'> BSSID: <input name='bssid'> <input type='submit' value='Add'></form><hr>";
    
    // WiFi Scan Table
    html += "<h3>Nearby Networks</h3><table><tr><th>SSID</th><th>BSSID</th><th>RSSI</th><th>Status</th><th>Encryption</th></tr>";
    int n = WiFi.scanNetworks(false, true);
    
    int indices[n];
    for (int i = 0; i < n; i++) {
        indices[i] = i;
    }
    
    // Simple bubble sort by RSSI
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
                int temp = indices[i];
                indices[i] = indices[j];
                indices[j] = temp;
            }
        }
    }
    
    for (int i = 0; i < n; ++i) {
        int index = indices[i];
        String ssid = WiFi.SSID(index);
        uint8_t* b = WiFi.BSSID(index);
        char bssidStr[18];
        sprintf(bssidStr, "%02X:%02X:%02X:%02X:%02X:%02X", b[0], b[1], b[2], b[3], b[4], b[5]);
        
        matched = false;
        for (size_t j = 0; j < targetSSIDs.size(); ++j) {
            if (targetSSIDs[j] == ssid || targetBSSIDs[j] == String(bssidStr)) {
                matched = true;
                break;
            }
        }
        
        html += "<tr><td>" + ssid + "</td><td>" + String(bssidStr) + "</td>";
        html += "<td style='color:" + strengthColor(WiFi.RSSI(index)) + "'>" + String(WiFi.RSSI(index)) + " dBm</td>";
        html += "<td>" + String(matched ? "&#9989;" : "&#10060;") + "</td>"; // ✅ : ❌
        html += "<td>" + encryptionTypeName(WiFi.encryptionType(index)) + "</td></tr>";
    }
    html += "<a href = '/refresh'><button>Toggle: " + String(toggle ? "ON" : "OFF") + "</button></a>";
    html += "</table>";
    if (toggle) html += "<p>Auto-refreshes every 10 seconds.</p>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

// MARK: handleAdd
void handleAdd() {
    if (server.hasArg("ssid") && server.hasArg("bssid")) {
        String ssid = server.arg("ssid");
        String bssid = server.arg("bssid");
        if (ssid.length() > 0 || bssid.length() > 0) {
            targetSSIDs.push_back(ssid);
            targetBSSIDs.push_back(bssid);
            saveTargets();
        }
    }
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}

// MARK: handleDelete
void handleDelete() {
    if (server.hasArg("i")) {
        int index = server.arg("i").toInt();
        if (index >= 0 && index < (int)targetSSIDs.size()) {
            targetSSIDs.erase(targetSSIDs.begin() + index);
            targetBSSIDs.erase(targetBSSIDs.begin() + index);
            saveTargets();
        }
    }
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}

// MARK: toggleRefresh
void toggleRefresh() {
    toggle = !toggle;
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}

void blinkLed(bool found) {
    static unsigned long lastBlink = 0;
    static bool ledState = false;
    
    if (found) {
        if (millis() - lastBlink >= 500) {
            lastBlink = millis();
            ledState = !ledState;
            digitalWrite(ledPin, ledState ? HIGH : LOW);
        }
    } else {
        digitalWrite(ledPin, ledState ? HIGH : LOW);
        ledState = false;
    }
}

// MARK: setup
void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_AP);
    WiFi.softAP("WiFi Ducky", "12345678");
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    
    if (!SPIFFS.begin()) {
        Serial.println("❌ SPIFFS failed to mount!");
        return;
    }
    
    loadTargets();
    
    server.on("/", handleRoot);
    server.on("/add", handleAdd);
    server.on("/delete", handleDelete);
    server.on("/refresh", toggleRefresh);
    server.begin();
    
    Serial.println("🌐 Web server started at 192.168.4.1");
    
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);
}

// MARK: loop
void loop() {
    dnsServer.processNextRequest();
    server.handleClient();
    blinkLed(matched);
}
