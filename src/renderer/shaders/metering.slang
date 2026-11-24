// Automatic exposure: a histogram of the scene's luminance, from the first sample of each pixel, in
// bins across the map's EV100 range, then the exposure it meters, which the frame's exposure moves
// toward at the adaptation speed.
#language slang 2026
import bindless;
import color;
import exposure;

struct MeteringConstants {
    // The scene's colour target, by its index among the bindless set's sampled images.
    uint2 scene;
    // How many of the scene's pixels fall in each bin, which metering empties again.
    uint* histogram;
    // The frame's exposure, in EV100, which metering moves and the AgX pass reads.
    float* ev100;
    // The scene's size in pixels.
    uint2 extent;
    // The map's range, in EV100, which the bins span from the first to the last.
    float min_ev100;
    float max_ev100;
    // The stops by which the metered exposure is brightened.
    float compensation;
    // How much of the way from its exposure to the metered one the frame moves: 1 to take it.
    float blend;
};

[vk::push_constant] ConstantBuffer<MeteringConstants> metering;

static const uint bin_count = 256;

// The pixels on each side of the tile that one group counts, a thread a pixel and a bin.
static const uint tile = 16;

groupshared uint group_bins[bin_count];

// The bin of a pixel's light, those beyond the map's range falling in the first or the last.
uint bin_of(float3 light) {
    let span = max(metering.max_ev100 - metering.min_ev100, 1e-6);
    let place = saturate((ev100_of(luminance(light)) - metering.min_ev100) / span);
    return uint(round(place * float(bin_count - 1)));
}

// A tile is counted in its group's own bins first, then into the histogram.
void begin_tile(uint thread) {
    group_bins[thread] = 0;
    GroupMemoryBarrierWithGroupSync();
}

void end_tile(uint thread) {
    GroupMemoryBarrierWithGroupSync();
    if (group_bins[thread] != 0) {
        InterlockedAdd(metering.histogram[thread], group_bins[thread]);
    }
}

// For a scene of more than one sample a pixel, counting each pixel's first.
[shader("compute")]
[numthreads(tile, tile, 1)]
void gather_samples(uint3 pixel : SV_DispatchThreadID, uint thread : SV_GroupIndex) {
    begin_tile(thread);
    if (all(pixel.xy < metering.extent)) {
        let scene = DescriptorHandle<Texture2DMS<float4>>(metering.scene);
        InterlockedAdd(group_bins[bin_of(scene.Load(int2(pixel.xy), 0).rgb)], 1);
    }
    end_tile(thread);
}

// For a scene of one sample a pixel, which isn't a multisampled image.
[shader("compute")]
[numthreads(tile, tile, 1)]
void gather_sample(uint3 pixel : SV_DispatchThreadID, uint thread : SV_GroupIndex) {
    begin_tile(thread);
    if (all(pixel.xy < metering.extent)) {
        let scene = DescriptorHandle<Texture2D<float4>>(metering.scene);
        InterlockedAdd(group_bins[bin_of(scene.Load(int3(int2(pixel.xy), 0)).rgb)], 1);
    }
    end_tile(thread);
}

groupshared float group_pixels[bin_count];
groupshared float group_weights[bin_count];

// Meters the histogram's average EV100, as the mean of its pixels' bins, and moves the frame's
// exposure toward it less the compensation, emptying the histogram for the next frame. One group
// of a thread a bin runs it.
[shader("compute")]
[numthreads(bin_count, 1, 1)]
void meter_exposure(uint bin : SV_GroupIndex) {
    let pixels = float(metering.histogram[bin]);
    metering.histogram[bin] = 0;
    group_pixels[bin] = pixels;
    group_weights[bin] = pixels * float(bin);
    GroupMemoryBarrierWithGroupSync();
    for (uint stride = bin_count / 2; stride > 0; stride /= 2) {
        if (bin < stride) {
            group_pixels[bin] += group_pixels[bin + stride];
            group_weights[bin] += group_weights[bin + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if (bin == 0 && group_pixels[0] > 0.0) {
        let place = group_weights[0] / group_pixels[0] / float(bin_count - 1);
        let metered = lerp(metering.min_ev100, metering.max_ev100, place) - metering.compensation;
        *metering.ev100 =
            metering.blend >= 1.0 ? metered : lerp(*metering.ev100, metered, metering.blend);
    }
}
