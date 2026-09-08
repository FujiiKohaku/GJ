#include "../Common/SpriteRenderCommon.hlsli"

SpriteVertexOutput main(SpriteVertexInput input)
{
    SpriteVertexOutput output;
    float time = gFrame.elapsedTime * gEffect.speed + gEffect.phase;
    float softPulse = sin(time);
    float afterPulse = sin(time * 1.73f + 0.8f) * 0.28f;
    float jellyPulse = (softPulse + afterPulse) * gEffect.amplitude;

    // Squash and stretch around the bottom-center, keeping the row grounded.
    float horizontalScale = 1.0f - jellyPulse * 0.48f;
    float verticalScale = 1.0f + jellyPulse * 0.62f;
    float4 position = input.position;
    position.x = 0.5f + (position.x - 0.5f) * horizontalScale;
    position.y = 1.0f + (position.y - 1.0f) * verticalScale;

    // The upper half trails sideways a little, producing a soft gelatin wobble
    // instead of moving the icon as one rigid rectangle.
    float upperWeight = sin(input.texcoord.y * 3.14159265f);
    float sideWobble = sin(time * 1.31f + input.texcoord.y * 2.4f) *
        gEffect.amplitude * 0.18f * upperWeight;
    position.x += sideWobble;

    output.position = mul(position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    return output;
}
