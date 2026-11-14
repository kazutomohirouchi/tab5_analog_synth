// ============================================================
//  tab5_analog v1.0 — M5Tab5 アナログシンセサイザー
//  Author: かずとも / KAZUTOMO
//  License: MIT (see LICENSE)
//
//  M5Stack Tab5 単体で動く減算合成アナログ風シンセサイザー。
//  音は内蔵スピーカーからソフトウェア合成で生成し、M5 Unit
//  8Encoder の物理つまみで音作りをします。5 ボイス
//  (POLY / MONO / UNISON)、プリセットの SD 保存、フレーズ
//  録音の ABC 記譜書き出し、内蔵マイクによるサンプリング
//  (鍵盤でピッチ再生、SD 保存・起動時読み込み) を搭載。
//
//  必要ライブラリ: M5Unified (M5GFX を含む)
//  ボード: M5Tab5 (Arduino IDE のボードマネージャから)
//
//  配線: Unit 8Encoder (U153) を Tab5 の PORT.A へ接続。
//  注意: 8Encoder は電源投入直後の I2C アクセスで固まる
//  ため、本スケッチは起動後約 10 秒間 I2C を一切触りませ
//  ん (その間も鍵盤は弾けます)。詳細は末尾の開発履歴へ。
// ============================================================
//  以下、開発履歴 (フェーズ毎の変更メモ)
// ============================================================
//  tab5_analog Phase 5r7 : playRaw バッファ上書きバグの修正
// ------------------------------------------------------------
//  5r7 の変更点 (2026-11-15):
//  真因が判明 (M5Unified Speaker_Class のソースで確認):
//  playRaw はデータをコピーせず、ポインタをキューに入れる
//  だけ。スピーカータスクはそのバッファを非同期に読む。
//  本スケッチは A/B 2 バッファを交互に使っていたが、playRaw
//  が true を返した直後 (まだ読み終わっていない) に次の次
//  のチャンクを同じバッファへ描いていたため、再生中の
//  チャンクの未読部分が新しいデータで上書きされ、音が欠落
//  していた。持続音では目立たず、声だけが飛んで早口に聞
//  こえる。spos 等のレンダ側の数値が全て正常だった理由も
//  これで説明がつく。対策 (公式ドキュメントのパターン):
//  (1) playRaw のチャンネルを 0 に固定する (従来は自動割当)
//  (2) バッファへ描く前に、チャンネル 0 の占有が 2 未満
//      (前の要求が読み終わった状態) になるまで待つ
// ------------------------------------------------------------
//  5r6 の変更点 (2026-11-15):
//  (1) 起動時に SD の /sample.wav があれば読み込んで使える
//      ようにした (Hirouchi さんの要望、USER バンクと同じ
//      考え方)。電源を切ってもサンプルが残り、起動直後から
//      MIC OK 表示でそのまま弾ける。PCM16 WAV (モノラル/
//      ステレオ、レートはヘッダから取得) に対応。
//  (2) 再生方式を変更: 録音/読込の時点で 44.1kHz バッファ
//      (g_smp44) へ変換しておき、再生中は 1:1 で読むだけ
//      にした。再生中の逐次リサンプリング (線形補間→区間
//      平均と変えてきた部分) を完全に排し、直接再生
//      (playRaw) と同じ条件にそろえる最終形。
//  5r5 までの対策 (SVF/ノイズ/リバーブ/GAIN のバイパス、
//  ゲートエンベロープ) は全て維持。
// ------------------------------------------------------------
//  5r5 の変更点 (2026-11-15):
//  5r4 でも聞こえ方は変わらず。数値面は全て正常 (spos は
//  毎秒 +15920、trigs 不変、note=60、録音データ正常、直接
//  再生正常)。残る差は「音量と残響」: 鍵盤経路のサンプルは
//  ADSR のサステイン (0.80) × リバーブのドライ減衰 ×
//  MASTER_GAIN (0.6) で直接再生の約 0.44 倍まで落ち、弱い
//  音節が埋もれる。対策としてサンプル経路を直接再生と
//  等価な条件にそろえる:
//  (1) エンベロープは ADSR でなく単純ゲート (5ms で 1.0、
//      保持中 1.0、離鍵 30ms 減衰)
//  (2) リバーブと MASTER_GAIN はオシレータ側だけに掛け、
//      サンプルは別バスでドライ・等音量に合算する
//  フィルタ/ノイズのバイパスと区間平均 (5r3/5r4) は維持。
// ------------------------------------------------------------
//  5r4 の変更点 (2026-11-15):
//  5r3 の動画解析ではテンポは正常化 (約 2.4 秒に母音 5 つが
//  約 0.5 秒間隔、声の高さも自然な約 150Hz)。それでも聞き
//  取りにくさが残るため、サンプル経路の音質を 2 点改善:
//  (1) ノイズ混合をオシレータ専用に戻す。5r3 でもノイズは
//      全経路共通で、SAW LEAD の NOISE 20% が声にヒスとし
//      て被っていた。
//  (2) サンプル読み取りを線形補間 (飛ばし読み) から区間
//      平均に変更。16kHz → 44.1kHz では出力 1 サンプルあ
//      たり入力を約 2.77 個進めるため、点だけ拾うと折り返
//      し雑音で声が金属的に潰れる。進む区間を平均すると
//      簡易アンチエイリアスになる (inc >= 0.9 の高音域は
//      従来の線形補間のまま)。
// ------------------------------------------------------------
//  5r3 の変更点 (2026-11-15):
//  [SMPL] ログで再生位置は完全に正常と確定 (毎秒 +15920、
//  len で正しくラップ、長押し中 trigs 不変 = 再トリガなし)。
//  残る症状「全部は再現されない」の真犯人は SVF: SAW LEAD
//  の LFO がカットオフを周期的に揺らし (設計上の「弱いビブ
//  ラート的揺れ」)、下がった瞬間に言葉が消えていた。動画
//  の鍵盤再生も約 0.54 秒周期の断片化を確認。対策:
//  (1) サンプル再生時は SVF をバイパスしドライで通す
//      (フィルタとその変調はオシレータ専用に戻す)
//  (2) 録音後にピーク正規化 (30000 へ、最大 x8)。静かな
//      録音 (peak=7128 の回) でも小さい音節が埋もれない
// ------------------------------------------------------------
//  Phase 5r2 の変更点 (2026-11-14):
//  実測で録音データ (16kHz / recMs=4020) と録音直後の直接
//  再生が正常と確定したため、残る容疑は「鍵盤を押している
//  間にタッチの一瞬の欠落で同音が OFF→ON し、サンプルが
//  先頭から再トリガされ続けている」こと。対策として、
//  (1) 同音の OFF から 150ms 以内の ON はドロップアウトと
//  みなして再トリガせず、リリース中のボイスをそのまま再
//  ゲートする (再生位置 spos は保持される)。
//  (2) 切り分け用に [SMPL] ログを 1 秒毎に出す (spos の
//  進み方と trigs = 実トリガ回数)。
// ------------------------------------------------------------
//  Phase 5r の変更点 (2026-11-14):
//  実機で C4 再生が速すぎる問題への対策。録音にかかった実
//  時間 (recMs) から「実際に届いたサンプルレート」を逆算し
//  (g_smpEffRate)、再生速度と WAV ヘッダに使う。録音デー
//  タ側のレートずれならこれで自動的に合う。あわせて切り分
//  け用に、録音直後にバッファを playRaw で直接 1 回再生す
//  る (単体テストと同じ経路)。直接再生が正常で鍵盤再生だ
//  け速ければエンジン側、両方速ければ録音データ側の問題。
// ------------------------------------------------------------
//  Phase 5 の変更点 (2026-11-14):
//  トランスポート帯右端に MIC ボタンを追加。タップすると
//  音声タスクを止めて内蔵マイクで 4 秒録音 (16kHz、PSRAM
//  バッファ、末尾の無音はトリム) し、SD に /sample.wav も
//  保存する。WAVE つまみに第 4 ゾーン SAMPLE (0.84 以上) を
//  追加し、録った音を鍵盤でピッチ再生 (C4 で原音、ループ
//  再生、ベンド/グライドも効く)。既存プリセットの波形値
//  (0.0 / 0.5) は従来どおり SAW / SQUARE のまま不変。
//  手順 (Speaker.end → Mic → Speaker.begin) は単体テスト
//  tab5_analog_mic_test で実機確認済みのものをそのまま使う。
// ------------------------------------------------------------
//  4b の変更点 (2026-11-14):
//  鍵盤下にトランスポート帯を追加 (REC/STOP ボタン、BPM の
//  - / + 、録音状態)。演奏を時刻付きイベントで記録し、STOP
//  で SD に ABC 記譜法テキストを書き出す (連番 /phrase.abc,
//  /phrase2.abc ...)。変換は tab5_synth Phase 9 の方式を
//  移植: BPM 基準で 16 分音符に丸め、和音は [CEG]、小節を
//  またぐ音はタイでつなぐ。MONO/UNISON では実際に鳴って
//  いる 1 音だけを残す (新しく押した音で前を閉じる)。
//  BPM は記譜の基準専用で、音や録音自体には影響しない。
// ------------------------------------------------------------
//  4r の変更点 (2026-11-14, Hirouchi の選択):
//  実機でダブルプッシュが安定しなかった (ボタン読みが
//  ポーリング周期 ≒350ms に 1 回で、速い連打の段階を取り
//  こぼす) ため、プリセットバンク切替はヘッダの画面ボタン
//  (SAMPLE / USER) に変更。あわせて:
//  (1) 短押しのモード循環を「離した瞬間に即確定」に戻した
//      (ダブル判定の 500ms 待ちを廃止)。
//  (2) 長押し (900ms) の保存はそのまま。
//  (3) 役目を終えた全ボタン表示用の読み (4 回に 1 回の追加
//      読み) を削除し、ボタンのサンプリング間隔を均した。
// ------------------------------------------------------------
//  Phase 4 前半の変更点 (2026-11-14):
//  (1) つまみ 8 のプッシュを 3 種に完成させた (設計どおり):
//      短押し = 発音モード循環 (ダブル判定のため最大 500ms 遅延)、
//      ダブル = プリセットバンク SAMPLE/USER 切替、
//      長押し (900ms) = 現在の音を USER の同番号スロットへ保存。
//  (2) USER バンクを SD に保存 (/analog_user.bin、マジック
//      "U8P1" + 保存フラグ 8 + Params 8 個分)。起動時に読み込み、
//      未保存スロットは SAMPLE の同番号の音で鳴る (設計どおり)。
//      SD 初期化は tab5_synth で実証済みの SPI → SDIO の順。
//  (3) 保存/バンク切替の結果はステータス行に一時表示 (トースト)。
//  ※ フレーズ録音 + ABC 出力は Phase 4 後半で実装する。
// ------------------------------------------------------------
//  3r8 の変更点 (2026-11-14, Hirouchi の指摘):
//  ノート表示時の状態行 (最大 39 文字 ≒ 468px) が、クリア範囲
//  (x<480) を超えて VOL 側に描かれ、短い表示に戻った後も
//  その部分が消えずに残っていた。対策は 2 つ:
//  (1) 状態行の書式を詰める (最大 32 文字 ≒ 384px に収める)。
//  (2) drawStatus の最後で必ず drawVolume() を呼び、
//      スライダー領域を描き直して残像が出ないようにする。
// ------------------------------------------------------------
//  3r7 の変更点 (2026-11-14, Hirouchi のリクエスト):
//  (1) 操作系の問題が片付いたため、画面下部の診断行
//      (ENC READY / BTN:...) を削除。NOTE 表示の 1 行だけを
//      左に残す (診断はシリアルで追える)。
//  (2) VOL/REV を上下 2 段から横並びに戻す (縦積みは誤操作が
//      多かったため)。各バー幅は 380px、タッチ判定は画面下部
//      全高 + 中央境界で明確に分ける。
// ------------------------------------------------------------
//  3r6 の変更点 (2026-11-14, Hirouchi のリクエスト):
//  VOL/REV のバーが狭く微調整できないため、画面下部を再配置。
//  左下の文字 (NOTE 表示とエンコーダ診断) は開発用の表示で
//  完成版では残さないものなので、左端に圧縮して残しつつ
//  (SW 生値は削除、Hz は整数化、開始位置を x=30 へ)、
//  スライダー領域を x=560 からに広げ、VOL と REV を
//  上下 2 段・バー幅 592px (従来 138px の約 4.3 倍) にした。
// ------------------------------------------------------------
//  3r5 の変更点 (2026-11-14, Hirouchi のリクエスト):
//  (1) リバーブ追加。Freeverb 方式 (コムフィルタ 8 本 +
//      オールパス 4 本) を全ボイスのミックス後に掛けるので、
//      鍵盤を離した後も残響の尾が自然に消えていく。
//      量は Params.reverb としてプリセットごとに初期値を持ち、
//      画面右下の REV スライダーでいつでも調整できる
//      (Phase 4 の SD プリセット保存にもそのまま含まれる)。
//  (2) 右下のスライダー領域を VOL / REV の 2 本に分割。
// ------------------------------------------------------------
//  3r4 の変更点 (2026-11-14):
//  (1) ボタンのレジスタを V1 プロトコルに合わせて修正。
//      公式プロトコル文書で判明: このユニットは FW V1 で、
//      ボタンは 0x50+ch の「1 チャンネル 1 バイト (0/1)」。
//      0x62 のビットマスクは V2 専用で、V1 では未定義アドレス
//      がスイッチの値 (0/1) を返すため B:0x00/0x01 に見えていた。
//      CH8 ボタン (0x57) を毎ポーリング読み、押して離すと
//      発音モード循環。全ボタンの生値は診断行に BTN:11111111
//      の形で表示 (4 ポーリングに 1 回更新)。
//      あわせて初期化成功時に FW 版数 (0xF0) をシリアルへ出力。
//  (2) 波形生成に PolyBLEP を導入 (ノコギリと矩形 PWM)。
//      素朴な波形の折り返し雑音 (音の粒) を抑える定番手法。
//      三角波は元々折り返しが少ないため据え置き。
// ------------------------------------------------------------
//  3r3 の変更点 (2026-11-14, Hirouchi のリクエスト):
//  スプラッシュのチェック行を「全項目が CHECKING (黄) で一斉に
//  表示され、上から順に OK (緑) へ変わっていく」形に変更。
//  SPEAKER / DISPLAY / TOUCH は 1.2 秒間隔、8ENCODER は実時間の
//  ウォームアップ完了で最後に OK になる。音声・ボタンまわりは
//  3r2 と同一 (ボタンのレジスタ特定は button_probe で調査中)。
// ------------------------------------------------------------
//  3r2 の変更点 (2026-11-14):
//  (1) 起動スプラッシュのチェック行を段階表示に (Hirouchi の
//      演出リクエスト): SPEAKER / DISPLAY / TOUCH が等間隔で
//      CHECKING (黄) → OK (緑) と進み、8ENCODER だけ本物の
//      ウォームアップ時間で最後に OK になる。
//  (2) 音声生成のオーバーラン対策 (Phase 3 の動画解析で特定:
//      50ms チャンクの生成に約 86ms かかり供給が枯れていた)。
//      原因は 1 サンプルあたり powf 3 回。音程はチャンク毎に
//      1 回、ベンド倍率とフィルタ係数は 16 サンプル毎のブロック
//      更新 + 線形補間に変更 (powf は約 6600 回 → 約 300 回)。
//  (3) つまみ 8 プッシュの取りこぼし対策: 500ms の時間判定を
//      廃止し「押して離せば切替」に単純化。ボタン読みは
//      ポーリングの先頭と末尾の 2 回に増やし、状態表示に
//      ボタンの生バイト (B:0x..) を追加 (ビット対応の目視検証用)。
// ------------------------------------------------------------
//  3r の変更点 (2026-11-14, Hirouchi の発案): 起動直後は鍵盤を
//  出さず、タイトル + 昔のコンピュータ風チェック表示の
//  スプラッシュにする。8Encoder のウォームアップ (約 10 秒) を
//  そのまま「Now warming up...」の演出に使い、初期化が成功して
//  LED が全点灯するタイミングで「I2C OK!」→ 鍵盤画面へ移行。
//  ウォームアップ中はタッチでスキップも可能 (従来動作に戻る)。
//  音声・演奏まわりのロジックは Phase 3 と同一。
// ------------------------------------------------------------
//  Phase 3 の変更点 (2026-11-14):
//  (1) 反応の鈍さ・離鍵後の残響・ブブブブという断続音の対策。
//      原因は loop() がエンコーダ読み (1 回約 300ms) に占有され、
//      タッチ処理が数 Hz に落ちていたこと。対策としてエンコーダ
//      処理を専用タスク (core 0) に分離し、loop() をタッチ専念に
//      戻した。表示更新はフラグ経由で loop() が行う (描画は
//      loop() のみ)。エンコーダの取引手順は 2r4 の実証どおり。
//  (2) 発音モード MONO / POLY / UNISON を実装。UI からの指示は
//      ノートイベントキューに変更し、音声タスク側でボイス
//      割り当て (POLY は空き優先→最も静かなボイスをスティール、
//      UNISON は 5 ボイスをデチューンで重ねる)。モード切替は
//      つまみ 8 の短押し (POLY→UNISON→MONO と循環)。
//  (3) 画面右下に音量スライダー (設計メモ: VOLUME は画面側)。
//  (4) 出力の硬いクリップをソフトクリップに (歪み対策)。
//  (5) ヘッダにモード表示と、プリセットから編集したら * 印。
// ------------------------------------------------------------
//  2r4 の変更点 (2026-11-14): 2r3 の診断結果を受けた読み方の修正。
//  2r3 の実機結果: LED 書き込みは毎回成功 (カウント 1 の瞬間に
//  CH1 赤 LED が点灯)、しかし読み取りは got=0 err=4 で全滅が
//  継続。err=4 はバスが詰まった状態で出るコードであり、0r9 と
//  残っていた違いは「32 バイト一括読み」と「トランザクション
//  間隔の短さ」。一括読みがバスを詰まらせ、直後の CH 別読みが
//  err=4 になる流れが最有力と判断し、2r4 では:
//  ・カウンタ読みは CH 別 4 バイト読みのみ (一括読み廃止)
//  ・LED 書き込みの後に 50ms、CH 読みの間に 20ms の間隔を入れ、
//    0r9 の緩やかなトランザクション間隔に戻す
//  ・読み失敗時はバスクリアしてから待機に入る
//  ・診断行は W err と RD got/err を 1 行に集約 (上書きで
//    途中の段階が消えないように)
// ------------------------------------------------------------
//  2r3 の変更点 (2026-11-14): ENC が初期化できない件の切り分け。
//  反省点: 0r9 で実証した読み取り前の待ちは 10ms だったのに、
//  Phase 2 系ではポーリング速度を優先して 3ms → 6ms に縮めて
//  いた。記録した実証レシピは「約10ms」であり、ここを勝手に
//  変えたのは誤り。2r3 では 10ms に戻す (カウンタはユニット側
//  ハードウェアに溜まるので、ポーリング間隔を 100ms に伸ばし
//  てもクリックは取りこぼさない)。
//  あわせて初期化の診断を画面にも出す: 最初の接触は 0r9 と
//  同じ LED 書き込みから始め、各段階の err と受信バイト数
//  (got) をステータス行 2 行目に表示する。LEDW err=0 で
//  READ got=0 なら「応答はあるがデータが来ない」= 待ち時間
//  や読み方側の問題、LEDW から err が立つなら電源・接触・
//  バス側の問題、と写真 1 枚で切り分けられる。
// ------------------------------------------------------------
//  2r2 の変更点 (2026-11-14): 2r の実機テストで残った2件の修正。
//  (A) 鍵を押しても音が出ない件: 容疑は VCF (Chamberlin SVF) の
//      発散。起動プリセット SAW LEAD は変調後のカットオフが
//      最大級で、係数が安定域を超えるとフィルタ状態が発散し、
//      NaN 化して恒常的に無音になる (RISC-V の NaN→int 変換は
//      DC になる)。対策: 変調後カットオフの上限 9000→7000、
//      係数上限 1.2→0.8、それでも壊れたら検出して状態を
//      リセットする NaN ガードをチャンク毎に追加。
//  (B) ENC が OK にならない件: 初期化の読み取りが全滅して
//      10 秒ループを繰り返していた。10 秒ごとの再試行では
//      ユニットに十分な「静かな時間」を与えられない可能性を
//      考え、段階方式に変更: 失敗1回目→電源入れたまま +15 秒
//      静かに待つ / 2回目以降→電源を入れ直して 20 秒待つ。
//      再テスト時は事前にユニットを 30 秒以上抜いて休ませ、
//      挿してから Tab5 をリセットして電源投入と静寂タイマを
//      同期させること (手順は納品メッセージ参照)。
// ------------------------------------------------------------
//  2r の変更点 (2026-11-14): 初回実機テストで判明した3件の修正。
//  (1) 画面が縦向き (720x1280) のまま描画され、1280 幅の UI の
//      右側が切れていた → setup で setRotation(1) を明示する
//      (0r8/0r9 では入れていたのに Phase 2 で落としていた)。
//      タッチ座標系もこれで想定どおりに揃う。
//  (2) つまみが効かない件への備え: カウンタ読みを自己回復式に
//      した。32 バイト一括読みが失敗したら、その場でチャンネル
//      別 4 バイト読み (0r9 で実証済みの形) に切り替えて続行
//      する。読み取り前の待ちも 3ms → 6ms に戻した。
//  (3) 初期化時の読み取りリトライを追加 (3 回まで)。
// ------------------------------------------------------------
//  メモ「フェーズ案」の Phase 2。Phase 1r2 (モノシンセ最小構成
//  + ベンド平滑化 + 鍵盤間隔) を土台に、以下を追加する。
//
//  ・8Encoder (U153) を全パラメータに接続 (バンクA/B は本体の
//    トグルスイッチで切替、8番は両バンク共通で PRESET)
//  ・音響エンジン拡張: レゾナンス付き VCF (2極 SVF)、ADSR
//    (A/D/R はつまみ、S はプリセット値)、LFO → カットオフ、
//    VCO2 デチューン + OSC MIX、PWM、ノイズ、グライド
//  ・見本プリセット 8 種を内蔵し、8番つまみの回転で即切替
//  ・エンジンは引き続き MONO (5ボイスプールの voice[0])。
//    POLY / UNISON とプッシュ操作 (モード切替・保存) は
//    Phase 3 以降、SD 保存と ABC 録音は Phase 4 (メモ通り)
//
//  ★ 8Encoder の作法 (2026-11-14 Phase 0r8/0r9 で実証済み):
//    1. 電源投入後 10 秒間は I2C を一切触らない (Wire.begin
//       すら呼ばない)。起動直後に話しかけるとユニットが固まる。
//       → 本スケッチでは電源投入だけ先に行い、鍵盤と音は即時
//         使用可能。つまみは 10 秒後に自動で有効になる
//         (画面右上に ENC: WAIT n と表示)。
//    2. 読み取りは STOP-start 方式 (endTransmission(true) で
//       STOP を打ち、数 ms 空けてから requestFrom)。
//       リピーテッドスタートだと 0 バイトしか返らない。
//    3. 失敗が続く場合はバスクリア → それでもダメなら
//       setExtOutput でユニットの電源を入れ直し、再度 10 秒
//       待つ (0r9 の復旧ラダーを移植)。
//
//  つまみ割り当て (メモ 2026-11-09 確定):
//    バンクA (ライブ): 1 CUTOFF 2 RESONANCE 3 F-ENV 4 DECAY
//                      5 LFO RATE 6 LFO DEPTH 7 GLIDE 8 PRESET
//    バンクB (音作り): 1 WAVE 2 DETUNE 3 OSC MIX 4 PWM
//                      5 ATTACK 6 RELEASE 7 NOISE 8 PRESET
//    バンクは LED 色でも区別 (A=オレンジ, B=青)。
//    カウントは 1 クリック ±2 で増減するのがこのユニットの
//    仕様 (文献確認済み)。内部で 2 で割ってクリックに直す。
//
//  未確定事項 (実機で確認してほしい点):
//    ・トグルスイッチのビット: レジスタ 0x60 の bit0 で
//      バンクを判定している。画面下部に SW:0xNN と生値を
//      表示するので、トグルを倒した方向と画面の BANK 表示
//      が逆だったら教えてほしい (1行で直せる)。
//    ・カウンタ読みは CH 別 4 バイト読みのみ (32 バイト一括
//      読みはバスを詰まらせる疑いが濃厚なため 2r4 で廃止)。
//
//  注意 (過去の教訓):
//  ・型・プロトタイプはファイル先頭に置く
//  ・loop() から呼ばれる関数に大きな配列をローカルに置かない
// ============================================================

