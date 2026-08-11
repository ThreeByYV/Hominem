// Temporal antialiasing resolve. Accumulates one jittered sample per frame into a
// history buffer, reprojecting it through the camera's motion and rejecting samples that
// no longer describe what is on screen.
//
// Follows Playdead's "Temporal Reprojection Anti-Aliasing in INSIDE" (GDC 2016) —
// github.com/playdeadgames/temporal, MIT, (c) 2015 Playdead. Not yet ported: per-object
// velocity, Catmull-Rom history, YCoCg clip_aabb, depth dilation, luminance feedback.

#type vertex
#version 450 core

out vec2 v_UV;

void main()
{
    const vec2 pos[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    const vec2 uv [3] = vec2[3](vec2( 0.0,  0.0), vec2(2.0,  0.0), vec2( 0.0, 2.0));
    v_UV        = uv [gl_VertexID];
    gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
}

#type fragment
#version 450 core

in  vec2 v_UV;
out vec4 FragColor;

uniform sampler2D u_Current;
uniform sampler2D u_History;
uniform sampler2D u_Depth;

uniform vec4  u_TexelSize;     // xy = 1/resolution, zw = resolution
uniform mat4  u_InvViewProj;   // this frame, unjittered
uniform mat4  u_PrevViewProj;  // last frame, unjittered
uniform float u_Feedback;      // history weight; 0 = passthrough
uniform int   u_DebugView;     // 0 off, 1 reprojection offset, 2 clamped history

// Where this pixel's surface sat on last frame's screen. Exact for anything that did not
// move in world space; wrong for anything that did, until velocity lands in stage 4.
vec2 Reproject(vec2 uv, float depth)
{
    vec4 ndc   = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = u_InvViewProj * ndc;
    world     /= world.w;

    vec4 prevClip = u_PrevViewProj * world;
    return (prevClip.xy / prevClip.w) * 0.5 + 0.5;
}

void main()
{
    vec3  cur   = texture(u_Current, v_UV).rgb;
    float depth = texture(u_Depth,   v_UV).r;

    if (u_Feedback <= 0.0)
    {
        FragColor = vec4(cur, 1.0);
        return;
    }

    // Sky sits on the cleared far plane, where Reproject's perspective divide degenerates.
    vec2 prevUV = (depth >= 1.0) ? v_UV : Reproject(v_UV, depth);

    if (u_DebugView == 1)
    {
        FragColor = vec4(abs(v_UV - prevUV) * u_TexelSize.zw * 0.1, 0.0, 1.0);
        return;
    }

    // Positive test so a NaN prevUV also fails it — every comparison against NaN is false.
    // A frame with no 3D camera leaves the matrices degenerate and Reproject returns one.
    bool onScreen = all(greaterThanEqual(prevUV, vec2(0.0)))
                 && all(lessThanEqual   (prevUV, vec2(1.0)));
    if (!onScreen)
    {
        FragColor = vec4(cur, 1.0);  // no history to accumulate, not even a wrong one
        return;
    }

    vec3 hist = texture(u_History, prevUV).rgb;

    // Reprojection finds the right address, not necessarily valid data — the surface may
    // have been hidden last frame, or shaded differently. The neighbourhood bounds what
    // is plausible here now; history outside it is stale. A no-op where history is good.
    vec3 cmin = cur;
    vec3 cmax = cur;
    for (int y = -1; y <= 1; ++y)
    for (int x = -1; x <= 1; ++x)
    {
        if (x == 0 && y == 0) continue;
        vec3 c = texture(u_Current, v_UV + vec2(x, y) * u_TexelSize.xy).rgb;
        cmin = min(cmin, c);
        cmax = max(cmax, c);
    }
    hist = clamp(hist, cmin, cmax);

    if (u_DebugView == 2)
    {
        FragColor = vec4(hist, 1.0);
        return;
    }

    FragColor = vec4(mix(cur, hist, u_Feedback), 1.0);
}
