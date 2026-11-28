// What the shaders know of a view: how it projects, where its eye is, the plane its surfaces are
// drawn beyond, and the opening it is seen through.
#language slang 2026
module views;

// The points p where dot(normal, p) == distance, facing where its normal points.
public struct Plane {
    public float3 normal;
    public float distance;
};

public struct ViewShading {
    // From the coordinates of the view's sectors into clip space, with depth reversed and no far
    // plane.
    public float4x4 view_projection;
    // Where the eye is, in those coordinates.
    public float3 eye;
    // The plane of the portal the view starts at, facing beyond it, where its surfaces are drawn.
    // The main view's faces away from every point, with a normal of zero and a distance of -1.
    public Plane clip;
    // The corners of the opening the view is seen through, in order round it, in the coordinates
    // of the view it is seen from, the last repeated after them where it has fewer than six.
    public float3 opening[6];
    // Whether the opening's plane lies so near the eye that the near plane would take away some of
    // what the screen shows of it, which is then drawn as near as depth goes.
    public uint opening_at_eye;
};

// How far a point lies beyond the plane a view clips at, which a clip distance keeps where it is 0
// or more.
public float beyond_clip(ViewShading view, float3 point) {
    return dot(view.clip.normal, point) - view.clip.distance;
}
