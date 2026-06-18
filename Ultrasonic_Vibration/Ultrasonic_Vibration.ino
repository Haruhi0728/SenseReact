/**
 * Ultrasonic_Vibration.ino
 *
 * 超音波センサー(US-015) × 振動モーター 統合。
 * 「前方の対象が近いほど振動が強くなる」= SenseReactのフィードバックの核。
 *
 * 【回路】
 *   US-015: VCC→VOUT(5V) / GND→GND / Trig→D08 / Echo→D09(直結)
 *   振動  : モーター(+)→3.3V / モーター(-)→2SC1815 C / E→GND
 *           B→1kΩ→D05 / 1N4148をモーターと並列(帯=+側)
 *
 * 【動作】
 *   距離 NEAR_CM 以下 … 最大振動(MAX_FELT%)
 *   距離 FAR_CM  以上 … 停止
 *   その間          … 近いほど強く（線形）
 *
 * 【調整】
 *   NEAR_CM / FAR_CM … 反応する距離レンジ
 *   MIN_FELT         … 範囲に入った時の最弱（モーターが回る下限。弱すぎて回らなければ上げる）
 *   ※もしD05でPWMが効かなければ VIB_PIN を 3(D03) に変更
 */

const int TRIG_PIN    = 8;   // D08
const int ECHO_PIN    = 9;   // D09
const int VIB_PIN     = 5;   // D05（PWM）
const int SERIAL_BAUD = 115200;

// 距離レンジ（cm）
const int NEAR_CM = 5;    // これ以下で最大
const int FAR_CM  = 40;   // これ以上で停止

// 振動の強さ範囲（%）
const int MIN_FELT = 35;  // 範囲に入った瞬間の弱さ（回る下限付近）
const int MAX_FELT = 100; // 最接近時

// pulseInタイムアウト（FAR_CMより少し先まで。短くしてループを軽く）
const unsigned long ECHO_TIMEOUT_US = 4000;  // 約69cm相当

// ---- 振動の強さを 0〜100% で出力 ----
void vibSet(int percent) {
  percent = constrain(percent, 0, 100);
  analogWrite(VIB_PIN, map(percent, 0, 100, 0, 255));
}
void vibOff() { analogWrite(VIB_PIN, 0); }

// ---- 距離を測る（cm）。範囲外/失敗は -1 ----
long readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long dur = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);
  if (dur == 0) return -1;
  return dur / 58;
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  unsigned long t = millis();
  while (!Serial && (millis() - t < 3000)) { ; }

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(VIB_PIN, OUTPUT);
  vibOff();

  Serial.println("========================================");
  Serial.println("  超音波 × 振動 統合（近いほど強く）");
  Serial.println("========================================");
  Serial.print("  反応レンジ: ");
  Serial.print(NEAR_CM); Serial.print("〜"); Serial.print(FAR_CM);
  Serial.println(" cm");
  Serial.println("----------------------------------------");
}

void loop() {
  long d = readDistanceCm();

  int percent;
  if (d < 0 || d > FAR_CM) {
    percent = 0;  // 範囲外 → 停止
  } else {
    int dc = constrain(d, NEAR_CM, FAR_CM);
    percent = map(dc, NEAR_CM, FAR_CM, MAX_FELT, MIN_FELT);  // 近い=強い
  }
  vibSet(percent);

  // 表示
  Serial.print("距離: ");
  if (d < 0) Serial.print(" 範囲外");
  else { if (d < 10) Serial.print(" "); Serial.print(d); Serial.print("cm"); }
  Serial.print("  振動: ");
  if (percent < 100) Serial.print(" ");
  if (percent < 10)  Serial.print(" ");
  Serial.print(percent);
  Serial.print("% [");
  int bar = percent / 5;  // 最大20
  for (int i = 0; i < 20; i++) Serial.print(i < bar ? "#" : "-");
  Serial.println("]");

  delay(80);  // 約12Hzで更新
}
