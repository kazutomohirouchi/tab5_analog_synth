# tab5_analog — M5Tab5 アナログシンセサイザー

M5Stack **Tab5** 単体で動く、減算合成のアナログ風シンセサイザーです。
音は外部音源を使わず、Tab5 内蔵スピーカーへソフトウェア合成で生成します。**Unit 8Encoder** の物理つまみで音作りをし、タッチ鍵盤や **USB MIDI キーボード**で演奏します。往年のアナログシンセのような音色作りと、5ボイスの和音演奏を目指しました。

*A subtractive analog-style synthesizer running entirely on the M5Stack Tab5: software-generated sound through the built-in speaker, hands-on sound design with the Unit 8Encoder knobs, 5-voice polyphony, 16 factory presets + 16 SD user slots, USB MIDI keyboard support, ABC notation export, and microphone sampling.*

## 機能

### 音源エンジン
- 2オシレータ（デチューン／ミックス可）、波形はノコギリ／矩形（PWM）／三角／サンプル
  - ノコギリと矩形は PolyBLEP で帯域制限し、折り返し雑音を抑えています
- ノイズジェネレータ、共振つき2極ローパスフィルタ（SVF）
- ADSRエンベロープ、フィルタ用エンベロープ（F-ENV）、LFO（カットオフ変調）
- グライド（ポルタメント）、Freeverb 系のリバーブ（REVスライダで調整）
- 発音モード：**POLY（5和音）／MONO／UNISON（5ボイス重ね）**

### 演奏インターフェース
- 61鍵タッチキーボード（MIDI 36–96）：マルチタッチで和音、スライドでグリッサンド
- 画面左端にピッチベンドスライダ（±2半音、バネで中央へ戻ります）
- **USB MIDI キーボード**（下記）

### USB MIDI キーボード（v2.0 新機能）
Tab5 の **USB-A ポート**に USB MIDI キーボードをつなぐと、そのまま演奏できます（設定不要）。

- 対応：ノートオン／オフ、ピッチベンド
- **動作確認済み：KORG microKEY-25**（ジョイスティック左右＝ピッチベンド、オクターブシフトボタンも動作）
- MIDI キーボードで弾いた演奏もフレーズ録音（ABC書き出し）にそのまま記録されます
- キーボードを抜くと、押さえていた音は自動で止まります
- ジョイスティック上下（モジュレーション CC）には現在未対応です

> **電源の注意**：キーボードへの給電は Tab5 の USB-A（5V）から行われます。Tab5 自体も最大約 2.6W を消費するため、PC の USB 給電では不足することがあります。キーボードを使うときは **5V / 2A 以上の USB AC アダプタ**での給電を推奨します。消費電力の大きいキーボード（パッド／RGB 搭載など）では給電が追いつかない場合があります（microKEY-25 は 100mA 未満で問題ありません）。

### Unit 8Encoder（つまみ操作）
トグルスイッチで2バンクを切り替えます。つまみ8は両バンク共通のプリセット操作です。

| | つまみ1〜7 | つまみ8 |
|---|---|---|
| **Bank A（演奏用）** | CUTOFF / RESONANCE / F-ENV / DECAY / LFO RATE / LFO DEPTH / GLIDE | PRESET |
| **Bank B（音作り用）** | WAVE / DETUNE / OSC MIX / PWM / ATTACK / RELEASE / NOISE | PRESET |

- つまみ8 **回転**：プリセット切替（今いる組＝1-8 または 9-16 の中を循環）
- つまみ8 **短押し**：発音モード切替（POLY → MONO → UNISON）
- つまみ8 **長押し**：現在の音色を USER バンクの同番号へ保存

### プリセット（v2.0 で 16 音色に拡張）
- **SAMPLE バンク**：ファクトリープリセット16種。ファームウェア内蔵で書き換え不可
  1 PIANO / 2 ORGAN / 3 FUNK BASS / 4 SAW LEAD / 5 SQUARE LEAD / 6 PWM PAD / 7 UNISON FAT / 8 NOISE FX / 9 STRINGS / 10 BRASS / 11 BELL / 12 SUB BASS / 13 PLUCK / 14 FC / 15 ELEC PIANO / 16 CHOIR PAD
  - 14 **FC** はファミコンの矩形波の素の音を狙った音色です。POLY で2音鳴らすと往年の2声ハーモニーになります
- **USER バンク**：SDカードに16スロット保存。未保存のスロットは同番号の SAMPLE と同じ音になります
  - v1.0 の USER ファイル（8スロット）もそのまま読み込めます（先頭8枠として引き継がれます）
- 画面上の **SAMPLE / USER ボタン**で出典を、**GRP ボタン**で組（1-8 / 9-16）を切り替えます

### フレーズ録音とABC書き出し
- トランスポート帯の **REC / STOP** で演奏をそのまま記録（BPMは記譜用の基準、40〜240）
- STOPすると ABC記譜法テキストとしてSDへ書き出し（`phrase.abc`、`phrase2.abc`、…と連番で上書きなし）
- EasyABCなどの無料ツールで五線譜に変換できます。和音は `[CEG]` のように書き、小節をまたぐ音はタイでつなぎます

