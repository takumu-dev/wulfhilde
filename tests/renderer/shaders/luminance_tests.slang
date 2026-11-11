// Evaluates the renderer's luminance function over a table of colours.
#language slang 2026
import color;

struct LuminanceTable {
    uint row_count;
    float3* inputs;
    float* results;
};

[vk::push_constant] ConstantBuffer<LuminanceTable> table;

[shader("compute")]
[numthreads(64, 1, 1)]
void luminance_of_colors(uint3 id : SV_DispatchThreadID) {
    if (id.x < table.row_count) {
        table.results[id.x] = luminance(table.inputs[id.x]);
    }
}
