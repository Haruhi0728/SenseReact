/**
 * Vibration_Test.ino
 *
 * 振動モーター（4E060 + 2SC1815）単体テスト＆配線切り分け。
 *
 * 【回路】
 *   モーター(+) → 3.3V
 *   モーター(-) → トランジスタC
 *   トランジスタE → GND
 *   トランジスタB → 1kΩ → D05
 *   ダイオード1N4148 をモーターと並列（帯=+側）
 *
 * 【切り分け】
 *   起動直後にD05をLOWにして3秒待つ。
 *   - ここで止まれば配線OK
 *   - ここで回り続けたら配線ミス（トランジスタを経由していない）
 */

const int VIB_PIN    = 5;   // D05（PWM対応）
const int SERIAL_BAUD = 115200;

void setup() {
  Serial.begin(SERIAL_BAUD);
  unsigned long t = millis();
  while (!Serial && (millis() - t < 3000)) { ; }

  pinMode(VIB_PIN, OUTPUT);
  digitalWrite(VIB_PIN, LOW);   // まず確実に停止

  Serial.println("========================================");
  Serial.println("  振動モーター 単体テスト");
  Serial.println("========================================");
  Serial.println("[切り分け] D05=LOW にしました。");
  Serial.println("  → ここでモーターが止まれば配線OK");
  Serial.println("  → 回り続けたら配線ミスです");
  Serial.println("3秒後に強さテストを開始します...");
  delay(3000);
}

void loop() {
  // 弱 → 強（PWMで強さが変わるか確認）
  Serial.println("[テスト] 弱→強");
  for (int v = 0; v <= 255; v += 15) {
    analogWrite(VIB_PIN, v);
    Serial.print("  強さ: ");
    Serial.println(v);
    delay(150);
  }

  // 停止
  analogWrite(VIB_PIN, 0);
  Serial.println("[テスト] 停止（1.5秒）");
  delay(1500);
}
