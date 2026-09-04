# white.pngによる炎

GAMELABで `Flame (white.png)` をオンにすると4層をまとめて再生します。
`Focus Flame` で炎にカメラを向け、`Flame Position` で発生位置を動かせます。
オフにすると新規発生を止め、残った粒子と光が消えます。

| エフェクト | 役割 | 合成 |
|---|---|---|
| FlameSmoke | 薄い煙 | Normal |
| Flame | 上昇する柔らかい丸形の粒子 | Normal |
| FlameCore | 黄色い芯・ポイントライト | Add |
| FlameSparks | 火の粉 | Add |

全層の画像は `resources/Textures/white.png` です。形はシェーダーで生成します。
新しい画像やテクスチャアニメーションは不要です。

各フォルダの Effect.json で次の値を調整し、ゲームを再起動してください。

- Emitter.Count / Frequency: 1回の発生数 / 発生間隔（秒）
- Emitter.Radius: 発生する円盤の半径
- Particle.StartScale / EndScale: 粒子の幅。炎本体と芯は縦横同じ大きさで描画
- Particle.Velocity / LifeTime: 上昇速度 / 最大寿命（秒）
- Particle.StartColor / EndColor: 色の倍率・透明度
- Simulation.NoiseStrength: 横揺れの強さ
- Simulation.EnableAttraction / AttractionStrength: 炎本体と芯を上昇につれて発生位置の中心軸へ寄せる。強いほど上部が細くなる
- Render.UvScrollSpeedX / UvScrollSpeedY: 輪郭ノイズの流れる速さ
- Render.DissolveThreshold: 炎本体と芯の透明度を下げる量（大きくすると薄くなる）
- Render.EmissionStrength: 明るさ
- Render.StopTailDuration: 消火後の更新時間。LifeTime以上に設定
- FlameCoreのLight: 色・強度・照射半径・消灯時間

Circleの発生位置はこの炎専用シェーダーでは円周ではなく円盤内に分散します。
地面や物体へのソフトな交差処理、熱による背景の歪みは含みません。
通常合成の煙・炎は厳密な粒子単位の奥行きソートを行っていないため、透明度を低めにしています。
ポイントライトはライティングを有効にしたオブジェクトに反映されます。