#include <M5Unified.h>
#include <Wire.h>
#include <FS.h>
#include <SPI.h>
#include <SD.h>
#include <SD_MMC.h>
#include <math.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <esp_heap_caps.h>

// ---------------- 型 (先頭に置く) ----------------
// パラメータ一式。値は全て 0.0..1.0 に正規化し、音響側で実値へ変換。
struct Params {
  float cutoff;    // VCF カットオフ
  float reso;      // VCF レゾナンス
  float fEnv;      // フィルタエンベロープ量
  float decay;     // エンベロープ DECAY 時間
  float sustain;   // サステインレベル (つまみ無し、プリセット値)
  float attack;    // エンベロープ ATTACK 時間
  float release;   // エンベロープ RELEASE 時間
  float glide;     // ポルタメント量
  float wave;      // 波形 (0..0.33 ノコギリ / ..0.66 矩形 / ..1 三角)
  float detune;    // VCO2 デチューン (0.5 が中央 = ±0、両端で ±50 cent)
  float oscMix;    // VCO2 の混ぜ具合 (0=VCO1のみ, 1=VCO2のみ)
  float pwm;       // 矩形波のパルス幅
  float noise;     // ノイズの混ぜ具合
  float lfoRate;   // LFO 速度
  float lfoDepth;  // LFO → カットオフの深さ
  float reverb;    // リバーブ量 (3r5, REV スライダーで調整)
};

struct ActiveKey {
  int id = -1;
  int note = -1;
  uint32_t seq = 0;
  bool active = false;
};

// ノートイベント (UI → 音声タスク)。type: EV_OFF=0 / EV_ON=1
struct NoteEvent {
  volatile int8_t type;
  volatile int8_t note;
};

// ボイス (音声タスクが所有。Phase 2 では audioTask 内にあったが、
// イベント処理を関数化するためファイルスコープに移した)
struct Voice {
  int note = -1;
  float ph1 = 0.0f, ph2 = 0.0f;  // VCO1 / VCO2 位相 0..1
  float spos = 0.0f;             // サンプル再生位置 (Phase 5)
  float freqSm = 0.0f;           // グライドで平滑化した音程 (ベンド抜き)
  float low = 0.0f, band = 0.0f; // SVF 状態
  float env = 0.0f;              // エンベロープ 0..1
  float relInc = 0.0f;           // Release 開始時に確定する減衰量
  int stage = 0;                 // 0=無音 1=A 2=D 3=S 4=R
  bool gate = false;
  uint32_t age = 0;              // 発音開始順 (スティール判定用)
};

enum { EV_OFF = 0, EV_ON = 1 };
enum { MODE_MONO = 0, MODE_POLY = 1, MODE_UNISON = 2 };
static const char* MODE_NAMES[3] = {"MONO", "POLY", "UNISON"};

// ---------------- プロトタイプ (先頭に置く) ----------------
static void drawAll(void);
static void drawTop(void);
static void drawParams(bool force);
static void drawParamBox(int i);
static void drawBend(void);
static void drawKeyboard(void);
static void drawWhiteKey(int vis);
static void drawBlackKey(int vis);
static void drawKeyByNote(int note);
static void drawStrip(void);
static void drawStatus(void);
static void drawTransport(void);
static void drawTransportStatus(void);
static void toggleRec(void);
static void handlePoints(void);
static void keyOn(int note, int id);
static void keyOffById(int id);
static void synthNoteOn(int note);
static void synthNoteOff(void);
static void pushEvent(int type, int note);
static void trigVoice(Voice& v, int note, bool glideSnap);
static void relVoice(Voice& v, float releaseSec);
static void applyNoteEvent(int type, int note, bool glideSnap,
                           float releaseSec, int mode);
static void audioTask(void* arg);
static void encTask(void* arg);
static void consumeUiUpdates(void);
static void drawSplash(void);
static void updateSplash(void);
static void drawVolume(void);
static void updateVolumeFromX(int x);
static float midiFreq(int note);
static const char* noteName(int note);
static int keyAtPoint(int x, int y);
static bool hasBlackAfter(int whiteIdx);
static int whiteIndexOf(int note);
static bool noteIsActive(int note);
static void scrollStripTo(int x);
static void updateBendFromY(int y);
static float* fieldPtr(Params& p, int f);
static void loadPreset(int idx);
static void applyKnob(int ch, int clicks);
static void encPollStep(void);
static void encHandleBtn8(uint8_t v, uint32_t now);
static void encBtn8Tick(uint32_t now);
static void setToast(const char* s, uint32_t durMs);
static void sdLoadUserBank(void);
static void doSavePreset(int slot);
static void consumePhase4(void);
static float polyblep(float ph, float dt);
static float reverbProcess(float in);
static void encTryInit(void);
static void encRestart(bool cyclePower, uint32_t quietMs);
static void encWriteLEDs(void);
static bool encWriteBlk(uint8_t reg, const uint8_t* data, size_t len);
static bool encReadSS(uint8_t reg, uint8_t* data, size_t len);
static int32_t encLe32(const uint8_t* b);
static bool encReadCounters(int32_t out[8]);
static void setEncDiag(const char* s);
static void encBusRecover(void);

// ---------------- 見本プリセット (メモ確定の 8 種) ----------------
// フィールド順: cutoff, reso, fEnv, decay, sustain, attack, release,
//               glide, wave, detune, oscMix, pwm, noise, lfoRate, lfoDepth,
//               reverb
static const Params PRESETS[8] = {
  // 1 PIANO (減算合成でのピアノ寄り: 速い減衰 + 控えめなフィルタ)
  {0.55f, 0.15f, 0.50f, 0.55f, 0.25f, 0.01f, 0.30f, 0.00f,
   0.00f, 0.50f, 0.00f, 0.50f, 0.00f, 0.00f, 0.00f, 0.25f},
  // 2 ORGAN (矩形波 + OSC2、即アタック + 持続型)
  {0.70f, 0.05f, 0.00f, 0.10f, 0.95f, 0.01f, 0.08f, 0.00f,
   0.50f, 0.54f, 0.40f, 0.50f, 0.00f, 0.00f, 0.00f, 0.30f},
  // 3 FUNK BASS (ノコギリ + 強い F-ENV + レゾナンス)
  {0.35f, 0.65f, 0.80f, 0.35f, 0.30f, 0.01f, 0.15f, 0.05f,
   0.00f, 0.50f, 0.00f, 0.50f, 0.00f, 0.00f, 0.00f, 0.08f},
  // 4 SAW LEAD (ノコギリ + グライド + 弱いビブラート的揺れ)
  {0.75f, 0.30f, 0.25f, 0.20f, 0.80f, 0.05f, 0.20f, 0.30f,
   0.00f, 0.56f, 0.50f, 0.50f, 0.00f, 0.45f, 0.10f, 0.30f},
  // 5 SQUARE LEAD (矩形波 + ポルタメント、YMO 風)
  {0.80f, 0.20f, 0.15f, 0.20f, 0.85f, 0.03f, 0.18f, 0.50f,
   0.50f, 0.52f, 0.20f, 0.35f, 0.00f, 0.45f, 0.06f, 0.30f},
  // 6 PWM PAD (PWM + 遅いアタック + デチューン)
  {0.50f, 0.25f, 0.10f, 0.30f, 0.80f, 0.55f, 0.55f, 0.10f,
   0.50f, 0.60f, 0.60f, 0.50f, 0.00f, 0.20f, 0.20f, 0.50f},
  // 7 UNISON FAT (太いデチューン。UNISON モードは Phase 3)
  {0.65f, 0.35f, 0.40f, 0.25f, 0.70f, 0.02f, 0.25f, 0.05f,
   0.00f, 0.75f, 0.50f, 0.50f, 0.00f, 0.00f, 0.00f, 0.35f},
  // 8 NOISE FX (ノイズ中心 + 短い減衰)
  {0.50f, 0.70f, 0.60f, 0.50f, 0.00f, 0.01f, 0.40f, 0.00f,
   0.00f, 0.50f, 0.00f, 0.50f, 1.00f, 0.30f, 0.30f, 0.55f},
};
static const char* PRESET_NAMES[8] = {
  "PIANO", "ORGAN", "FUNK BASS", "SAW LEAD",
  "SQUARE LEAD", "PWM PAD", "UNISON FAT", "NOISE FX"
};

