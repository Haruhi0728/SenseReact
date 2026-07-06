/**
 * Camera_Motion_Gemini.ino
 *
 * HDRカメラのストリーミング映像から「前方の変化／動き」を検知し、
 * 動き検知ONのタイミングでJPEG静止画を撮影してESP32へUART送信するスケッチ。
 *
 * SenseReactの動作フローでカメラが担う「察知」＋ Gemini API連携の入口。
 *
 * 【仕組み】
 *   1. カメラを QVGA / YUV422 で映像ストリーミング
 *   2. コールバックで輝度(Y)を8pxごとにサンプリング
 *   3. 直前フレームとの差分から変化領域%を算出
 *   4. 変化領域%が閾値を超えたら「動き検知」
 *   5. 動き検知がOFF→ONになった瞬間だけ VISION_TRIGGER を出力
 *   6. loop側でJPEG静止画を撮影
 *   7. Serial2でESP32へ画像を送信
 *
 * 【UART画像送信フォーマット】
 *   IMG_BEGIN,<画像サイズ>,image/jpeg\n
 *   <JPEGバイナリ本体>
 *   \nIMG_END\n
 *
 * 【配線】
 *   SPRESENSE D01(TX) → ESP32 GPIO16(RX2)
 *   SPRESENSE GND     → ESP32 GND
 */

#include <Camera.h>

// ========================================================
// シリアル設定
// ========================================================
const int SERIAL_BAUD = 115200;  // PCデバッグ用
const int ESP_BAUD    = 921600;  // ESP32画像送信用UART

// ========================================================
// カメラ差分検知設定（QVGA = 320x240）
// ========================================================
const int IMG_W   = 320;
const int IMG_H   = 240;
const int STEP    = 8;
const int GRID_W  = IMG_W / STEP;     // 40
const int GRID_H  = IMG_H / STEP;     // 30
const int SAMPLES = GRID_W * GRID_H;  // 1200

// ========================================================
// 動体検知パラメータ
// ========================================================
const int CELL_THRESHOLD = 12;  // 輝度差がこの値を超えたセルを「変化」とみなす
const int MOTION_ON_PCT  = 4;   // 変化領域がこの%以上で動き検知ON
const int MOTION_OFF_PCT = 2;   // この%未満まで下がったらOFF

// ========================================================
// Gemini / VPS連携用トリガー設定
// ========================================================
const unsigned long AI_TRIGGER_COOLDOWN_MS = 5000; // 5秒に1回まで
unsigned long lastAiTriggerMs = 0;

bool prevMotionState = false;
bool motionState = false;

// コールバック内では撮影せず、loop側に依頼する
volatile bool captureRequested = false;
bool captureBusy = false;

// ========================================================
// 表示間隔
// ========================================================
const unsigned long PRINT_INTERVAL_MS = 100;
unsigned long lastPrint = 0;

// ========================================================
// JPEG撮影設定
// ========================================================
const int JPEG_QUALITY = 35; // 低め。UART転送を軽くするため

// ========================================================
// フレーム差分用バッファ
// ========================================================
uint8_t prevY[SAMPLES];
bool prevReady = false;

// ========================================================
// Gemini/API連携用トリガー出力
// ========================================================
void emitVisionTrigger(int changedPct, int avgDiff) {
  Serial.println("VISION_TRIGGER");
  Serial.print("changedPct=");
  Serial.println(changedPct);
  Serial.print("avgDiff=");
  Serial.println(avgDiff);
}

// ========================================================
// ESP32へJPEG画像をUART送信
// ========================================================
void sendImageToESP32(CamImage &photo) {
  int imgSize = photo.getImgSize();

  Serial.print("[AI] ESP32へ画像送信開始 size=");
  Serial.println(imgSize);

  // ヘッダ送信
  Serial2.print("IMG_BEGIN,");
  Serial2.print(imgSize);
  Serial2.println(",image/jpeg");

  delay(20);

  // JPEGバイナリ本体送信
  Serial2.write(photo.getImgBuff(), imgSize);
  Serial2.flush();

  delay(20);

  // 終端送信
  Serial2.println();
  Serial2.println("IMG_END");

  Serial.println("[AI] ESP32へ画像送信完了");
}

// ========================================================
// JPEG撮影してESP32へ送る
// ========================================================
void captureAndSendToESP32() {
  if (captureBusy) {
    return;
  }

  captureBusy = true;

  Serial.println("[AI] JPEG撮影開始");

  CamImage photo = theCamera.takePicture();

  if (!photo.isAvailable()) {
    Serial.println("[AI] 撮影失敗: 画像データが取得できませんでした");
    captureBusy = false;
    return;
  }

  Serial.println("[AI] 撮影成功");
  Serial.print("  画像サイズ: ");
  Serial.println(photo.getImgSize());
  Serial.print("  幅: ");
  Serial.print(photo.getWidth());
  Serial.print(" / 高さ: ");
  Serial.println(photo.getHeight());

  sendImageToESP32(photo);

  captureBusy = false;
}

