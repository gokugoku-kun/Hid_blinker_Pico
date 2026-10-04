# Pico W HID LED Controller

PCからUSB HIDでPico Wの内蔵LEDと外付けLEDを個別にON／OFF／点滅させるファームウェア。

## 実装内容

- LED 0：Pico W内蔵LED。
- LED 1～4：GP2、GP3、GP4、GP5。GPIOがHIGHで点灯する配線。
- LEDごとに独立した点滅。点灯・消灯時間は各10ms～1時間。
- 通信が止まっても、給電中は最後の動作を継続。通信タイムアウト監視なし。
- ハードウェアWDT：初期化中5秒、通常動作中2秒。
- 起動・WDT再起動後は全消灯。
- 個人試作用VID／PID：`1209:0001`。製品名：`Hid_blinker_Pico`。

USB識別値は共有のテスト専用であり、配布・販売する機器には使用しない。[使用条件](https://pid.codes/1209/0001/)

通信データの定義は[HID_PROTOCOL.md](HID_PROTOCOL.md)、設計方針は[BASIC_DESIGN.md](BASIC_DESIGN.md)を参照。

## ビルド

必要な環境はPico SDK 2.2.0以降、SDKのTinyUSB／CYW43サブモジュール、ARM GCC、CMake、対応するビルドツール。現環境ではPico SDK 2.2.0とARM GCC 10.3.1を使用する。

リポジトリ直下から実行する。

```powershell
cmake -S Hid_blinker_Pico -B Hid_blinker_Pico/build -DCMAKE_BUILD_TYPE=Release
cmake --build Hid_blinker_Pico/build --parallel 4
```

新しいWindows環境でMinGW Makefilesを使用する場合は、初回の設定時に`-G "MinGW Makefiles"`を追加する。SDKの場所を変更する場合は`-DPICO_SDK_PATH=<SDKのパス>`または環境変数`PICO_SDK_PATH`を指定する。既存のビルドディレクトリに設定済みの生成方式やコンパイラと混在させない。

生成ファイル：`Hid_blinker_Pico/build/Hid_blinker_Pico.uf2`

## 書き込みと配線

1. Pico WのBOOTSELボタンを押しながら、データ通信対応のUSBケーブルでPCに接続する。
2. `RPI-RP2`ドライブへ生成したUF2をコピーする。
3. 再起動後はHID機器として動作する。全LEDは消灯した状態で命令を待つ。

外付けLEDはGPIO→電流制限抵抗→LED→GNDの順に接続する。抵抗値は使用するLEDの順方向電圧と目標電流に合わせて決める。USB給電を抜けばPico自体も停止する。通信を切っても動作を続ける実験には、適切に構成した別給電が必要。

UARTログはSDK標準設定のGP0（TX）／GP1（RX）を使用する。USB CDC／USBシリアルは有効にしていない。

## 設定変更

`Hid_blinker_Pico/app_config.h`にGPIO割り当て、LED個数、時間範囲、WDT期限、USB識別値・文字列をまとめている。

外付けLEDを増減する場合は`APP_EXTERNAL_LED_PINS`と`APP_EXTERNAL_LED_COUNT`を揃えて変更する。LED 0は内蔵LED、外付けLEDはピン配列の順に1から割り当てる。GPIOはPico Wで使用可能な端子から選び、UART用端子などと重複させない。

## テスト

PC上でLED状態管理・命令処理を検証できる。PicoやUSB機器を接続せず実行する。

```powershell
./tests/run_tests.ps1
./tests/run_usb_transport_tests.ps1
```

ホスト用GCCが必要。出力は`Hid_blinker_Pico/build/host-tests/`に作成する。ARM向けコンパイラで生成した実行ファイルはPC上では実行できない。

これらのテストは実際のUSB列挙、GPIO電圧、CYW43の動作、WDTの実時間を確認するものではない。実機では基本設計の確認項目に沿って、個別LED操作、Python停止後の点滅継続、主処理停止からの復帰を確認する。

WDTはデバッガ停止中も動作する設定。ブレークポイントで長時間停止すると再起動する。WDT試験は開発用の一時的な無限ループなどで主処理の停止を作り、通常動作のファームウェアへ戻して確認する。

## ファイル構成

| ファイル | 役割 |
|---|---|
| `Hid_blinker_Pico.c` | 初期化、GPIO出力、メインループ、WDT |
| `app_config.h` | ハードウェア・通信の設定 |
| `led_controller.c/.h` | LEDモードと点滅位相の管理 |
| `led_protocol.c/.h` | 命令の検証・適用、応答の生成 |
| `usb_descriptors.c` | USB／HID記述子、製品名、シリアル番号 |
| `usb_transport.c/.h` | HID受信キューと応答送信 |
| `tusb_config.h` | TinyUSBの構成 |

上記ソースは`Hid_blinker_Pico/`内にある。Python側の操作クラスは未実装。
