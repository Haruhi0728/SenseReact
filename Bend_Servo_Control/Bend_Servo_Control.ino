/**
 * Bend_Servo_Control.ino
 *
 * 曲げセンサーの曲げ具合に応じて連続回転サーボの速度が変わる
 * まっすぐ(0%) → 停止、100%に近づくほど高速回転
 *
 * 【連続回転サーボの制御値】
 *   write(90)  → 停止
 *   write(180) → 最高速（正転）
 *   write(0)   → 最高速（逆転）
 *
 * 【回路】
 *   曲げセンサー: IOREF → センサー → A3 → 10kΩ → GND
 *   サーボ 茶（GND）   → 拡張ボード GND
 *   サーボ 赤（VCC）   → 拡張ボード VOUT（5V）
 *   サーボ 黄（Signal）→ 拡張ボード D06
 */

#include <Servo.h>

const int BEND_PIN      = A3;
const int SERVO_PIN     = 6;
const int SERIAL_BAUD   = 115200;
const int BEND_MAX_DIFF = 260;  // フラット(840) - 100%閾値(580) = 260

Servo myServo;
int flatValue = 0;

void setup() {
  Serial.begin(SERIAL_BAUD);
  unsigned long startWait = millis();
  while (!Serial && (millis() - startWait < 3000)) { ; }

  Serial.println("========================================");
  Serial.println("  曲げセンサー × 連続回転サーボ");
  Serial.println("========================================");

  myServo.attach(SERVO_PIN);
  myServo.write(90);  // まず停止

  delay(500);
  flatValue = analogRead(BEND_PIN);
  Serial.print("基準値（まっすぐ）: ");
  Serial.println(flatValue);
  Serial.println("→ 曲げるほど速く回転します");
  Serial.println("----------------------------------------");
}

void loop() {
  // 曲げセンサー読み取り
  int raw     = analogRead(BEND_PIN);
  int diff    = flatValue - raw;
  int percent = constrain(map(diff, 0, BEND_MAX_DIFF, 0, 100), 0, 100);

  // パーセントをサーボ速度に変換（マイクロ秒で制御）
  // 1500μs = 停止、2000μs = 最高速（正転）
  int servoUs;
  if (percent < 5) {
    servoUs = 1500;  // 停止
  } else {
    servoUs = map(percent, 5, 100, 1510, 2500);
  }

  myServo.writeMicroseconds(servoUs);

  Serial.print("曲げ: ");
  Serial.print(percent);
  Serial.print("% | μs: ");
  Serial.println(servoUs);

  delay(50);
}
