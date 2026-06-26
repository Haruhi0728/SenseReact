/**
 * Nav_Compass_GPS.ino  （方位の第六感：磁気＋GPS）
 *
 * - 方位：MPU-9250内蔵の磁気センサー(AK8963)を読んで方角を出す
 * - GPS ：SPRESENSE内蔵GNSSで現在地（緯度・経度）を取得
 *
 * 【配線】
 *   IMU(MPU-9250) I2C : SDA→D14, SCL→D15, VCC→3.3V, GND, AD0→GND(0x68)
 *   GPS：SPRESENSE内蔵（追加配線なし。屋外/窓際で測位）
 *
 * 【注意】
 *   - 磁気は要キャリブレーション（hard-iron補正 mox/moy）。回しながら値を見て調整。
 *   - GPSは屋内ではほぼ測位できない。窓際か屋外で「衛星」数が増えるのを待つ。
 */

#include <Wire.h>
#include <GNSS.h>

const uint8_t MPU = 0x68;   // MPU-9250本体
const uint8_t AK  = 0x0C;   // 内蔵磁気センサー AK8963
const int SERIAL_BAUD = 115200;

static SpGnss Gnss;

// 感度補正（ASA）と hard-iron補正（キャリブで調整、初期0）
float asax = 1, asay = 1, asaz = 1;
float mox = 0, moy = 0, moz = 0;

// ===== I2Cヘルパー =====
void wMPU(uint8_t r, uint8_t v) { Wire.beginTransmission(MPU); Wire.write(r); Wire.write(v); Wire.endTransmission(); }
void wAK(uint8_t r, uint8_t v)  { Wire.beginTransmission(AK);  Wire.write(r); Wire.write(v); Wire.endTransmission(); }
uint8_t rAK(uint8_t r) {
  Wire.beginTransmission(AK); Wire.write(r); Wire.endTransmission(false);
  Wire.requestFrom((int)AK, 1);
  if (Wire.available() < 1) return 0;
  return Wire.read();
}

// 磁気センサー初期化（バイパスモードでバスに出す）
void setupMag() {
  wMPU(0x6B, 0x00); delay(10);   // MPU起動
  wMPU(0x6A, 0x00); delay(10);   // USER_CTRL: 内蔵I2Cマスタ無効化（無いとAK8963が見えない）
  wMPU(0x37, 0x02); delay(10);   // INT_PIN_CFG: BYPASS_EN=1 → AK8963がI2Cバスに出る
  wAK(0x0A, 0x00);  delay(10);   // power down
  wAK(0x0A, 0x0F);  delay(10);   // fuse ROMアクセス → ASA読む
  uint8_t ax = rAK(0x10), ay = rAK(0x11), az = rAK(0x12);
  asax = (ax - 128) / 256.0 + 1.0;
  asay = (ay - 128) / 256.0 + 1.0;
  asaz = (az - 128) / 256.0 + 1.0;
  wAK(0x0A, 0x00);  delay(10);
  wAK(0x0A, 0x16);  delay(10);   // 16bit・100Hz連続測定
}

// 磁気読み取り（成功でtrue）
bool readMag(float &mx, float &my, float &mz) {
  if (!(rAK(0x02) & 0x01)) return false;   // ST1: データ準備OK?
  Wire.beginTransmission(AK); Wire.write(0x03); Wire.endTransmission(false);
  Wire.requestFrom((int)AK, 7);            // HXL..HZH + ST2
  if (Wire.available() < 7) { while (Wire.available()) Wire.read(); return false; }
  uint8_t b[7];
  for (int i = 0; i < 7; i++) b[i] = Wire.read();
  if (b[6] & 0x08) return false;           // ST2 HOFL: オーバーフロー

  // AK8963はリトルエンディアン（下位バイト先）
  int16_t rx = (int16_t)((b[1] << 8) | b[0]);
  int16_t ry = (int16_t)((b[3] << 8) | b[2]);
  int16_t rz = (int16_t)((b[5] << 8) | b[4]);
  mx = rx * asax - mox;
  my = ry * asay - moy;
  mz = rz * asaz - moz;
  return true;
}

const char *dirName(float h) {
  const char *names[] = {"北", "北東", "東", "南東", "南", "南西", "西", "北西"};
  int i = (int)((h + 22.5) / 45.0) % 8;
  return names[i];
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  unsigned long t = millis();
  while (!Serial && (millis() - t < 3000)) { ; }

  Wire.begin();
  Wire.setClock(100000);
  setupMag();

  Serial.println("========================================");
  Serial.println("  方位（磁気）＋ GPS");
  Serial.println("========================================");

  // 診断：チップ認識（MPU=0x71期待 / AK8963=0x48期待）
  Wire.beginTransmission(MPU); Wire.write(0x75); Wire.endTransmission(false);
  Wire.requestFrom((int)MPU, 1);
  uint8_t mpuWho = Wire.available() ? Wire.read() : 0;
  Serial.print("MPU WHO_AM_I = 0x");    Serial.println(mpuWho, HEX);
  Serial.print("AK8963 WHO_AM_I = 0x"); Serial.println(rAK(0x00), HEX);

  if (Gnss.begin() != 0) {
    Serial.println("[GPS] 初期化失敗");
  } else {
    Gnss.start(COLD_START);
    Serial.println("[GPS] 測位開始（屋外/窓際で数十秒〜数分）");
  }
  Serial.println("----------------------------------------");
}

float lat = 0, lon = 0;
int sats = 0;
bool fix = false;

void loop() {
  // 磁気 → 方位
  float mx, my, mz;
  float heading = -1;
  if (readMag(mx, my, mz)) {
    heading = atan2(my, mx) * 180.0 / PI;
    if (heading < 0) heading += 360;
  }

  // GPS（新しいデータが来ていれば更新）
  if (Gnss.waitUpdate(0)) {
    SpNavData nav;
    Gnss.getNavData(&nav);
    sats = nav.numSatellites;
    if (nav.posDataExist) {
      fix = true;
      lat = nav.latitude;
      lon = nav.longitude;
    }
  }

  // 表示
  Serial.print("方位 ");
  if (heading < 0) Serial.print("--");
  else { Serial.print((int)heading); Serial.print("° "); Serial.print(dirName(heading)); }

  Serial.print(" | GPS ");
  if (fix) {
    Serial.print("緯"); Serial.print(lat, 6);
    Serial.print(" 経"); Serial.print(lon, 6);
    Serial.print(" 衛星"); Serial.print(sats);
  } else {
    Serial.print("測位待ち 衛星"); Serial.print(sats);
  }
  Serial.println();

  delay(200);
}
