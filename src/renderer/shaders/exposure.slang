// Exposure as a camera's EV at ISO 100, which the map's range is written in, and what it makes of
// the scene's light for AgX.
#language slang 2026
module exposure;

// The EV100 at which a camera metering an average luminance in nits shows it as it should, by the
// usual calibration of a light meter, 12.5. Light too faint to measure meters as very dark.
public float ev100_of(float average_luminance) {
    return log2(max(average_luminance * 100.0 / 12.5, 1e-10));
}

// What luminance in nits is multiplied by for AgX, which takes 0.18 as mid-grey, at an exposure:
// the luminance that just saturates a camera's sensor, by the usual calibration of 12.5 and a
// sensor saturating at 1.2 times its nominal response, maps to 1.
public float exposure_of(float ev100) {
    return 1.0 / (1.2 * exp2(ev100));
}
