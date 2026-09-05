# GJ3 → KohakuEngine アーカイブのステージセレクト移植計画

作成日：2026-09-04。ソース確認に基づく実装前の計画。コード変更・リソースコピー・ビルドは未実施。

## 1. 完成させる動作

- KohakuEngineのタイトル文字・案内表示は現状を維持する。GJ3のタイトル文字・案内表示は移植しない。
- KohakuEngineのプレイヤー周回、追従カメラ、海、障害物の描画は維持する。
- スタートすると、本が開くアーカイブ形式のステージセレクトへ進む。
- 本のページをめくって、KohakuEngine側のステージを選択・開始する。
- 戻る操作では、プレイヤーが動くKohakuEngineのタイトルへ戻る。
- 必要なモデル・画像・音声・シェーダーも移行する。

移植元：`C:\project\GJ3`  
移植先：`C:\project\MyEngine\project`（KohakuEngine.slnを確認済み）

## 2. 現状と設計方針

GJ3には独立したTitleSceneはなく、`App/Scene/ArchiveScene.cpp/.h`がタイトル待機と本の選択画面を兼ねている。起動時もArchiveSceneへ遷移する。残存する`StageSelectScene.cpp`はGJ.vcxprojの登録対象ではなく、今回の移植元はArchiveSceneとする。

KohakuEngineはTitleSceneとStageSelectSceneが分離済み。TitleSceneは魚モデルを18秒でレール周回させ、4方向へカメラを切り替えている。StageSelectSceneはStageCatalogを読み込むカード一覧で、ステージIDをLoadingScene経由でGamePlaySceneへ渡している。

**移植方針：ArchiveSceneから本の選択画面に必要な部分を取り出し、既存のStageSelectSceneへ組み込む。TitleSceneは現状を維持する。**

| GJ3の機能 | KohakuEngineでの配置・扱い |
|---|---|
| TitleIdleのタイトル文字・開始案内 | 移植対象外 |
| 資料庫の部屋・本・紙・金具・浮遊する埃 | StageSelectSceneの3D表示へ移植 |
| カメラ接近・本を開く・ページめくり・決定演出 | StageSelectSceneへ移植 |
| ReturningToTitle | 戻り演出の完了後にTitleSceneへ遷移 |
| GJ3固定のGamePlay／Test／GameLab一覧 | KohakuEngineのStageCatalogと既存テスト項目に接続 |
| GJ3のEditorSceneへのF12遷移 | 移植対象外。移植先に不要な依存を持ち込まない |

KohakuEngineの既存ロゴ・操作案内・プレイヤー背景・NeonGlowを維持する。資料庫背景とArchiveAtmosphereはステージセレクト側に限定する。

## 3. 実装手順

### 手順1：変更前の記録とリソース整理

移植先の作業中差分を確認し、タイトルの動作・画面を記録する。同名リソースは内容を比較し、同一なら既存を再利用、異なる場合は専用パスに分ける。モデルのMTLや画像への相対参照も追跡する。

### 手順2：本の描画に必要な機能を追加

- GJ3の`Engine/3D/ModelManager.cpp/.h`にある`CreateBookLeaf`と必要な依存処理を移植する。移植先には同名機能が見当たらないため、両面のページ画像・分割メッシュ・キャッシュキーを含めて確認する。
- `resources/Shaders/Object3D/Object3d.PS.hlsl`の紙・革・真鍮用処理を移植する。GJ3はenableLightingの3～5を専用質感に使用している。移植先の既存照明処理を保ったまま専用分岐を追加する。
- ArchiveAtmosphereをPostEffectType、CopyImageRendererのシェーダー選択、PostEffectManagerのパラメータ転送、SceneManagerの進行値管理へ追加する。
- CPUとHLSLの定数バッファの配置を照合する。GJ3のシェーダーはpadding0～2を資料庫用に使用しているため、移植先へのファイル丸ごとの上書きは避ける。
- 深度テクスチャの接続、初期値、シーン退出時のポストエフェクト解除・進行値リセットを整える。

### 手順3：既存TitleSceneからの接続を確認

`TitleScene.cpp/.h`は変更対象に含めない。既存のロゴ・操作案内、`UpdatePlayerShowcase`、レール生成、海の更新、障害物、カメラ計算を維持し、Enter／Spaceから既存StageSelectSceneへの遷移をそのまま使う。

### 手順4：StageSelectSceneを本の選択画面へ変更

`StageSelectScene.cpp/.h`にArchiveSceneのオブジェクトと状態遷移を組み込む。入口はTitleIdleで待たず、自動的にカメラ接近・本の開閉演出へ進める。元のEnterTitleModeには遷移用Spriteの生成も含まれるため、必要な初期化を分離してからタイトル待機を省く。

