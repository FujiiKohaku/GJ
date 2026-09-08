#include "../Common/SpriteRenderCommon.hlsli"

SpriteVertexOutput main(SpriteVertexInput input)
{
    SpriteVertexOutput output;

    // input.position.xy は 0.0 ~ 1.0 (アンカーが0,0の場合) または -0.5 ~ 0.5 
    // Sprite側でアンカーを(0.5, 0.5)に設定する前提。
    // gEffect.phase を進行度(progress)として使用する。
    
    float progress = gEffect.phase; 
    
    // 矢印の全長と太さ
    float arrowLength = gEffect.spriteSize.x;
    float arrowThickness = gEffect.spriteSize.y;
    
    // ローカルのX座標を物理的な距離に変換
    float localX = input.position.x * arrowLength;
    
    // 進行度に応じて矢印全体を右から左へ移動させる
    // progressが0のとき右端、1のとき左端へ抜けるようにする
    float startOffset = 2500.0f;  // 開始時の右方向オフセット
    float endOffset = -2500.0f; // 終了時の左方向オフセット
    float currentOffset = lerp(startOffset, endOffset, progress);
    
    // パス上の絶対座標 t (t=0 が画面中央のループ位置)
    float t = localX + currentOffset;
    
    // ループの半径とパラメータ
    // 画面に収まるように半径を 90.0f に縮小
    float R = 90.0f;
    float L = 3.141592f * R; // 半円周長
    
    float deformedX = 0.0f;
    float deformedY = 0.0f;
    
    if (t > L) {
        // 右側の直線部分
        deformedX = t - L;
        deformedY = 0.0f;
    } else if (t < -L) {
        // 左側の直線部分
        deformedX = t + L;
        deformedY = 0.0f;
    } else {
        // 完璧な円の軌道（右から左へ 0 -> 2π）
        float theta = 3.141592f * (L - t) / L; 
        deformedX = -R * sin(theta);
        deformedY = -R * (1.0f - cos(theta));
    }
    
    float2 deformedPos = float2(deformedX, deformedY);
    
    // 線の太さ（Y方向のオフセット）を法線方向に適用するための接線計算
    float delta = 1.0f;
    float t2 = t + delta;
    float dX2 = 0.0f;
    float dY2 = 0.0f;
    
    if (t2 > L) {
        dX2 = t2 - L;
        dY2 = 0.0f;
    } else if (t2 < -L) {
        dX2 = t2 + L;
        dY2 = 0.0f;
    } else {
        float theta2 = 3.141592f * (L - t2) / L;
        dX2 = -R * sin(theta2);
        dY2 = -R * (1.0f - cos(theta2));
    }
    
    float2 tangent = normalize(float2(dX2 - deformedPos.x, dY2 - deformedPos.y));
    float2 normal = float2(-tangent.y, tangent.x);
    
    // 太さを加算
    float localY = input.position.y * arrowThickness;
    deformedPos += normal * localY;
    
    // WVP行列によるスケールとアンカーオフセット（-0.5）を相殺するため、
    // 変形後の座標を正規化し、+0.5 することで Spriteの position_ (画面中央) にループの起点が来るようにする。
    float4 finalLocalPos = float4(
        (deformedPos.x / arrowLength) + 0.5f, 
        (deformedPos.y / arrowThickness) + 0.5f, 
        0.0f, 1.0f);
    
    output.position = mul(finalLocalPos, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    
    return output;
}
