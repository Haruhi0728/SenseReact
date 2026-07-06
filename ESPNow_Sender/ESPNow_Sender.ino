/**
 * ESPNow_Sender.ino
 *
 * ESP-NOW 送信テスト（ブロードキャスト）。
 * 0.5秒ごとにカウンタ付きメッセージを全方位に送る。MAC指定不要。
 * 書き込み: FQBN = esp32:esp32:esp32
 */

#include <esp_now.h>
#include <WiFi.h>

// ブロードキャストアドレス（全ESP32宛て）
// uint8_t broadcastAddr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t broadcastAddr[] = {0xFC, 0xF5, 0xC4, 0x1A, 0x47, 0xDC};
int count = 0;

void setup() {
  Serial.begin(115200);
  delay(300);

  WiFi.mode(WIFI_STA);
  Serial.print("送信側 MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW 初期化失敗");
    return;
  }

  // ブロードキャストを peer として登録
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, broadcastAddr, 6);
  peer.channel = 1;      // 現在のチャンネル
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("peer 追加失敗");
    return;
  }
  Serial.println("送信開始");
}

void loop() {
  char msg[32];
  snprintf(msg, sizeof(msg), "Hello %d", count++);
  esp_now_send(broadcastAddr, (const uint8_t *)msg, strlen(msg) + 1);
  Serial.print("送信: ");
  Serial.println(msg);
  delay(500);
}