// ---------------- Phase 4: SD と USER バンク ----------------
static int g_pbank = 0;              // プリセットバンク: 0=SAMPLE / 1=USER
static Params g_userBank[8];         // USER バンクの音 (SD から読み込み)
static bool g_userSaved[8] = {false, false, false, false, false, false, false, false};
static bool g_sdReady = false;
static fs::FS* g_fs = nullptr;
static const char* g_sdMode = "none";
static volatile int g_saveReq = -1;  // 保存要求スロット (enc タスク → loop)
static char g_toast[48] = "";
static uint32_t g_toastUntil = 0;

// ステータス行への一時表示 (トースト)。定義は g_encUiDirty の
// 宣言より後 (consumeUiUpdates の直前) に置く

// SD 初期化 (tab5_synth で実証済み: SPI → SDIO の順に試す)
static bool initSD(void) {
  if (g_fs) return true;
  SPI.begin(43, 39, 44, 42);  // SCK, MISO, MOSI, CS
  if (SD.begin(42, SPI, 25000000)) { g_fs = &SD; g_sdMode = "SPI"; return true; }
  SD_MMC.setPins(43, 44, 39, 40, 41, 42);
  if (SD_MMC.begin("/sdcard", false, false, 20000)) { g_fs = &SD_MMC; g_sdMode = "SDIO"; return true; }
  return false;
}

// USER バンクファイル: マジック 4B + 保存フラグ 8B + Params 8 個
static const char* USER_FILE = "/analog_user.bin";
static const int USER_FILE_SIZE = 4 + 8 + (int)sizeof(Params) * 8;
static uint8_t s_bankImg[4 + 8 + sizeof(Params) * 8];

static void sdLoadUserBank(void) {
  if (!initSD()) {
    Serial.println("[P4] SD not ready (USER bank empty)");
    return;
  }
  g_sdReady = true;
  Serial.printf("[P4] SD ready (%s)\n", g_sdMode);
  File f = g_fs->open(USER_FILE, FILE_READ);
  if (!f) {
    Serial.println("[P4] no USER bank file yet");
    return;
  }
  if ((int)f.size() != USER_FILE_SIZE || f.read(s_bankImg, USER_FILE_SIZE) != USER_FILE_SIZE ||
      memcmp(s_bankImg, "U8P1", 4) != 0) {
    f.close();
    Serial.println("[P4] USER bank file invalid, ignored");
    return;
  }
  f.close();
  for (int i = 0; i < 8; i++) g_userSaved[i] = s_bankImg[4 + i] != 0;
  memcpy(g_userBank, s_bankImg + 12, sizeof(Params) * 8);
  Serial.println("[P4] USER bank loaded");
}

static bool sdWriteUserBank(void) {
  memcpy(s_bankImg, "U8P1", 4);
  for (int i = 0; i < 8; i++) s_bankImg[4 + i] = g_userSaved[i] ? 1 : 0;
  memcpy(s_bankImg + 12, g_userBank, sizeof(Params) * 8);
  g_fs->remove(USER_FILE);  // SD ライブラリの FILE_WRITE は追記のため先に消す
  File f = g_fs->open(USER_FILE, FILE_WRITE);
  if (!f) return false;
  size_t w = f.write(s_bankImg, USER_FILE_SIZE);
  f.close();
  return (int)w == USER_FILE_SIZE;
}

// ---------------- Phase 4 後半: フレーズ録音 ----------------
static const int TR_Y = 528, TR_H = 44;             // トランスポート帯 (鍵盤と帯の間)
static const int REC_BTN_X = 100, REC_BTN_W = 130;  // REC/STOP ボタン
static const int BPM_M_X = 305, BPM_P_X = 435, BPM_BTN_W = 50;
struct RecEv { uint32_t t; int8_t note; int8_t type; };
static RecEv g_recEv[4096];
static int g_recEvN = 0;
static bool g_recording = false;
static bool g_recOverflow = false;
static uint32_t g_recStartMs = 0;
static int g_bpm = 120;  // 記譜の基準テンポ (音には影響しない)
static char g_lastAbcName[24] = "";

static void recCapture(int type, int note) {
  if (!g_recording) return;
  if (g_recEvN < 4096) {
    g_recEv[g_recEvN].t = millis() - g_recStartMs;
    g_recEv[g_recEvN].note = (int8_t)note;
    g_recEv[g_recEvN].type = (int8_t)type;
    g_recEvN++;
  } else {
    g_recOverflow = true;
  }
}

// ---------------- Phase 5: サンプリング ----------------
static const uint32_t SMP_RATE = 16000;
static const int SMP_N = 64000;             // 4 秒 (16kHz)
static const float SMP_ROOT = 261.626f;     // C4 の周波数 = 原音ピッチ
static const int MIC_X = 1140, MIC_W = 132; // MIC ボタン (トランスポート帯右端)
static int16_t* g_smpBuf = nullptr;
static volatile int g_smpLen = SMP_N;       // トリム後の有効サンプル数
static volatile float g_smpEffRate = 16000.0f;  // 実測の録音レート (5r)
static volatile uint32_t g_trigCount = 0;       // 実トリガ回数 (5r2 診断)
static volatile bool g_sampleReady = false;
static volatile bool g_micPause = false;    // 音声タスクの送出停止要求
static bool g_micRec = false;               // 録音中 (表示用、loop 側のみ)
// 5r6: 再生専用の 44.1kHz 化済みバッファ (録音/読込時に構築)
static const int SMP44_MAX = 384000;
static int16_t* g_smp44 = nullptr;
static volatile int g_smp44Len = 0;
static void buildSmp44(void);
static bool sdLoadSampleWav(void);

// パラメータフィールド ID とバンク割り当て
enum {
  F_CUTOFF = 0, F_RESO, F_FENV, F_DECAY, F_LFORATE, F_LFODEPTH, F_GLIDE,
  F_WAVE, F_DETUNE, F_OSCMIX, F_PWM, F_ATTACK, F_RELEASE, F_NOISE, F_COUNT
};
static const int BANK_FIELDS[2][7] = {
  {F_CUTOFF, F_RESO, F_FENV, F_DECAY, F_LFORATE, F_LFODEPTH, F_GLIDE},   // A
  {F_WAVE, F_DETUNE, F_OSCMIX, F_PWM, F_ATTACK, F_RELEASE, F_NOISE}      // B
};
static const char* FIELD_NAMES[F_COUNT] = {
  "CUTOFF", "RESONANCE", "F-ENV", "DECAY", "LFO RATE", "LFO DEPTH", "GLIDE",
  "WAVE", "DETUNE", "OSC MIX", "PWM", "ATTACK", "RELEASE", "NOISE"
};

// ---------------- 音声の定数 ----------------
static const int SR = 44100;
static const int CHUNK = 2205;  // 50ms
static const float MASTER_GAIN = 0.6f;
static int16_t s_bufA[CHUNK];
static int16_t s_bufB[CHUNK];
static bool s_useA = true;

// UI → 音声タスクへの指示 (Phase 3: ノートイベントキュー方式)
static NoteEvent g_evq[32];
static volatile int g_evHead = 0, g_evTail = 0;
static volatile int g_voiceMode = MODE_MONO;  // 発音モード (つまみ8短押しで変更)
static volatile float g_cmdBend = 0.0f;  // ピッチベンド (半音単位, ±2)

// ボイスプール (音声タスクが所有)
static Voice s_voices[5];
static uint32_t s_ageCounter = 0;
// フィルタ係数のブロック率補間用 (3r2: powf を毎サンプル呼ばない)
static float s_fcCoef[5] = {0, 0, 0, 0, 0};
static float s_fcStep[5] = {0, 0, 0, 0, 0};

// 現在のパラメータ (loop 側で書き、音声タスクがチャンク毎にコピー)
static Params g_params = PRESETS[3];  // 起動時は SAW LEAD
static int g_preset = 3;              // PRESETS のインデックス (0..7)
static int g_bank = 0;                // 0=A, 1=B (トグルスイッチで切替)

// ---------------- 8Encoder ドライバ ----------------
static const uint8_t ENC_ADDR = 0x41;
static const int ENC_SDA = 53, ENC_SCL = 54;
static const uint32_t ENC_QUIET_MS = 10000;  // 電源投入後の無通信時間
static const uint32_t ENC_POLL_MS = 60;
enum EncState { ENC_WARM = 0, ENC_RUN = 1 };
static EncState g_encState = ENC_WARM;
static uint32_t g_encPowerMs = 0;   // ユニットに電源を入れた時刻
static uint32_t g_encTryMs = 0;     // 次に初期化を試みてよい時刻
static int g_encInitFails = 0;      // 初期化の連続失敗回数
static uint32_t g_encPollMs = 0;
static int g_encFails = 0;
static int32_t g_encPrev[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static int32_t g_encAcc[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static uint8_t g_swRaw = 0;         // 0x60 の生値 (表示・診断用)
static int g_encWaitShown = -1;     // ウォームアップ表示の秒 (描画節約)
static uint8_t g_encLastErr = 0;    // 直近トランザクションの err (診断用)
static int g_encLastGot = 0;        // 直近読み取りの受信バイト数 (診断用)
static char g_encDiag[80] = "ENC: -";  // 画面に出す診断文字列
static volatile bool g_encUiDirty = true;  // loop() への表示更新依頼

// ---------------- 鍵盤の定数 (Phase 1r2 と同一) ----------------
static const uint8_t WHITES[36] = {
  36,38,40,41,43,45,47, 48,50,52,53,55,57,59, 60,62,64,65,67,69,71,
  72,74,76,77,79,81,83, 84,86,88,89,91,93,95, 96
};
static const int VIS_WHITES = 12;
static const int KEY_X = 100, KEY_Y = 200, KEY_W = 96, KEY_H = 320;
static const int BLK_W = 56, BLK_H = 200;
static const int STRIP_X = 150, STRIP_Y = 580, STRIP_W = 980, STRIP_H = 48;
static const int BEND_X0 = 0, BEND_X1 = 90;
static const int BEND_Y0 = 150, BEND_Y1 = 650;
static const float BEND_RANGE = 2.0f;

static int g_keyWin = 7;
static ActiveKey g_activeKeys[10];
static uint32_t g_pressSeq = 0;
static int g_bendId = -1;
static float g_bend = 0.0f;
static int g_curNote = -1;

// 音量スライダー (画面右下。設計メモ: VOLUME は画面側)
static const int VOL_X = 480, VOL_Y = 660, VOL_H = 60;  // スライダー領域の左端 (3r7)
static const int PB_Y = 20, PB_H = 34, PB_W = 140;   // バンク切替ボタンの大きさ (4r)
static const int PB1_X = 690, PB2_X = 840;           // SAMPLE / USER ボタンの左端 (4r)
static const int SL1_X = 480, SL2_X = 890, SL_BAR_W = 380;  // VOL / REV のバー (3r7)
static const int SL_MID_X = 875;  // VOL/REV タッチ判定の境界 (3r7)
static int g_volume = 110;
static int g_volId = -1;
static int g_revId = -1;  // REV スライダーを掴んでいるタッチ ID (3r5)

// 起動スプラッシュ (3r): true になるまで鍵盤画面を出さない
static bool g_booted = false;
static uint32_t g_splashStartMs = 0;

// ---------------- setup / loop ----------------
void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);  // 横向き 1280x720 を明示 (2r: 落とすと縦で切れる)
  // 8Encoder へ先に電源だけ入れる (ユニットの起動時計はここから)。
  // I2C は ENC_QUIET_MS の間一切触らない (encTask 内で管理)。
  M5.Power.setExtOutput(true);
  g_encPowerMs = millis();
  g_encTryMs = g_encPowerMs + ENC_QUIET_MS;
  M5.Speaker.begin();
  M5.Speaker.setVolume(g_volume);
  // Phase 5: サンプル用バッファ (PSRAM 優先、4 秒 @16kHz)
  g_smpBuf = (int16_t*)heap_caps_malloc(SMP_N * sizeof(int16_t),
                                        MALLOC_CAP_SPIRAM);
  if (!g_smpBuf) g_smpBuf = (int16_t*)malloc(SMP_N * sizeof(int16_t));
  Serial.printf("[P5] sample buffer: %s\n", g_smpBuf ? "OK" : "FAILED");
  sdLoadUserBank();  // Phase 4: SD の USER バンクを読み込む (無ければ全て未保存)
  sdLoadSampleWav();  // 5r6: SD の /sample.wav があれば読み込む
  drawSplash();  // 3r: 最初はタイトル画面。鍵盤は ENC READY の後に出す
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 5, nullptr, 1);
  // エンコーダは専用タスク (core 0) で処理し、loop() を軽く保つ (Phase 3)
  xTaskCreatePinnedToCore(encTask, "enc", 4096, nullptr, 1, nullptr, 0);
  Serial.println("Phase5r7: playRaw buffer-release wait + ch0 fixed.");
}

void loop() {
  M5.update();
  if (!g_booted) {
    updateSplash();
  } else {
    handlePoints();
    consumeUiUpdates();
    consumePhase4();
    if (g_recording) {  // 録音中は経過表示を定期更新 (Phase 4b)
      static uint32_t lastRecDraw = 0;
      if (millis() - lastRecDraw >= 250) {
        lastRecDraw = millis();
        drawTransportStatus();
      }
    }
  }
  delay(5);
}

// ステータス行への一時表示 (トースト)。文字列はコピーする
static void setToast(const char* s, uint32_t durMs) {
  snprintf(g_toast, sizeof(g_toast), "%s", s);
  g_toastUntil = millis() + durMs;
  g_encUiDirty = true;
}

// エンコーダ側からの表示更新依頼をまとめて反映 (描画は loop() のみ)
static void consumeUiUpdates(void) {
  if (!g_encUiDirty) return;
  g_encUiDirty = false;
  drawTop();
  drawParams(false);
  drawStatus();
}

// Phase 4: 保存要求の実行 (SD 書き込みは loop() 側でのみ行う)
static void doSavePreset(int slot) {
  if (!g_sdReady && !initSD()) {
    setToast("NO SD CARD", 2500);
    Serial.println("[P4] save failed: no SD");
    return;
  }
  g_sdReady = true;
  g_userBank[slot] = g_params;
  g_userSaved[slot] = true;
  if (sdWriteUserBank()) {
    char b[32];
    snprintf(b, sizeof(b), "SAVED -> USER %d", slot + 1);
    setToast(b, 2500);
    Serial.printf("[P4] saved USER %d\n", slot + 1);
  } else {
    setToast("SAVE FAILED", 2500);
    Serial.println("[P4] save failed: write error");
  }
  g_encUiDirty = true;
}

// Phase 4: 保存要求とトースト消去を loop() から処理する
static void consumePhase4(void) {
  if (g_saveReq >= 0) {
    int slot = g_saveReq;
    g_saveReq = -1;
    doSavePreset(slot);
  }
  static bool toastWas = false;
  bool toastOn = (g_toastUntil != 0) && ((int32_t)(g_toastUntil - millis()) > 0);
  if (toastWas && !toastOn) drawStatus();
  toastWas = toastOn;
}

// ---- ABC 記譜法テキスト (tab5_synth Phase 9 の変換を移植) ----
static String abcNoteName(int midi) {
  static const char letters[12] = {'C','C','D','D','E','F','F','G','G','A','A','B'};
  static const bool isSharp[12] = {false,true,false,true,false,false,true,
                                   false,true,false,true,false};
  int pc = midi % 12;
  int oct = midi / 12 - 1;  // C4 = 60
  String str = isSharp[pc] ? "^" : "";
  if (oct <= 4) {
    str += letters[pc];
    for (int i = 0; i < 4 - oct; ++i) str += ',';
  } else {
    str += (char)(letters[pc] - 'A' + 'a');
    for (int i = 0; i < oct - 5; ++i) str += '\'';
  }
  return str;
}

struct AbcSeg {  // 小節内に収まる音の断片
  int bar, start, len;        // 小節番号, 小節内の開始ステップ, 長さ (16分単位)
  std::vector<int> pitches;   // 和音は複数
  bool tieOut;                // 次の断片へタイでつながる
};

