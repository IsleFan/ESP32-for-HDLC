import paho.mqtt.client as mqtt
import requests
import json
import threading
import time
import lz4.frame

ESP32_MQTT_SERVER = "mqtt.example.com"
ESP32_MQTT_TOPIC = "esp32/hdlc"
ESP32_HTTP_API_URL = "http://esp32-ip-address/api/config"

def set_esp32_config(new_mqtt_server, new_api_url):
    payload = {"mqtt_server": new_mqtt_server, "api_url": new_api_url}
    headers = {"Content-Type": "application/json"}
    response = requests.post(ESP32_HTTP_API_URL, data=json.dumps(payload), headers=headers)
    print(response.status_code, response.text)

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        client.subscribe(ESP32_MQTT_TOPIC)
    else:
        print(f"MQTT 連接失敗，錯誤碼: {rc}")

def on_message(client, userdata, msg):
    try:
        decompressed_data = lz4.frame.decompress(msg.payload)
        print(f"收到 MQTT 訊息: {decompressed_data.hex()}")
    except Exception as e:
        print(f"LZ4 解壓縮失敗: {e}")

def start_mqtt():
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(ESP32_MQTT_SERVER, 1883, 60)
    client.loop_forever()

if __name__ == "__main__":
    mqtt_thread = threading.Thread(target=start_mqtt, daemon=True)
    mqtt_thread.start()
    time.sleep(5)
    set_esp32_config("new-mqtt.example.com", "http://new-api-server.com/hdlc-data")
    while True:
        time.sleep(10)
