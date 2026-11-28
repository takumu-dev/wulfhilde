// Evaluates the renderer's geometric specular anti-aliasing over a table of roughnesses and the
// changes of the shading normal from one pixel to the next.
#language slang 2026
import brdf;

struct WideningRow {
    float roughness;
    float3 normal_dx;
    float3 normal_dy;
};

struct WideningTable {
    uint row_count;
    WideningRow* inputs;
    float* results;
};

[vk::push_constant] ConstantBuffer<WideningTable> table;

[shader("compute")]
[numthreads(64, 1, 1)]
void widened_roughness_of_rows(uint3 id : SV_DispatchThreadID) {
    if (id.x < table.row_count) {
        let row = table.inputs[id.x];
        table.results[id.x] = widened_roughness(row.roughness, row.normal_dx, row.normal_dy);
    }
}