static String buildAbcText(uint32_t recDurMs) {
  // 記録イベント列 → 音符区間 (開始 ms, 長さ ms, 音高)
  struct Iv { uint32_t start, dur; int note; };
  static Iv ivs[2048];
  int ivN = 0;
  static int32_t openMs[128];
  for (int i = 0; i < 128; i++) openMs[i] = -1;
  auto closeIv = [&](int note, uint32_t endMs) {
    if (openMs[note] < 0) return;
    if (ivN < 2048) {
      ivs[ivN].start = (uint32_t)openMs[note];
      ivs[ivN].dur = (endMs > (uint32_t)openMs[note])
                         ? (endMs - (uint32_t)openMs[note]) : 0;
      ivs[ivN].note = note;
      ivN++;
    }
    openMs[note] = -1;
  };
  const bool monoLike = (g_voiceMode != MODE_POLY);
  for (int i = 0; i < g_recEvN; i++) {
    const RecEv& e = g_recEv[i];
    if (e.type == EV_ON) {
      if (monoLike) {  // 単音系モードでは鳴っているのは常に 1 音
        for (int n = 0; n < 128; n++) if (n != e.note) closeIv(n, e.t);
      }
      if (openMs[e.note] >= 0) closeIv(e.note, e.t);  // 同音の再打鍵
      openMs[e.note] = (int32_t)e.t;
    } else {
      closeIv(e.note, e.t);
    }
  }
  for (int n = 0; n < 128; n++) closeIv(n, recDurMs);
  if (ivN == 0) return "";

  // BPM 基準で 16 分ステップへ丸める
  const double stepMs = 60000.0 / (double)g_bpm / 4.0;
  static int ivStep[2048], ivLen[2048];
  int maxEnd = 0;
  for (int i = 0; i < ivN; i++) {
    ivStep[i] = (int)(ivs[i].start / stepMs + 0.5);
    ivLen[i] = (int)(ivs[i].dur / stepMs + 0.5);
    if (ivLen[i] < 1) ivLen[i] = 1;
    if (ivStep[i] + ivLen[i] > maxEnd) maxEnd = ivStep[i] + ivLen[i];
  }
  int totalSteps = ((maxEnd + 15) / 16) * 16;
  if (totalSteps > 4096) totalSteps = 4096;
  if (totalSteps == 0) return "";
  const int bars = totalSteps / 16;

  // 開始ステップごとに音をまとめる (和音対応、長さは最長、重複音高は除く)
  std::vector<std::vector<int>> atStep(totalSteps);
  std::vector<int> lenStep(totalSteps, 0);
  for (int i = 0; i < ivN; i++) {
    int st = ivStep[i];
    if (st >= totalSteps) continue;
    bool dup = false;
    for (int p : atStep[st]) if (p == ivs[i].note) { dup = true; break; }
    if (!dup) atStep[st].push_back(ivs[i].note);
    if (ivLen[i] > lenStep[st]) lenStep[st] = ivLen[i];
  }

  // 小節境界で分割してタイをつける
  std::vector<AbcSeg> segs;
  for (int st = 0; st < totalSteps; ++st) {
    if (atStep[st].empty()) continue;
    std::vector<int> ps = atStep[st];
    std::sort(ps.begin(), ps.end());
    int pos = st, remain = lenStep[st];
    if (pos + remain > totalSteps) remain = totalSteps - pos;
    while (remain > 0) {
      int bar = pos / 16;
      int inBar = pos % 16;
      int take = remain;
      if (inBar + take > 16) take = 16 - inBar;
      segs.push_back({bar, inBar, take, ps, remain > take});
      pos += take;
      remain -= take;
    }
  }

  String out = "X:1\nT:tab5_analog phrase\nM:4/4\nL:1/16\nQ:1/4=" +
               String(g_bpm) + "\nK:C\n";
  size_t si = 0;
  for (int b = 0; b < bars; ++b) {
    int cur = 0;
    String line;
    while (si < segs.size() && segs[si].bar == b) {
      const AbcSeg& sg = segs[si++];
      if (sg.start > cur) {
        if (line.length()) line += ' ';
        line += 'z';
        if (sg.start - cur > 1) line += String(sg.start - cur);
        cur = sg.start;
      }
      if (line.length()) line += ' ';
      if (sg.pitches.size() == 1) line += abcNoteName(sg.pitches[0]);
      else {
        line += '[';
        for (int pch : sg.pitches) line += abcNoteName(pch);
        line += ']';
      }
      if (sg.len > 1) line += String(sg.len);
      if (sg.tieOut) line += '-';
      cur = sg.start + sg.len;
    }
    if (cur < 16) {
      if (line.length()) line += ' ';
      line += 'z';
      if (16 - cur > 1) line += String(16 - cur);
    }
    out += line;
    out += (b == bars - 1) ? " |]\n" : " | ";
    if (b % 2 == 1 && b != bars - 1) out += '\n';
  }
  return out;
}

// 戻り値: 0以上=書き込みバイト数, -1=SDなし, -2=作成失敗, -3=連番満杯, -4=音なし
static int exportPhraseAbc(uint32_t recDurMs) {
  if (!g_sdReady && !initSD()) return -1;
  g_sdReady = true;
  String text = buildAbcText(recDurMs);
  if (text.length() == 0) return -4;
  bool found = false;
  for (int i = 1; i <= 99; i++) {
    if (i == 1) snprintf(g_lastAbcName, sizeof(g_lastAbcName), "/phrase.abc");
    else snprintf(g_lastAbcName, sizeof(g_lastAbcName), "/phrase%d.abc", i);
    if (!g_fs->exists(g_lastAbcName)) { found = true; break; }
  }
  if (!found) return -3;
  File f = g_fs->open(g_lastAbcName, FILE_WRITE);
  if (!f) return -2;
  size_t w = f.print(text);
  f.close();
  return (int)w;
}

// ---- トランスポート帯の描画 ----
static void drawTransportStatus(void) {
  M5.Display.fillRect(520, TR_Y - 2, 600, TR_H + 4, TFT_BLACK);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(2);
  if (g_recording) {
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);
    char b[48];
    float sec = (millis() - g_recStartMs) / 1000.0f;
    snprintf(b, sizeof(b), "REC %.1fs  EV %d%s", sec, g_recEvN,
             g_recOverflow ? " FULL" : "");
    M5.Display.drawString(b, 520, TR_Y + 13);
  } else {
    M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Display.drawString("phrase: REC -> play -> STOP = ABC to SD", 520, TR_Y + 13);
  }
}

static void drawTransport(void) {
  M5.Display.fillRect(100, TR_Y - 2, 420, TR_H + 4, TFT_BLACK);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(2);
  if (g_recording) {
    M5.Display.fillRect(REC_BTN_X, TR_Y, REC_BTN_W, TR_H, TFT_RED);
    M5.Display.fillRect(REC_BTN_X + 16, TR_Y + 15, 14, 14, TFT_WHITE);
    M5.Display.setTextColor(TFT_WHITE, TFT_RED);
    M5.Display.drawString("STOP", REC_BTN_X + 44, TR_Y + 13);
  } else {
    M5.Display.drawRect(REC_BTN_X, TR_Y, REC_BTN_W, TR_H, TFT_WHITE);
    M5.Display.fillCircle(REC_BTN_X + 23, TR_Y + TR_H / 2, 8, TFT_RED);
    M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    M5.Display.drawString("REC", REC_BTN_X + 44, TR_Y + 13);
  }
  // BPM (記譜の基準。- / + で 5 ずつ、40..240)
  M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  M5.Display.drawString("BPM", 252, TR_Y + 13);
  M5.Display.drawRect(BPM_M_X, TR_Y, BPM_BTN_W, TR_H, TFT_WHITE);
  M5.Display.drawString("-", BPM_M_X + 19, TR_Y + 13);
  char bb[8];
  snprintf(bb, sizeof(bb), "%d", g_bpm);
  M5.Display.drawString(bb, 368, TR_Y + 13);
  M5.Display.drawRect(BPM_P_X, TR_Y, BPM_BTN_W, TR_H, TFT_WHITE);
  M5.Display.drawString("+", BPM_P_X + 19, TR_Y + 13);
  // MIC ボタン (Phase 5: サンプリング録音)
  M5.Display.fillRect(MIC_X, TR_Y - 2, MIC_W, TR_H + 4, TFT_BLACK);
  if (g_micRec) {
    M5.Display.fillRect(MIC_X, TR_Y, MIC_W, TR_H, TFT_RED);
    M5.Display.setTextColor(TFT_WHITE, TFT_RED);
    M5.Display.drawString("REC...", MIC_X + 30, TR_Y + 13);
  } else if (g_sampleReady) {
    M5.Display.fillRect(MIC_X, TR_Y, MIC_W, TR_H, TFT_DARKGREEN);
    M5.Display.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    M5.Display.drawString("MIC OK", MIC_X + 33, TR_Y + 13);
  } else {
    M5.Display.drawRect(MIC_X, TR_Y, MIC_W, TR_H, TFT_WHITE);
    M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    M5.Display.drawString("MIC REC", MIC_X + 24, TR_Y + 13);
  }
  drawTransportStatus();
}

static void toggleRec(void) {
  if (!g_recording) {
    g_recEvN = 0;
    g_recOverflow = false;
    g_recStartMs = millis();
    g_recording = true;
    Serial.println("[P4B] REC start");
  } else {
    g_recording = false;
    uint32_t dur = millis() - g_recStartMs;
    int r = exportPhraseAbc(dur);
    if (r >= 0) {
      char b[48];
      snprintf(b, sizeof(b), "ABC SAVED: %s", g_lastAbcName + 1);
      setToast(b, 3000);
    } else if (r == -1) setToast("NO SD CARD", 3000);
    else if (r == -4) setToast("NO NOTES RECORDED", 3000);
    else if (r == -3) setToast("ABC FILES FULL", 3000);
    else setToast("ABC SAVE FAILED", 3000);
    Serial.printf("[P4B] REC stop, export=%d\n", r);
  }
  drawTransport();
}

// ---- Phase 5: サンプル録音と WAV 保存 ----
// 手順は単体テストで実機確認済みの往復 (Speaker.end → Mic →
// Speaker.begin)。音声タスクは g_micPause で送出を止めてから
// 切り替える。録音中は loop が約 4.5 秒ブロックする。
static bool saveSampleWav(void) {
  if (!g_sdReady && !initSD()) return false;
  g_sdReady = true;
  g_fs->remove("/sample.wav");  // FILE_WRITE は追記なので先に消す
  File f = g_fs->open("/sample.wav", FILE_WRITE);
  if (!f) return false;
  const uint32_t dataSize = (uint32_t)g_smpLen * 2;
  uint8_t hdr[44] = {0};
  auto put32 = [&](int off, uint32_t v) {
    hdr[off] = v & 0xFF; hdr[off + 1] = (v >> 8) & 0xFF;
    hdr[off + 2] = (v >> 16) & 0xFF; hdr[off + 3] = (v >> 24) & 0xFF;
  };
  auto put16 = [&](int off, uint16_t v) {
    hdr[off] = v & 0xFF; hdr[off + 1] = (v >> 8) & 0xFF;
  };
  memcpy(hdr, "RIFF", 4);
  put32(4, 36 + dataSize);
  memcpy(hdr + 8, "WAVE", 4);
  memcpy(hdr + 12, "fmt ", 4);
  put32(16, 16); put16(20, 1); put16(22, 1);  // PCM, mono
  const uint32_t wavRate = (uint32_t)(g_smpEffRate + 0.5f);
  put32(24, wavRate); put32(28, wavRate * 2);
  put16(32, 2); put16(34, 16);
  memcpy(hdr + 36, "data", 4);
  put32(40, dataSize);
  f.write(hdr, 44);
  f.write((const uint8_t*)g_smpBuf, dataSize);
  f.close();
  return true;
}

// 録音/読込したバッファを再生専用の 44.1kHz バッファへ変換 (5r6)
static void buildSmp44(void) {
  g_smp44Len = 0;
  if (!g_smpBuf || g_smpLen <= 0) return;
  if (!g_smp44) {
    g_smp44 = (int16_t*)heap_caps_malloc(SMP44_MAX * sizeof(int16_t),
                                         MALLOC_CAP_SPIRAM);
    if (!g_smp44) g_smp44 = (int16_t*)malloc(SMP44_MAX * sizeof(int16_t));
    if (!g_smp44) { Serial.println("[P5] smp44 alloc FAILED"); return; }
  }
  int64_t n = (int64_t)g_smpLen * (int64_t)SR /
              (int64_t)(g_smpEffRate + 0.5f);
  if (n > SMP44_MAX) n = SMP44_MAX;
  float step = g_smpEffRate / (float)SR;
  for (int i = 0; i < (int)n; i++) {
    float pos = (float)i * step;
    int i0 = (int)pos;
    if (i0 >= g_smpLen - 1) { g_smp44[i] = g_smpBuf[g_smpLen - 1]; continue; }
    float fr2 = pos - (float)i0;
    float a = (float)g_smpBuf[i0], b = (float)g_smpBuf[i0 + 1];
    g_smp44[i] = (int16_t)(a + (b - a) * fr2);
  }
  g_smp44Len = (int)n;
  Serial.printf("[P5] smp44 built: len=%d (from %d @ %.0f)\n",
                g_smp44Len, (int)g_smpLen, g_smpEffRate);
}

// 起動時: SD に /sample.wav があれば読み込んで使えるようにする (5r6)
static bool sdLoadSampleWav(void) {
  if (!g_smpBuf) return false;
  if (!g_sdReady && !initSD()) return false;
  g_sdReady = true;
  File f = g_fs->open("/sample.wav", FILE_READ);
  if (!f) { Serial.println("[P5] no /sample.wav on SD"); return false; }
  uint8_t hdr[12];
  if (f.read(hdr, 12) != 12 || memcmp(hdr, "RIFF", 4) != 0 ||
      memcmp(hdr + 8, "WAVE", 4) != 0) {
    f.close();
    Serial.println("[P5] sample.wav: not a WAV");
    return false;
  }
  uint16_t fmt = 0, ch = 0, bits = 0;
  uint32_t rate = 0, dataPos = 0, dataSize = 0;
  while (f.position() + 8 <= f.size()) {
    uint8_t chdr[8];
    if (f.read(chdr, 8) != 8) break;
    uint32_t csize = (uint32_t)chdr[4] | ((uint32_t)chdr[5] << 8) |
                     ((uint32_t)chdr[6] << 16) | ((uint32_t)chdr[7] << 24);
    if (memcmp(chdr, "fmt ", 4) == 0) {
      uint8_t fb[16];
      if (f.read(fb, 16) != 16) break;
      fmt = (uint16_t)(fb[0] | (fb[1] << 8));
      ch = (uint16_t)(fb[2] | (fb[3] << 8));
      rate = (uint32_t)fb[4] | ((uint32_t)fb[5] << 8) |
             ((uint32_t)fb[6] << 16) | ((uint32_t)fb[7] << 24);
      bits = (uint16_t)(fb[14] | (fb[15] << 8));
      if (csize > 16) f.seek(f.position() + (csize - 16));
    } else if (memcmp(chdr, "data", 4) == 0) {
      dataPos = f.position();
      dataSize = csize;
      break;
    } else {
      f.seek(f.position() + csize + (csize & 1));
    }
  }
  if (fmt != 1 || bits != 16 || (ch != 1 && ch != 2) ||
      rate < 4000 || rate > 48000 || dataSize == 0) {
    f.close();
    Serial.printf("[P5] sample.wav: unsupported fmt=%u ch=%u bits=%u rate=%lu\n",
                  fmt, ch, bits, (unsigned long)rate);
    return false;
  }
  int frames = (int)(dataSize / (2u * (uint32_t)ch));
  if (frames > SMP_N) frames = SMP_N;
  f.seek(dataPos);
  if (ch == 1) {
    f.read((uint8_t*)g_smpBuf, (size_t)frames * 2);
  } else {
    static int16_t tmp[2048];
    int done = 0;
    while (done < frames) {
      int want = frames - done;
      if (want > 1024) want = 1024;
      int got = (int)f.read((uint8_t*)tmp, (size_t)want * 4) / 4;
      if (got <= 0) { frames = done; break; }
      for (int i = 0; i < got; i++) {
        g_smpBuf[done + i] =
            (int16_t)(((int32_t)tmp[i * 2] + tmp[i * 2 + 1]) / 2);
      }
      done += got;
    }
  }
  f.close();
  if (frames <= 0) return false;
  g_smpLen = frames;
  g_smpEffRate = (float)rate;
  g_sampleReady = true;
  buildSmp44();
  Serial.printf("[P5] sample.wav loaded: len=%d rate=%lu ch=%u\n",
                frames, (unsigned long)rate, ch);
  return true;
}

