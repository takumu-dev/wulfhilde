// AgX, the tone mapping from the scene's light to the display's: it rolls bright light off toward
// white, coloured light included, instead of shifting its hue. This is the usual fit of Troy
// Sobotka's AgX base: an inset into a gamut where the curve treats every channel alike, a log2
// encoding of the 16.5 stops it covers, a polynomial for its sigmoid, and the outset back.
#language slang 2026
module agx;

// The matrices as columns written row by row, so that a colour, as a row on their left, is turned
// as by the matrices themselves.
static const float3x3 inset = float3x3(
    0.842479062253094, 0.0423282422610123, 0.0423756549057051,
    0.0784335999999992, 0.878468636469772, 0.0784336,
    0.0792237451477643, 0.0791661274605434, 0.879142973793104);
static const float3x3 outset = float3x3(
    1.19687900512017, -0.0528968517574562, -0.0529716355144438,
    -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
    -0.0990297440797205, -0.0989611768448433, 1.15107367264116);

// The stops below and above 1 that the curve covers.
static const float darkest_stop = -12.47393;
static const float brightest_stop = 4.026069;

float3 sigmoid(float3 x) {
    let x2 = x * x;
    let x4 = x2 * x2;
    return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2
           + 0.1191 * x - 0.00232;
}

// Tone-maps an exposed colour in linear Rec.709, where 0.18 is a mid-grey, into the display's
// linear light, from 0 to 1, for an sRGB target to encode.
public float3 agx(float3 exposed) {
    let inset_color = mul(max(exposed, float3(1e-10)), inset);
    let stops = clamp(log2(inset_color), darkest_stop, brightest_stop);
    let curved = sigmoid((stops - darkest_stop) / (brightest_stop - darkest_stop));
    // The curve gives values for a display that decodes with a power of 2.2.
    return pow(saturate(mul(curved, outset)), 2.2);
}