ステージ名・説明・IDはStageCatalogから取得する。ページ画像は装飾、選択項目はカタログとして扱い、画像枚数とステージ数を混同しない。画像はファイル名順で読み込み、ページ不足・奇数枚でも安全に表示する。カタログが空／読み込み失敗の場合は説明と戻る操作を表示する。

操作は左右／A・Dでページ切替、Enter／Spaceで決定、Backspaceでタイトル復帰とする。既存のマウス操作も、本の左右領域・選択カードに割り当てて残す。開閉・ページめくり・決定中は入力の二重処理を防ぐ。

F1のゲームテスト、F2のスプライトテスト、F3のテキストテストとgimmick_testへの入口は維持する。通常ステージとテスト項目の表示順・分類は既存仕様を引き継ぐ。

### 手順5：決定演出と既存ロード処理を接続

選択確定時にステージIDを保持し、紙面へ入る演出の終了後に`SetNextSceneWithLoading<LoadingScene, GamePlayScene>(stageId)`を一度だけ呼ぶ。GJ3の引数なしGamePlayScene生成はコピーしない。

`PageTransition.h`の紙色フェードは必要な部分を移植し、LoadingSceneまたは共通遷移表示で受け取る。移植先のローディング表示との描画順を調整し、フェード要求が未消費のまま次回に残らないようにする。ゲーム本体のルールは変更しない。

戻り演出の終点ではEnterTitleModeへ戻さず、KohakuEngineのTitleSceneへ切り替える。カメラ参照・音声・ポストエフェクトの後始末を行い、再入場時も正しく初期化する。

### 手順6：プロジェクト登録とビルド

追加ヘッダーや必要なソースを`CG2_LE2C_FUJII.vcxproj`とfiltersへ登録する。HLSLは移植先の`Tools/CompileShaders.ps1`を利用して再コンパイルする。GJ3のコンパイル済みシェーダーをそのまま流用しない。

## 4. リソース移行一覧

パスは各プロジェクトルートからの相対パス。同じ配置を基本とする。

| 移植元 | 移行内容 |
|---|---|
| resources/Models/StageSelectBook/ | 部屋・カードのOBJ／MTL、革・パレット・紙面画像、Pages内PNG・READMEを一式コピー。現状18ファイル、約4.5MB |
| resources/Audio/StageSelect/ | page_turn、page_flip、page_riffle、confirm、backのWAVとLICENSE.txt |
| resources/Fonts/NotoSansJP/ | 使用するTTFと関連ライセンス。移植先の同一ファイルは再利用 |
| resources/Textures/white.png | 同一なら移植先の既存ファイルを使用 |
| resources/Shaders/PostEffect/ArchiveAtmosphere.PS.hlsl | 新規追加。Fullscreen.hlsliの互換性を確認 |
| resources/Shaders/Object3D/Object3d.PS.hlsl | 専用質感の差分のみ統合 |
| resources/CompiledShaders/ | 移植先で再生成 |

GJ3のゲームステージ・プレイヤー・エディタ・クリア／ゲームオーバーシーンの素材は今回の移植対象外。実行時に追加依存が判明した場合のみ一覧に追記する。

## 5. 完了条件・確認方法

1. Debug／Release x64でC++とシェーダーのビルドが成功する。
2. タイトルのロゴ・操作案内が従来どおり表示され、プレイヤー周回・4方向カメラ・海が動く。変更前の記録と比較する。
3. スタート1回で本が開く選択画面へ進み、追加のスタート入力を要求しない。
4. 部屋・革・金具・紙の表裏・埃・効果音が表示／再生され、欠落ファイルがない。
5. 全ステージを選べて、選択したIDのゲームが既存LoadingScene経由で始まる。先頭／末尾、画像と項目の数が異なる場合も確認する。
6. テストシーンへの入口、マウス操作、戻る操作が機能する。連打中も遷移は一度だけ発生する。
7. タイトル→選択→ゲーム→選択→タイトルを往復し、カメラ・画面効果・フェード・音声が残留しない。
8. 多数の紙片を描画する演出中のフレーム時間を確認する。重い場合は描画されない紙片の更新・描画を省き、見た目を保つ。

## 6. 前提と実装時に解決する点

- ユーザーの修正指示により、GJ3のタイトル文字・案内表示の移植は対象外とする。KohakuEngineのタイトルは現状を維持し、資料庫の3D背景と本の選択演出をステージセレクトへ配置する。
- 実装の中心はシーンの分割と描画依存の移植。ファイルコピーだけでは同じ見た目にならない。
- 今回はソース調査まで。シェーダー全体の互換性、素材の競合、実機の描画負荷は実装時に確認する。
- 移植先は現在の書き込み許可範囲外。実装時に必要な書き込み許可を取得してから変更する。本計画書は書き込み可能なGJ3内へ保存する。