static void doMicRecord(void) {
  if (!g_smpBuf) { setToast("MIC: NO BUFFER", 2500); return; }
  g_micRec = true;
  drawTransport();   // REC... 表示を先に出す
  g_micPause = true; // 音声タスクの送出を止める
  delay(250);        // 送出中チャンクの完了待ち
  M5.Speaker.end();
  delay(150);
  M5.Mic.begin();
  uint32_t recT0 = millis();
  bool ok = M5.Mic.record(g_smpBuf, SMP_N, SMP_RATE);
  if (ok) {
    while (M5.Mic.isRecording() && (millis() - recT0) < 8000) delay(20);
  }
  uint32_t recMs = millis() - recT0;
  M5.Mic.end();
  if (ok && recMs > 100) {  // 実測レートを逆算して再生に使う (5r)
    float eff = (float)SMP_N * 1000.0f / (float)recMs;
    if (eff < 4000.0f) eff = 4000.0f;
    if (eff > 192000.0f) eff = 192000.0f;
    g_smpEffRate = eff;
  }
  int32_t peak = 0;
  for (int i = 0; i < SMP_N; i++) {
    int32_t v = g_smpBuf[i];
    if (v < 0) v = -v;
    if (v > peak) peak = v;
  }
  // 5r3: 録音レベルを正規化 (ピークを 30000 へ、最大 x8)。静かな
  // 録音でも鍵盤再生で小さい音節が埋もれないようにする
  if (peak >= 500 && peak < 30000) {
    float g = 30000.0f / (float)peak;
    if (g > 8.0f) g = 8.0f;
    if (g > 1.05f) {
      for (int i = 0; i < SMP_N; i++) {
        float s = g_smpBuf[i] * g;
        if (s > 32767.0f) s = 32767.0f;
        if (s < -32767.0f) s = -32767.0f;
        g_smpBuf[i] = (int16_t)s;
      }
    }
  }
  // 末尾の無音をトリム (ループの空白を短くする)。末尾に 0.1 秒残す
  int last = SMP_N - 1;
  while (last > 0 && g_smpBuf[last] < 800 && g_smpBuf[last] > -800) last--;
  int len = last + 1600;
  if (len > SMP_N) len = SMP_N;
  if (len < 1600) len = 1600;
  g_smpLen = len;
  M5.Speaker.begin();
  M5.Speaker.setVolume((uint8_t)g_volume);
  g_micRec = false;
  if (!ok) {
    setToast("MIC REC FAILED", 3000);
  } else if (peak < 500) {
    g_sampleReady = false;
    setToast("MIC: TOO QUIET", 3000);
  } else {
    g_sampleReady = true;
    buildSmp44();  // 5r6: 再生用 44.1kHz バッファを先に作る
    bool wav = saveSampleWav();
    // 5r 切り分け: 単体テストと同じ直接再生を 1 回だけ行う
    setToast("DIRECT PLAY (CHECK SPEED)", 2500);
    drawStatus();
    M5.Speaker.playRaw(g_smpBuf, g_smpLen,
                       (uint32_t)(g_smpEffRate + 0.5f), false, 1, 0);
    uint32_t pt0 = millis();
    while (M5.Speaker.isPlaying() && (millis() - pt0) < 8000) delay(20);
    setToast(wav ? "SAMPLE OK + WAV SAVED" : "SAMPLE RECORDED", 3000);
  }
  g_micPause = false;
  Serial.printf("[P5] mic rec ok=%d peak=%ld len=%d recMs=%lu effRate=%.0f\n",
                ok, (long)peak, (int)g_smpLen, (unsigned long)recMs,
                g_smpEffRate);
  drawTransport();
}

// ---------------- パラメータ操作 ----------------
static float* fieldPtr(Params& p, int f) {
  switch (f) {
    case F_CUTOFF: return &p.cutoff;
    case F_RESO: return &p.reso;
    case F_FENV: return &p.fEnv;
    case F_DECAY: return &p.decay;
    case F_LFORATE: return &p.lfoRate;
    case F_LFODEPTH: return &p.lfoDepth;
    case F_GLIDE: return &p.glide;
    case F_WAVE: return &p.wave;
    case F_DETUNE: return &p.detune;
    case F_OSCMIX: return &p.oscMix;
    case F_PWM: return &p.pwm;
    case F_ATTACK: return &p.attack;
    case F_RELEASE: return &p.release;
    case F_NOISE: return &p.noise;
  }
  return &p.cutoff;
}

static void loadPreset(int idx) {
  g_preset = idx;
  if (g_pbank == 1 && g_userSaved[idx]) {
    g_params = g_userBank[idx];
    Serial.printf("[P4] PRESET USER %d %s (saved)\n", idx + 1, PRESET_NAMES[idx]);
  } else {
    g_params = PRESETS[idx];
    Serial.printf("[P4] PRESET %s %d %s\n", g_pbank ? "USER" : "SAMPLE",
                  idx + 1, PRESET_NAMES[idx]);
  }
  g_encUiDirty = true;
}

// つまみ ch (0..7) が clicks クリック分回った
static void applyKnob(int ch, int clicks) {
  if (ch == 7) {  // PRESET (両バンク共通): 1クリックで 1 つ送る
    int np = (g_preset + clicks) % 8;
    if (np < 0) np += 8;
    if (np != g_preset) loadPreset(np);
    return;
  }
  int f = BANK_FIELDS[g_bank][ch];
  float step = (f == F_WAVE) ? 0.04f : (1.0f / 60.0f);
  float* fp = fieldPtr(g_params, f);
  float nv = *fp + clicks * step;
  if (nv < 0.0f) nv = 0.0f;
  if (nv > 1.0f) nv = 1.0f;
  if (nv != *fp) {
    *fp = nv;
    Serial.printf("[P2] %s = %.2f\n", FIELD_NAMES[f], nv);
    g_encUiDirty = true;
  }
}

// ---------------- 8Encoder 低レベル (0r9 の実証方式) ----------------
static bool encWriteBlk(uint8_t reg, const uint8_t* data, size_t len) {
  Wire.beginTransmission(ENC_ADDR);
  Wire.write(reg);
  for (size_t i = 0; i < len; i++) Wire.write(data[i]);
  uint8_t err = Wire.endTransmission(true);
  g_encLastErr = err;
  return err == 0;
}

// STOP-start 読み: レジスタ指定 → STOP → 待つ → 別途 requestFrom
static bool encReadSS(uint8_t reg, uint8_t* data, size_t len) {
  Wire.beginTransmission(ENC_ADDR);
  Wire.write(reg);
  uint8_t err = Wire.endTransmission(true);  // STOP を打つ (リピーテッドスタート不可)
  g_encLastErr = err;
  if (err != 0) {
    g_encLastGot = 0;
    return false;
  }
  delay(10);  // スレーブ側が送信データを用意する猶予 (0r9 で実証した値)
  size_t got = Wire.requestFrom((int)ENC_ADDR, (int)len);
  g_encLastGot = (int)got;
  if (got != len) return false;
  for (size_t i = 0; i < len; i++) data[i] = Wire.read();
  return true;
}

static int32_t encLe32(const uint8_t* b) {
  return (int32_t)((uint32_t)b[0] | ((uint32_t)b[1] << 8) |
                   ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24));
}

// カウンタ 8ch 分を読む。CH 別 4 バイト読みのみ (0r9 で実証済みの形)。
// 32 バイト一括読みは 2r4 で廃止 (バスを詰まらせる疑い)。CH 間にも
// 間隔を入れ、0r9 の緩やかなトランザクション間隔に寄せる。
static bool encReadCounters(int32_t out[8]) {
  for (int ch = 0; ch < 8; ch++) {
    uint8_t b[4];
    if (!encReadSS((uint8_t)(ch * 4), b, 4)) return false;
    out[ch] = encLe32(b);
    if (ch < 7) delay(20);
  }
  return true;
}

static void encBusRecover(void) {
  Wire.end();
  pinMode(ENC_SDA, INPUT_PULLUP);
  pinMode(ENC_SCL, OUTPUT);
  for (int i = 0; i < 9; i++) {
    digitalWrite(ENC_SCL, HIGH);
    delayMicroseconds(5);
    digitalWrite(ENC_SCL, LOW);
    delayMicroseconds(5);
  }
  pinMode(ENC_SDA, OUTPUT);
  digitalWrite(ENC_SDA, LOW);
  digitalWrite(ENC_SCL, HIGH);
  delayMicroseconds(5);
  digitalWrite(ENC_SDA, HIGH);
  delayMicroseconds(5);
  Wire.begin(ENC_SDA, ENC_SCL, 100000);
  Wire.setTimeOut(50);
  Serial.println("[P2ENC] bus recover");
}

static void encWriteLEDs(void) {
  uint8_t rgb[27];
  // バンク色: A=オレンジ, B=青 (9 個の LED 全部)
  uint8_t r = (g_bank == 0) ? 200 : 0;
  uint8_t g = (g_bank == 0) ? 80 : 50;
  uint8_t b = (g_bank == 0) ? 0 : 200;
  for (int i = 0; i < 9; i++) {
    rgb[i * 3] = r;
    rgb[i * 3 + 1] = g;
    rgb[i * 3 + 2] = b;
  }
  if (!encWriteBlk(0x70, rgb, 27)) Serial.println("[P2ENC] LED write failed");
}

// 診断文字列を画面ステータス行 (2 行目) に出す
static void setEncDiag(const char* s) {
  snprintf(g_encDiag, sizeof(g_encDiag), "%s", s);
  g_encUiDirty = true;  // Phase 3: 描画は loop() が行う
}

// ユニットを静かな待機状態に戻す。cyclePower=true なら電源も入れ直す
static void encRestart(bool cyclePower, uint32_t quietMs) {
  Wire.end();
  if (cyclePower) {
    M5.Power.setExtOutput(false);
    delay(2000);
    M5.Power.setExtOutput(true);
  }
  g_encPowerMs = millis();
  g_encTryMs = g_encPowerMs + quietMs;
  g_encFails = 0;
  g_encState = ENC_WARM;
  g_encWaitShown = -1;
  Serial.printf("[P2ENC] restart: quiet %lu ms (power cycle=%d)\n",
                (unsigned long)quietMs, cyclePower ? 1 : 0);
  g_encUiDirty = true;
}

// 初期化を 1 回だけ試みる。失敗したら静かな時間を延ばして再挑戦する
static void encTryInit(void) {
  char msg[96];
  Wire.begin(ENC_SDA, ENC_SCL, 100000);
  Wire.setTimeOut(50);
  // 最初の接触は 0r9 と同じく LED 書き込みから (読みより先に書き込みを試す)
  const uint8_t red[3] = {255, 0, 0};
  bool wOk = encWriteBlk(0x70, red, 3);
  uint8_t wErr = g_encLastErr;
  Serial.printf("[P2ENC] INIT#%d LEDW err=%u -> %s\n", g_encInitFails + 1,
                wErr, wOk ? "OK" : "NG");
  delay(50);  // 書き込みの直後には読みを始めない (0r9 には常に間隔があった)
  int32_t seed[8];
  bool ok = encReadCounters(seed);
  snprintf(msg, sizeof(msg), "INIT#%d W err=%u RD got=%d err=%u",
           g_encInitFails + 1, wErr, g_encLastGot, g_encLastErr);
  setEncDiag(msg);
  Serial.printf("[P2ENC] %s -> seed %s\n", msg, ok ? "OK" : "NG");
  if (!ok) encBusRecover();  // 失敗した読みで詰まったバスをほぐしてから待つ
  if (!ok) {
    g_encInitFails++;
    Serial.printf("[P2ENC] init failed (%d)\n", g_encInitFails);
    if (g_encInitFails == 1) {
      encRestart(false, 15000);  // 電源は入れたまま、静かな時間を延ばす
    } else {
      encRestart(true, 20000);   // 電源を入れ直して 20 秒静かに待つ
    }
    return;
  }
  for (int ch = 0; ch < 8; ch++) {
    g_encPrev[ch] = seed[ch];
    g_encAcc[ch] = 0;
  }
  uint8_t swb = 0;
  if (encReadSS(0x60, &swb, 1)) {
    g_swRaw = swb;
    g_bank = (swb & 0x01) ? 1 : 0;
  }
  g_encFails = 0;
  g_encInitFails = 0;
  g_encState = ENC_RUN;
  uint8_t fwv = 0;
  if (encReadSS(0xF0, &fwv, 1)) {
    Serial.printf("[P2ENC] FW version (0xF0) = %u\n", fwv);
  }
  encWriteLEDs();
  Serial.printf("[P2ENC] ready. bank=%c sw=0x%02X\n", g_bank ? 'B' : 'A', g_swRaw);
  snprintf(msg, sizeof(msg), "ENC READY");
  setEncDiag(msg);
  g_encUiDirty = true;
}

// つまみ 8 のボタン処理 (V1 プロトコル: 0x57 の 1 バイト値)。
// 短押し = 発音モード循環 (離した瞬間に確定)、長押し (900ms) =
// USER の同番号スロットへ保存 (loop へ要求)。バンク切替は
// 4r で画面ボタンに移行し、ダブルプッシュは廃止した。
// 0=押下と想定。極性が逆でも押→離の組で判定するので影響しない。
static const uint32_t BTN8_LONG_MS = 900;
static uint32_t s_b8PressMs = 0;
static bool s_b8Held = false;
static bool s_b8LongFired = false;

static void btn8CycleMode(void) {
  g_voiceMode = (g_voiceMode + 1) % 3;
  Serial.printf("[P4] MODE -> %s (knob8 short push)\n", MODE_NAMES[g_voiceMode]);
  g_encUiDirty = true;
}

static void encHandleBtn8(uint8_t v, uint32_t now) {
  static uint8_t prevV = 0xFF;
  if (v == prevV) return;
  Serial.printf("[P4] BTN8 byte=%u\n", v);
  bool pressed = (v == 0);
  bool wasPressed = (prevV == 0);
  if (pressed && !wasPressed) {
    s_b8Held = true;
    s_b8PressMs = now;
    s_b8LongFired = false;
  } else if (!pressed && wasPressed) {
    s_b8Held = false;
    if (!s_b8LongFired) btn8CycleMode();
  }
  prevV = v;
}

// ポーリングごとの時間判定 (長押し発火)
static void encBtn8Tick(uint32_t now) {
  if (s_b8Held && !s_b8LongFired && (now - s_b8PressMs) >= BTN8_LONG_MS) {
    s_b8LongFired = true;
    g_saveReq = g_preset;
    Serial.printf("[P4] SAVE REQ slot %d (knob8 long push)\n", g_preset + 1);
  }
}

static void encPollStep(void) {
  uint32_t now = millis();
  if (g_encState == ENC_WARM) {
    int remain = (int)(((int32_t)(g_encTryMs - now) + 999) / 1000);
    if (remain < 0) remain = 0;
    if ((int32_t)(now - g_encTryMs) >= 0) {
      encTryInit();
    } else if (remain != g_encWaitShown) {
      g_encWaitShown = remain;
      g_encUiDirty = true;
    }
    return;
  }
  uint32_t interval = ENC_POLL_MS;
  if (now - g_encPollMs < interval) return;
  g_encPollMs = now;
  encBtn8Tick(now);  // Phase 4: つまみ 8 の時間判定 (長押し/短押し確定)

  // ボタンは V1 プロトコル: 0x50+ch の 1 バイト値 (3r4)。
  // 0x62 のビットマスクは V2 専用なので読まない。
  uint8_t btn8 = 0;
  bool okB8 = encReadSS(0x57, &btn8, 1);
  if (okB8) encHandleBtn8(btn8, now);
  delay(20);
  int32_t cur8[8];
  uint8_t swb = 0;
  bool okC = encReadCounters(cur8);
  delay(20);
  bool okS = encReadSS(0x60, &swb, 1);
  if (!okC || !okS || !okB8) {
    char msg[96];
    g_encFails++;
    snprintf(msg, sizeof(msg), "POLL got=%d err=%u fails=%d", g_encLastGot,
             g_encLastErr, g_encFails);
    setEncDiag(msg);
    Serial.printf("[P2ENC] poll fail cnt=%d sw=%d btn8=%d (fails=%d)\n",
                  okC, okS, okB8, g_encFails);
    encBusRecover();
    if (g_encFails >= 5) encRestart(true, 20000);
    g_encUiDirty = true;
    return;
  }
  g_encFails = 0;

  // スイッチ → バンク (bit0 で判定。逆だったら SW 生値で分かる)
  g_swRaw = swb;
  int bank = (swb & 0x01) ? 1 : 0;
  if (bank != g_bank) {
    g_bank = bank;
    Serial.printf("[P2] BANK %c (SW raw 0x%02X)\n", bank ? 'B' : 'A', swb);
    encWriteLEDs();
    g_encUiDirty = true;
  }

  // カウンタ → クリック換算 (1クリック = ±2)
  for (int ch = 0; ch < 8; ch++) {
    int32_t cur = cur8[ch];
    int32_t d = cur - g_encPrev[ch];
    g_encPrev[ch] = cur;
    if (d == 0) continue;
    g_encAcc[ch] += d;
    int clicks = (int)(g_encAcc[ch] / 2);
    g_encAcc[ch] -= clicks * 2;
    if (clicks != 0) applyKnob(ch, clicks);
  }
}

