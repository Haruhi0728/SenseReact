/**
 * Servo_Test.ino
 *
 * サーボモーター動作確認スケッチ
 *
 * 【回路】
 *   サーボ 黒（GND）   → 拡張ボード GND
 *   サーボ 赤（VCC）   → 拡張ボード VOUT（5V）
 *   サーボ 白（Signal）→ 拡張ボード D06
 */

#include <Servo.h>

const int SERVO_PIN = 6;   // D06
const int SERIAL_BAUD = 115200;

Servo myServo;

void setup() {
  Serial.begin(SERIAL_BAUD);
  unsigned long startWait = millis();
  while (!Serial && (millis() - startWait < 3000)) { ; }

  Serial.println("========================================");
  Serial.println("  サーボ 動作確認スケッチ");
  Serial.println("========================================");

  myServo.attach(SERVO_PIN);
  Serial.println("サーボを中央（90度）に移動します...");
  myServo.write(90);
  delay(1000);

  Serial.println("スイープ開始（0度 → 180度 → 0度）");
}

void loop() {
  // 0度 → 180度
  for (int angle = 0; angle <= 180; angle += 5) {
    myServo.write(angle);
    Serial.print("角度: ");
    Serial.println(angle);
    delay(50);
  }

  delay(500);

  // 180度 → 0度
  for (int angle = 180; angle >= 0; angle -= 5) {
    myServo.write(angle);
    Serial.print("角度: ");
    Serial.println(angle);
    delay(50);
  }

  delay(500);
}
