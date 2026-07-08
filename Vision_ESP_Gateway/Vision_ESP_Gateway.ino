/**
 * Vision_ESP_Gateway.ino
 *
 * SPRESENSEからUARTで送られてくるJPEG画像を受信し、
 * Base64化してVPSのGemini API中継サーバへHTTP POSTするESP32用スケッチ。
 *
 * 【SPRESENSE → ESP32 UART】
 *   SPRESENSE D01(TX) → ESP32 GPIO16(RX2)
 *   SPRESENSE GND     → ESP32 GND
 *
 * 【受信フォーマット】
 *   IMG_BEGIN,<画像サイズ>,image/jpeg\n
 *   <JPEGバイナリ本体>
 *   \nIMG_END\n
 *
 * 【送信先】
 *   http://220.158.22.214:3000/analyze
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include "mbedtls/base64.h"

// ========================================================
// Wi-Fi設定
// ========================================================
#include "secrets.h"
// ========================================================
// UART設定
// ========================================================
// SPRESENSE D01(TX) → ESP32 GPIO16(RX2)
// ESP32 TX2(GPIO17) は今回は未使用でもOK
const int UART_BAUD = 921600;
const int ESP32_RX2 = 15;
const int ESP32_TX2 = 17;

HardwareSerial SpresenseSerial(2);

// ========================================================
// 画像受信設定
// ========================================================
// QVGA JPEG品質35想定なら、多くの場合30KB前後に収まる想定。
// もし「Invalid image size」が出る場合は増やす。
const int MAX_IMAGE_SIZE = 80000;  // 80KBまで許可

// 受信タイムアウト
const unsigned long IMAGE_RECEIVE_TIMEOUT_MS = 12000;

// ========================================================
// Wi-Fi接続
// ========================================================
void connectWiFi() {
  Serial.println();
  Serial.println("========================================");
  Serial.println("  ESP32 Vision Gateway");
  Serial.println("========================================");

  Serial.print("[WiFi] Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    retry++;

    if (retry > 60) {
      Serial.println();
      Serial.println("[WiFi] 接続失敗。再起動します。");
      ESP.restart();
    }
  }

  Serial.println();
  Serial.println("[WiFi] Connected");
  Serial.print("[WiFi] IP: ");
  Serial.println(WiFi.localIP());
}

// ========================================================
// IMG_BEGINヘッダ解析
// 例: IMG_BEGIN,23456,image/jpeg
// ========================================================
bool parseImageHeader(const String& header, int& imgSize, String& mimeType) {
  if (!header.startsWith("IMG_BEGIN")) {
    return false;
  }

  int firstComma = header.indexOf(',');
  int secondComma = header.indexOf(',', firstComma + 1);

  if (firstComma < 0 || secondComma < 0) {
    return false;
  }

  imgSize = header.substring(firstComma + 1, secondComma).toInt();
  mimeType = header.substring(secondComma + 1);
  mimeType.trim();

  if (imgSize <= 0 || imgSize > MAX_IMAGE_SIZE) {
    return false;
  }

  return true;
}

// ========================================================
// UARTから指定サイズ分の画像バイナリを受信
// ========================================================
bool receiveImageBytes(uint8_t* imgBuf, int imgSize) {
  int received = 0;
  unsigned long startMs = millis();

  Serial.print("[UART] Receiving image bytes: ");
  Serial.println(imgSize);

  while (received < imgSize) {
    if (millis() - startMs > IMAGE_RECEIVE_TIMEOUT_MS) {
      Serial.println("[UART] 画像受信タイムアウト");
      Serial.print("[UART] received=");
      Serial.println(received);
      return false;
    }

    int availableBytes = SpresenseSerial.available();

    if (availableBytes > 0) {
      int remain = imgSize - received;
      int readLen = availableBytes;

      if (readLen > remain) {
        readLen = remain;
      }

      int n = SpresenseSerial.readBytes(imgBuf + received, readLen);
      received += n;
    }
  }

  Serial.print("[UART] Image received: ");
  Serial.println(received);

  return true;
}

// ========================================================
// IMG_ENDを読み捨て確認
// ========================================================
void consumeImageEndMarker() {
  unsigned long startMs = millis();

  while (millis() - startMs < 2000) {
    if (SpresenseSerial.available()) {
      String line = SpresenseSerial.readStringUntil('\n');
      line.trim();

      if (line.length() == 0) {
        continue;
      }

      if (line == "IMG_END") {
        Serial.println("[UART] IMG_END received");
        return;
      } else {
        Serial.print("[UART] extra line: ");
        Serial.println(line);
      }
    }
  }

  Serial.println("[UART] IMG_END not found, continue");
}

// ========================================================
// JPEG画像をBase64化してVPSへPOST
// ========================================================
void sendImageToVPS(uint8_t* imgBuf, int imgSize, const String& mimeType) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] WiFi disconnected. Reconnecting...");
    connectWiFi();
  }

  Serial.println("[BASE64] Encoding...");

  size_t base64Len = 0;

  // 必要なBase64バッファサイズを取得
  mbedtls_base64_encode(
    NULL,
    0,
    &base64Len,
    imgBuf,
    imgSize
  );

  char* base64Buf = (char*)malloc(base64Len + 1);

  if (!base64Buf) {
    Serial.println("[BASE64] malloc failed");
    return;
  }

  int ret = mbedtls_base64_encode(
    (unsigned char*)base64Buf,
    base64Len,
    &base64Len,
    imgBuf,
    imgSize
  );

  if (ret != 0) {
    Serial.print("[BASE64] encode failed ret=");
    Serial.println(ret);
    free(base64Buf);
    return;
  }

  base64Buf[base64Len] = '\0';

  Serial.print("[BASE64] length=");
  Serial.println(base64Len);

  // JSON作成
  String body;
  body.reserve(base64Len + 64);
  body = "{\"image\":\"";
  body += base64Buf;
  body += "\"}";

  free(base64Buf);

  Serial.println("[HTTP] POST to VPS...");
  Serial.print("[HTTP] URL: ");
  Serial.println(API_URL);

  HTTPClient http;
  http.begin(API_URL);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(body);

  Serial.print("[HTTP] code=");
  Serial.println(httpCode);

  String response = http.getString();

  Serial.println("[HTTP] response:");
  Serial.println(response);

  http.end();
}

// ========================================================
// 画像1枚の受信処理
// ========================================================
void handleImagePacket(const String& header) {
  Serial.println("----------------------------------------");
  Serial.print("[UART] Header: ");
  Serial.println(header);

  int imgSize = 0;
  String mimeType = "";

  if (!parseImageHeader(header, imgSize, mimeType)) {
    Serial.println("[UART] Invalid IMG_BEGIN header");
    return;
  }

  Serial.print("[UART] imgSize=");
  Serial.println(imgSize);
  Serial.print("[UART] mimeType=");
  Serial.println(mimeType);

  uint8_t* imgBuf = (uint8_t*)malloc(imgSize);

  if (!imgBuf) {
    Serial.println("[MEM] image malloc failed");
    return;
  }

  bool ok = receiveImageBytes(imgBuf, imgSize);

  if (!ok) {
    free(imgBuf);
    return;
  }

  consumeImageEndMarker();

  sendImageToVPS(imgBuf, imgSize, mimeType);

  free(imgBuf);

  Serial.println("[DONE] Image packet processed");
  Serial.println("----------------------------------------");
}

// ========================================================
// setup
// ========================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  // SPRESENSE UART受信用
  SpresenseSerial.begin(
    UART_BAUD,
    SERIAL_8N1,
    ESP32_RX2,
    ESP32_TX2
  );

  Serial.println();
  Serial.println("ESP32 Vision Gateway booting...");
  Serial.print("[UART] RX2 GPIO");
  Serial.print(ESP32_RX2);
  Serial.print(" baud=");
  Serial.println(UART_BAUD);

  connectWiFi();

  Serial.println("[READY] Waiting for IMG_BEGIN from SPRESENSE...");
}

// ========================================================
// loop
// ========================================================
void loop() {
  if (SpresenseSerial.available()) {
    String line = SpresenseSerial.readStringUntil('\n');
    line.trim();

    if (line.length() == 0) {
      return;
    }

    if (line.startsWith("IMG_BEGIN")) {
      handleImagePacket(line);
    } else {
      Serial.print("[UART] ignored line: ");
      Serial.println(line);
    }
  }
}