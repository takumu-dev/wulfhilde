// Colours in linear light, with the primaries and white point of sRGB.
#language slang 2026
module color;

// The luminance of a colour, relative to that of white: how bright it looks, with green counting
// most and blue least, by the coefficients of ITU-R BT.709.
public float luminance(float3 color) {
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}
