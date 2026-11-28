// Evaluates how much of uniform ambient light the renderer's materials reflect, over a table of
// materials and the cosines of the angle between the normal and the way to the eye.
#language slang 2026
import brdf;

struct AmbientRow {
    float3 base_color;
    float metallic;
    float roughness;
    float normal_dot_eye;
};

struct AmbientTable {
    uint row_count;
    AmbientRow* inputs;
    float3* results;
};

[vk::push_constant] ConstantBuffer<AmbientTable> table;

[shader("compute")]
[numthreads(64, 1, 1)]
void ambient_response_of_rows(uint3 id : SV_DispatchThreadID) {
    if (id.x < table.row_count) {
        let row = table.inputs[id.x];
        let response = Response(row.base_color, row.metallic, row.roughness);
        table.results[id.x] = ambient_response(response, row.normal_dot_eye);
    }
}
