# ESP32-for-HDLC

🔥 主要功能

✅ ESP32 透過 GPIO 讀取 HDLC 並解析
✅ ESP32 優先使用 MQTT，失敗則切換 API 傳輸
✅ ESP32 內建 HTTP API，允許遠端變更 MQTT & API 設定
✅ ESP32 設定存入 EEPROM，確保重啟後仍適用
✅ ESP32 支援 LZ4 壓縮，提高傳輸效率
✅ ESP32 支援 WiFi & Ethernet，優先使用 Ethernet
✅ Python 端支援 MQTT & HTTP API 接收 HDLC 數據
✅ Python 端可變更 ESP32 設定，確保即時生效
