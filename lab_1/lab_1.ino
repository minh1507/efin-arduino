#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h> 
#include <WebSocketsServer.h>
#include <DHT.h>

const char* apSSID = "ESP32_AP";  
const char* apPassword = "12345678";

Preferences preferences;
WebServer server(80);
WebSocketsServer webSocket(81);

#define DHTPIN 4        
#define DHTTYPE DHT11   
DHT dht(DHTPIN, DHTTYPE);

const int ledPins[] = {5, 18, 19, 21};  
const int ledCount = 4;
bool ledStates[ledCount] = {false, false, false, false};

String storedSSID;
String storedPassword;

void startAccessPoint() {
    WiFi.softAP(apSSID, apPassword);
    Serial.println("ESP32 in AP mode.");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
}

void handleGetDHT() {
    StaticJsonDocument<100> doc;
    doc["temperature"] = dht.readTemperature();
    doc["humidity"] = dht.readHumidity();
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void sendDHTData() {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    StaticJsonDocument<100> doc;
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;

    String jsonString;
    serializeJson(doc, jsonString);
    webSocket.broadcastTXT(jsonString);
}

bool connectToWiFi() {
    if (storedSSID.length() > 0 && storedPassword.length() > 0) {
        WiFi.begin(storedSSID.c_str(), storedPassword.c_str());
        Serial.print("Connecting to WiFi...");

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 10) {
            delay(1000);
            Serial.print(".");
            attempts++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nConnected!");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
            return true;
        }
    }
    return false;
}

void handleSetWiFi() {
    if (server.hasArg("plain")) {
        StaticJsonDocument<200> doc;
        deserializeJson(doc, server.arg("plain"));

        String newSSID = doc["ssid"].as<String>();
        String newPassword = doc["password"].as<String>();

        if (newSSID.length() > 0 && newPassword.length() > 0) {
            preferences.putString("ssid", newSSID);
            preferences.putString("password", newPassword);

            server.send(200, "application/json", "{\"status\": \"success\", \"message\": \"WiFi saved, restarting...\"}");
            delay(2000);
            ESP.restart();
        } else {
            server.send(400, "application/json", "{\"status\": \"error\", \"message\": \"Invalid SSID or password\"}");
        }
    } else {
        server.send(400, "application/json", "{\"status\": \"error\", \"message\": \"No data received\"}");
    }
}

void handleGetWiFi() {
    StaticJsonDocument<100> doc;
    doc["ssid"] = storedSSID;
    doc["status"] = WiFi.status() == WL_CONNECTED ? "connected" : "not connected";

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleResetWiFi() {
    preferences.remove("ssid");
    preferences.remove("password");
    preferences.end();
    
    server.send(200, "application/json", "{\"status\": \"success\", \"message\": \"WiFi credentials erased, restarting...\"}");
    delay(2000);
    ESP.restart();
}

void handleWiFiStatus() {
    StaticJsonDocument<50> doc;
    doc["status"] = (WiFi.status() == WL_CONNECTED) ? "on" : "off";

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSetLeds() {
    if (server.hasArg("plain")) {
        StaticJsonDocument<200> doc;
        deserializeJson(doc, server.arg("plain"));

        JsonArray leds = doc["leds"];
        for (int i = 0; i < leds.size(); i++) {
            int state = leds[i].as<int>();  
            digitalWrite(ledPins[i], state ? HIGH : LOW);
            ledStates[i] = state;
        }
        server.send(200, "application/json", "{\"status\": \"success\"}");
    } else {
        server.send(400, "application/json", "{\"status\": \"error\"}");
    }
}

void handleGetLeds() {
    StaticJsonDocument<100> doc;
    JsonArray array = doc.createNestedArray("leds");
    for (int i = 0; i < ledCount; i++) {
        array.add(ledStates[i]);
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void setup() {
    Serial.begin(115200);
    preferences.begin("wifi", false);

    storedSSID = preferences.getString("ssid", "");
    storedPassword = preferences.getString("password", "");

    for (int i = 0; i < ledCount; i++) {
        pinMode(ledPins[i], OUTPUT);
        digitalWrite(ledPins[i], LOW);
    }

    if (!connectToWiFi()) {
        startAccessPoint();
    }

    server.on("/set-wifi", HTTP_POST, handleSetWiFi);
    server.on("/get-wifi", HTTP_GET, handleGetWiFi);
    server.on("/reset-wifi", HTTP_POST, handleResetWiFi);
    server.on("/wifi-status", HTTP_GET, handleWiFiStatus);
    server.on("/set-leds", HTTP_POST, handleSetLeds);
    server.on("/get-leds", HTTP_GET, handleGetLeds);
    server.on("/get-dht", HTTP_GET, handleGetDHT);

    server.begin();
    Serial.println("Web server started.");
}

void loop() {
    server.handleClient();
    webSocket.loop();
    sendDHTData();
    delay(2000);
}