// ========================================================
// カメラフレームごとのコールバック
// ========================================================
void CamCB(CamImage img) {
  if (!img.isAvailable()) {
    return;
  }

  // 撮影中は差分検知を軽く止める
  if (captureBusy) {
    return;
  }

  uint8_t* buf = img.getImgBuff();  // YUV422: [Y0 U Y1 V ...]、Yは偶数バイト

  long sad = 0;
  int changedCells = 0;
  int idx = 0;

  for (int gy = 0; gy < GRID_H; gy++) {
    int y = gy * STEP;

    for (int gx = 0; gx < GRID_W; gx++) {
      int x = gx * STEP;

      uint8_t Y = buf[(y * IMG_W + x) * 2];

      if (prevReady) {
        int d = abs((int)Y - (int)prevY[idx]);
        sad += d;

        if (d > CELL_THRESHOLD) {
          changedCells++;
        }
      }

      prevY[idx] = Y;
      idx++;
    }
  }

  prevReady = true;

  int avgDiff    = sad / SAMPLES;
  int changedPct = changedCells * 100 / SAMPLES;

  // ヒステリシス判定
  if (!motionState && changedPct >= MOTION_ON_PCT) {
    motionState = true;
  } else if (motionState && changedPct < MOTION_OFF_PCT) {
    motionState = false;
  }

  unsigned long now = millis();

  bool motionRisingEdge = (!prevMotionState && motionState);
  bool cooldownPassed   = (now - lastAiTriggerMs >= AI_TRIGGER_COOLDOWN_MS);

  // 動き検知がOFF→ONになった瞬間だけAIトリガー
  if (motionRisingEdge && cooldownPassed) {
    emitVisionTrigger(changedPct, avgDiff);

    // loop側でJPEG撮影・ESP32送信する
    captureRequested = true;

    lastAiTriggerMs = now;
  }

  prevMotionState = motionState;

  // 状態表示
  if (now - lastPrint >= PRINT_INTERVAL_MS) {
    lastPrint = now;

    int barLen = constrain(changedPct, 0, 20);
    String bar = "";

    for (int i = 0; i < 20; i++) {
      bar += (i < barLen) ? "#" : "-";
    }

    Serial.print("変化領域: ");
    if (changedPct < 10) Serial.print(" ");
    Serial.print(changedPct);
    Serial.print("% [");
    Serial.print(bar);
    Serial.print("]  平均差:");
    Serial.print(avgDiff);
    Serial.print("  -> ");
    Serial.println(motionState ? "▲ 動き検知" : "静止");
  }
}

// ========================================================
// カメラ初期化
// ========================================================
CamErr beginCamera(CAM_VIDEO_FPS fps) {
  return theCamera.begin(
    1,
    fps,
    CAM_IMGSIZE_QVGA_H,
    CAM_IMGSIZE_QVGA_V,
    CAM_IMAGE_PIX_FMT_YUV422
  );
}

// ========================================================
// setup()
// ========================================================
void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial2.begin(ESP_BAUD);

  unsigned long t = millis();
  while (!Serial && (millis() - t < 3000)) {
    ;
  }

  Serial.println("========================================");
  Serial.println("  HDRカメラ 動体検知 + JPEG UART送信版");
  Serial.println("========================================");

  Serial.print("  サンプル数: ");
  Serial.print(SAMPLES);
  Serial.print(" (");
  Serial.print(GRID_W);
  Serial.print("x");
  Serial.print(GRID_H);
  Serial.println(")");

  Serial.print("  MOTION_ON_PCT: ");
  Serial.print(MOTION_ON_PCT);
  Serial.println("%");

  Serial.print("  MOTION_OFF_PCT: ");
  Serial.print(MOTION_OFF_PCT);
  Serial.println("%");

  Serial.print("  AI_TRIGGER_COOLDOWN_MS: ");
  Serial.print(AI_TRIGGER_COOLDOWN_MS);
  Serial.println(" ms");

  Serial.print("  ESP UART baud: ");
  Serial.println(ESP_BAUD);

  // カメラ初期化
  Serial.print("[カメラ] 初期化中（30fps）... ");

  CamErr err = beginCamera(CAM_VIDEO_FPS_30);

  if (err != CAM_ERR_SUCCESS) {
    Serial.println("失敗！");
    Serial.print("  エラーコード: ");
    Serial.println((int)err);
    return;
  }

  Serial.println("OK");

  // JPEG画質設定
  Serial.print("[カメラ] JPEG画質設定中... ");

  err = theCamera.setJPEGQuality(JPEG_QUALITY);

  if (err != CAM_ERR_SUCCESS) {
    Serial.println("警告: JPEG画質設定に失敗しました");
    Serial.print("  エラーコード: ");
    Serial.println((int)err);
  } else {
    Serial.println("OK");
  }

  // 静止画フォーマット設定
  Serial.print("[カメラ] 静止画フォーマット設定中... ");

  err = theCamera.setStillPictureImageFormat(
    CAM_IMGSIZE_QVGA_H,
    CAM_IMGSIZE_QVGA_V,
    CAM_IMAGE_PIX_FMT_JPG
  );

  if (err != CAM_ERR_SUCCESS) {
    Serial.println("失敗！");
    Serial.print("  エラーコード: ");
    Serial.println((int)err);
    return;
  }

  Serial.println("OK");

  // ストリーミング開始
  Serial.print("[カメラ] ストリーミング開始... ");

  err = theCamera.startStreaming(true, CamCB);

  if (err != CAM_ERR_SUCCESS) {
    Serial.println("失敗！");
    Serial.print("  エラーコード: ");
    Serial.println((int)err);
    return;
  }

  Serial.println("OK");
  Serial.println("----------------------------------------");
  Serial.println("→ カメラの前で手や物を動かすと反応します");
  Serial.println("→ 動き検知ONでJPEG撮影し、Serial2でESP32へ送信します");
  Serial.println("→ SPRESENSE D01(TX) → ESP32 GPIO16(RX2)");
  Serial.println("----------------------------------------");
}

// ========================================================
// loop()
// ========================================================
void loop() {
  if (captureRequested) {
    captureRequested = false;
    captureAndSendToESP32();
  }

  delay(50);
}