// ---------------- エンコーダ専用タスク (Phase 3) ----------------
// 取引手順は 2r4 の実証どおり。loop() を占有しないよう分離した。
static void encTask(void* arg) {
  (void)arg;
  M5.Power.setExtOutput(true);
  g_encPowerMs = millis();
  g_encTryMs = g_encPowerMs + ENC_QUIET_MS;
  for (;;) {
    encPollStep();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ---------------- UI → 音声への指示 (イベントキュー) ----------------
static void pushEvent(int type, int note) {
  recCapture(type, note);  // Phase 4b: 録音中ならイベントを記録
  int h = g_evHead;
  int nh = (h + 1) & 31;
  if (nh == g_evTail) g_evTail = (g_evTail + 1) & 31;  // 満杯なら最古を捨てる
  g_evq[h].type = (int8_t)type;
  g_evq[h].note = (int8_t)note;
  g_evHead = nh;
}

// MONO / UNISON 用: 「今鳴らすべき 1 音」の開始と停止
static void synthNoteOn(int note) {
  pushEvent(EV_ON, note);
  g_curNote = note;
  drawStatus();
}
static void synthNoteOff(void) {
  pushEvent(EV_OFF, g_curNote);
  g_curNote = -1;
  drawStatus();
}

// ---------------- 描画: 上部 (タイトル + プリセット + パラメータ) ----------------
static void drawTop(void) {
  M5.Display.fillRect(0, 0, 1280, 56, TFT_BLACK);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  M5.Display.drawString("tab5_analog Phase 5r3", 100, 4);
  // エンコーダ状態 (右寄せ)
  char enc[32];
  if (g_encState == ENC_WARM) {
    int remain = (int)(((int32_t)(g_encTryMs - millis()) + 999) / 1000);
    if (remain < 0) remain = 0;
    snprintf(enc, sizeof(enc), "ENC: WAIT %ds", remain);
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
  } else if (g_encFails > 0) {
    snprintf(enc, sizeof(enc), "ENC: ERR %d", g_encFails);
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);
  } else {
    snprintf(enc, sizeof(enc), "ENC: OK");
    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  }
  M5.Display.setTextDatum(top_right);
  M5.Display.drawString(enc, 1272, 4);
  M5.Display.setTextDatum(top_left);
  // プリセット + モード + 編集マーク (* はプリセットから変更した印)
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
  char line[80];
  bool edited = memcmp(&g_params, &PRESETS[g_preset], sizeof(Params)) != 0;
  if (g_pbank == 1 && g_userSaved[g_preset]) {
    edited = memcmp(&g_params, &g_userBank[g_preset], sizeof(Params)) != 0;
  }
  snprintf(line, sizeof(line), "%s %d: %s [%s]%s", g_pbank ? "USER" : "SAMPLE",
           g_preset + 1, PRESET_NAMES[g_preset], MODE_NAMES[g_voiceMode],
           edited ? " *" : "");
  M5.Display.drawString(line, 100, 26);
  // プリセットバンク切替ボタン (4r: 画面ボタン方式)
  for (int b = 0; b < 2; b++) {
    int bx = (b == 0) ? PB1_X : PB2_X;
    bool act = (g_pbank == b);
    if (act) M5.Display.fillRect(bx, PB_Y, PB_W, PB_H, TFT_DARKGREY);
    M5.Display.drawRect(bx, PB_Y, PB_W, PB_H, TFT_WHITE);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(act ? TFT_YELLOW : TFT_LIGHTGREY,
                            act ? TFT_DARKGREY : TFT_BLACK);
    M5.Display.drawString(b == 0 ? "SAMPLE" : "USER",
                          bx + (b == 0 ? 34 : 46), PB_Y + 9);
  }
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(g_bank == 0 ? TFT_ORANGE : TFT_SKYBLUE, TFT_BLACK);
  snprintf(line, sizeof(line), "BANK %c", g_bank ? 'B' : 'A');
  M5.Display.setTextDatum(top_right);
  M5.Display.drawString(line, 1272, 26);
  M5.Display.setTextDatum(top_left);
}

// パラメータ表示のキャッシュ (変わった箱だけ描き直す)
static int s_paramCache[8];
static int s_paramBank = -1;
static int s_paramPreset = -1;

static void drawParams(bool force) {
  if (force || s_paramBank != g_bank || s_paramPreset != g_preset) {
    for (int i = 0; i < 8; i++) {
      drawParamBox(i);
      if (i < 7) {
        float v = *fieldPtr(g_params, BANK_FIELDS[g_bank][i]);
        s_paramCache[i] = (int)(v * 100.0f + 0.5f);
      }
    }
    s_paramCache[7] = g_preset;
    s_paramBank = g_bank;
    s_paramPreset = g_preset;
    return;
  }
  for (int i = 0; i < 7; i++) {
    float v = *fieldPtr(g_params, BANK_FIELDS[g_bank][i]);
    int pct = (int)(v * 100.0f + 0.5f);
    if (pct != s_paramCache[i]) {
      s_paramCache[i] = pct;
      drawParamBox(i);
    }
  }
}

static void drawParamBox(int i) {
  int col = i % 4, row = i / 4;
  int x = 100 + col * 296, y = 58 + row * 64, w = 288, h = 60;
  uint16_t bankCol = (g_bank == 0) ? TFT_ORANGE : TFT_SKYBLUE;
  M5.Display.fillRect(x, y, w, h, TFT_BLACK);
  M5.Display.drawRect(x, y, w, h, TFT_DARKGREY);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(2);
  if (i == 7) {  // PRESET 箱
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.drawString("PRESET", x + 8, y + 6);
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    char v[40];
    snprintf(v, sizeof(v), "%d %s", g_preset + 1, PRESET_NAMES[g_preset]);
    M5.Display.drawString(v, x + 8, y + 32);
    return;
  }
  int f = BANK_FIELDS[g_bank][i];
  float v = *fieldPtr(g_params, f);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.drawString(FIELD_NAMES[f], x + 8, y + 6);
  char val[24];
  if (f == F_WAVE) {
    snprintf(val, sizeof(val), "%s", v < 0.34f ? "SAW" : (v < 0.67f ? "SQUARE" : (v < 0.84f ? "TRI" : "SAMPLE")));
  } else if (f == F_DETUNE) {
    snprintf(val, sizeof(val), "%+d ct", (int)((v - 0.5f) * 100.0f));
  } else {
    snprintf(val, sizeof(val), "%d%%", (int)(v * 100.0f + 0.5f));
  }
  M5.Display.setTextDatum(top_right);
  M5.Display.drawString(val, x + w - 8, y + 6);
  M5.Display.setTextDatum(top_left);
  // 値バー
  M5.Display.fillRect(x + 8, y + 36, w - 16, 12, TFT_DARKGREY);
  int bw = (int)((w - 16) * v);
  if (bw > 0) M5.Display.fillRect(x + 8, y + 36, bw, 12, bankCol);
}

// ---------------- 音声タスク (Phase 2 エンジン) ----------------
// PolyBLEP: 波形の段差を 1 サンプル幅でならし、折り返し雑音を抑える
static float polyblep(float ph, float dt) {
  if (dt <= 0.0f || dt >= 0.5f) return 0.0f;
  if (ph < dt) {
    float t = ph / dt;
    return t + t - t * t - 1.0f;
  }
  if (ph > 1.0f - dt) {
    float t = (ph - 1.0f) / dt;
    return t * t + t + t + 1.0f;
  }
  return 0.0f;
}

// ---------------- リバーブ (Freeverb 方式、モノラル, 3r5) ----------------
// コムフィルタ 8 本 (帰還 + ローパスで残響を作る) の並列和を
// オールパス 4 本で拡散させる定番構成。遅延長は 44.1kHz 用の
// 標準チューニング。状態が NaN になったら全クリア (SVF の教訓)。
static const int RVB_COMB_LEN[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
static float s_combBuf[8][1617];
static int s_combIdx[8];
static float s_combStore[8];
static const int RVB_AP_LEN[4] = {556, 441, 341, 225};
static float s_apBuf[4][556];
static int s_apIdx[4];

static void reverbReset(void) {
  memset(s_combBuf, 0, sizeof(s_combBuf));
  memset(s_combStore, 0, sizeof(s_combStore));
  memset(s_combIdx, 0, sizeof(s_combIdx));
  memset(s_apBuf, 0, sizeof(s_apBuf));
  memset(s_apIdx, 0, sizeof(s_apIdx));
}

static float reverbProcess(float in) {
  if (s_combStore[0] != s_combStore[0]) reverbReset();  // NaN ガード
  float acc = 0.0f;
  for (int c = 0; c < 8; c++) {
    float b = s_combBuf[c][s_combIdx[c]];
    s_combStore[c] = b * 0.65f + s_combStore[c] * 0.35f;
    if (s_combStore[c] < 1e-12f && s_combStore[c] > -1e-12f) s_combStore[c] = 0.0f;
    s_combBuf[c][s_combIdx[c]] = in + s_combStore[c] * 0.84f;
    if (++s_combIdx[c] >= RVB_COMB_LEN[c]) s_combIdx[c] = 0;
    acc += b;
  }
  float sig = acc * 0.16f;
  for (int a = 0; a < 4; a++) {
    float b = s_apBuf[a][s_apIdx[a]];
    s_apBuf[a][s_apIdx[a]] = sig + b * 0.5f;
    if (++s_apIdx[a] >= RVB_AP_LEN[a]) s_apIdx[a] = 0;
    sig = b - sig;
  }
  return sig;
}

// ---------------- ボイス操作 (音声タスク側) ----------------
static void trigVoice(Voice& v, int note, bool glideSnap) {
  g_trigCount++;
  v.note = note;
  v.gate = true;
  v.ph1 = 0.0f;
  v.ph2 = 0.0f;
  v.spos = 0.0f;
  v.low = 0.0f;
  v.band = 0.0f;
  v.env = 0.0f;
  v.stage = 1;
  v.age = ++s_ageCounter;
  if (glideSnap || v.freqSm <= 0.0f) v.freqSm = midiFreq(note);
}

static void relVoice(Voice& v, float releaseSec) {
  if (v.stage >= 1 && v.stage <= 3) {
    v.stage = 4;
    v.relInc = v.env / (releaseSec * SR);  // 現在値から releaseSec で 0 へ
  }
  v.gate = false;
}

static void applyNoteEvent(int type, int note, bool glideSnap,
                           float releaseSec, int mode) {
  static int s_lastOffNote = -1;      // 5r2: ドロップアウト判定用
  static uint32_t s_lastOffMs = 0;
  if (type == EV_ON) {
    // 5r2: 同音の OFF から 150ms 以内の ON はタッチの一瞬の欠落と
    // みなし、再トリガしない。リリース中のボイスを再ゲートして
    // 再生位置 (spos) とエンベロープを保持する
    if (note == s_lastOffNote && (millis() - s_lastOffMs) < 150) {
      bool regated = false;
      for (int vi = 0; vi < 5; ++vi) {
        Voice& v = s_voices[vi];
        if (v.note == note && v.stage == 4) {
          v.stage = 3;
          v.gate = true;
          regated = true;
        }
      }
      if (regated) return;
    }
    if (mode == MODE_POLY) {
      // 同じ音が鳴っていればそれを、次に空きボイス、最後に一番静かな声部
      int pick = -1;
      for (int vi = 0; vi < 5; ++vi)
        if (s_voices[vi].gate && s_voices[vi].note == note) { pick = vi; break; }
      if (pick < 0)
        for (int vi = 0; vi < 5; ++vi)
          if (s_voices[vi].stage == 0) { pick = vi; break; }
      if (pick < 0) {
        float minEnv = 1e9f;
        uint32_t minAge = 0xFFFFFFFFu;
        for (int vi = 0; vi < 5; ++vi) {
          if (s_voices[vi].env < minEnv ||
              (s_voices[vi].env == minEnv && s_voices[vi].age < minAge)) {
            minEnv = s_voices[vi].env;
            minAge = s_voices[vi].age;
            pick = vi;
          }
        }
      }
      trigVoice(s_voices[pick], note, glideSnap);
    } else if (mode == MODE_UNISON) {
      for (int vi = 0; vi < 5; ++vi) trigVoice(s_voices[vi], note, glideSnap);
    } else {
      trigVoice(s_voices[0], note, glideSnap);
    }
  } else {  // EV_OFF
    s_lastOffNote = note;             // 5r2: ドロップアウト判定用
    s_lastOffMs = millis();
    if (mode == MODE_POLY) {
      int pick = -1;
      uint32_t maxAge = 0;
      for (int vi = 0; vi < 5; ++vi)
        if (s_voices[vi].gate && s_voices[vi].note == note &&
            s_voices[vi].age >= maxAge) { maxAge = s_voices[vi].age; pick = vi; }
      if (pick >= 0) relVoice(s_voices[pick], releaseSec);
    } else if (mode == MODE_UNISON) {
      for (int vi = 0; vi < 5; ++vi) relVoice(s_voices[vi], releaseSec);
    } else {
      relVoice(s_voices[0], releaseSec);
    }
  }
}

static void audioTask(void* arg) {
  (void)arg;
  int lastMode = MODE_MONO;
  float bend = 0.0f, bendSm = 0.0f;
  float bendFCur = 1.0f, bendFStep = 0.0f;  // 3r2: ベンド倍率の補間用
  const float bendK = 1.0f - expf(-1.0f / (SR * 0.030f));  // 1r の平滑化
  float lfoPh = 0.0f;
  uint32_t rng = 22222u;  // ノイズ用 LCG
  uint32_t chunkNo = 0;

  for (;;) {
    // Phase 5: マイク録音中はスピーカー送出を止める (ES8388 共用のため)
    if (g_micPause) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }
    Params p = g_params;  // チャンク境界でパラメータをまとめて取り込む
    // フィルタ状態の NaN ガード: 発散して NaN 化すると恒常的に無音に
    // なるため、異常を検出したら状態を初期化する (2r2)
    for (int vi = 0; vi < 5; ++vi) {
      Voice& v = s_voices[vi];
      if (!isfinite(v.low) || !isfinite(v.band) || !isfinite(v.env) ||
          !isfinite(v.freqSm)) {
        v.low = 0.0f;
        v.band = 0.0f;
        v.env = 0.0f;
        v.freqSm = (v.note >= 0) ? midiFreq(v.note) : 0.0f;
        if (v.stage != 0) v.stage = 4;
        Serial.println("[P2] voice state reset (NaN guard)");
      }
    }
    // --- 実値への変換 (チャンク毎) ---
    const float cutoffHz = 80.0f * powf(150.0f, p.cutoff);          // 80..12000
    const float attackSec = 0.002f * powf(1000.0f, p.attack);       // 2ms..2s
    const float decaySec = 0.01f * powf(200.0f, p.decay);           // 10ms..2s
    const float releaseSec = 0.01f * powf(300.0f, p.release);       // 10ms..3s
    const float atkInc = 1.0f / (attackSec * SR);
    const float decInc = (1.0f - p.sustain) / (decaySec * SR);
    const float glideSec = 0.004f + p.glide * p.glide * 0.596f;     // 4ms..600ms
    const float glideK = 1.0f - expf(-1.0f / (SR * glideSec));
    const bool glideSnap = p.glide < 0.03f;
    const float lfoHz = 0.05f * powf(400.0f, p.lfoRate);            // 0.05..20Hz
    const float lfoInc = lfoHz / (float)SR;
    const float detRatio = powf(2.0f, (p.detune - 0.5f) * 100.0f / 1200.0f);
    const int waveSel = (p.wave < 0.34f) ? 0 : ((p.wave < 0.67f) ? 1 : ((p.wave < 0.84f) ? 2 : 3));
    const float damp = 2.0f - 1.85f * p.reso;                       // SVF 減衰

    // --- ノートイベントを反映。モード変更時は全ボイスをリリース ---
    int mode = g_voiceMode;
    if (mode != lastMode) {
      for (int vi = 0; vi < 5; ++vi) relVoice(s_voices[vi], releaseSec);
      lastMode = mode;
    }
    while (g_evTail != g_evHead) {
      int t = g_evq[g_evTail].type;
      int n = g_evq[g_evTail].note;
      g_evTail = (g_evTail + 1) & 31;
      applyNoteEvent(t, n, glideSnap, releaseSec, mode);
    }
    bend = g_cmdBend;

    int16_t* buf = s_useA ? s_bufA : s_bufB;
    s_useA = !s_useA;
    // 5r7: playRaw はバッファをコピーしない (ポインタのみ
    // キュー)。スピーカーが読み終わっていないバッファへ描
    // くと再生中の音が上書きで壊れるため、チャンネル 0 の
    // 占有が 2 未満 (前の要求の読み終わり) になるまで待つ。
    // 安全側のタイムアウト付き (異常時は従来動作で継続)
    {
      uint32_t tw0 = millis();
      while (M5.Speaker.isPlaying(0) >= 2 &&
             (millis() - tw0) < 500) {
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }

    // UNISON の声部デチューン (ボイス毎の音程倍率)。他モードでは 1.0
    float detMul[5];
    for (int vi = 0; vi < 5; ++vi) {
      detMul[vi] = (mode == MODE_UNISON)
          ? powf(2.0f, ((float)(vi - 2) * p.detune * 25.0f) / 1200.0f)
          : 1.0f;
    }
    // 3r2: ノート周波数はチャンク毎に 1 回だけ計算 (旧: 毎サンプル powf)
    float tgtFreq[5];
    for (int vi = 0; vi < 5; ++vi) {
      tgtFreq[vi] = (s_voices[vi].note >= 0) ? midiFreq(s_voices[vi].note)
                                             : 0.0f;
    }

    for (int i = 0; i < CHUNK; ++i) {
      bendSm += (bend - bendSm) * bendK;
      lfoPh += lfoInc;
      if (lfoPh >= 1.0f) lfoPh -= 1.0f;
      float lfo = (lfoPh < 0.5f) ? (4.0f * lfoPh - 1.0f) : (3.0f - 4.0f * lfoPh);
      // 3r2: powf は 16 サンプル毎に 1 回だけ。間は線形補間で滑らかに
      if ((i & 15) == 0) {
        float bendFTgt = powf(2.0f, bendSm / 12.0f);
        bendFStep = (bendFTgt - bendFCur) * (1.0f / 16.0f);
        for (int vi = 0; vi < 5; ++vi) {
          Voice& v = s_voices[vi];
          if (v.stage == 0 || v.note < 0) continue;
          float fc = cutoffHz * (1.0f + p.fEnv * v.env * 6.0f) *
                     powf(2.0f, lfo * p.lfoDepth);
          if (fc < 40.0f) fc = 40.0f;
          if (fc > 7000.0f) fc = 7000.0f;  // 2r2: SVF 安定域に収める
          float coef = 2.0f * (float)M_PI * fc / (float)SR;
          if (coef > 0.8f) coef = 0.8f;  // Chamberlin SVF の発散防止
          s_fcStep[vi] = (coef - s_fcCoef[vi]) * (1.0f / 16.0f);
        }
      }
      bendFCur += bendFStep;
      float mix = 0.0f;
      float mixS = 0.0f;  // 5r5: サンプル専用バス (ドライ/等音量)
      for (int vi = 0; vi < 5; ++vi) {
        Voice& v = s_voices[vi];
        if (v.stage == 0 || v.note < 0) continue;
        // 音程: チャンク毎の目標周波数をグライドで平滑化し、ベンドを掛ける
        v.freqSm += (tgtFreq[vi] - v.freqSm) * glideK;
        float fr = v.freqSm * bendFCur * detMul[vi];
        v.ph1 += fr / (float)SR;
        if (v.ph1 >= 1.0f) v.ph1 -= 1.0f;
        v.ph2 += fr * detRatio / (float)SR;
        if (v.ph2 >= 1.0f) v.ph2 -= 1.0f;
        // 波形 (0=ノコギリ, 1=矩形 PWM, 2=三角)。3r4: ノコギリと
        // 矩形は PolyBLEP で帯域制限 (折り返し雑音 = 音の粒の対策)
        float dt1 = fr * (1.0f / (float)SR);
        float dt2 = dt1 * detRatio;
        float sig;
        if (waveSel == 3) {
          // Phase 5: サンプル再生 (C4 で原音ピッチ、ループ)。
          // fr にはベンド/グライド/デチューンが既に掛かっている
          // 5r6: バッファは録音/読込時に 44.1kHz 化済み
          // (g_smp44)。再生中は 1:1 で読むだけ (逐次リサン
          // プリングを廃止し、直接再生と同じ条件にそろえた)
          if (g_sampleReady && g_smp44 && g_smp44Len > 0) {
            int idx = (int)v.spos;
            if (idx < 0 || idx >= g_smp44Len) idx = 0;
            float frac = v.spos - (float)idx;
            int idx2 = idx + 1;
            if (idx2 >= g_smp44Len) idx2 = 0;
            float s0 = g_smp44[idx] * (1.0f / 32768.0f);
            float s1 = g_smp44[idx2] * (1.0f / 32768.0f);
            sig = s0 + (s1 - s0) * frac;
            v.spos += fr / SMP_ROOT;
            if (v.spos >= (float)g_smp44Len) v.spos -= (float)g_smp44Len;
          } else {
            sig = 0.0f;  // 未録音なら無音 (MIC REC で録音する)
          }
        } else {
          float o1, o2;
          if (waveSel == 0) {
            o1 = (v.ph1 * 2.0f - 1.0f) - polyblep(v.ph1, dt1);
            o2 = (v.ph2 * 2.0f - 1.0f) - polyblep(v.ph2, dt2);
          } else if (waveSel == 1) {
            float w1 = v.ph1 - p.pwm; if (w1 < 0.0f) w1 += 1.0f;
            float w2 = v.ph2 - p.pwm; if (w2 < 0.0f) w2 += 1.0f;
            o1 = ((v.ph1 < p.pwm) ? 1.0f : -1.0f)
                 + polyblep(v.ph1, dt1) - polyblep(w1, dt1);
            o2 = ((v.ph2 < p.pwm) ? 1.0f : -1.0f)
                 + polyblep(v.ph2, dt2) - polyblep(w2, dt2);
          } else {
            o1 = (v.ph1 < 0.5f) ? (4.0f * v.ph1 - 1.0f) : (3.0f - 4.0f * v.ph1);
            o2 = (v.ph2 < 0.5f) ? (4.0f * v.ph2 - 1.0f) : (3.0f - 4.0f * v.ph2);
          }
          sig = o1 * (1.0f - p.oscMix) + o2 * p.oscMix;
        }
        if (waveSel != 3 && p.noise > 0.001f) {
          // 5r4: ノイズはオシレータ専用。サンプル (声) にヒスを
          // 被せない (5r3 までは全経路共通で混ざっていた)
          rng = rng * 1664525u + 1013904223u;
          float nz = ((rng >> 9) & 0xFFFF) / 32767.5f - 1.0f;
          sig = sig * (1.0f - 0.5f * p.noise) + nz * p.noise;
        }
        // エンベロープ
        if (waveSel == 3) {
          // 5r5: サンプルは ADSR を使わず単純ゲート (直接再生と
          // 同じ音量推移)。アタック 5ms で 1.0、離鍵で 30ms 減衰
          if (v.stage == 4) {
            v.env -= 1.0f / (0.030f * (float)SR);
            if (v.env <= 0.0f) { v.env = 0.0f; v.stage = 0; }
          } else {
            v.env += 1.0f / (0.005f * (float)SR);
            if (v.env >= 1.0f) { v.env = 1.0f; v.stage = 3; }
          }
        } else {
          switch (v.stage) {
            case 1: v.env += atkInc; if (v.env >= 1.0f) { v.env = 1.0f; v.stage = 2; } break;
            case 2: v.env -= decInc; if (v.env <= p.sustain) { v.env = p.sustain; v.stage = 3; } break;
            case 3: v.env = p.sustain; break;
            case 4: v.env -= v.relInc; if (v.env <= 0.0f) { v.env = 0.0f; v.stage = 0; } break;
          }
        }
        if (waveSel == 3) {
          // 5r3: サンプルは SVF を通さない。LFO / F-ENV でカットオフが
          // 揺れると言葉が周期的に消えてしまう (実測で確定)。フィルタ
          // とその変調はオシレータ専用とし、サンプルはドライで通す
          // 5r5: 合算先はサンプル専用バス mixS (リバーブ/GAIN の外)
          mixS += sig * v.env;
        } else {
          // VCF: 2極 SVF (レゾナンス付き)。係数はブロック率で補間済み (3r2)
          s_fcCoef[vi] += s_fcStep[vi];
          float fcCoef = s_fcCoef[vi];
          v.low += fcCoef * v.band;
          float high = sig - v.low - damp * v.band;
          v.band += fcCoef * high;
          mix += v.low * v.env;
        }
      }
      // リバーブ (3r5): ミックスに残響を混ぜてからゲイン/クリップ
      float wetSig = reverbProcess(mix);
      float wetAmt = p.reverb;
      // 5r5: リバーブと MASTER_GAIN はオシレータ側だけ。サンプル
      // バスは直接再生と同じ音量でドライに足す
      float out = (mix * (1.0f - 0.30f * wetAmt) + wetSig * wetAmt) * MASTER_GAIN + mixS;
      if (out > 1.0f) out = 1.0f;
      if (out < -1.0f) out = -1.0f;
      out = out * (1.5f - 0.5f * out * out);  // ソフトクリップ (Phase 3)
      buf[i] = (int16_t)(out * 32767.0f);
    }

    // 5r7: チャンネルは 0 に固定 (自動割当だと isPlaying(0)
    // で追えず、複数チャンネルに分散する恐れがある)
    while (!M5.Speaker.playRaw(buf, CHUNK, SR, false, 1, 0)) {
      vTaskDelay(pdMS_TO_TICKS(5));
    }
    if ((++chunkNo & 0x3FF) == 0) {
      Serial.printf("AUDIO chunk %u note=%d stage=%d preset=%d cutoff=%.0f\n",
                    chunkNo, s_voices[0].note, s_voices[0].stage, g_preset,
                    80.0f * powf(150.0f, g_params.cutoff));
    }
    // 5r2: サンプル再生中は 1 秒毎に再生位置と実トリガ回数を出す
    if (waveSel == 3 && s_voices[0].stage != 0 && (chunkNo % 20) == 0) {
      Serial.printf("[SMPL] spos=%.0f len=%d trigs=%lu stage=%d\n",
                    s_voices[0].spos, (int)g_smp44Len,
                    (unsigned long)g_trigCount, s_voices[0].stage);
    }
  }
}

// ---------------- 起動スプラッシュ (3r / 3r2 / 3r3) ----------------
// 3r3: 全項目が CHECKING (黄) で一斉に表示され、上から順に
// OK (緑) へ変わる。SPEAKER / DISPLAY / TOUCH は等間隔の演出、
// 8ENCODER だけ本物のウォームアップ時間で最後に OK になる。
static const char* SPLASH_LABEL[4] = {
  "SPEAKER ......... ", "DISPLAY ......... ", "TOUCH ........... ",
  "8ENCODER ........ "
};

static void drawCheckLine(int idx, const char* tail, uint16_t color) {
  int y = 300 + idx * 36;
  M5.Display.fillRect(280, y - 4, 760, 36, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(color, TFT_BLACK);
  M5.Display.setTextDatum(top_left);
  char b[64];
  snprintf(b, sizeof(b), "%s%s", SPLASH_LABEL[idx], tail);
  M5.Display.drawString(b, 300, y);
}

static void drawSplashStatus(const char* text, uint16_t color) {
  M5.Display.fillRect(0, 548, 1280, 44, TFT_BLACK);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(color, TFT_BLACK);
  M5.Display.drawString(text, 640, 556);
  M5.Display.setTextDatum(top_left);
}

static void drawSplash(void) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(5);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.drawString("Tab5 Analog Synth", 640, 130);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  M5.Display.drawString("by KAZUTOMO with Muse", 640, 205);
  M5.Display.drawLine(240, 262, 1040, 262, TFT_DARKGREY);
  M5.Display.setTextDatum(top_left);
  g_splashStartMs = millis();
}

// スプラッシュの進行管理。チェック行は段階表示、8ENCODER は実時間。
// READY (LED 全点灯) で I2C OK! を出し、少し置いて鍵盤画面へ。
// タッチでスキップもできる。
static void updateSplash(void) {
  static int lineState[4] = {0, 0, 0, 0};  // 0=未表示 1=CHECKING 2=OK
  static int lastCount = -1;
  static int statusShown = 0;              // 0=なし 1=checking 2=warming
  static uint32_t readyAt = 0;
  uint32_t el = millis() - g_splashStartMs;

  if (readyAt != 0) {  // OK 表示の余韻が終わったら鍵盤へ
    if (millis() - readyAt >= 900) { drawAll(); g_booted = true; }
    return;
  }
  if (M5.Touch.getCount() > 0) {  // タッチでスキップ (従来動作に戻る)
    drawAll();
    g_booted = true;
    return;
  }
  // SPEAKER / DISPLAY / TOUCH: 全行 CHECKING で出てから順に OK へ (3r3)
  for (int i = 0; i < 3; i++) {
    int want = (el >= 1500 + (uint32_t)i * 1200) ? 2 : ((el >= 300) ? 1 : 0);
    if (want != 0 && want != lineState[i]) {
      lineState[i] = want;
      drawCheckLine(i, want == 2 ? "OK" : "CHECKING",
                    want == 2 ? TFT_GREEN : TFT_YELLOW);
    }
  }
  // 下段ステータス: checking → warming
  int wantStatus = (el >= 3900) ? 2 : 1;
  if (wantStatus != statusShown) {
    statusShown = wantStatus;
    drawSplashStatus(wantStatus == 2 ? "Now warming up..." : "Now checking...",
                     TFT_YELLOW);
  }
  // 8ENCODER 行: 最初から CHECKING (秒数付き)、実時間で最後に OK (3r3)
  if (el >= 300) {
    if (g_encState == ENC_RUN) {
      lineState[3] = 2;
      drawCheckLine(3, "OK!", TFT_GREEN);
      drawSplashStatus("I2C OK!", TFT_GREEN);
      readyAt = millis();
    } else {
      int remain = (int)(((int32_t)(g_encTryMs - millis()) + 999) / 1000);
      if (remain < 0) remain = 0;
      int cnt = remain + g_encInitFails * 1000;  // リトライ時も再描画させる
      if (lineState[3] != 1 || cnt != lastCount) {
        lineState[3] = 1;
        lastCount = cnt;
        char tail[24];
        snprintf(tail, sizeof(tail), "CHECKING %ds", remain);
        drawCheckLine(3, tail, TFT_YELLOW);
      }
    }
  }
}

// ---------------- 描画 (鍵盤まわりは Phase 1r2 と同一) ----------------
static void drawAll(void) {
  M5.Display.fillScreen(TFT_BLACK);
  drawTop();
  drawParams(true);
  drawBend();
  drawKeyboard();
  drawStrip();
  drawTransport();
  drawStatus();
  drawVolume();
}

static void drawStatus(void) {
  M5.Display.fillRect(0, 660, VOL_X, 60, TFT_BLACK);  // 右側は VOL/REV スライダー
  M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setTextDatum(top_left);
  char buf[128];
  if (g_toastUntil != 0 && (int32_t)(g_toastUntil - millis()) > 0) {
    // トースト表示中はステータス行を差し替える (Phase 4)
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.drawString(g_toast, 30, 680);
    drawVolume();
    return;
  }
  if (g_curNote >= 0) {
    snprintf(buf, sizeof(buf), "NOTE %s%d %.0fHz BEND %+0.2f BANK %c",
             noteName(g_curNote), g_curNote,
             midiFreq(g_curNote) * powf(2.0f, g_bend / 12.0f), g_bend,
             g_bank ? 'B' : 'A');
  } else {
    snprintf(buf, sizeof(buf), "BEND %+0.2f BANK %c",
             g_bend, g_bank ? 'B' : 'A');
  }
  M5.Display.drawString(buf, 30, 680);
  drawVolume();  // スライダー領域を必ず描き直す (残像対策, 3r8)
  // 診断行 (2 行目) は 3r7 で削除 (操作系の問題が片付いたため)
}

// ---------------- スライダー (画面下部: VOL / REV 横並び, 3r7) ----------------
static void drawVolume(void) {
  M5.Display.fillRect(VOL_X, VOL_Y, 1280 - VOL_X, VOL_H, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  M5.Display.setTextDatum(top_left);
  char b[16];
  // 左: VOL (ラベル上、バー下)
  snprintf(b, sizeof(b), "VOL %d", g_volume);
  M5.Display.drawString(b, SL1_X, VOL_Y + 4);
  M5.Display.drawRect(SL1_X, VOL_Y + 28, SL_BAR_W, 14, TFT_DARKGREY);
  int fw = (SL_BAR_W - 2) * g_volume / 127;
  if (fw > 0) M5.Display.fillRect(SL1_X + 1, VOL_Y + 29, fw, 12, TFT_GREEN);
  // 右: REV (リバーブ量、Params の一部)
  snprintf(b, sizeof(b), "REV %d", (int)(g_params.reverb * 100.0f + 0.5f));
  M5.Display.drawString(b, SL2_X, VOL_Y + 4);
  M5.Display.drawRect(SL2_X, VOL_Y + 28, SL_BAR_W, 14, TFT_DARKGREY);
  int rw = (int)((SL_BAR_W - 2) * g_params.reverb);
  if (rw > 0) M5.Display.fillRect(SL2_X + 1, VOL_Y + 29, rw, 12, TFT_CYAN);
}

static void updateVolumeFromX(int x) {
  int v = (x - SL1_X) * 127 / SL_BAR_W;
  if (v < 0) v = 0;
  if (v > 127) v = 127;
  if (v != g_volume) {
    g_volume = v;
    M5.Speaker.setVolume((uint8_t)v);
    drawVolume();
  }
}

static void updateReverbFromX(int x) {
  float v = (float)(x - SL2_X) / (float)SL_BAR_W;
  if (v < 0.0f) v = 0.0f;
  if (v > 1.0f) v = 1.0f;
  if (fabsf(v - g_params.reverb) > 0.002f) {
    g_params.reverb = v;
    drawVolume();
    drawTop();  // ヘッダの * (編集マーク) を追随させる
  }
}

static void drawBend(void) {
  M5.Display.fillRect(BEND_X0, BEND_Y0, BEND_X1 - BEND_X0, BEND_Y1 - BEND_Y0, TFT_DARKGREY);
  M5.Display.drawRect(BEND_X0, BEND_Y0, BEND_X1 - BEND_X0, BEND_Y1 - BEND_Y0, TFT_WHITE);
  int cy = (BEND_Y0 + BEND_Y1) / 2;
  M5.Display.drawLine(BEND_X0, cy, BEND_X1, cy, TFT_WHITE);
  int hy = cy - (int)(g_bend / BEND_RANGE * ((BEND_Y1 - BEND_Y0) / 2 - 10));
  M5.Display.fillRect(BEND_X0 + 6, hy - 14, BEND_X1 - BEND_X0 - 12, 28, TFT_ORANGE);
  M5.Display.drawRect(BEND_X0 + 6, hy - 14, BEND_X1 - BEND_X0 - 12, 28, TFT_WHITE);
  M5.Display.setTextColor(TFT_WHITE, TFT_DARKGREY);
  M5.Display.setTextSize(2);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString("+", (BEND_X0 + BEND_X1) / 2, BEND_Y0 + 16);
  M5.Display.drawString("-", (BEND_X0 + BEND_X1) / 2, BEND_Y1 - 16);
  M5.Display.setTextDatum(top_left);
}

static bool hasBlackAfter(int whiteIdx) {
  int pc = WHITES[whiteIdx] % 12;
  return (pc == 0 || pc == 2 || pc == 5 || pc == 7 || pc == 9) && whiteIdx < 35;
}
static int whiteIndexOf(int note) {
  for (int i = 0; i < 36; ++i) if (WHITES[i] == note) return i;
  return -1;
}
static bool noteIsActive(int note) {
  for (const ActiveKey& k : g_activeKeys) if (k.active && k.note == note) return true;
  return false;
}
static void drawWhiteKey(int vis) {
  int w = g_keyWin + vis;
  int note = WHITES[w];
  int x = KEY_X + vis * KEY_W;
  uint16_t col = noteIsActive(note) ? TFT_SKYBLUE : TFT_WHITE;
  M5.Display.fillRect(x, KEY_Y, KEY_W, KEY_H, col);
  M5.Display.drawRect(x, KEY_Y, KEY_W, KEY_H, TFT_BLACK);
  if (note % 12 == 0) {
    M5.Display.setTextColor(TFT_BLACK, col);
    M5.Display.setTextSize(2);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString("C" + String(note / 12 - 1), x + KEY_W / 2, KEY_Y + KEY_H - 22);
    M5.Display.setTextDatum(top_left);
  }
}
static void drawBlackKey(int vis) {
  int w = g_keyWin + vis;
  if (!hasBlackAfter(w)) return;
  int note = WHITES[w] + 1;
  int x = KEY_X + (vis + 1) * KEY_W - BLK_W / 2;
  uint16_t col = noteIsActive(note) ? TFT_NAVY : TFT_BLACK;
  M5.Display.fillRect(x, KEY_Y, BLK_W, BLK_H, col);
  M5.Display.drawRect(x, KEY_Y, BLK_W, BLK_H, TFT_WHITE);
}
static void drawKeyboard(void) {
  for (int i = 0; i < VIS_WHITES; ++i) drawWhiteKey(i);
  for (int i = 0; i < VIS_WHITES; ++i) drawBlackKey(i);
}
static void drawKeyByNote(int note) {
  int w = whiteIndexOf(note);
  if (w >= 0) {
    int vis = w - g_keyWin;
    if (vis >= 0 && vis < VIS_WHITES) {
      drawWhiteKey(vis);
      drawBlackKey(vis - 1);
      drawBlackKey(vis);
    }
  } else {
    int bw = whiteIndexOf(note - 1);
    int vis = bw - g_keyWin;
    if (bw >= 0 && vis >= 0 && vis < VIS_WHITES) drawBlackKey(vis);
  }
}
static void drawStrip(void) {
  M5.Display.fillRect(STRIP_X, STRIP_Y, STRIP_W, STRIP_H, TFT_DARKGREY);
  M5.Display.drawRect(STRIP_X, STRIP_Y, STRIP_W, STRIP_H, TFT_WHITE);
  int wx = STRIP_X + g_keyWin * STRIP_W / 36;
  int ww = VIS_WHITES * STRIP_W / 36;
  M5.Display.fillRect(wx, STRIP_Y + 4, ww, STRIP_H - 8, TFT_BLUE);
  M5.Display.drawRect(wx, STRIP_Y + 4, ww, STRIP_H - 8, TFT_WHITE);
  M5.Display.fillRect(100, STRIP_Y, 44, STRIP_H, TFT_DARKGREY);
  M5.Display.drawRect(100, STRIP_Y, 44, STRIP_H, TFT_WHITE);
  M5.Display.fillRect(1140, STRIP_Y, 44, STRIP_H, TFT_DARKGREY);
  M5.Display.drawRect(1140, STRIP_Y, 44, STRIP_H, TFT_WHITE);
  M5.Display.setTextColor(TFT_WHITE, TFT_DARKGREY);
  M5.Display.setTextSize(3);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString("<", 122, STRIP_Y + STRIP_H / 2);
  M5.Display.drawString(">", 1162, STRIP_Y + STRIP_H / 2);
  M5.Display.setTextDatum(top_left);
}

// ---------------- 鍵盤タッチ (Phase 1r2 と同一) ----------------
static void keyOn(int note, int id) {
  for (ActiveKey& k : g_activeKeys) if (k.active && k.id == id) return;
  ActiveKey* slot = nullptr;
  for (ActiveKey& k : g_activeKeys) if (!k.active) { slot = &k; break; }
  if (!slot) return;
  slot->id = id;
  slot->note = note;
  slot->seq = ++g_pressSeq;
  slot->active = true;
  if (g_voiceMode == MODE_POLY) {
    pushEvent(EV_ON, note);
    g_curNote = note;
    drawStatus();
  } else {
    synthNoteOn(note);  // MONO/UNISON: 最後に押した鍵が鳴る
  }
  drawKeyByNote(note);
}

static void keyOffById(int id) {
  int offNote = -1;
  for (ActiveKey& k : g_activeKeys) {
    if (!k.active || k.id != id) continue;
    offNote = k.note;
    k.active = false;
    k.id = -1;
    drawKeyByNote(offNote);
    break;
  }
  if (offNote < 0) return;
  if (g_voiceMode == MODE_POLY) {
    pushEvent(EV_OFF, offNote);
    if (offNote == g_curNote) { g_curNote = -1; drawStatus(); }
    return;
  }
  if (offNote != g_curNote) return;
  int best = -1;
  uint32_t bestSeq = 0;
  for (const ActiveKey& k : g_activeKeys) {
    if (k.active && k.seq > bestSeq) { bestSeq = k.seq; best = k.note; }
  }
  if (best >= 0) synthNoteOn(best);
  else synthNoteOff();
}

static int keyAtPoint(int x, int y) {
  if (y < KEY_Y || y >= KEY_Y + KEY_H || x < KEY_X || x >= KEY_X + VIS_WHITES * KEY_W) return -1;
  if (y < KEY_Y + BLK_H) {
    for (int i = 0; i < VIS_WHITES; ++i) {
      if (!hasBlackAfter(g_keyWin + i)) continue;
      int bx = KEY_X + (i + 1) * KEY_W - BLK_W / 2;
      if (x >= bx && x < bx + BLK_W) return WHITES[g_keyWin + i] + 1;
    }
  }
  int i = (x - KEY_X) / KEY_W;
  if (i >= 0 && i < VIS_WHITES) return WHITES[g_keyWin + i];
  return -1;
}

static void scrollStripTo(int x) {
  float cell = (float)STRIP_W / 36.0f;
  int center = (int)((x - STRIP_X) / cell);
  int win = center - VIS_WHITES / 2;
  if (win < 0) win = 0;
  if (win > 36 - VIS_WHITES) win = 36 - VIS_WHITES;
  if (win != g_keyWin) {
    g_keyWin = win;
    drawKeyboard();
    drawStrip();
  }
}

static void updateBendFromY(int y) {
  int cy = (BEND_Y0 + BEND_Y1) / 2;
  int half = (BEND_Y1 - BEND_Y0) / 2 - 10;
  float b = (float)(cy - y) / (float)half * BEND_RANGE;
  if (b > BEND_RANGE) b = BEND_RANGE;
  if (b < -BEND_RANGE) b = -BEND_RANGE;
  if (fabsf(b - g_bend) < 0.01f) return;
  g_bend = b;
  g_cmdBend = b;
  drawBend();
  drawStatus();
}

static void handlePoints(void) {
  int cnt = (int)M5.Touch.getCount();
  if (cnt > 10) cnt = 10;

  struct Pt { int id, x, y; bool pressed, down, released; };
  Pt pts[10];
  bool idDown[256] = {};
  for (int i = 0; i < cnt; ++i) {
    auto d = M5.Touch.getDetail(i);
    pts[i] = {(int)d.id, (int)d.x, (int)d.y,
              d.wasPressed(), d.isPressed(), d.wasReleased()};
    if (pts[i].down || pts[i].pressed) idDown[pts[i].id & 0xFF] = true;
  }

  for (const ActiveKey& k : g_activeKeys) {
    if (k.active && !idDown[k.id & 0xFF]) keyOffById(k.id);
  }
  if (g_bendId >= 0 && !idDown[g_bendId & 0xFF]) {
    g_bendId = -1;
    g_bend = 0.0f;
    g_cmdBend = 0.0f;
    drawBend();
    drawStatus();
  }
  if (g_volId >= 0 && !idDown[g_volId & 0xFF]) g_volId = -1;
  if (g_revId >= 0 && !idDown[g_revId & 0xFF]) g_revId = -1;

  for (int i = 0; i < cnt; ++i) {
    const Pt& p = pts[i];
    bool inBend = (p.x >= BEND_X0 && p.x < BEND_X1 &&
                   p.y >= BEND_Y0 && p.y < BEND_Y1);
    bool inStrip = (p.y >= STRIP_Y && p.y < STRIP_Y + STRIP_H &&
                    p.x >= STRIP_X && p.x < STRIP_X + STRIP_W);
    bool inOctL = (p.y >= STRIP_Y && p.y < STRIP_Y + STRIP_H &&
                   p.x >= 100 && p.x < 144);
    bool inOctR = (p.y >= STRIP_Y && p.y < STRIP_Y + STRIP_H &&
                   p.x >= 1140 && p.x < 1184);
    bool inVol = (p.x >= VOL_X && p.x < SL_MID_X &&
                  p.y >= VOL_Y && p.y < VOL_Y + VOL_H);
    bool inRev = (p.x >= SL_MID_X &&
                  p.y >= VOL_Y && p.y < VOL_Y + VOL_H);
    bool inPbS = (p.x >= PB1_X && p.x < PB1_X + PB_W &&
                  p.y >= PB_Y && p.y < PB_Y + PB_H);
    bool inPbU = (p.x >= PB2_X && p.x < PB2_X + PB_W &&
                  p.y >= PB_Y && p.y < PB_Y + PB_H);
    bool inRecBtn = (p.x >= REC_BTN_X && p.x < REC_BTN_X + REC_BTN_W &&
                     p.y >= TR_Y && p.y < TR_Y + TR_H);
    bool inBpmM = (p.x >= BPM_M_X && p.x < BPM_M_X + BPM_BTN_W &&
                   p.y >= TR_Y && p.y < TR_Y + TR_H);
    bool inBpmP = (p.x >= BPM_P_X && p.x < BPM_P_X + BPM_BTN_W &&
                   p.y >= TR_Y && p.y < TR_Y + TR_H);
    bool inMicBtn = (p.x >= MIC_X && p.x < MIC_X + MIC_W &&
                     p.y >= TR_Y && p.y < TR_Y + TR_H);

    if (p.released) {
      if (p.id == g_volId) {
        g_volId = -1;
      } else if (p.id == g_revId) {
        g_revId = -1;
      } else if (p.id == g_bendId) {
        g_bendId = -1;
        g_bend = 0.0f;
        g_cmdBend = 0.0f;
        drawBend();
        drawStatus();
      } else {
        keyOffById(p.id);
      }
      continue;
    }
    if (inBend) {
      if (p.pressed && g_bendId < 0) g_bendId = p.id;
      if (p.id == g_bendId) updateBendFromY(p.y);
      continue;
    }
    if (inVol) {
      if (p.pressed && g_volId < 0) g_volId = p.id;
      if (p.id == g_volId) updateVolumeFromX(p.x);
      continue;
    }
    if (inRev) {
      if (p.pressed && g_revId < 0) g_revId = p.id;
      if (p.id == g_revId) updateReverbFromX(p.x);
      continue;
    }
    if (p.pressed && (inPbS || inPbU)) {
      int want = inPbU ? 1 : 0;
      if (g_pbank != want) {
        g_pbank = want;
        loadPreset(g_preset);
        setToast(want ? "PRESET BANK: USER" : "PRESET BANK: SAMPLE", 1500);
      }
      continue;
    }
    if (p.pressed && inRecBtn) { toggleRec(); continue; }
    if (p.pressed && inBpmM) {
      if (g_bpm > 40) { g_bpm -= 5; drawTransport(); }
      continue;
    }
    if (p.pressed && inBpmP) {
      if (g_bpm < 240) { g_bpm += 5; drawTransport(); }
      continue;
    }
    if (p.pressed && inMicBtn) { doMicRecord(); continue; }
    if (p.pressed) {
      if (inStrip) {
        scrollStripTo(p.x);
      } else if (inOctL) {
        if (g_keyWin > 0) { g_keyWin -= 7; if (g_keyWin < 0) g_keyWin = 0; drawKeyboard(); drawStrip(); }
      } else if (inOctR) {
        if (g_keyWin < 36 - VIS_WHITES) { g_keyWin += 7; if (g_keyWin > 36 - VIS_WHITES) g_keyWin = 36 - VIS_WHITES; drawKeyboard(); drawStrip(); }
      } else {
        int n = keyAtPoint(p.x, p.y);
        if (n >= 0) keyOn(n, p.id);
      }
    } else if (p.down) {
      if (inStrip) {
        scrollStripTo(p.x);
      } else {
        for (const ActiveKey& k : g_activeKeys) {
          if (k.active && k.id == p.id) {
            int n = keyAtPoint(p.x, p.y);
            if (n >= 0 && n != k.note) { keyOffById(p.id); keyOn(n, p.id); }
            break;
          }
        }
        bool hasSlot = false;
        for (const ActiveKey& k : g_activeKeys) if (k.active && k.id == p.id) hasSlot = true;
        if (!hasSlot) {
          int n = keyAtPoint(p.x, p.y);
          if (n >= 0) keyOn(n, p.id);
        }
      }
    }
  }
}

// ---------------- 便利関数 ----------------
static float midiFreq(int note) {
  return 440.0f * powf(2.0f, (float)(note - 69) / 12.0f);
}
static const char* noteName(int note) {
  static const char* NAMES[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
  };
  return NAMES[note % 12];
}
