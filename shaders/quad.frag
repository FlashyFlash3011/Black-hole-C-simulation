#version 430 core

uniform sampler2D uTex;
out vec4 fragColor;

// ACES filmic tone mapping — gives a natural cinematic S-curve
vec3 aces(vec3 x) {
    return clamp((x*(2.51*x + 0.03)) / (x*(2.43*x + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    vec2  uv  = gl_FragCoord.xy / vec2(textureSize(uTex, 0));
    vec3  col = texture(uTex, uv).rgb;

    // ── Approximate bloom via HDR neighbourhood sampling ─────────────
    // Collect emission from pixels above the bloom threshold (HDR > 1.2)
    // and spread it as a soft glow.  This makes the inner disk, photon
    // ring, and jet tips bloom naturally without a second render pass.
    vec3  bloom = vec3(0.0);
    vec2  ts    = 1.0 / vec2(textureSize(uTex, 0));
    const int R = 3;
    for (int dy = -R; dy <= R; dy++) {
        for (int dx = -R; dx <= R; dx++) {
            if (dx == 0 && dy == 0) continue;
            vec3  s    = texture(uTex, uv + vec2(dx, dy) * ts * 3.0).rgb;
            float lum  = dot(s, vec3(0.2126, 0.7152, 0.0722));
            float thr  = max(0.0, lum - 1.2);               // only over-bright pixels
            float wgt  = 1.0 / (1.0 + float(dx*dx + dy*dy)); // distance weight
            bloom     += thr * s * wgt;
        }
    }
    bloom /= float((2*R+1)*(2*R+1)) * 0.6;

    // Tone map (HDR → [0,1]) then gamma-correct for display
    vec3 final = aces(col + bloom * 0.55);
    final      = pow(max(final, vec3(0.0)), vec3(1.0/2.2));

    fragColor = vec4(final, 1.0);
}
