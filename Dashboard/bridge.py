#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SenseReact ダッシュボード中継サーバー

SPRESENSEのシリアル出力(CSV "$距離,振動,腕,画面変化,検知")を読み取り、
ブラウザにJSONで渡す。HTML(index.html)も配信する。

使い方:
  1) pip install pyserial
  2) Arduinoのシリアルモニタは閉じる（COMポートを取り合うため）
  3) 下の SERIAL_PORT を自分のCOM番号に合わせる
  4) python bridge.py
  5) ブラウザで http://localhost:8000 を開く
     （同じWi-Fi内ならスマホから http://<PCのIP>:8000 でも見れる）
"""

import os
import json
import time
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import serial  # pip install pyserial

# ---- 設定 ----
SERIAL_PORT = "COM4"   # ← 自分の環境に合わせる（board listで確認）
BAUD = 115200
HTTP_PORT = 8000

latest = {"dist": None, "vib": 0, "arm": 0, "chg": 0, "detect": 0}
HERE = os.path.dirname(os.path.abspath(__file__))


def serial_reader():
    while True:
        try:
            ser = serial.Serial(SERIAL_PORT, BAUD, timeout=1)
            print(f"[OK] シリアル接続: {SERIAL_PORT}")
            while True:
                line = ser.readline().decode("utf-8", "ignore").strip()
                if line.startswith("$"):
                    p = line[1:].split(",")
                    if len(p) >= 5:
                        try:
                            d = int(p[0])
                            latest["dist"] = None if d < 0 else d
                            latest["vib"] = int(p[1])
                            latest["arm"] = int(p[2])
                            latest["chg"] = int(p[3])
                            latest["detect"] = int(p[4])
                        except ValueError:
                            pass
        except Exception as e:
            print(f"[再接続] シリアルエラー: {e}（2秒後に再試行）")
            time.sleep(2)


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass  # アクセスログ抑制

    def do_GET(self):
        if self.path.startswith("/data"):
            body = json.dumps(latest).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Cache-Control", "no-store, no-cache, must-revalidate")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        else:
            try:
                with open(os.path.join(HERE, "index.html"), "rb") as f:
                    body = f.read()
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            except FileNotFoundError:
                self.send_error(404, "index.html not found")


if __name__ == "__main__":
    threading.Thread(target=serial_reader, daemon=True).start()
    print(f"[起動] ダッシュボード → http://localhost:{HTTP_PORT}")
    ThreadingHTTPServer(("0.0.0.0", HTTP_PORT), Handler).serve_forever()
