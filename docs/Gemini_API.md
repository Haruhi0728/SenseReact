# システム構成

SenseReactでは、SPRESENSEのHDRカメラによる動体検知をトリガーとして、必要なタイミングのみGemini APIへ画像を送信する構成を採用している。

常時クラウドへ映像を送信するのではなく、SPRESENSE側で前処理（動体検知）を行うことで通信量およびAPI利用回数を削減する。

---

## 全体構成

```text
┌─────────────────────┐
│ HDR Camera          │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│ SPRESENSE           │
│                     │
│ ・映像ストリーミング取得
│ ・フレーム差分計算
│ ・動体検知
│ ・JPEG撮影
└──────────┬──────────┘
           │ UART(921600bps)
           ▼
┌─────────────────────┐
│ ESP32               │
│                     │
│ ・JPEG受信
│ ・Base64変換
│ ・HTTP POST
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│ Xserver VPS         │
│ Node.js Server      │
│ server.js           │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│ Gemini API          │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────┐
│ 認識結果             │
│ （人物・机など）      │
└─────────────────────┘
```

---

## SPRESENSEの役割

HDRカメラから取得した映像に対し、フレーム差分による動体検知を行う。

動きを検知した場合のみJPEG画像を撮影する。

取得したJPEG画像をUART経由でESP32へ送信する。

### 入力

```text
HDR Camera映像
```

### 出力

```text
JPEG画像
```

### 動作

```text
映像取得
↓
フレーム差分計算
↓
動体検知
↓
VISION_TRIGGER
↓
JPEG撮影
↓
UART送信
```

---

## ESP32の役割

SPRESENSEから送られたJPEG画像を受信し、HTTP通信可能な形式へ変換する。

### 入力

```text
JPEG画像(UART)
```

### 処理

```text
JPEG受信
↓
Base64変換
↓
JSON生成
↓
HTTP POST
```

### 出力

```text
HTTPリクエスト
```

---

## VPS(Node.js)の役割

ESP32から受信した画像データをGemini APIへ中継する。

### 入力

```json
{
  "image": "base64..."
}
```

### 処理

```text
画像受信
↓
Gemini API送信
↓
認識結果取得
↓
JSON返却
```

### 出力例

```json
{
  "result": "人物"
}
```

---

## Gemini APIの役割

送信された画像を解析し、内容を認識する。

### 入力

```text
JPEG画像
```

### 出力

```text
人物
机
椅子
通路
壁
その他
```

などの認識結果。

---

## 配線構成

### SPRESENSE → ESP32

```text
SPRESENSE TX
    ↓
ESP32 GPIO15

SPRESENSE GND
    ↓
ESP32 GND
```

---

## ネットワーク構成

```text
ESP32
↓
Wi-Fi
↓
220.158.22.214:3000
↓
Node.js server.js
↓
Gemini API
```

---

## 実証済みデータフロー

```text
HDRカメラ
↓
動体検知
↓
VISION_TRIGGER
↓
JPEG撮影
↓
UART送信
↓
ESP32受信
↓
Base64変換
↓
VPS送信
↓
Gemini認識
↓
認識結果取得
```

---

## 実機確認結果

以下を実機にて確認済み。

```text
✅ HDRカメラ動体検知

✅ JPEG撮影

✅ SPRESENSE→ESP32 UART通信

✅ ESP32画像受信

✅ Base64変換

✅ ESP32→VPS HTTP通信

✅ VPS→Gemini API通信

✅ Gemini認識結果取得
```

本システムにより、動体検知をトリガーとして必要時のみGeminiによる画像認識を実行するリアルタイム認識パイプラインの構築に成功した。


# 操作手順

## 1. VPSサーバー起動

### VPSへSSH接続

PowerShellを開き、以下を実行する。

```bash
ssh root@220.158.22.214
```

パスワード入力を求められるので入力する。

```text
teamhightech06
```

---

### gemini-serverディレクトリへ移動

```bash
cd gemini-server
```

---

### Gemini APIキー設定

以下を実行する。

```bash
export GEMINI_API_KEY="AQ～"
```

※実際には発行済みのGemini APIキーを入力する。APIキーは片山さんに聞いてください。

---

### サーバー起動

```bash
node server.js
```

---

### 起動確認

以下が表示されたら起動成功。

