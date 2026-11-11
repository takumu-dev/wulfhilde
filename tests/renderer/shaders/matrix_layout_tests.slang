// Multiplies a matrix and a vector written from C++, which comes out right only if a matrix is laid
// out alike on both sides: column after column, with nothing between them.
#language slang 2026

struct MatrixRow {
    float4x4 matrix;
    float4 vector;
};

struct MatrixTable {
    uint row_count;
    MatrixRow* inputs;
    float4* results;
};

[vk::push_constant] ConstantBuffer<MatrixTable> table;

[shader("compute")]
[numthreads(64, 1, 1)]
void matrix_times_vector(uint3 id : SV_DispatchThreadID) {
    if (id.x < table.row_count) {
        let row = table.inputs[id.x];
        table.results[id.x] = mul(row.matrix, row.vector);
    }
}
