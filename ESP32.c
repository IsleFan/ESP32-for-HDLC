#include <WiFi.h>
#include <ETH.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include <LZ4.h>
#include "esp_task_wdt.h"

#define WIFI_SSID "Your_SSID"
#define WIFI_PASSWORD "Your_PASSWORD"
#define EEPROM_SIZE 512

WebServer server(80);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

#define API_URL_ADDRESS 0
#define MQTT_SERVER_ADDRESS 128
#define HDLC_DI_PIN  34
#define DEFAULT_BAUD_RATE 9600
#define HDLC_FLAG 0x7E
#define HDLC_BUFFER_SIZE 256
#define HDLC_QUEUE_SIZE 10
#define SAMPLES_PER_BIT  8
#define WDT_TIMEOUT 5

char mqttServer[128];
char apiUrl[128];
bool eth_connected = false;
hw_timer_t *timer = NULL;
volatile int timerInterval = 0;
QueueHandle_t hdlcQueue;

// 📌 EEPROM 讀取/寫入
void readEEPROM(int address, char* data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        data[i] = EEPROM.read(address + i);
    }
    data[length - 1] = '\0';
}

void writeEEPROM(int address, const char* data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        EEPROM.write(address + i, data[i]);
    }
    EEPROM.commit();
}

void saveSettings(const char* newMqtt, const char* newApi) {
    strncpy(mqttServer, newMqtt, sizeof(mqttServer));
    strncpy(apiUrl, newApi, sizeof(apiUrl));
    writeEEPROM(MQTT_SERVER_ADDRESS, mqttServer, 128);
    writeEEPROM(API_URL_ADDRESS, apiUrl, 128);
}

void handleConfigUpdate() {
    if (server.hasArg("plain")) {
        DynamicJsonDocument doc(256);
        deserializeJson(doc, server.arg("plain"));
        const char* newMqtt = doc["mqtt_server"];
        const char* newApi = doc["api_url"];
        if (newMqtt && newApi) {
            saveSettings(newMqtt, newApi);
            server.send(200, "application/json", "{\"status\":\"success\"}");
            return;
        }
    }
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid request\"}");
}

void handleGetConfig() {
    DynamicJsonDocument doc(256);
    doc["mqtt_server"] = mqttServer;
    doc["api_url"] = apiUrl;
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

int calculateTimerInterval(int baudRate) {
    return (1.0 / baudRate * 1e6) / SAMPLES_PER_BIT;
}

void WiFiEvent(WiFiEvent_t event) {
    if (event == SYSTEM_EVENT_ETH_GOT_IP) {
        eth_connected = true;
        WiFi.disconnect();
    } else if (event == SYSTEM_EVENT_ETH_DISCONNECTED) {
        eth_connected = false;
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
}

void IRAM_ATTR onTimer() {
    static uint8_t hdlcBuffer[HDLC_BUFFER_SIZE];
    static size_t hdlcIndex = 0;
    uint8_t receivedByte = digitalRead(HDLC_DI_PIN);
    if (receivedByte == HDLC_FLAG && hdlcIndex > 2) {
        xQueueSendFromISR(hdlcQueue, hdlcBuffer, NULL);
        hdlcIndex = 0;
    } else if (hdlcIndex < HDLC_BUFFER_SIZE) {
        hdlcBuffer[hdlcIndex++] = receivedByte;
    }
}

void sendData(const uint8_t* data, size_t length) {
    uint8_t compressedBuffer[HDLC_BUFFER_SIZE];
    int compressedSize = (length > 64) ? LZ4_compress((char*)data, (char*)compressedBuffer, length) : length;
    if (mqttClient.publish("esp32/hdlc", compressedBuffer, compressedSize) == false) {
        HTTPClient http;
        http.begin(apiUrl);
        http.addHeader("Content-Type", "application/octet-stream");
        http.POST(compressedBuffer, compressedSize);
        http.end();
    }
}

void transmissionTask(void *pvParameters) {
    uint8_t dataBuffer[HDLC_BUFFER_SIZE];
    while (true) {
        if (xQueueReceive(hdlcQueue, dataBuffer, portMAX_DELAY)) sendData(dataBuffer, HDLC_BUFFER_SIZE);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(HDLC_DI_PIN, INPUT);
    EEPROM.begin(EEPROM_SIZE);
    readEEPROM(MQTT_SERVER_ADDRESS, mqttServer, 128);
    readEEPROM(API_URL_ADDRESS, apiUrl, 128);
    WiFi.onEvent(WiFiEvent);
    ETH.begin();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    server.on("/api/config", HTTP_POST, handleConfigUpdate);
    server.on("/api/config", HTTP_GET, handleGetConfig);
    server.begin();
    mqttClient.setServer(mqttServer, 1883);
    hdlcQueue = xQueueCreate(HDLC_QUEUE_SIZE, HDLC_BUFFER_SIZE);
    xTaskCreatePinnedToCore(transmissionTask, "TransmissionTask", 4096, NULL, 1, NULL, 1);
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, calculateTimerInterval(DEFAULT_BAUD_RATE), true);
    timerAlarmEnable(timer);
}

void loop() {
    server.handleClient();
    mqttClient.loop();
    delay(100);
}
