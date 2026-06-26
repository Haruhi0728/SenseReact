/**
 * Bridge_SPRESENSE.ino  （装着側 SPRESENSE）
 *
 * 曲げセンサーを読んで、左右の速度指令を UART(Serial2) で ESP32 へ送る。
 * 今は「曲げ＝前進アクセル」（左右同じ値）。ステア(IMU)は後で left/right に差をつけて追加。
 *
 * 【配線】
 *   曲げセンサー : IOREF(1.8V)→センサー→A3→10kΩ→GND
 *   UART        : SPRESENSE D01(TX) → ESP32 GPIO16(RX2)
 *                 （GND を ESP32 と共通に。最低この2本でOK）
 *
 * 送信フォーマット: "left,right\n"（例 "60,60"）
 */

const int BEND_PIN      = A3;
const int BEND_MAX_DIFF = 260;   // README準拠
const int SERIAL_BAUD   = 115200;

int flatValue = 0;

void setup() {
  Serial.begin(SERIAL_BAUD);    // USB（デバッグ表示用）
  Serial2.begin(SERIAL_BAUD);   // D00/D01 のUART（ESP32へ）

  delay(500);
  flatValue = analogRead(BEND_PIN);  // 起動時のまっすぐを基準に
  Serial.print("基準値（まっすぐ）= ");
  Serial.println(flatValue);
  Serial.println("曲げると前進指令を送ります");
}

void loop() {
  int raw  = analogRead(BEND_PIN);
  int diff = flatValue - raw;
  int throttle = constrain(map(diff, 0, BEND_MAX_DIFF, 0, 100), 0, 100);

  int left  = throttle;   // 今は直進のみ（左右同じ）
  int right = throttle;

  // ESP32へ "left,right"
  Serial2.print(left);
  Serial2.print(",");
  Serial2.println(right);

  // デバッグ
  Serial.print("送信 L=");
  Serial.print(left);
  Serial.print(" R=");
  Serial.println(right);

  delay(50);
}