```text
Server running on port 3000
```

---

### 想定ログ

```text
root@x220-158-22-214:~# cd gemini-server

root@x220-158-22-214:~/gemini-server# export GEMINI_API_KEY="AQ～"

root@x220-158-22-214:~/gemini-server# node server.js

Server running on port 3000
```

Server running on port 3000 が表示されている間は、Gemini APIサーバーが起動している状態となる。

このターミナルは閉じず、そのままにしておく。

## 2. SPRESENSEとESP32のシリアルモニタを開く

### SPRESENSE側のシリアルモニタを開く

VS Codeのシリアルモニタ画面を開く。

SPRESENSEのCOMポートを選択し、

```text
監視を開始
```

をクリックする。

---

### ESP32側のシリアルモニタを開く

別ウィンドウまたは別タブでESP32のCOMポートを選択し、

```text
監視を開始
```

をクリックする。

---

### ESP32起動確認

ESP32側に以下のようなログが表示されていることを確認する。

```text
ESP32 Vision Gateway booting...

[READY] Waiting for IMG_BEGIN from SPRESENSE...
```

---

## 3. 動体検知テスト

SPRESENSEに接続されているHDRカメラの前で、

- 手を振る
- 物体を動かす
- カメラの前を横切る

などの動作を行う。

---

## 4. SPRESENSE側ログ確認

動きを検知すると以下のようなログが表示される。

```text
VISION_TRIGGER

changedPct=49

avgDiff=12

[AI] JPEG撮影開始

[AI] 撮影成功

画像サイズ: 4768

幅: 320
高さ: 240

[AI] ESP32へ画像送信開始 size=4768

[AI] ESP32へ画像送信完了
```

---

### 確認ポイント

以下が表示されれば成功。

```text
VISION_TRIGGER

JPEG撮影開始

撮影成功

ESP32へ画像送信完了
```

---

## 5. ESP32側ログ確認

SPRESENSEから画像を受信すると以下のようなログが表示される。

```text
[UART] Header: IMG_BEGIN,4768,image/jpeg

[UART] imgSize=4768

[UART] mimeType=image/jpeg

[UART] Receiving image bytes: 4768

[UART] Image received: 4768

[UART] IMG_END received
```

---

### 確認ポイント

以下が表示されればUART通信成功。

```text
[UART] Header: IMG_BEGIN

[UART] Image received

[UART] IMG_END received
```

---

## 6. Base64変換確認

ESP32が受信したJPEGをBase64化する。

ログ例

```text
[BASE64] Encoding...

[BASE64] length=11224
```

---

### 確認ポイント

以下が表示されれば成功。

```text
[BASE64] Encoding...
```

---

## 7. VPS送信確認

ESP32からVPSへHTTP送信を行う。

ログ例

```text
[HTTP] POST to VPS...

[HTTP] URL: http://220.158.22.214:3000/analyze
```

---

## 8. Gemini認識結果確認

正常に認識された場合

```text
[HTTP] code=200
```

が表示される。

例

```text
[HTTP] code=200

[HTTP] response:

{"result":"人物"}
```

または

```text
[HTTP] code=200

[HTTP] response:

{"result":"手"}
```

など。

---

### 成功判定

以下が表示されれば一連の処理は成功。

```text
[HTTP] code=200

[HTTP] response:
{"result":"○○"}
```

---

## 9. 動作確認完了

以下の流れが確認できれば実機動作確認完了。

```text
HDRカメラ
↓
動体検知
↓
VISION_TRIGGER
↓
JPEG撮影
↓
SPRESENSE→ESP32 UART送信
↓
ESP32受信
↓
Base64変換
↓
VPS送信
↓
Gemini認識
↓
認識結果取得
```

---

## 10. 発生しうるエラー

### VPS未起動

ESP32ログ

```text
[HTTP] code=-1
```

対処

```text
VPSへSSH接続

cd gemini-server

export GEMINI_API_KEY="..."

node server.js
```

を実行する。

---

### Gemini無料枠制限

ESP32ログ

```text
[HTTP] code=429
```

対処

```text
Gemini無料枠の利用上限到達。

時間を空けるか、
API利用枠の拡張を検討する。
```

---

## 完了条件

以下が取得できれば成功。

```text
[HTTP] code=200

[HTTP] response:
{"result":"認識結果"}
```