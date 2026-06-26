/**
 * Bridge_ESP_Sender.ino  （送信ブリッジ ESP32）
 *
 * SPRESENSEからUART(Serial2)で受けた "left,right" を
 * ESP-NOWで車体(Car_Receiver)へブロードキャストする。
 * ＝ Drive_Test_Sender の置き換え（テストパターンではなく実データを流す）。
 *
 * 【配線】
 *   ESP32 GPIO16(RX2) ← SPRESENSE D01(TX)
 *   GND を SPRESENSE と共通に
 *   （双方向にするなら GPIO17(TX2) → SPRESENSE D00 も。今は受信のみでOK）
 */

#include <esp_now.h>
#include <WiFi.h>

typedef struct {
  int16_t left;
  int16_t right;
} DriveCmd;

uint8_t broadcastAddr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
DriveCmd cmd = {0, 0};
unsigned long lastRx = 0;

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);  // RX=GPIO16, TX=GPIO17

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW 初期化失敗");
    return;
  }
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, broadcastAddr, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  Serial.println("ブリッジ開始（SPRESENSE→ESP-NOW）");
}

void loop() {
  // SPRESENSEから "left,right\n" を受信
  if (Serial2.available()) {
    String line = Serial2.readStringUntil('\n');
    int comma = line.indexOf(',');
    if (comma > 0) {
      cmd.left  = line.substring(0, comma).toInt();
      cmd.right = line.substring(comma + 1).toInt();
      lastRx = millis();
    }
  }

  // フェイルセーフ：500ms受信が無ければ停止
  if (millis() - lastRx > 500) {
    cmd.left = 0;
    cmd.right = 0;
  }

  // 100msごとに車体へ送信
  static unsigned long t = 0;
  if (millis() - t >= 100) {
    t = millis();
    esp_now_send(broadcastAddr, (const uint8_t *)&cmd, sizeof(cmd));
    Serial.print("→車体 L=");
    Serial.print(cmd.left);
    Serial.print(" R=");
    Serial.println(cmd.right);
  }
}
