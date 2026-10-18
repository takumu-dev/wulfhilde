/// \file
/// A colour of light or of a surface, in linear Rec.709.

#pragma once

#include <wulfhilde/core/assertion.hpp>

#include <algorithm>
#include <cmath>

namespace wulfhilde {

/// A colour in linear Rec.709, the primaries of sRGB with no transfer curve.
///
/// A light's colour multiplies its light level, and a surface's base colour filters the light
/// that falls on it. Components aren't clamped: a light's colour from a temperature can be above
/// 1, and its light level is then still the luminance written. A colour is three packed floats,
/// 12 bytes, so it can sit in a struct that a shader reads under scalar layout.
///
/// It isn't a vector, so it has no dot product or sum. Its only arithmetic is scaling, and
/// multiplying another colour component by component, as a filter does.
struct Color {
    float r = 0.0F; ///< The red component.
    float g = 0.0F; ///< The green component.
    float b = 0.0F; ///< The blue component.

    /// Returns the colour of a black body at a temperature, at the luminance of white.
    ///
    /// It integrates Planck's law from 360 to 830 nm against Wyman, Sloan and Shirley's
    /// multi-lobe fit of the CIE 1931 observer, from "Simple Analytic Approximations to the CIE
    /// XYZ Color Matching Functions" (JCGT, 2013), and scales the result to the luminance of white,
    /// (1, 1, 1). A light of 800 lumens at 2,700 K so gives out 800 lumens, as the box of a bulb
    /// means, and its red component is above 1.
    ///
    /// Below about 1,900 K, a black body is redder than Rec.709 can show, and its blue would be
    /// negative. A light can't give out negative light, so the blue is then 0, and the colour is
    /// scaled back to the luminance of white.
    ///
    /// \param kelvin The temperature in kelvin, from 1,000 to 40,000.
    /// \return The colour, which may have components above 1.
    [[nodiscard]] static Color from_temperature(float kelvin);

    /// Compares two colours exactly, component by component.
    constexpr bool operator==(const Color&) const noexcept = default;
};

/// Returns a colour scaled by a number.
[[nodiscard]] constexpr Color operator*(const Color& c, float factor) noexcept {
    return {.r = c.r * factor, .g = c.g * factor, .b = c.b * factor};
}

/// Returns a colour scaled by a number.
[[nodiscard]] constexpr Color operator*(float factor, const Color& c) noexcept {
    return c * factor;
}

/// Returns the product of two colours component by component, as one filters the other.
[[nodiscard]] constexpr Color operator*(const Color& a, const Color& b) noexcept {
    return {.r = a.r * b.r, .g = a.g * b.g, .b = a.b * b.b};
}

namespace detail {

/// One lobe of Wyman, Sloan and Shirley's fit: a Gaussian with a different width on each side of
/// its peak.
///
/// \param wavelength The wavelength in nanometres.
/// \param peak The wavelength of the peak.
/// \param below The reciprocal of the lobe's width below the peak.
/// \param above The reciprocal of the lobe's width above the peak.
/// \return The lobe's height at `wavelength`, 1 at the peak.
[[nodiscard]] inline double observer_lobe(double wavelength, double peak, double below,
                                          double above) noexcept {
    const auto t = (wavelength - peak) * (wavelength < peak ? below : above);
    return std::exp(-0.5 * t * t);
}

} // namespace detail

inline Color Color::from_temperature(float kelvin) {
    WULFHILDE_ASSERT(kelvin >= 1000.0F && kelvin <= 40000.0F,
                     "a colour temperature is from 1,000 to 40,000 K");
    using detail::observer_lobe;

    // The second radiation constant, hc/k, in nanometre kelvins.
    constexpr double second_radiation_constant = 1.438776877e7;
    const auto temperature = static_cast<double>(kelvin);

    // Planck's law without its constant factor, which the scaling to white removes.
    auto x = 0.0;
    auto y = 0.0;
    auto z = 0.0;
    for (auto nanometres = 360; nanometres <= 830; ++nanometres) {
        const auto wavelength = static_cast<double>(nanometres);
        const auto squared = wavelength * wavelength;
        const auto radiance =
            1.0 / (squared * squared * wavelength *
                   std::expm1(second_radiation_constant / (wavelength * temperature)));
        // The coefficients are the paper's Table 1.
        x += radiance * ((0.362 * observer_lobe(wavelength, 442.0, 0.0624, 0.0374)) +
                         (1.056 * observer_lobe(wavelength, 599.8, 0.0264, 0.0323)) -
                         (0.065 * observer_lobe(wavelength, 501.1, 0.0490, 0.0382)));
        y += radiance * ((0.821 * observer_lobe(wavelength, 568.8, 0.0213, 0.0247)) +
                         (0.286 * observer_lobe(wavelength, 530.9, 0.0613, 0.0322)));
        z += radiance * ((1.217 * observer_lobe(wavelength, 437.0, 0.0845, 0.0278)) +
                         (0.681 * observer_lobe(wavelength, 459.0, 0.0385, 0.0725)));
    }

    // From CIE XYZ to linear Rec.709, the matrix derived from Rec.709's primaries and D65 white.
    // Dividing by Y gives the luminance of white, which a negative blue clipped to 0 then raises.
    x /= y;
    z /= y;
    const auto red = std::max(0.0, (3.2409699419 * x) - 1.5373831776 - (0.4986107603 * z));
    const auto green = std::max(0.0, (-0.9692436363 * x) + 1.8759675015 + (0.0415550574 * z));
    const auto blue = std::max(0.0, (0.0556300797 * x) - 0.2039769589 + (1.0569715142 * z));
    const auto luminance = (0.2126390059 * red) + (0.7151686788 * green) + (0.0721923154 * blue);
    return {
        .r = static_cast<float>(red / luminance),
        .g = static_cast<float>(green / luminance),
        .b = static_cast<float>(blue / luminance),
    };
}

} // namespace wulfhilde
