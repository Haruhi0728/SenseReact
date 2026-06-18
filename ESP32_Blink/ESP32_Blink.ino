/**
 * ESP32_Blink.ino
 *
 * ESP32-WROOM-32 の動作確認（Lチカ＋シリアル）。
 * 書き込み: FQBN = esp32:esp32:esp32 / ポート = COM7（環境による）
 */

const int LED_PIN = 2;  // 多くのESP32 DevKitはGPIO2にオンボードLED

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  delay(500);
  Serial.println("ESP32 起動OK");
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  Serial.println("LED ON");
  delay(500);
  digitalWrite(LED_PIN, LOW);
  Serial.println("LED OFF");
  delay(500);
}
