// A fixed pattern that proves the graphics path: a triangle covering the whole target, sampling a
// texture through the bindless set. Each row of texels is darker than the one above, in linear
// light, from full brightness at the top to a quarter at the bottom, so that the target's sRGB
// encoding shows. How many rows the texture has is a specialisation constant.
#language slang 2026
import bindless;

struct PatternConstants {
    DescriptorHandle<Texture2D> texture;
    DescriptorHandle<SamplerState> sampler;
};

[vk::push_constant] ConstantBuffer<PatternConstants> pattern;

// With one row, as by default, nothing is darkened.
[SpecializationConstant] const uint texel_rows = 1;

struct Covering {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// Vertices 0, 1 and 2 make a triangle twice the target's size, whose part inside the target runs
// from (0, 0) at its top left to (1, 1) at its bottom right.
[shader("vertex")]
Covering cover(uint vertex : SV_VulkanVertexID) {
    let uv = float2(float((vertex << 1) & 2), float(vertex & 2));
    Covering covering;
    covering.position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    covering.uv = uv;
    return covering;
}

[shader("fragment")]
float4 paint(Covering covering) : SV_Target {
    let texel = pattern.texture.Sample(pattern.sampler, covering.uv);
    let last_row = float(texel_rows - 1);
    let row = min(floor(covering.uv.y * float(texel_rows)), last_row);
    let darkening = texel_rows > 1 ? row / last_row : 0.0;
    return float4(texel.rgb * lerp(1.0, 0.25, darkening), texel.a);
}
