// Evaluates the renderer's BRDF over a table of materials, normals and the ways to the eye and to a
// light.
#language slang 2026
import brdf;

struct BrdfRow {
    float3 base_color;
    float metallic;
    float roughness;
    float3 normal;
    float3 toward_eye;
    float3 toward_light;
};

struct BrdfTable {
    uint row_count;
    BrdfRow* inputs;
    float3* results;
};

[vk::push_constant] ConstantBuffer<BrdfTable> table;

[shader("compute")]
[numthreads(64, 1, 1)]
void brdf_of_rows(uint3 id : SV_DispatchThreadID) {
    if (id.x < table.row_count) {
        let row = table.inputs[id.x];
        let response = Response(row.base_color, row.metallic, row.roughness);
        table.results[id.x] = brdf(response, normalize(row.normal), normalize(row.toward_eye),
                                   normalize(row.toward_light));
    }
}
