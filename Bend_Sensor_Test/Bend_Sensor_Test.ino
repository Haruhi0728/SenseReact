/**
 * Bend_Sensor_Test.ino
 *
 * 曲げセンサー（MB090-N-221-A02）の動作確認スケッチ
 *
 * 【回路】
 *   IOREF(1.8V) → 曲げセンサー → A3 → 10kΩ抵抗 → GND
 *
 * 【確認方法】
 *   シリアルモニタ（115200bps）を開き、
 *   センサーを曲げると数値が変わることを確認する。
 */

// ピン番号（A3に接続）
const int BEND_PIN = A3;

// シリアル通信速度
const int SERIAL_BAUD = 115200;

// 読み取り間隔（ミリ秒）
const int READ_INTERVAL = 100;

// キャリブレーション用（起動時に自動取得）
int flatValue  = 0;  // まっすぐのときの値
int bentValue  = 0;  // 曲げたときの最大値（手動更新）

void setup() {
  Serial.begin(SERIAL_BAUD);

  unsigned long startWait = millis();
  while (!Serial && (millis() - startWait < 3000)) { ; }

  Serial.println("========================================");
  Serial.println("  曲げセンサー 動作確認スケッチ");
  Serial.println("========================================");
  Serial.println("  ピン: A3");
  Serial.println("  給電: IOREF(1.8V) / GND");
  Serial.println("----------------------------------------");

  // 起動時の値を「まっすぐ」の基準値として記録
  delay(500);
  flatValue = analogRead(BEND_PIN);
  Serial.print("基準値（まっすぐ）: ");
  Serial.println(flatValue);
  Serial.println("→ センサーを曲げて変化を確認してください");
  Serial.println("----------------------------------------");
  Serial.println("ADC値\t曲げ具合（目安）");
}

void loop() {
  // アナログ値を読み取る（0〜1023）
  int raw = analogRead(BEND_PIN);

  // 曲げ具合を0〜100%に換算（まっすぐ=0%、最大曲げ=100%）
  // flatValueより小さくなる方向に変化するため反転して計算
  int diff = flatValue - raw;              // 差分（曲げると増える）
  int percent = map(diff, 0, 478, 0, 100); // ADC410(diff=430)が90%になる設定
  percent = constrain(percent, 0, 100);    // 0〜100にクランプ

  // バー表示（視覚的に確認しやすい）
  String bar = "";
  int barLen = percent / 5;  // 最大20マス
  for (int i = 0; i < 20; i++) {
    bar += (i < barLen) ? "#" : "-";
  }

  // シリアルに出力
  Serial.print(raw);
  Serial.print("\t[");
  Serial.print(bar);
  Serial.print("] ");
  Serial.print(percent);
  Serial.println("%");

  delay(READ_INTERVAL);
}
