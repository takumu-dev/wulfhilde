// Evaluates the renderer's sRGB encoding over a table of colours.
#language slang 2026
import color;

struct SrgbTable {
    uint row_count;
    float3* inputs;
    float3* results;
};

[vk::push_constant] ConstantBuffer<SrgbTable> table;

[shader("compute")]
[numthreads(64, 1, 1)]
void srgb_encoded_colors(uint3 id : SV_DispatchThreadID) {
    if (id.x < table.row_count) {
        table.results[id.x] = srgb_encoded(table.inputs[id.x]);
    }
}
