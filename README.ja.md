[한국어](README.md) | [English](README.en.md) | **日本語**

# thod2-native

### [最新版をダウンロード (thod2-native.zip)](https://github.com/BangJeongHyeok/thod2-native/releases/latest/download/thod2-native.zip)
(任意) [高解像度UIテクスチャパック (thod2-native-hd-ui.zip、約400MB)](https://github.com/BangJeongHyeok/thod2-native/releases/download/hd-ui-1/thod2-native-hd-ui.zip) · 過去のバージョンと変更内容は [Releases](https://github.com/BangJeongHyeok/thod2-native/releases) にあります。

PC版『**ザ・ハウス・オブ・ザ・デッド2**』(SEGA、2001年)を最新のWindowsで快適に動かすためのModです。
元のゲームの上で動く方式で、**ゲームのファイルは含まれていません。**
お持ちのゲームのフォルダーにファイルを入れて使います。

## 何が変わるか

| 項目 | オリジナル | 導入後 |
|---|---|---|
| 画面 | モニターを640×480に切り替える全画面、4:3 | 枠なしの全画面ウィンドウ。モニターの解像度と縦横比はそのまま |
| 3D解像度 | 640×480 | 画面に収まる最大の解像度(1080pモニターで1440×1080)、アンチエイリアス |
| 高リフレッシュレート(任意) | 毎秒60フレーム | 144Hz・240Hzモニターのリフレッシュレートまでなめらかに(ゲーム速度は同じ) |
| メニュー・HUD・文字(任意) | 640×480用の低解像度の画像 | 高解像度UIテクスチャパックを入れると4倍の解像度でくっきり |
| CPU使用率 | コア1つが100% | 約25% |
| Alt+Enter | 反応しない | ウィンドウ ↔ 全画面の切り替え |
| マウス照準 | ウィンドウ表示では画面の左上しか狙えない | 画面全体を狙える |
| セーブ | ゲームフォルダーに書き込めないと保存に失敗する | `%APPDATA%\thod2-native\save\` に保存 |
| 起動時の確認ダイアログ | 毎回表示 | 表示されない |

## オリジナルとの比較
![タイトル画面の比較](docs/images/compare-title.jpg)
![タイトルメニューの比較](docs/images/compare-menu.jpg)
![遊び方画面の比較](docs/images/compare-tutorial.jpg)
![オープニングムービーの比較](docs/images/compare-cutscene.jpg)
![ゲーム画面の比較](docs/images/compare-gameplay.jpg)
![デモ画面の比較](docs/images/compare-alley.jpg)

## 必要なもの

1. **PC版『ザ・ハウス・オブ・ザ・デッド2』のゲームフォルダー**
   - 現在対応している `Hod2.exe` は1種類です: SHA-256 `1D71265096661CF034568FF71184FCBBED0CD9AEA0E5E2F041801C39A0FFCE69`
   - 別の版では、ゲームはそのまま起動し、thod2-nativeの機能だけが有効になりません。その場合は下の「問題の報告」からお知らせください。対応する版を増やすのに役立ちます。
2. [**dgVoodoo2** 公式リリース](https://github.com/dege-diosg/dgVoodoo2/releases)(確認したバージョン: 2.87.5)
   - 古いDirectX 7のゲームを最新のグラフィックカードで動かすツールです。
   ライセンスの都合でこのプロジェクトには含めていないため、別途ダウンロードしてください。

## インストール

1. 上の **最新版をダウンロード** から `thod2-native.zip` をダウンロードし、中のファイルをすべてゲームフォルダー(`Hod2.exe` と同じ場所)に展開します。
2. dgVoodoo2のzipをダウンロードし、中の次の3つのファイルをゲームフォルダーにコピーします。
   - `MS\x86\DDraw.dll`
   - `MS\x86\D3DImm.dll`
   - `dgVoodoo.conf`
3. `Hod2.exe` を起動します。ゲームフォルダーに `thod2-native.log` ができていれば動作しています。

### 高解像度UIテクスチャの適用(任意)

メニュー、HUD、案内の文字、字幕などの2D画像を4倍の解像度に置き換えます。上の比較画像の右側がこのパックを入れた状態です。

1. 一番上の [**高解像度UIテクスチャパック**](https://github.com/BangJeongHyeok/thod2-native/releases/download/hd-ui-1/thod2-native-hd-ui.zip)(`thod2-native-hd-ui.zip`)をダウンロードします。
2. ゲームフォルダー(`Hod2.exe` がある場所)に展開します。下のように `data\tex` フォルダーが `Hod2.exe` の隣に来れば完了です。
   Windowsの「すべて展開」は標準ではzipの名前の新しいフォルダーを作るので、展開先をゲームフォルダーに変えてください。
   ```text
   THOTD2\
     Hod2.exe
     thod2_core.dll
     data\
       tex\
         common\
         scr_common\
         ... (フォルダー119個、画像3,110枚)
   ```
3. ゲームを起動するとすぐに適用されます。設定の変更は必要ありません。

- thod2-nativeを導入した状態でのみ動作します。元のゲームだけではこのフォルダーを読み込みません。
- 適用されたかどうかは `thod2-native.log` で確認できます。`replace_textures: texture 10B1 512x32 -> 2048x128` のように、置き換えたテクスチャが1行ずつ記録されます。
- 元の画像に戻すには、`data` フォルダーを削除するか、`thod2-native.ini` の `[assets]` にある `replace_textures = 1` を `0` に変えます。
- thod2-nativeを更新しても、テクスチャパックをダウンロードし直す必要はありません。

## 設定

ゲームフォルダーの `thod2-native.ini` をメモ帳で開いて変更し、保存してからゲームを起動し直します。よく使うもの:

| やりたいこと | 変更する行 |
|---|---|
| ワイドスクリーン | `[display]` の下に `widescreen = 1` |
| 高リフレッシュレートのモニターでなめらかに | `[display]` の下に `interpolate = 1`(60Hzのモニターでは有効にしないでください) |
| アンチエイリアスの強さ | `[dgvoodoo]` の下の `antialiasing = ` の後に `2x`、`4x`(標準)、`8x`、`16x` のどれか。数字が大きいほど縁がなめらかになり、グラフィックカードの負荷が増えます。`16x` はグラフィックカードが対応している場合のみ |
| アンチエイリアスを切る | `[dgvoodoo]` の下に `antialiasing = off` |
| テクスチャをなめらかに | `[dgvoodoo]` の下に `filtering = 16` |
| dgVoodoo2を自分で設定する | `[dgvoodoo]` の下に `preset = 0` |

すべての設定は [docs/INSTALL.md](docs/INSTALL.md)(韓国語)の「설정」(設定)にあります。

## 更新と元に戻す方法

- **更新:** 新しい `thod2-native.zip` を同じ方法で展開します。`thod2-native.ini` を上書きするか聞かれたら、変更した設定を残すには「いいえ」を選びます。
- **元に戻す:** ゲームフォルダーから `dinput.dll`、`thod2_core.dll`、`DDraw.dll`、`D3DImm.dll`(テクスチャパックを入れた場合は `data` フォルダーも)を削除します。セーブは `%APPDATA%\thod2-native\save\` に残ります。元のゲームで続きを遊ぶには、その中のファイルをゲームフォルダーの同じ場所に上書きします。

## まだ確認できていないこと

以下は開発中に確認できなかった部分です。おかしな点に気づいたら、ぜひお知らせください。

- ゲームを最初から最後まで通してプレイすること(特にボス戦、ステージの結果画面)
- 開発に使ったPC以外のPC、別のグラフィックカード、Windowsの表示スケール(125%、150%など)
- 60Hz・144Hzモニターでの高リフレッシュレート設定(240Hzでのみ確認)
- ワイドスクリーンで敵が画面の左右から大勢出てくる場面、スコアやボスゲージなど一部のHUDの位置
- ガンコントローラー、ゲームパッド

## 問題の報告

不具合や要望はGitHubの **Issue** でお知らせください。フォームは韓国語ですが、日本語で書いていただいて構いません。
   - [**問題の報告**](https://github.com/BangJeongHyeok/thod2-native/issues/new?template=bug_report.yml): 起動しない、画面や操作がおかしい、ゲームが落ちるとき
   - [**自分のHod2.exeが対応していない**](https://github.com/BangJeongHyeok/thod2-native/issues/new?template=unsupported_exe.yml): ログに `unknown host executable` と出るとき

**ゲームを終了してから、ゲームフォルダーの `thod2-native.log` ファイルを「thod2-native.log」の欄にドラッグして** ください。スクリーンショットも同じようにドラッグすればアップロードできます。
**ゲームのファイル(exe、binなど)はアップロードしないでください。**

## 開発

プロジェクトの資料は韓国語です。

- 目標と段階: [docs/ROADMAP.md](docs/ROADMAP.md)
- ビルド方法と開発用の設定: [docs/work/build.md](docs/work/build.md)
- 解析結果と作業記録: [docs/work/](docs/work/README.md)、[AGENTS.md](AGENTS.md)
- 自分でビルドしてインストールする方法: [docs/INSTALL.md](docs/INSTALL.md)

## ライセンス

[CC BY-NC-SA 4.0](LICENSE)(表示 - 非営利 - 継承)。

- 改変して再配布してもかまいません。このプロジェクトを出典として示し、同じライセンスで公開してください。
- **商用利用はできません。** このプロジェクト自体が原作をもとにした二次創作物だからです。

The House of the Deadはセガの商標です。このプロジェクトはセガとは関係ありません。