### サンプリング
- **MICボタン**で内蔵マイクから4秒録音（16kHz）。録音した音は自動で `/sample.wav` としてSDにも保存されます
- Bank B の **WAVEつまみ右端（SAMPLEゾーン）**で、録音した音を鍵盤で演奏できます（C4＝原音ピッチ、上下の鍵でピッチが変わります）
- 起動時にSDの `/sample.wav` があれば自動で読み込むので、電源を切ってもサンプルは残ります。PCで用意したPCM16 WAV（モノラル/ステレオ）を置いて使うこともできます

## 必要なもの

| 品目 | 備考 |
|---|---|
| M5Stack Tab5（C145）または Tab5 Kit（K145） | |
| M5Stack Unit 8Encoder（U153） | つまみ操作用 |
| Grove 接続ケーブル（HY2.0-4P） | 8Encoder に付属 |
| microSD カード | FAT32 フォーマット。プリセット保存・ABC書き出し・サンプル保存用 |
| USB MIDI キーボード（任意） | KORG microKEY-25 動作確認済み |
| USB AC アダプタ 5V / 2A 以上（推奨） | USB MIDI キーボード使用時の給電用 |

## 配線

- Tab5 の **HY2.0-4P ポート（PORT.A）** と Unit 8Encoder を付属の Grove ケーブルでつなぎます（PORT.A は SDA = G53 / SCL = G54、スケッチ内で設定済み）
- USB MIDI キーボードは Tab5 の **USB-A ポート**へつなぎます（電源を入れたまま抜き差しできます）

## ビルド方法（Arduino IDE）

1. Arduino IDE に M5Stack ボードマネージャを追加し、ボードパッケージをインストール（**最新のものに更新**してください。USB ホスト機能のため ESP32 コア 3.3.1 以降が必要です）
2. ライブラリマネージャから **M5Unified** と **EspUsbHost** をインストール
3. ボードに **M5Tab5** を選択
4. `tab5_analog.ino` を同名のフォルダ（`tab5_analog`）に入れて開き、書き込み

## 起動と操作の流れ

1. 電源を入れるとレトロな起動チェック画面（"Now Warming up..."）が表示されます
2. 8Encoder の準備が整うまで約10秒かかります。**この間も鍵盤と USB MIDI は弾けます**（ヘッダ表示が `ENC: WAIT` → `ENC: OK` に変わります）
3. つまみ8を回してプリセットを選び、トグルでバンクを切り替えながら音作りをします
4. 気に入った音は、つまみ8長押しで USER バンクへ保存できます

## 技術メモ（開発で得た知見）

- **8Encoder の静音ブート**：本ユニットは電源投入直後・起動中に I2C アクセスを受けると固まってしまいます。電源だけ先に入れ、約10秒間 I2C に一切触らない（`Wire.begin()` もしない）ことで安定動作します。読み出しは STOP コンディションを挟む方式で、レジスタごとに細かく分けて読みます（大きなブロック読みも固まる原因になります）
- **playRaw のバッファ所有権**：`M5.Speaker.playRaw()` はデータをコピーせずポインタだけをキューに入れるため、スピーカーが読み終わる前にバッファを書き換えると再生中の音が壊れます。本スケッチではチャンネルを固定し、`isPlaying()` で読み終わりを確認してから次のチャンクを描いています
- **USB MIDI の取り込み**：EspUsbHost のコールバック（別タスク）からはリングキューに積むだけにし、実際の発音処理は loop 側で行います。MIDI ノートは仮想タッチ ID（1000+ノート番号）でタッチ鍵盤と同じ経路に流すことで、発音モード・鍵盤表示・ABC 記録がそのまま効きます。このとき、タッチの整合処理（押されていない鍵を消す処理）の対象を実タッチ ID のみに限定しないと、MIDI の音が約10msで消えてしまうので注意が必要です

## 更新履歴

### v2.0
- USB MIDI キーボード対応（USB-A ポート、KORG microKEY-25 で動作確認）
- プリセットを 16 音色に拡張（9 STRINGS / 10 BRASS / 11 BELL / 12 SUB BASS / 13 PLUCK / 14 FC / 15 ELEC PIANO / 16 CHOIR PAD を追加）。GRP ボタンで組を切り替え、つまみ8は組の中で切替
- USER バンクを 16 スロットに拡張（v1.0 のファイルも読み込み可）

### v1.0
- 初版リリース（Phase 0〜5 の全機能：音源エンジン／8Encoder連携／プリセットSD保存／ABC書き出し／マイクサンプリング）

## ライセンス

MIT License — 詳細は [LICENSE](LICENSE) を参照してください。
使用ライブラリ（M5Unified / M5GFX / EspUsbHost など）のライセンスは各ライブラリに従います。
