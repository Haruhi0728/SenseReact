/**
 * Vibration_Test.ino
 *
 * 振動モーター（4E060 + 2SC1815）の制御テスト。
 * 強さを段階的に変え、PWM（analogWrite）が効くかを確認する。
 *
 * 【回路】
 *   モーター(+) → 3.3V
 *   モーター(-) → トランジスタC
 *   トランジスタE → GND
 *   トランジスタB → 1kΩ → D05
 *   ダイオード1N4148 をモーターと並列（帯=+側）
 *
 * 【確認ポイント】
 *   - 起動直後にOFF（D05=LOW）で止まるか → 配線OKの確認
 *   - 25→50→75→100% で振動が強くなるか → PWMが効いている
 *   ※もし強弱が変わらずON/OFFだけなら、D05がPWM非対応の可能性。
 *     VIB_PIN を 3（D03）に変えて試す。
 *
 * 【MVPへの流用】
 *   vibSet() / vibOff() / vibPulse() は超音波連動でそのまま使える。
 */

const int VIB_PIN     = 5;   // D05（PWM対応のはず。効かなければ3=D03へ）
const int SERIAL_BAUD = 115200;

// ---- 振動の強さを 0〜100% で指定 ----
void vibSet(int percent) {
  percent = constrain(percent, 0, 100);
  int duty = map(percent, 0, 100, 0, 255);
  analogWrite(VIB_PIN, duty);
}

// ---- 停止 ----
void vibOff() {
  analogWrite(VIB_PIN, 0);
}

// ---- パルス振動（times回、percentの強さで onMs鳴らしてoffMs休む）----
void vibPulse(int times, int percent, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    vibSet(percent);
    delay(onMs);
    vibOff();
    delay(offMs);
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  unsigned long t = millis();
  while (!Serial && (millis() - t < 3000)) { ; }

  pinMode(VIB_PIN, OUTPUT);
  vibOff();  // まず確実に停止

  Serial.println("========================================");
  Serial.println("  振動モーター 制御テスト");
  Serial.println("========================================");
  Serial.println("[OFF] D05=LOW。ここで止まれば配線OK");
  Serial.println("3秒後に強さテストを開始...");
  delay(3000);
}

void loop() {
  // ---- 強さ段階テスト（PWM確認）----
  int levels[] = {25, 50, 75, 100};
  for (int i = 0; i < 4; i++) {
    Serial.print("[強さ] ");
    Serial.print(levels[i]);
    Serial.println("%");
    vibSet(levels[i]);
    delay(1200);
  }

  // ---- 停止 ----
  vibOff();
  Serial.println("[OFF] 停止");
  delay(1000);

  // ---- パルス振動（通知っぽい動き）----
  Serial.println("[パルス] 100%で3回");
  vibPulse(3, 100, 200, 200);

  Serial.println("----------------------------------------");
  delay(1500);
}
