/**
 * IMU_Mag_Test.ino
 *
 * MPU-9250 の加速度・ジャイロ・地磁気(AK8963) を Serial に出力するテスト。
 * 振動モーター(D05)は起動時に強制OFF。
 *
 * 【配線】
 *   SDA→D14, SCL→D15, VCC→3.3V, GND, AD0→GND(0x68)
 *
 * 【シリアルモニタで確認すること】
 *   1) WHO_AM_I: MPU=0x71, MAG=0x48 が正常値
 *   2) 加速度(g): 机に置いた時 Z≒1.0, X/Y≒0
 *   3) ジャイロ(dps): 静止時 ≒0
 *   4) 地磁気(uT): 日本では X/Y≒±30, Z≒-50付近（目安）
 */

#include <Wire.h>

const uint8_t MPU_ADDR = 0x68;
const uint8_t AK_ADDR  = 0x0C;
const int     VIB_PIN  = 5;   // 振動モーター強制OFF用

// MPU-9250 レジスタ
const uint8_t REG_WHO_AM_I   = 0x75;
const uint8_t REG_PWR_MGMT1  = 0x6B;
const uint8_t REG_USER_CTRL  = 0x6A;
const uint8_t REG_INT_PIN    = 0x37;
const uint8_t REG_ACCEL_XOUT = 0x3B;
const uint8_t REG_GYRO_XOUT  = 0x43;

// AK8963 レジスタ
const uint8_t AK_WHO_AM_I = 0x00;
const uint8_t AK_CNTL1   = 0x0A;
const uint8_t AK_HXL      = 0x03;

bool mag_present = false;

// ---- I2C ヘルパー ----
uint8_t readReg(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((int)addr, 1);
  return Wire.available() ? Wire.read() : 0xFF;
}

void writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

bool readBytes(uint8_t addr, uint8_t reg, uint8_t *buf, int len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((int)addr, len);
  if (Wire.available() < len) { while (Wire.available()) Wire.read(); return false; }
  for (int i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

void setup() {
  Serial.begin(115200);
  unsigned long t = millis();
  while (!Serial && millis() - t < 3000) {}

  // 振動モーター強制OFF
  pinMode(VIB_PIN, OUTPUT);
  analogWrite(VIB_PIN, 0);
  digitalWrite(VIB_PIN, LOW);

  Wire.begin();
  Wire.setClock(100000);

  Serial.println("========================================");
  Serial.println("  IMU + 地磁気テスト");
  Serial.println("========================================");

  // --- MPU-9250 WHO_AM_I ---
  uint8_t mpu_id = readReg(MPU_ADDR, REG_WHO_AM_I);
  Serial.print("[MPU] WHO_AM_I = 0x");
  Serial.print(mpu_id, HEX);
  if (mpu_id == 0x71)      Serial.println("  ← OK (本物MPU-9250)");
  else if (mpu_id == 0x73) Serial.println("  ← OK (MPU-9255互換)");
  else if (mpu_id == 0x68) Serial.println("  ← MPU-6050(磁気なし)");
  else                      Serial.println("  ← 不明 / 未接続");

  // MPU起動
  writeReg(MPU_ADDR, REG_PWR_MGMT1, 0x00);
  delay(100);

  // --- AK8963 バイパス設定 ---
  writeReg(MPU_ADDR, REG_USER_CTRL, 0x00);   // I2Cマスター無効
  delay(10);
  writeReg(MPU_ADDR, REG_INT_PIN, 0x02);     // BYPASS_EN = 1
  delay(10);

  // AK8963 WHO_AM_I（存在チェック）
  uint8_t ak_id = readReg(AK_ADDR, AK_WHO_AM_I);
  Serial.print("[MAG] WHO_AM_I = 0x");
  Serial.print(ak_id, HEX);
  if (ak_id == 0x48) {
    Serial.println("  ← OK (AK8963)");
    mag_present = true;
    writeReg(AK_ADDR, AK_CNTL1, 0x16);  // 連続測定モード2 (100Hz, 16bit)
    delay(10);
  } else {
    Serial.println("  ← 非搭載（クローン品）。地磁気はスキップ");
  }

  Serial.println("----------------------------------------");
  Serial.println("Accel(g)       Gyro(dps)      Mag(uT)");
  Serial.println("Ax   Ay   Az   Gx   Gy   Gz   Mx   My   Mz");
  Serial.println("----------------------------------------");
}

void loop() {
  // 加速度・ジャイロ（14バイト連続）
  uint8_t buf[14];
  bool ok = readBytes(MPU_ADDR, REG_ACCEL_XOUT, buf, 14);

  float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
  if (ok) {
    int16_t raw_ax = (int16_t)((buf[0]  << 8) | buf[1]);
    int16_t raw_ay = (int16_t)((buf[2]  << 8) | buf[3]);
    int16_t raw_az = (int16_t)((buf[4]  << 8) | buf[5]);
    // buf[6-7]: temp (スキップ)
    int16_t raw_gx = (int16_t)((buf[8]  << 8) | buf[9]);
    int16_t raw_gy = (int16_t)((buf[10] << 8) | buf[11]);
    int16_t raw_gz = (int16_t)((buf[12] << 8) | buf[13]);

    ax = raw_ax / 16384.0f;  // ±2g レンジ
    ay = raw_ay / 16384.0f;
    az = raw_az / 16384.0f;
    gx = raw_gx / 131.0f;   // ±250dps レンジ
    gy = raw_gy / 131.0f;
    gz = raw_gz / 131.0f;
  }

  // 地磁気 (AK8963)
  float mx = 0, my = 0, mz = 0;
  bool mag_ok = false;
  uint8_t mag[7];
  if (mag_present && readBytes(AK_ADDR, AK_HXL, mag, 7)) {
    // mag[6]=ST2。OVF(bit3)チェック
    if (!(mag[6] & 0x08)) {
      int16_t raw_mx = (int16_t)((mag[1] << 8) | mag[0]);
      int16_t raw_my = (int16_t)((mag[3] << 8) | mag[2]);
      int16_t raw_mz = (int16_t)((mag[5] << 8) | mag[4]);
      // 16bitモード: 0.15 uT/LSB
      mx = raw_mx * 0.15f;
      my = raw_my * 0.15f;
      mz = raw_mz * 0.15f;
      mag_ok = true;
    }
  }

  // 出力
  Serial.print(ax, 2); Serial.print("  ");
  Serial.print(ay, 2); Serial.print("  ");
  Serial.print(az, 2); Serial.print("  |  ");
  Serial.print(gx, 1); Serial.print("  ");
  Serial.print(gy, 1); Serial.print("  ");
  Serial.print(gz, 1); Serial.print("  |  ");
  if (mag_ok) {
    Serial.print(mx, 1); Serial.print("  ");
    Serial.print(my, 1); Serial.print("  ");
    Serial.println(mz, 1);
  } else {
    Serial.println("---  ---  ---");
  }

  delay(200);
}
