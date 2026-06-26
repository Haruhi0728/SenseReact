/**
 * Sense_Core.ino  （SPRESENSE 装着側「察知コア」統合）
 *
 * SPRESENSEに繋がる4つを1つに統合：
 *   - 超音波(US-015)     : 前方の距離を測る
 *   - 振動モーター        : 近いほど強く振動（フィードバック）
 *   - HDRカメラ          : フレーム差分で前方の変化を検知
 *   - 9軸IMU(MPU-9250)   : 腕の動きを検知し、「自分の動き」を差分から除外
 *
 * 融合ロジック：
 *   腕が静止 かつ 画面変化あり → ●対象を検知（本物の動き）
 *   腕が動いてる              → 画面変化は自分の揺れ＝無視
 *
 * 【配線】
 *   超音波 : Trig→D08, Echo→D09
 *   振動   : D05(PWM)→1kΩ→2SC1815(B) / C→モーター(-) / E→GND / モーター(+)→3.3V / 1N4148並列
 *   IMU    : SDA→D14, SCL→D15, VCC→3.3V, GND, AD0→GND(0x68)
 *   カメラ : カメラコネクタ
 */

#include <Camera.h>
#include <Wire.h>

// ---- ピン ----
const int TRIG_PIN = 8;
const int ECHO_PIN = 9;
const int VIB_PIN  = 5;
const uint8_t MPU  = 0x68;
const int SERIAL_BAUD = 115200;

// ---- 超音波→振動 ----
const int NEAR_CM = 5;
const int FAR_CM  = 40;
const int MIN_FELT = 35;
const int MAX_FELT = 100;
const unsigned long ECHO_TIMEOUT_US = 4000;

// ---- カメラ差分 ----
const int IMG_W = 320, IMG_H = 240, STEP = 8;
const int GRID_W = IMG_W / STEP, GRID_H = IMG_H / STEP;
const int SAMPLES = GRID_W * GRID_H;   // 1200
const int CELL_THRESHOLD = 12;
const int MOTION_ON_PCT = 4;
uint8_t prevY[SAMPLES];
bool prevReady = false;
volatile int g_changedPct = 0;

// ---- IMU ----
const float ARM_MOVE_THRESH = 30.0;    // deg/s合計。これ以上で「腕が動いてる」

bool cameraReady = false;

// ===== ヘルパー =====
void vibSet(int p) {
  p = constrain(p, 0, 100);
  analogWrite(VIB_PIN, map(p, 0, 100, 0, 255));
}

long readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long d = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);
  if (d == 0) return -1;
  return d / 58;
}

void writeReg(uint8_t r, uint8_t v) {
  Wire.beginTransmission(MPU); Wire.write(r); Wire.write(v); Wire.endTransmission();
}

// ジャイロ合計（腕の動き量 deg/s）。失敗時 -1
float readArmMotion() {
  Wire.beginTransmission(MPU); Wire.write(0x43); Wire.endTransmission(false);
  Wire.requestFrom((int)MPU, 6);
  if (Wire.available() < 6) { while (Wire.available()) Wire.read(); return -1; }
  uint8_t b[6];
  for (int i = 0; i < 6; i++) b[i] = Wire.read();
  int16_t gx = (int16_t)((b[0] << 8) | b[1]);
  int16_t gy = (int16_t)((b[2] << 8) | b[3]);
  int16_t gz = (int16_t)((b[4] << 8) | b[5]);
  return (fabs(gx) + fabs(gy) + fabs(gz)) / 131.0;
}

// 映像フレームごと：差分（変化領域%）を計算
void CamCB(CamImage img) {
  if (!img.isAvailable()) return;
  uint8_t *buf = img.getImgBuff();
  int changed = 0, idx = 0;
  for (int gy = 0; gy < GRID_H; gy++) {
    int y = gy * STEP;
    for (int gx = 0; gx < GRID_W; gx++) {
      int x = gx * STEP;
      uint8_t Y = buf[(y * IMG_W + x) * 2];
      if (prevReady) {
        int dd = abs((int)Y - (int)prevY[idx]);
        if (dd > CELL_THRESHOLD) changed++;
      }
      prevY[idx] = Y;
      idx++;
    }
  }
  prevReady = true;
  g_changedPct = changed * 100 / SAMPLES;
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  unsigned long t = millis();
  while (!Serial && (millis() - t < 3000)) { ; }

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(VIB_PIN, OUTPUT);
  vibSet(0);

  Wire.begin();
  Wire.setClock(100000);
  writeReg(0x6B, 0x00);  // IMU起動
  delay(100);

  Serial.println("========================================");
  Serial.println("  SenseReact 察知コア（統合）");
  Serial.println("========================================");

  Serial.print("[カメラ] 初期化... ");
  CamErr e = theCamera.begin(1, CAM_VIDEO_FPS_30,
                             CAM_IMGSIZE_QVGA_H, CAM_IMGSIZE_QVGA_V,
                             CAM_IMAGE_PIX_FMT_YUV422);
  if (e == CAM_ERR_SUCCESS && theCamera.startStreaming(true, CamCB) == CAM_ERR_SUCCESS) {
    cameraReady = true;
    Serial.println("OK");
  } else {
    Serial.print("失敗 ("); Serial.print((int)e); Serial.println(")");
  }
  Serial.println("----------------------------------------");
}

void loop() {
  // 腕の動き（IMU）
  float arm = readArmMotion();
  bool armMoving = (arm >= 0) && (arm > ARM_MOVE_THRESH);

  // 超音波 → 振動
  long d = readDistanceCm();
  int vib;
  if (d < 0 || d > FAR_CM) vib = 0;
  else vib = map(constrain((int)d, NEAR_CM, FAR_CM), NEAR_CM, FAR_CM, MAX_FELT, MIN_FELT);
  vibSet(vib);

  // カメラ差分（コールバックが更新）
  int chg = g_changedPct;

  // 融合判定
  bool realDetect = !armMoving && (chg >= MOTION_ON_PCT);

  // 表示
  Serial.print("距離 ");
  if (d < 0) Serial.print("--"); else Serial.print(d);
  Serial.print("cm 振動 "); Serial.print(vib);
  Serial.print("% | 腕 "); Serial.print(armMoving ? "動" : "静");
  Serial.print(" | 画面変化 "); Serial.print(chg);
  Serial.print("% -> ");
  if (armMoving)        Serial.println("（自分の動き・無視）");
  else if (realDetect)  Serial.println("●対象を検知");
  else                  Serial.println("静観");

  // 機械可読CSV（ダッシュボード用）: $距離,振動%,腕dps,画面変化%,検知0/1
  Serial.print("$");
  Serial.print(d < 0 ? -1 : (int)d);      Serial.print(",");
  Serial.print(vib);                      Serial.print(",");
  Serial.print(arm < 0 ? 0 : (int)arm);   Serial.print(",");
  Serial.print(chg);                      Serial.print(",");
  Serial.println(realDetect ? 1 : 0);

  delay(80);
}
