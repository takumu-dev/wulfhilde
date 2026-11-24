// Evaluates the renderer's exposure functions over a table of luminances or EV100s.
#language slang 2026
import exposure;

struct ExposureTable {
    uint row_count;
    float* inputs;
    float* results;
};

[vk::push_constant] ConstantBuffer<ExposureTable> table;

[shader("compute")]
[numthreads(64, 1, 1)]
void ev100_of_luminances(uint3 id : SV_DispatchThreadID) {
    if (id.x < table.row_count) {
        table.results[id.x] = ev100_of(table.inputs[id.x]);
    }
}

[shader("compute")]
[numthreads(64, 1, 1)]
void exposure_of_ev100s(uint3 id : SV_DispatchThreadID) {
    if (id.x < table.row_count) {
        table.results[id.x] = exposure_of(table.inputs[id.x]);
    }
}
