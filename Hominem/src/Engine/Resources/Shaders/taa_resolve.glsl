// Temporal antialiasing resolve. Accumulates one jittered sample per frame into a
// history buffer, reprojecting it through per-object motion and rejecting samples that
// no longer describe what is on screen.
//
// Ported from Playdead's "Temporal Reprojection Anti-Aliasing in INSIDE" (GDC 2016) -
// github.com/playdeadgames/temporal, MIT, (c) 2015 Playdead. Their saturate() in
// YCoCg_RGB is dropped: this resolves before tone mapping, on values above 1.

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
uniform sampler2D u_Velocity;  // hdr attachment 1, RG = UV-space motion
uniform sampler2D u_Depth;

uniform vec4  u_TexelSize;     // xy = 1/resolution, zw = resolution
uniform mat4  u_InvViewProj;   // this frame, unjittered
uniform mat4  u_PrevViewProj;  // last frame, unjittered
uniform float u_FeedbackMin;   // history weight where the pixel is changing
uniform float u_FeedbackMax;   // history weight where it is stable
uniform int   u_Reset;         // 1 = history is meaningless, pass the current frame through
uniform int   u_UseVelocity;   // 0 falls back to camera-only reprojection from depth
uniform int   u_UseDilation;   // 1 = take velocity from the closest fragment of the 3x3
uniform int   u_DebugView;     // 0 off, 1 motion, 2 clamped history, 3 rejection

const float FLT_EPS = 0.00000001;

vec3 RGBToYCoCg(vec3 c)
{
    return vec3( 0.25 * c.r + 0.5 * c.g + 0.25 * c.b,
                 0.5  * c.r              - 0.5  * c.b,
                -0.25 * c.r + 0.5 * c.g - 0.25 * c.b);
}

vec3 YCoCgToRGB(vec3 c)
{
    return vec3(c.x + c.y - c.z,
                c.x       + c.z,
                c.x - c.y - c.z);
}

// Reinhard on the YCoCg luma channel, applied around the blend and undone after. Without
// it one very bright sample dominates the neighbourhood bounds and smears into a comet
// tail. Tonemapping on .x rather than an RGB luma is what makes the inverse exact: the
// box, the clip and the blend all bound .x below 1, so the divide below can't explode.
vec3 Tonemap  (vec3 c) { return c / (1.0 + c.x); }
vec3 Untonemap(vec3 c) { return c / max(1.0 - c.x, 1e-4); }

vec3 SampleCurrent(vec2 uv)
{
    return Tonemap(RGBToYCoCg(texture(u_Current, uv).rgb));
}

// Clips towards the centre of the box along the ray from history to it, rather than
// clamping each channel independently - per-channel clamping lands on colours that were
// never on that ray, which shifts hue and flickers frame to frame.
vec3 ClipToAABB(vec3 boxMin, vec3 boxMax, vec3 q)
{
    vec3 centre = 0.5 * (boxMax + boxMin);
    vec3 extent = 0.5 * (boxMax - boxMin) + FLT_EPS;

    vec3  v      = q - centre;
    vec3  unit   = v / extent;
    float maxAxis = max(abs(unit.x), max(abs(unit.y), abs(unit.z)));

    return (maxAxis > 1.0) ? centre + v / maxAxis : q;
}

// 5-tap Catmull-Rom. Bilinear resampling blurs a little every frame and compounds over
// the accumulation window; this holds the history sharp instead.
vec3 SampleHistory(vec2 uv)
{
    vec2 texSize   = u_TexelSize.zw;
    vec2 samplePos = uv * texSize;
    vec2 texPos1   = floor(samplePos - 0.5) + 0.5;
    vec2 f         = samplePos - texPos1;

    vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    vec2 w3 = f * f * (-0.5 + 0.5 * f);

    vec2 w12      = w1 + w2;
    vec2 offset12 = w2 / w12;

    vec2 texPos0  = (texPos1 - 1.0)      / texSize;
    vec2 texPos3  = (texPos1 + 2.0)      / texSize;
    vec2 texPos12 = (texPos1 + offset12) / texSize;

    // Corner taps dropped and the rest renormalised - 5 fetches instead of 9.
    vec3  result = vec3(0.0);
    float weight = 0.0;

    result += texture(u_History, vec2(texPos12.x, texPos0.y )).rgb * w12.x * w0.y;
    weight += w12.x * w0.y;
    result += texture(u_History, vec2(texPos0.x,  texPos12.y)).rgb * w0.x  * w12.y;
    weight += w0.x * w12.y;
    result += texture(u_History, vec2(texPos12.x, texPos12.y)).rgb * w12.x * w12.y;
    weight += w12.x * w12.y;
    result += texture(u_History, vec2(texPos3.x,  texPos12.y)).rgb * w3.x  * w12.y;
    weight += w3.x * w12.y;
    result += texture(u_History, vec2(texPos12.x, texPos3.y )).rgb * w12.x * w3.y;
    weight += w12.x * w3.y;

    return max(result / weight, 0.0);
}

