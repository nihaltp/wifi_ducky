#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <vector>

ESP8266WebServer server(80);
std::vector<String> targetSSIDs;
std::vector<String> targetBSSIDs;
const char* filename = "/targets.txt";

// MARK: saveTargets
void saveTargets() {
    File file = SPIFFS.open(filename, "w");
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
    File file = SPIFFS.open(filename, "r");
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

// MARK: handleRoot
void handleRoot() {
    String html = "<!DOCTYPE html><html><head><title>WiFi Tracker</title><meta http-equiv='refresh' content='10'>";
    html += "<style>body{font-family:sans-serif;}table{border-collapse:collapse;width:100%;}th,td{border:1px solid #ccc;padding:8px;}</style></head><body>";
    html += "<h2>Tracked SSIDs / BSSIDs</h2><table><tr><th>SSID</th><th>BSSID</th><th>Remove</th></tr>";
    
    for (size_t i = 0; i < targetSSIDs.size(); ++i) {
        html += "<tr><td>" + targetSSIDs[i] + "</td><td>" + targetBSSIDs[i] + "</td>";
        html += "<td><a href='/delete?i=" + String(i) + "'>❌</a></td></tr>";
    }
    
    html += "</table><br><h3>Add New Target</h3>";
    html += "<form action='/add' method='GET'>";
    html += "SSID: <input name='ssid'> BSSID: <input name='bssid'> <input type='submit' value='Add'></form><hr>";
    
    // WiFi Scan Table
    html += "<h3>Nearby Networks</h3><table><tr><th>SSID</th><th>BSSID</th><th>RSSI</th><th>Status</th></tr>";
    int n = WiFi.scanNetworks(false, true);
    for (int i = 0; i < n; ++i) {
        String ssid = WiFi.SSID(i);
        uint8_t* b = WiFi.BSSID(i);
        char bssidStr[18];
        sprintf(bssidStr, "%02X:%02X:%02X:%02X:%02X:%02X", b[0], b[1], b[2], b[3], b[4], b[5]);
        
        bool matched = false;
        for (size_t j = 0; j < targetSSIDs.size(); ++j) {
            if (targetSSIDs[j] == ssid || targetBSSIDs[j] == String(bssidStr)) {
                matched = true;
                break;
            }
        }
        
        html += "<tr><td>" + ssid + "</td><td>" + String(bssidStr) + "</td>";
        html += "<td>" + String(WiFi.RSSI(i)) + " dBm</td>";
        html += "<td>" + String(matched ? "✅" : "❌") + "</td></tr>";
    }
    html += "</table><p>Auto-refreshes every 10 seconds.</p></body></html>";
    
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

// MARK: setup
void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_AP);
    WiFi.softAP("WiFi Ducky", "12345678");
    
    if (!SPIFFS.begin()) {
        Serial.println("❌ SPIFFS failed to mount!");
        return;
    }
    
    loadTargets();
    
    server.on("/", handleRoot);
    server.on("/add", handleAdd);
    server.on("/delete", handleDelete);
    server.begin();
    
    Serial.println("🌐 Web server started at 192.168.4.1");
}

// MARK: loop
void loop() {
    server.handleClient();
}
