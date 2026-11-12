// Colours in linear light, with the primaries and white point of sRGB.
#language slang 2026
module color;

// The luminance of a colour, relative to that of white: how bright it looks, with green counting
// most and blue least, by the coefficients of ITU-R BT.709.
public float luminance(float3 color) {
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

// A colour in linear light, from 0 to 1, encoded with sRGB's transfer function, as an image of 8-bit
// sRGB holds it: in proportion near black, and by a power of 1/2.4 above.
public float3 srgb_encoded(float3 color) {
    let near_black = color * 12.92;
    let above = 1.055 * pow(max(color, 0.0031308), 1.0 / 2.4) - 0.055;
    return select(color <= 0.0031308, near_black, above);
}