// Offset of the nearest fragment in the 3x3. Sampling velocity there instead of at the
// centre keeps a moving silhouette from wobbling: edge pixels then take the mover's
// velocity rather than the background's.
vec2 ClosestFragmentOffset(vec2 uv)
{
    vec2  best      = vec2(0.0);
    float bestDepth = texture(u_Depth, uv).r;

    for (int y = -1; y <= 1; ++y)
    for (int x = -1; x <= 1; ++x)
    {
        vec2  offset = vec2(x, y) * u_TexelSize.xy;
        float d      = texture(u_Depth, uv + offset).r;
        if (d < bestDepth) { bestDepth = d; best = offset; }
    }
    return best;
}

// Camera-only reprojection from depth. Identical to the velocity buffer for anything that
// did not move in world space, and wrong for anything that did.
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
    vec3  curRGB = texture(u_Current, v_UV).rgb;
    float depth  = texture(u_Depth,   v_UV).r;

    if (u_Reset != 0)
    {
        FragColor = vec4(curRGB, 1.0);
        return;
    }

    // Hoisted: texture() picks its LOD from quad derivatives, which are undefined for
    // lanes that branched away.
    vec2 velocityOffset = (u_UseDilation != 0) ? ClosestFragmentOffset(v_UV) : vec2(0.0);
    vec2 velocity       = texture(u_Velocity, v_UV + velocityOffset).rg;

    // Sky sits on the cleared far plane: Reproject's divide degenerates there, and being
    // at infinity it doesn't move under camera translation anyway.
    vec2 prevUV;
    if (depth >= 1.0)
        prevUV = v_UV;
    else if (u_UseVelocity != 0)
        prevUV = v_UV - velocity;
    else
        prevUV = Reproject(v_UV, depth);

    if (u_DebugView == 1)
    {
        FragColor = vec4(abs(v_UV - prevUV) * u_TexelSize.zw * 0.1, 0.0, 1.0);
        return;
    }

    // Positive test so a NaN prevUV also fails it - every comparison against NaN is false.
    bool onScreen = all(greaterThanEqual(prevUV, vec2(0.0)))
                 && all(lessThanEqual   (prevUV, vec2(1.0)));
    if (!onScreen)
    {
        FragColor = vec4(curRGB, 1.0);
        return;
    }

    vec3 cur  = Tonemap(RGBToYCoCg(curRGB));
    vec3 hist = Tonemap(RGBToYCoCg(SampleHistory(prevUV)));

    // Reprojection finds the right address, not necessarily valid data - the surface may
    // have been hidden last frame, or shaded differently since. The neighbourhood bounds
    // what is plausible here now, and history outside it is stale.
    vec2 du = vec2(u_TexelSize.x, 0.0);
    vec2 dv = vec2(0.0, u_TexelSize.y);

    vec3 ctl = SampleCurrent(v_UV - dv - du);
    vec3 ctc = SampleCurrent(v_UV - dv);
    vec3 ctr = SampleCurrent(v_UV - dv + du);
    vec3 cml = SampleCurrent(v_UV      - du);
    vec3 cmr = SampleCurrent(v_UV      + du);
    vec3 cbl = SampleCurrent(v_UV + dv - du);
    vec3 cbc = SampleCurrent(v_UV + dv);
    vec3 cbr = SampleCurrent(v_UV + dv + du);

    vec3 boxMin = min(ctl, min(ctc, min(ctr, min(cml, min(cur, min(cmr, min(cbl, min(cbc, cbr))))))));
    vec3 boxMax = max(ctl, max(ctc, max(ctr, max(cml, max(cur, max(cmr, max(cbl, max(cbc, cbr))))))));

    // Averaged with the plus-shaped 5-tap box: trims the corners, which are usually empty,
    // for a tighter fit than the full square without the cross's tendency to over-reject.
    vec3 min5 = min(ctc, min(cml, min(cur, min(cmr, cbc))));
    vec3 max5 = max(ctc, max(cml, max(cur, max(cmr, cbc))));
    boxMin = 0.5 * (boxMin + min5);
    boxMax = 0.5 * (boxMax + max5);

    // Colour variation is mostly luma, so a box sized on chroma is far looser than it needs
    // to be. Shrink chroma around the current pixel's and keep the luma extent.
    float chromaExtent = 0.25 * 0.5 * (boxMax.x - boxMin.x);
    boxMin.yz = cur.yz - chromaExtent;
    boxMax.yz = cur.yz + chromaExtent;

    hist = ClipToAABB(boxMin, boxMax, hist);

    if (u_DebugView == 2)
    {
        FragColor = vec4(max(YCoCgToRGB(Untonemap(hist)), 0.0), 1.0);
        return;
    }

    // Trust history less where the pixel is changing. Luma is the Y channel already.
    float lum0 = cur.x;
    float lum1 = hist.x;
    float diff = abs(lum0 - lum1) / max(lum0, max(lum1, 0.2));
    float w    = 1.0 - diff;
    float k    = mix(u_FeedbackMin, u_FeedbackMax, w * w);

    if (u_DebugView == 3)
    {
        FragColor = vec4(vec3(1.0 - (k - u_FeedbackMin) / max(u_FeedbackMax - u_FeedbackMin, 1e-4)), 1.0);
        return;
    }

    // Clamped because Catmull-Rom overshoot and the YCoCg round trip can both go slightly
    // negative, and a negative feeding next frame's history compounds.
    FragColor = vec4(max(YCoCgToRGB(Untonemap(mix(cur, hist, k))), 0.0), 1.0);
}
