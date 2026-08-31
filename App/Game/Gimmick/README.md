# マップチップギミックの追加方法

CSVの番号は次の意味です。

- `0`: 空白
- `1`: 通常ブロック
- `2`: 上下移動ブロック

新しいギミックを追加するときは、次の3か所を変更します。

1. `MapChipField.h`の`MapChipType`へ新しい番号を追加する。
2. `BaseMapChipGimmick`を継承したクラスをこのフォルダーへ追加する。
3. `MapChipGimmickFactory.cpp`の`switch`へ生成処理を追加する。

最後にマップCSVの設置したいマスへ、そのギミック番号を書きます。
`GamePlayScene`側の変更は不要です。

ギミックの描画に`Object3d`を利用しても、`Engine/3D/Object3d`本体へ
ギミック固有処理を追加しないでください。
