// Evaluates the renderer's AgX tone mapping over a table of exposed colours.
#language slang 2026
import agx;

struct AgxTable {
    uint row_count;
    float3* inputs;
    float3* results;
};

[vk::push_constant] ConstantBuffer<AgxTable> table;

[shader("compute")]
[numthreads(64, 1, 1)]
void agx_of_colors(uint3 id : SV_DispatchThreadID) {
    if (id.x < table.row_count) {
        table.results[id.x] = agx(table.inputs[id.x]);
    }
}
