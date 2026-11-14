#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>

// AP Settings
const char* ssid = "Hehe Wifi";

// DNS Settings
const byte DNS_PORT = 53;
IPAddress apIP(200, 200,200, 1); // Default ESP32 AP IP is usually 4.1
DNSServer dnsServer;

// Start web server
AsyncWebServer server(80);

// Global Variables
int visitors = 0;
int totalSeconds = 0;
int avgSeconds = 0;
int timesInfoViewed = 0;

// Function Prototypes
void savePersistentData();
void saveIndividualData(int seconds, bool scrolledDown);

// Adds a visitor, recalculates average time spent
void incrementVisitorCount(int secondsSpent, bool scrolledDown) {
    visitors += 1;
    totalSeconds += secondsSpent;
    avgSeconds = (visitors > 0) ? (totalSeconds / visitors) : 0;
    
    if (scrolledDown) {
        timesInfoViewed += 1;
    }

    savePersistentData();
    saveIndividualData(secondsSpent, scrolledDown);
}

void savePersistentData() {
    File file = SPIFFS.open("/persistent.txt", "w");
    if (!file) return;

    StaticJsonDocument<128> doc;
    doc["v"] = visitors;
    doc["s"] = avgSeconds;
    doc["i"] = timesInfoViewed;

    serializeJson(doc, file);
    file.close();
}

void saveIndividualData(int seconds, bool scrolledDown) {
    File file = SPIFFS.open("/stats.txt", "a");
    if (!file) return;
    
    file.printf("v: % d, %s\n", seconds, scrolledDown ? "true" : "false");
    file.close();
}

void setup() {
    Serial.begin(115200);

    // 1. Initialize SPIFFS
    if (!SPIFFS.begin()) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    // 2. Start Access Point
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(ssid);

    // 3. Start DNS server (Captive Portal Magic)
    dnsServer.start(DNS_PORT, "*", apIP);

    // 4. Load saved data
    if (SPIFFS.exists("/persistent.txt")) {
        File file = SPIFFS.open("/persistent.txt", "r");
        StaticJsonDocument<128> doc;
        DeserializationError error = deserializeJson(doc, file);
        if (!error) {
            visitors = doc["v"];
            avgSeconds = doc["s"];
            timesInfoViewed = doc["i"];
            totalSeconds = visitors * avgSeconds;
        }
        file.close();
    }

    // --- Web Server Routes ---

    // Captive Portal Detection
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", "text/html");
    });

    server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", "text/html");
    });

    // Static Assets
    server.on("/css/site.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/css/site.css", "text/css");
    });
    
    server.on("/js/site.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/js/site.js", "application/javascript");
    });

    server.on("/media/rick.gif", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/media/rick.gif", "image/gif");
    });

    // Data Endpoints
    server.on("/addvisit", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("s") && request->hasParam("v")) {
            int s = request->getParam("s")->value().toInt();
            bool v = request->getParam("v")->value() == "1";
            incrementVisitorCount(s, v);
        }
        request->send(200, "text/plain", "OK");
    });

    server.begin();
    Serial.println("Server Started");
}

void loop() {
    dnsServer.processNextRequest();
}