/**
 * MPU9250_Motion.ino
 *
 * MPU-9250（加速度＋ジャイロ＋磁気）で「腕の動き量」を測るテスト。
 * ジャイロの回転速度の大きさ＝動きの指標（静止≈0、動かすと跳ねる）。
 *
 * 【配線（SPRESENSE I2C）】
 *   VCC → 3.3V / GND → GND / SDA → D14 / SCL → D15 / AD0 → GND（0x68）
 *
 * ※ライブラリ不要：I2Cレジスタを直接読む（SPRESENSEで確実に動かすため）。
 *   まず加速度・ジャイロのみ。磁気（方位）は後で追加。
 */

#include <Wire.h>

const uint8_t MPU = 0x68;          // AD0=GNDなら0x68（HIGHなら0x69）
const int SERIAL_BAUD = 115200;
const float MOTION_THRESH = 30.0;  // 動き判定のしきい値（deg/s合計）

void writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

uint8_t readReg(uint8_t reg) {
  Wire.beginTransmission(MPU);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((int)MPU, 1);
  return Wire.read();
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  unsigned long t = millis();
  while (!Serial && (millis() - t < 3000)) { ; }

  Wire.begin();
  Wire.setClock(100000);  // 100kHzに下げて安定化（ブレッドボード配線対策）

  uint8_t who = readReg(0x75);  // WHO_AM_I（MPU9250は0x71期待）
  Serial.print("WHO_AM_I = 0x");
  Serial.println(who, HEX);
  if (who != 0x71 && who != 0x73) {
    Serial.println("⚠ MPU9250が見つからない？配線/アドレスを確認");
  }

  writeReg(0x6B, 0x00);  // PWR_MGMT_1=0 → スリープ解除
  delay(100);

  Serial.println("========================================");
  Serial.println("  MPU-9250 腕の動き量モニタ");
  Serial.println("========================================");
}

// 軸の値を符号付きで表示
void pAxis(const char *name, float v) {
  Serial.print(name);
  if (v >= 0) Serial.print("+");
  Serial.print(v, 2);
  Serial.print(" ");
}

void loop() {
  // 加速度6 ＋ 温度2 ＋ ジャイロ6 ＝ 14バイトを0x3Bからまとめて読む
  Wire.beginTransmission(MPU);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom((int)MPU, 14);

  // 14バイト揃わなければ読み取り失敗 → その回はスキップ（バスをクリア）
  if (Wire.available() < 14) {
    Serial.println("I2C読み取り失敗（配線/接触を確認）");
    while (Wire.available()) Wire.read();
    delay(50);
    return;
  }

  uint8_t b[14];
  for (int i = 0; i < 14; i++) b[i] = Wire.read();

  int16_t axr = (int16_t)((b[0]  << 8) | b[1]);
  int16_t ayr = (int16_t)((b[2]  << 8) | b[3]);
  int16_t azr = (int16_t)((b[4]  << 8) | b[5]);
  int16_t gxr = (int16_t)((b[8]  << 8) | b[9]);
  int16_t gyr = (int16_t)((b[10] << 8) | b[11]);
  int16_t gzr = (int16_t)((b[12] << 8) | b[13]);

  // 加速度 ±2g → 16384 LSB/g（傾き・向き）
  float ax = axr / 16384.0, ay = ayr / 16384.0, az = azr / 16384.0;
  // ジャイロ ±250dps → 131 LSB/(deg/s)（各軸の回転速度）
  float gx = gxr / 131.0, gy = gyr / 131.0, gz = gzr / 131.0;

  float motion = fabs(gx) + fabs(gy) + fabs(gz);  // 合計回転速度

  Serial.print("加速度g ");
  pAxis("X", ax); pAxis("Y", ay); pAxis("Z", az);
  Serial.print("| 回転dps ");
  pAxis("X", gx); pAxis("Y", gy); pAxis("Z", gz);
  Serial.print("| 動き ");
  Serial.print(motion, 0);
  Serial.println(motion >= MOTION_THRESH ? " ▲動いてる" : " 静止");

  delay(100);
}
