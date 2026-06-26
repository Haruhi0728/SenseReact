/**
 * Audio_Test.ino
 *
 * SPRESENSE ハイレゾ音声のビープ・ボリュームテスト。
 * 音量を段階的に変えて2kHzのビープを鳴らす。ちょうどいい音量を選ぶ用。
 *
 * 【接続】
 *   イヤホン or スピーカーを拡張ボードの3.5mmヘッドホンジャックに挿す。
 *
 * 【次のステップ】
 *   超音波の距離が近すぎる時にビープを鳴らす（距離→音）。
 */

#include <Audio.h>

AudioClass *theAudio = AudioClass::getInstance();

void setup() {
  Serial.begin(115200);
  delay(300);

  theAudio->begin();
  theAudio->setRenderingClockMode(AS_CLKMODE_NORMAL);
  // ヘッドホンジャック出力（LINEOUT）。スピーカー端子直結なら AS_SP_DRV_MODE_4DRIVER
  theAudio->setPlayerMode(AS_SETPLAYER_OUTPUTDEVICE_SPHP, AS_SP_DRV_MODE_LINEOUT);

  Serial.println("========================================");
  Serial.println("  音(ビープ) ボリュームテスト");
  Serial.println("========================================");
  Serial.println("イヤホン/スピーカーをジャックに挿してください");
}

void loop() {
  // 音量を -60 → 0 と上げながらビープ（数字が大きいほど大きい音）
  short vols[] = {-60, -40, -20, 0};
  for (int i = 0; i < 4; i++) {
    Serial.print("ビープ vol = ");
    Serial.println(vols[i]);
    theAudio->setBeep(1, vols[i], 2000);  // ON, 音量, 2000Hz
    delay(700);
    theAudio->setBeep(0, 0, 0);           // OFF
    delay(400);
  }
  Serial.println("----------------------------------------");
  delay(1000);
}
