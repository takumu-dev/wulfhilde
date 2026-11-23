// The AgX pass, which is also the resolve: it exposes and tone-maps each sample of a pixel of the
// scene, then averages them in the display's light, so an edge against bright light stays smooth
// where an average of the scene's light would let the brightest sample win.
#language slang 2026
import agx;
import bindless;
import color;

struct ResolveConstants {
    // The scene's colour target, by its index among the bindless set's sampled images.
    uint2 scene;
    // What the scene's luminance, in nits, is multiplied by for AgX, which takes 0.18 as mid-grey.
    float exposure;
};

[vk::push_constant] ConstantBuffer<ResolveConstants> resolve;

// The samples each pixel of a multisampled scene holds.
[SpecializationConstant] const int sample_count = 4;

// Whether the target holds its colours as they are written, as a window's 8-bit image may, so that
// the pass encodes them in sRGB itself. A target of an sRGB format encodes them as it stores them.
[SpecializationConstant] const bool encodes_srgb = false;

// Vertices 0, 1 and 2 make a triangle that covers the whole target.
[shader("vertex")]
float4 cover(uint vertex : SV_VulkanVertexID) : SV_Position {
    let corner = float2(float((vertex << 1) & 2), float(vertex & 2));
    return float4(corner * 2.0 - 1.0, 0.0, 1.0);
}

float3 displayed(float3 luminance) {
    return agx(luminance * resolve.exposure);
}

float4 written(float3 color) {
    return float4(encodes_srgb ? srgb_encoded(color) : color, 1.0);
}

// For a scene of more than one sample a pixel.
[shader("fragment")]
float4 resolve_samples(float4 position : SV_Position) : SV_Target {
    let scene = DescriptorHandle<Texture2DMS<float4>>(resolve.scene);
    let pixel = int2(position.xy);
    var sum = float3(0.0);
    for (int sample = 0; sample < sample_count; ++sample) {
        sum += displayed(scene.Load(pixel, sample).rgb);
    }
    return written(sum / float(sample_count));
}

// For a scene of one sample a pixel, which isn't a multisampled image.
[shader("fragment")]
float4 resolve_sample(float4 position : SV_Position) : SV_Target {
    let scene = DescriptorHandle<Texture2D<float4>>(resolve.scene);
    return written(displayed(scene.Load(int3(int2(position.xy), 0)).rgb));
}
