# EOS・2～3人オンライン協力プレイ

## 現在の実装

- ステージセレクトは `ArchiveScene`。画面下のランタイムUIから、接続・ロビー作成・検索・参加・退出・準備完了・開始をマウスで操作します。Releaseでも表示されます。
- 定員は3人。2人または3人の参加者全員が「準備完了」にすると、ホストが選択ステージを開始できます。ステージ変更はホストが行い、参加者へ同期します。
- EOS ConnectのDevice IDで端末ごとにログインし、EOS LobbyとEOS P2Pを使用します。ゲーム内でEpicアカウント入力やロビーコード入力はありません。
- ホストが毎秒30回、参加中の2～3人の入力をまとめてReliable Orderedで配信します。各端末は同じ固定時間で地形・罠・スイッチ・復活・足場を処理します。ステージファイルのハッシュと各フレームの状態ハッシュを照合し、不一致は画面に表示して終了します。
- オンライン用シーンは `OnlineGamePlayScene`。参加人数に応じた2～3色のスライムと個別カメラ、各10命、共有チェックポイント、誰か1人のゴールで全員クリアです。形状作成中も世界全体はスローになりません。硬化足場は確定した当たり判定形状から生成します。1人用のGPU流体による死体形状・残機ルールは既存の `GamePlayScene` に残しています。
- ホスト移譲・途中参加・途中復帰は行いません。切断や残機切れの際はロビーを作り直します。2～3台すべてで同じGitリビジョンを使用してください。異なるビルド間の決定性やインターネット上の実測遅延は未検証です。

## 1. EOS SDK

GitにはWindows x64版のビルドと実行に必要なファイルだけを含めています。リポジトリをpullすると次の構成が取得されるため、各開発者がSDKを別途配置する必要はありません。

```text
externals/EOS-Win64/
  Include/eos_sdk.h
  Include/eos_connect.h
  Include/eos_lobby.h
  Include/eos_p2p.h
  Lib/EOSSDK-Win64-Shipping.lib
  Bin/EOSSDK-Win64-Shipping.dll
```

`Tools/EOS.props` がEOSを有効化し、DLLを出力先へコピーします。ReleaseではSDKが不足しているとビルドを明示的に失敗させ、EOSなしのRelease版が誤って作られることを防ぎます。

SDKを更新する場合は、Windows x64用のInclude、lib、DLL、ThirdPartyNoticesを `externals/EOS-Win64` へ反映し、クリーンして再ビルドしてください。

## 2. Developer Portalの設定

1. [Developer Portal](https://dev.epicgames.com/portal/)でこのゲーム用のProductを作成します。
2. 使用するSandboxとDeploymentを作成または選択します。全端末で同じ値を使用します。
3. EOS Game Servicesを使うゲームクライアントを作成し、Client PolicyでConnect、Lobby、P2Pに必要な機能を許可します。開発中はPeer-to-peer向けポリシーを基に、ロビー作成・検索・参加・属性更新とP2P接続が可能な設定にしてください。Portalの現行UIと公式資料で権限を確認してください。
4. ConnectでDevice IDの認証を利用できるようにします。この実装はWindows端末用のゲスト認証です。同じWindowsユーザーで複数起動すると同一Product User IDになるため、複数人試験には別々のPCを使ってください。

参照: [Lobby公式ガイド](https://dev.epicgames.com/docs/epic-online-services/multiplayer/lobbies-and-sessions/lobby-interface/lobbies-guide/lobbies-guide-intro)、[P2P公式ガイド](https://dev.epicgames.com/docs/epic-online-services/multiplayer/nat-p2p-interface)。SDK同梱のConnect/Lobbies/P2Pサンプルも参照してください。

## 3. 接続設定

共有するゲームクライアント用設定は `resources/Config/eos.json` にあります。別のProductを使う開発者は `resources/Config/eos.example.json` を `resources/Config/eos.local.json` にコピーして値を入力すると、共有設定より優先できます。

```json
{
  "productId": "Product ID",
  "sandboxId": "Sandbox ID",
  "deploymentId": "Deployment ID",
  "clientId": "Client ID",
  "clientSecret": "Client Secret"
}
```

`eos.local.json` はGit管理対象外です。実行時は実行ファイルと作業ディレクトリの親フォルダを探索し、`eos.local.json`、`eos.json` の順で読みます。Visual Studioからの起動とexeの直接起動のどちらでも同じ設定を利用できます。

## 4. 2～3台での確認手順

1. 同じ構成でビルドしたexe、DLL、resourcesを2～3台へ配置します。SDK有効ビルドが成功したことを確認します。
2. 各PCでタイトルをクリックしてステージセレクトへ進み、「オンラインに接続」をクリックします。
3. 1台で「ロビーを作る」、残る1～2台で「参加 / 一覧を更新」→一覧の「参加」をクリックします。ロビー名・コードの入力は不要です。
4. 全端末で2人または3人の表示を確認します。ホストが前後のステージボタンをクリックし、参加者側にも選択が反映されることを確認します。
5. 参加者全員が「準備完了」をクリックします。ホストが「2人でこのステージを開始」または「3人でこのステージを開始」をクリックします。
6. P1は緑、P2は青、P3は橙です。各PCでそれぞれ移動・ジャンプ・硬化を行い、ほかの端末へ反映されることを確認します。罠・感圧板・ジャンプ台・硬化足場・チェックポイントも参加者それぞれで確認します。
7. 誰かがゴールした際の全員クリアと、画面右上のボタンでステージセレクトへ戻れることを確認します。
8. ロビーの満員、準備の取り消し、検索後にロビーが消えた場合の参加失敗、ホスト終了、参加者の通信断を確認します。読み込み待ちは60秒、プレイ中のP2P応答待ちは15秒でタイムアウトします。
9. 1台のステージファイルだけ変更し、ゲーム開始時に不一致を検出することを確認します。

## ローカル検証

通信データの検証テストは `Tools/Tests/OnlineProtocolTests.vcxproj` をRelease/x64でビルドし、`generated/eos-tests/OnlineProtocolTests.exe` を実行します。2人・3人分のCBOR変換、入力トリガーの保持・消費、旧マッチID、不正パケット、範囲外入力、NaN/Infinityを検証します。

EOS SDKとPortalの設定が未配置の環境では、実サービスへのログイン、ロビー検索・参加、複数端末間通信を検証できません。SDK有効ビルドと上記の2～3台試験は、設定後に必ず実施してください。
