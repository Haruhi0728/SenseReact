/**
 * ESPNow_Receiver.ino
 *
 * ESP-NOW 受信テスト（ブロードキャスト受信）。
 * 送信側ESP32からのメッセージを受け取ってシリアルに表示する。
 * 書き込み: FQBN = esp32:esp32:esp32
 *
 * ※ESP32コア3.x系のため、受信コールバックは esp_now_recv_info_t 形式。
 */

#include <esp_now.h>
#include <WiFi.h>

// 受信コールバック（メッセージが届くたびに呼ばれる）
void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           info->src_addr[0], info->src_addr[1], info->src_addr[2],
           info->src_addr[3], info->src_addr[4], info->src_addr[5]);

  Serial.print("受信 (");
  Serial.print(macStr);
  Serial.print("): ");
  for (int i = 0; i < len; i++) Serial.print((char)data[i]);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(300);

  WiFi.mode(WIFI_STA);

  WiFi.printDiag(Serial);
  
  Serial.print("受信側 MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW 初期化失敗");
    return;
  }
  esp_now_register_recv_cb(onRecv);
  Serial.println("受信待ち...");
}

void loop() {
  delay(1000);
}
