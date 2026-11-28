// How a material reflects light: glTF's metallic-roughness BRDF, a Lambertian diffuse lobe under a
// GGX specular lobe with height-correlated Smith visibility and Schlick's Fresnel, mixed between a
// dielectric's response and a metal's by its metalness. GGX's alpha is the square of the
// material's roughness, and a dielectric reflects 4% at normal incidence, as glass of index 1.5
// does.
#language slang 2026
module brdf;

static const float pi = 3.14159265358979;

// What a dielectric reflects at normal incidence.
static const float dielectric_reflectance = 0.04;

// How a material responds to light at a point of a surface, once its textures are read.
public struct Response {
    // The base colour, in linear Rec.709: a dielectric's diffuse colour, or a metal's reflectance.
    public float3 base_color;
    // From 0 for a dielectric to 1 for a metal.
    public float metallic;
    // From 0 for a mirror to 1 for fully rough.
    public float roughness;
};

// What the material reflects at normal incidence.
float3 reflectance_of(Response response) {
    return lerp(float3(dielectric_reflectance), response.base_color, response.metallic);
}

// The colour its diffuse lobe takes, which a metal has none of.
float3 diffuse_color_of(Response response) {
    return response.base_color * (1.0 - response.metallic);
}

// Schlick's Fresnel term, from the reflectance at normal incidence toward white at grazing.
float3 fresnel(float3 reflectance, float cosine) {
    let grazing = 1.0 - cosine;
    let grazing2 = grazing * grazing;
    return reflectance + ((1.0 - reflectance) * grazing2 * grazing2 * grazing);
}

// Below this, a roughness would make GGX's peak too sharp for single precision, so it is taken as
// this instead.
static const float smoothest = 0.045;

// The BRDF, in each channel per steradian: how much of the light arriving at a point from one way
// leaves it toward another. It is 0 where either way is behind the surface.
//
// The normal and the two ways are unit vectors, away from the surface.
public float3 brdf(Response response, float3 normal, float3 toward_eye, float3 toward_light) {
    let n_dot_l = dot(normal, toward_light);
    let n_dot_v = dot(normal, toward_eye);
    if (n_dot_l <= 0.0 || n_dot_v <= 0.0) {
        return float3(0.0);
    }
    let halfway = normalize(toward_eye + toward_light);
    let n_dot_h = saturate(dot(normal, halfway));
    let v_dot_h = saturate(dot(toward_eye, halfway));

    let roughness = max(response.roughness, smoothest);
    let alpha = roughness * roughness;
    let alpha2 = alpha * alpha;
    let spread = (n_dot_h * n_dot_h * (alpha2 - 1.0)) + 1.0;
    let distribution = alpha2 / (pi * spread * spread);
    let visibility = 0.5 / ((n_dot_l * sqrt((n_dot_v * n_dot_v * (1.0 - alpha2)) + alpha2))
                            + (n_dot_v * sqrt((n_dot_l * n_dot_l * (1.0 - alpha2)) + alpha2)));
    let f = fresnel(reflectance_of(response), v_dot_h);
    return ((1.0 - f) * diffuse_color_of(response) / pi) + (f * distribution * visibility);
}

// The part of uniform light from every direction that the material reflects toward the eye, in
// each channel: what the BRDF, weighted by the cosine of each way the light arrives, comes to over
// the hemisphere above the surface. A white surface reflects it all, metal or not, rough or not.
//
// The specular part is split into a scale and a bias of the reflectance at normal incidence, by
// Lazarov's fit of them, as Karis gives it. The light that a rough surface's microfacets reflect
// more than once, which a single bounce of GGX loses, is added back as Fdez-Agüera does, and the
// diffuse lobe takes what the specular leaves.
//
// \param normal_dot_eye The cosine of the angle between the normal and the way to the eye.
public float3 ambient_response(Response response, float normal_dot_eye) {
    let r = (response.roughness * float4(-1.0, -0.0275, -0.572, 0.022))
            + float4(1.0, 0.0425, 1.04, -0.04);
    let a004 = (min(r.x * r.x, exp2(-9.28 * saturate(normal_dot_eye))) * r.x) + r.y;
    let scale_bias = (float2(-1.04, 1.04) * a004) + r.zw;

    let reflectance = reflectance_of(response);
    let single = saturate((reflectance * scale_bias.x) + scale_bias.y);
    let single_white = saturate(scale_bias.x + scale_bias.y);
    let multiple_white = 1.0 - single_white;
    let average_fresnel = reflectance + ((1.0 - reflectance) / 21.0);
    let multiple = single * average_fresnel / (1.0 - (multiple_white * average_fresnel));
    let specular = single + (multiple * multiple_white);
    return specular + (diffuse_color_of(response) * (1.0 - specular));
}

// How much roughness the spread of a pixel's normals adds, and the most it adds: Filament's
// defaults for Kaplanyan and Hoffman's geometric specular anti-aliasing.
static const float screen_variance = 0.15;
static const float most_added = 0.2;

// A roughness widened by how fast the shading normal changes across the screen, so that a
// highlight narrower than the normals a pixel covers spreads over them instead of sparkling.
//
// \param normal_dx How far the unit shading normal changes from one pixel to the next across.
// \param normal_dy How far it changes from one pixel to the next down.
public float widened_roughness(float roughness, float3 normal_dx, float3 normal_dy) {
    let variance = screen_variance * (dot(normal_dx, normal_dx) + dot(normal_dy, normal_dy));
    let added = min(2.0 * variance, most_added);
    let alpha = roughness * roughness;
    return sqrt(sqrt(saturate((alpha * alpha) + added)));
}
