#version 450

layout(location = 0) in  vec2 frag_uv;
layout(location = 0) out vec4 out_color;

layout(binding = 0) uniform sampler2D u_image;
layout(binding = 2) uniform sampler2D u_feedback;

layout(std140, binding = 1) uniform UBO {
    float rms;
    float bass;
    float mid;
    float treble;
    float spectral_centroid;
    float spectral_flux;
    float time;
    float beat;
    float onset;
    float zoom_intensity;
    float wave_intensity;
    float hue_shift_intensity;
    float glitch_intensity;
    float glow_intensity;
    float displacement_intensity;
    float perlin_intensity;
    float image_ar;
    float viewport_ar;
    float pan_x;
    float pan_y;
    float zoom_scale;
    float warp_scale;
    float brightness_intensity;
    float saturation_intensity;
    float vignette_intensity;
    float chromatic_intensity;
    float film_grain_intensity;
    float pixelate_intensity;
    float edge_glow_intensity;
    float morph_intensity;
    float barrel_intensity;
    float kaleidoscope_intensity;
    float polar_intensity;
    float ripple_intensity;
    float posterize_intensity;
    float solarize_intensity;
    float channel_swap_intensity;
    float duotone_intensity;
    float emboss_intensity;
    float scanline_intensity;
    float feedback_intensity;
    float cinematic_zoom;
    float cinematic_pan_x;
    float cinematic_pan_y;
    float fb_uv_offset_x;
    float fb_uv_offset_y;
    float fb_uv_scale_x;
    float fb_uv_scale_y;
    float zoom_feedback;
    float wave_feedback;
    float hue_shift_feedback;
    float glitch_feedback;
    float glow_feedback;
    float displacement_feedback;
    float perlin_feedback;
    float brightness_feedback;
    float saturation_feedback;
    float vignette_feedback;
    float chromatic_feedback;
    float film_grain_feedback;
    float pixelate_feedback;
    float edge_glow_feedback;
    float morph_feedback;
    float barrel_feedback;
    float kaleidoscope_feedback;
    float polar_feedback;
    float ripple_feedback;
    float posterize_feedback;
    float solarize_feedback;
    float channel_swap_feedback;
    float duotone_feedback;
    float emboss_feedback;
    float scanline_feedback;
    float reserved0;
    float reserved1;
    float reserved2;
} u;

// ── Colour helpers ────────────────────────────────────────────────────────────

vec3 rgb_to_hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0/3.0, 2.0/3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + 1e-10)),
                d / (q.x + 1e-10), q.x);
}

vec3 hsv_to_rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

// ── Noise helpers (for perlin warp) ──────────────────────────────────────────

float hash21(vec2 p) {
    p = fract(p * vec2(127.1, 311.7));
    p += dot(p, p.yx + 19.19);
    return fract(p.x * p.y);
}

float value_noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(hash21(i),                    hash21(i + vec2(1.0, 0.0)), f.x),
        mix(hash21(i + vec2(0.0, 1.0)),   hash21(i + vec2(1.0, 1.0)), f.x),
        f.y);
}

// ── UV effects ────────────────────────────────────────────────────────────────

vec2 pixelate_uv(vec2 uv) {
    float drive = u.pixelate_intensity * (0.5 + u.bass * 0.5);
    float cells = mix(300.0, 15.0, drive);
    return (floor(uv * cells) + 0.5) / cells;
}

vec2 zoom_pulse(vec2 uv) {
    vec2  c = uv - 0.5;
    float z = 1.0 + u.rms  * u.zoom_intensity * 0.25
                  + u.beat * u.zoom_intensity * 0.06;
    return c / z + 0.5;
}

vec2 wave_warp(vec2 uv) {
    // base_amp keeps the effect visible at any audio level so the slider is
    // clearly audible; audio multiplies on top for reactivity.
    float amp = u.wave_intensity * (0.025 + u.bass * 0.07);
    uv.x += sin(uv.y * 8.0 * u.warp_scale + u.time * 2.0)  * amp;
    uv.y += sin(uv.x * 8.0 * u.warp_scale + u.time * 1.3)  * amp * (0.5 + u.mid * 0.5);
    return uv;
}

vec2 displacement_warp(vec2 uv) {
    float amp = u.displacement_intensity * (0.3 + u.bass * 0.7);
    float dx = sin(uv.y * 12.0 * u.warp_scale + u.time * 1.5) * amp * 0.04;
    float dy = cos(uv.x * 12.0 * u.warp_scale + u.time * 0.9) * amp * 0.04;
    return uv + vec2(dx, dy);
}

vec2 perlin_warp(vec2 uv) {
    float amp = u.perlin_intensity * (0.3 + u.mid * 0.7);
    vec2  t   = vec2(u.time * 0.15);
    float nx  = value_noise(uv * 3.0 * u.warp_scale + t)                    * 2.0 - 1.0;
    float ny  = value_noise(uv * 3.0 * u.warp_scale + t + vec2(5.2, 1.3))   * 2.0 - 1.0;
    return uv + vec2(nx, ny) * amp * 0.06;
}

vec2 barrel_warp(vec2 uv) {
    vec2  c  = uv - 0.5;
    float r2 = dot(c, c);
    float k  = u.barrel_intensity * (0.3 + u.bass * 0.7) * 3.0;
    return uv + c * r2 * k;
}

vec2 ripple_warp(vec2 uv) {
    vec2  d    = uv - 0.5;
    float dist = length(d);
    float amp  = u.ripple_intensity * (0.3 + u.bass * 0.7) * 0.03;
    if (dist > 0.001)
        uv += normalize(d) * sin(dist * 20.0 - u.time * 3.0) * amp;
    return uv;
}

vec2 polar_warp(vec2 uv) {
    vec2  c   = uv - 0.5;
    float r   = length(c);
    float a   = atan(c.y, c.x);
    float amp = u.polar_intensity * (0.3 + u.mid * 0.7) * 0.2;
    r += amp * sin(a * 3.0 + u.time * 1.5);
    return vec2(cos(a), sin(a)) * r + 0.5;
}

vec2 kaleidoscope_uv(vec2 uv) {
    const float tau = 6.28318;
    vec2  c   = uv * 2.0 - 1.0;
    float r   = length(c);
    float a   = atan(c.y, c.x) + u.time * u.kaleidoscope_intensity * 0.3;
    float seg = tau / 6.0;
    a = mod(a, seg);
    a = abs(a - seg * 0.5);
    c = vec2(cos(a), sin(a)) * r;
    return fract(c * 0.5 + 0.5);
}

// Chromatic aberration: blends between u_image and u_feedback per-channel.
vec4 apply_chromatic(vec2 uv, vec2 uv_fb) {
    float amount = u.chromatic_intensity * (0.3 + u.treble * 0.5 + u.onset * 0.2);
    vec2  dir    = (uv - 0.5) * amount * 0.03;
    float fb     = u.chromatic_feedback;
    float r, g, b, a;
    if (fb > 0.0) {
        r = mix(texture(u_image,    clamp(uv    + dir, 0.0, 1.0)).r,
                texture(u_feedback, clamp(uv_fb + dir, 0.0, 1.0)).r, fb);
        g = mix(texture(u_image,    clamp(uv,          0.0, 1.0)).g,
                texture(u_feedback, clamp(uv_fb,        0.0, 1.0)).g, fb);
        b = mix(texture(u_image,    clamp(uv    - dir, 0.0, 1.0)).b,
                texture(u_feedback, clamp(uv_fb - dir, 0.0, 1.0)).b, fb);
        a = mix(texture(u_image,    clamp(uv,          0.0, 1.0)).a,
                texture(u_feedback, clamp(uv_fb,        0.0, 1.0)).a, fb);
    } else {
        r = texture(u_image, clamp(uv + dir, 0.0, 1.0)).r;
        g = texture(u_image, clamp(uv,       0.0, 1.0)).g;
        b = texture(u_image, clamp(uv - dir, 0.0, 1.0)).b;
        a = texture(u_image, clamp(uv,       0.0, 1.0)).a;
    }
    return vec4(r, g, b, a);
}

// ── Colour effects ────────────────────────────────────────────────────────────

vec3 apply_saturation(vec3 color) {
    float boost = u.saturation_intensity * (0.5 + u.rms * 0.5);
    vec3  hsv   = rgb_to_hsv(color);
    hsv.y = clamp(hsv.y * (1.0 + boost * 2.0), 0.0, 1.0);
    return hsv_to_rgb(hsv);
}

vec3 apply_brightness(vec3 color) {
    float boost = u.brightness_intensity * (0.4 + u.rms * 0.4 + u.beat * 0.2);
    return clamp(color * (1.0 + boost), 0.0, 1.0);
}

vec3 apply_vignette(vec3 color, vec2 uv) {
    float dist     = length(uv - 0.5) * 1.8;
    float strength = u.vignette_intensity * (0.5 + u.rms * 0.5);
    float v        = 1.0 - smoothstep(0.3, 1.2, dist * strength);
    return color * v;
}

vec3 apply_film_grain(vec3 color, vec2 uv) {
    float strength = u.film_grain_intensity * (0.3 + u.spectral_flux * 0.7);
    float t        = floor(u.time * 60.0);
    float grain    = fract(sin(dot(uv + t * 0.001, vec2(127.1, 311.7))) * 43758.5453);
    grain = (grain * 2.0 - 1.0) * strength * 0.12;
    return clamp(color + grain, 0.0, 1.0);
}

vec3 apply_posterize(vec3 color) {
    float levels = ceil(mix(8.0, 2.0, u.posterize_intensity * (0.4 + u.spectral_flux * 0.6)));
    return floor(color * levels) / max(levels - 1.0, 1.0);
}

vec3 apply_solarize(vec3 color) {
    float thresh = 1.0 - u.solarize_intensity * (0.4 + u.rms * 0.6);
    float luma   = dot(color, vec3(0.299, 0.587, 0.114));
    return mix(color, 1.0 - color, step(thresh, luma));
}

vec3 apply_duotone(vec3 color) {
    float luma  = dot(color, vec3(0.299, 0.587, 0.114));
    float hue_a = mod(u.spectral_centroid * 0.5 + u.time * 0.02 * u.duotone_intensity, 1.0);
    float hue_b = mod(hue_a + 0.5, 1.0);
    vec3 dark   = hsv_to_rgb(vec3(hue_a, 0.9, 0.15));
    vec3 light  = hsv_to_rgb(vec3(hue_b, 0.6, 1.0));
    return mix(color, mix(dark, light, luma), u.duotone_intensity);
}

vec3 apply_channel_swap(vec3 color) {
    float t = u.channel_swap_intensity * clamp(u.beat * 0.6 + u.onset * 0.3 + u.rms * 0.1, 0.0, 1.0);
    return mix(color, color.brg, t);
}

// Emboss: blends neighbor samples between u_image and u_feedback.
vec3 apply_emboss(vec3 color, vec2 uv, vec2 uv_fb) {
    float strength = u.emboss_intensity * (0.4 + u.treble * 0.6);
    float angle    = u.time * 0.3;
    vec2  dir      = vec2(cos(angle), sin(angle)) * 0.003;
    float fb       = u.emboss_feedback;
    vec3  hi, lo;
    if (fb > 0.0) {
        hi = mix(texture(u_image,    clamp(uv    + dir, 0.0, 1.0)).rgb,
                 texture(u_feedback, clamp(uv_fb + dir, 0.0, 1.0)).rgb, fb);
        lo = mix(texture(u_image,    clamp(uv    - dir, 0.0, 1.0)).rgb,
                 texture(u_feedback, clamp(uv_fb - dir, 0.0, 1.0)).rgb, fb);
    } else {
        hi = texture(u_image, clamp(uv + dir, 0.0, 1.0)).rgb;
        lo = texture(u_image, clamp(uv - dir, 0.0, 1.0)).rgb;
    }
    vec3 emboss = (hi - lo) * 0.5 + 0.5;
    return mix(color, emboss, strength);
}

vec3 apply_scanlines(vec3 color, vec2 uv) {
    float density = (100.0 + u.scanline_intensity * 150.0) * 3.14159;
    float line    = pow(sin(uv.y * density), 2.0);
    float depth   = u.scanline_intensity * (0.3 + u.bass * 0.5);
    return color * (1.0 - depth * (1.0 - line));
}

// Morph (dilate/erode): blends neighbor samples between u_image and u_feedback.
vec3 apply_morph(vec3 color, vec2 uv, vec2 uv_fb) {
    // loud (high rms+bass) → dilate (max of neighbourhood, bright areas grow)
    // quiet (low rms+bass) → erode  (min of neighbourhood, bright areas shrink)
    float drive = u.rms * 0.6 + u.bass * 0.4;
    float r = u.morph_intensity * 0.015;
    float d = r * 0.707;
    float fb = u.morph_feedback;

    vec3 hi = color, lo = color, s;
    if (fb > 0.0) {
        s = mix(texture(u_image, clamp(uv + vec2( r,  0.0), 0.0, 1.0)).rgb, texture(u_feedback, clamp(uv_fb + vec2( r,  0.0), 0.0, 1.0)).rgb, fb); hi = max(hi, s); lo = min(lo, s);
        s = mix(texture(u_image, clamp(uv + vec2(-r,  0.0), 0.0, 1.0)).rgb, texture(u_feedback, clamp(uv_fb + vec2(-r,  0.0), 0.0, 1.0)).rgb, fb); hi = max(hi, s); lo = min(lo, s);
        s = mix(texture(u_image, clamp(uv + vec2( 0.0,  r), 0.0, 1.0)).rgb, texture(u_feedback, clamp(uv_fb + vec2( 0.0,  r), 0.0, 1.0)).rgb, fb); hi = max(hi, s); lo = min(lo, s);
        s = mix(texture(u_image, clamp(uv + vec2( 0.0, -r), 0.0, 1.0)).rgb, texture(u_feedback, clamp(uv_fb + vec2( 0.0, -r), 0.0, 1.0)).rgb, fb); hi = max(hi, s); lo = min(lo, s);
        s = mix(texture(u_image, clamp(uv + vec2( d,   d),  0.0, 1.0)).rgb, texture(u_feedback, clamp(uv_fb + vec2( d,   d),  0.0, 1.0)).rgb, fb); hi = max(hi, s); lo = min(lo, s);
        s = mix(texture(u_image, clamp(uv + vec2(-d,   d),  0.0, 1.0)).rgb, texture(u_feedback, clamp(uv_fb + vec2(-d,   d),  0.0, 1.0)).rgb, fb); hi = max(hi, s); lo = min(lo, s);
        s = mix(texture(u_image, clamp(uv + vec2( d,  -d),  0.0, 1.0)).rgb, texture(u_feedback, clamp(uv_fb + vec2( d,  -d),  0.0, 1.0)).rgb, fb); hi = max(hi, s); lo = min(lo, s);
        s = mix(texture(u_image, clamp(uv + vec2(-d,  -d),  0.0, 1.0)).rgb, texture(u_feedback, clamp(uv_fb + vec2(-d,  -d),  0.0, 1.0)).rgb, fb); hi = max(hi, s); lo = min(lo, s);
    } else {
        s = texture(u_image, clamp(uv + vec2( r,  0.0), 0.0, 1.0)).rgb; hi = max(hi, s); lo = min(lo, s);
        s = texture(u_image, clamp(uv + vec2(-r,  0.0), 0.0, 1.0)).rgb; hi = max(hi, s); lo = min(lo, s);
        s = texture(u_image, clamp(uv + vec2( 0.0,  r), 0.0, 1.0)).rgb; hi = max(hi, s); lo = min(lo, s);
        s = texture(u_image, clamp(uv + vec2( 0.0, -r), 0.0, 1.0)).rgb; hi = max(hi, s); lo = min(lo, s);
        s = texture(u_image, clamp(uv + vec2( d,   d),  0.0, 1.0)).rgb; hi = max(hi, s); lo = min(lo, s);
        s = texture(u_image, clamp(uv + vec2(-d,   d),  0.0, 1.0)).rgb; hi = max(hi, s); lo = min(lo, s);
        s = texture(u_image, clamp(uv + vec2( d,  -d),  0.0, 1.0)).rgb; hi = max(hi, s); lo = min(lo, s);
        s = texture(u_image, clamp(uv + vec2(-d,  -d),  0.0, 1.0)).rgb; hi = max(hi, s); lo = min(lo, s);
    }

    float t = clamp((drive - 0.5) * 2.0, -1.0, 1.0);
    return t >= 0.0 ? mix(color, hi, t) : mix(color, lo, -t);
}

// Edge glow: blends corner samples between u_image and u_feedback.
vec3 apply_edge_glow(vec3 color, vec2 uv, vec2 uv_fb) {
    // Bass and beat drive the kernel radius so edges physically expand on hits.
    float drive    = u.bass * 0.6 + u.beat * 0.3 + u.onset * 0.1;
    float r        = 0.002 + drive * u.edge_glow_intensity * 0.025;
    float strength = u.edge_glow_intensity * drive;
    float fb       = u.edge_glow_feedback;
    vec3 tl, tr, bl, br;
    if (fb > 0.0) {
        tl = mix(texture(u_image,    clamp(uv    + vec2(-r, -r), 0.0, 1.0)).rgb,
                 texture(u_feedback, clamp(uv_fb + vec2(-r, -r), 0.0, 1.0)).rgb, fb);
        tr = mix(texture(u_image,    clamp(uv    + vec2( r, -r), 0.0, 1.0)).rgb,
                 texture(u_feedback, clamp(uv_fb + vec2( r, -r), 0.0, 1.0)).rgb, fb);
        bl = mix(texture(u_image,    clamp(uv    + vec2(-r,  r), 0.0, 1.0)).rgb,
                 texture(u_feedback, clamp(uv_fb + vec2(-r,  r), 0.0, 1.0)).rgb, fb);
        br = mix(texture(u_image,    clamp(uv    + vec2( r,  r), 0.0, 1.0)).rgb,
                 texture(u_feedback, clamp(uv_fb + vec2( r,  r), 0.0, 1.0)).rgb, fb);
    } else {
        tl = texture(u_image, clamp(uv + vec2(-r, -r), 0.0, 1.0)).rgb;
        tr = texture(u_image, clamp(uv + vec2( r, -r), 0.0, 1.0)).rgb;
        bl = texture(u_image, clamp(uv + vec2(-r,  r), 0.0, 1.0)).rgb;
        br = texture(u_image, clamp(uv + vec2( r,  r), 0.0, 1.0)).rgb;
    }
    vec3 gx   = (tr + br) - (tl + bl);
    vec3 gy   = (bl + br) - (tl + tr);
    vec3 edge = sqrt(gx * gx + gy * gy);
    return min(color + edge * strength * 6.0, vec3(1.0));
}

vec3 hue_shift(vec3 rgb) {
    vec3 hsv  = rgb_to_hsv(rgb);
    hsv.x     = fract(hsv.x + u.spectral_centroid * u.hue_shift_intensity * 0.5);
    return hsv_to_rgb(hsv);
}

// Glow: blends neighbor samples between u_image and u_feedback.
vec3 apply_glow(vec3 color, vec2 uv, vec2 uv_fb) {
    float strength = u.glow_intensity * (0.4 + u.rms * 0.6);
    float r        = 0.008 * strength;
    float fb       = u.glow_feedback;
    vec3  s        = vec3(0.0);
    if (fb > 0.0) {
        s += mix(texture(u_image,    clamp(uv    + vec2( r,  0.0), 0.0, 1.0)).rgb,
                 texture(u_feedback, clamp(uv_fb + vec2( r,  0.0), 0.0, 1.0)).rgb, fb);
        s += mix(texture(u_image,    clamp(uv    + vec2(-r,  0.0), 0.0, 1.0)).rgb,
                 texture(u_feedback, clamp(uv_fb + vec2(-r,  0.0), 0.0, 1.0)).rgb, fb);
        s += mix(texture(u_image,    clamp(uv    + vec2( 0.0,  r), 0.0, 1.0)).rgb,
                 texture(u_feedback, clamp(uv_fb + vec2( 0.0,  r), 0.0, 1.0)).rgb, fb);
        s += mix(texture(u_image,    clamp(uv    + vec2( 0.0, -r), 0.0, 1.0)).rgb,
                 texture(u_feedback, clamp(uv_fb + vec2( 0.0, -r), 0.0, 1.0)).rgb, fb);
    } else {
        s += texture(u_image, clamp(uv + vec2( r,  0.0), 0.0, 1.0)).rgb;
        s += texture(u_image, clamp(uv + vec2(-r,  0.0), 0.0, 1.0)).rgb;
        s += texture(u_image, clamp(uv + vec2( 0.0,  r), 0.0, 1.0)).rgb;
        s += texture(u_image, clamp(uv + vec2( 0.0, -r), 0.0, 1.0)).rgb;
    }
    vec3 bloom = max(s / 4.0 - 0.4, vec3(0.0));
    return min(color + bloom * strength * 4.0, vec3(1.0));
}

// Glitch: blends channel samples between u_image and u_feedback.
vec4 apply_glitch(vec2 uv, vec2 uv_fb) {
    float intensity = u.glitch_intensity * (0.3 + u.beat * 0.4 + u.onset * 0.3);

    float t        = floor(u.time * 12.0) / 12.0;
    float row      = floor(uv.y * 24.0);
    float rand_row = fract(sin(row * 38.5 + t * 73.1) * 4375.85);
    float shift    = (rand_row > (1.0 - intensity * 0.4))
                     ? (rand_row - 0.5) * intensity * 0.15
                     : 0.0;

    float split = intensity * 0.012;
    float fb    = u.glitch_feedback;
    float rv, gv, bv, av;
    if (fb > 0.0) {
        rv = mix(texture(u_image,    clamp(uv    + vec2(shift + split, 0.0), 0.0, 1.0)).r,
                 texture(u_feedback, clamp(uv_fb + vec2(shift + split, 0.0), 0.0, 1.0)).r, fb);
        gv = mix(texture(u_image,    clamp(uv    + vec2(shift,          0.0), 0.0, 1.0)).g,
                 texture(u_feedback, clamp(uv_fb + vec2(shift,          0.0), 0.0, 1.0)).g, fb);
        bv = mix(texture(u_image,    clamp(uv    + vec2(shift - split, 0.0), 0.0, 1.0)).b,
                 texture(u_feedback, clamp(uv_fb + vec2(shift - split, 0.0), 0.0, 1.0)).b, fb);
        av = mix(texture(u_image,    clamp(uv    + vec2(shift,          0.0), 0.0, 1.0)).a,
                 texture(u_feedback, clamp(uv_fb + vec2(shift,          0.0), 0.0, 1.0)).a, fb);
    } else {
        rv = texture(u_image, clamp(uv + vec2(shift + split, 0.0), 0.0, 1.0)).r;
        gv = texture(u_image, clamp(uv + vec2(shift,          0.0), 0.0, 1.0)).g;
        bv = texture(u_image, clamp(uv + vec2(shift - split, 0.0), 0.0, 1.0)).b;
        av = texture(u_image, clamp(uv + vec2(shift,          0.0), 0.0, 1.0)).a;
    }
    return vec4(rv, gv, bv, av);
}

// ── Main ──────────────────────────────────────────────────────────────────────

void main() {
    vec2 uv = frag_uv;

    // Cinematic zoom in screen space so the target coordinates from the
    // cinematic target widget (also screen space) are in the same frame.
    uv = (uv - 0.5) / u.cinematic_zoom + 0.5;
    uv -= vec2(u.cinematic_pan_x, u.cinematic_pan_y);

    // User pan and zoom.
    uv = (uv - 0.5) / u.zoom_scale + 0.5;
    uv -= vec2(u.pan_x, u.pan_y);

    // Cover scaling: uniform scale to fill viewport, cropping the overflow.
    if (u.image_ar > u.viewport_ar) {
        float frac = u.viewport_ar / u.image_ar;
        uv.x = (1.0 - frac) * 0.5 + uv.x * frac;
    } else {
        float frac = u.image_ar / u.viewport_ar;
        uv.y = (1.0 - frac) * 0.5 + uv.y * frac;
    }

    // Precompute feedback UV: frag_uv mapped into scene_texture_ coordinate space.
    vec2 fb_uv = vec2(u.fb_uv_offset_x + frag_uv.x * u.fb_uv_scale_x,
                      u.fb_uv_offset_y + frag_uv.y * u.fb_uv_scale_y);

    // UV warps applied to both the source image UV and the feedback UV.
    // warp_fb tracks the maximum feedback blend requested by any active warp.
    vec2  uv_fb  = fb_uv;
    float warp_fb = 0.0;

    if (u.barrel_intensity > 0.0) {
        uv    = barrel_warp(uv);
        uv_fb = barrel_warp(uv_fb);
        warp_fb = max(warp_fb, u.barrel_feedback);
    }
    if (u.zoom_intensity > 0.0) {
        uv    = zoom_pulse(uv);
        uv_fb = zoom_pulse(uv_fb);
        warp_fb = max(warp_fb, u.zoom_feedback);
    }
    if (u.wave_intensity > 0.0) {
        uv    = wave_warp(uv);
        uv_fb = wave_warp(uv_fb);
        warp_fb = max(warp_fb, u.wave_feedback);
    }
    if (u.displacement_intensity > 0.0) {
        uv    = displacement_warp(uv);
        uv_fb = displacement_warp(uv_fb);
        warp_fb = max(warp_fb, u.displacement_feedback);
    }
    if (u.perlin_intensity > 0.0) {
        uv    = perlin_warp(uv);
        uv_fb = perlin_warp(uv_fb);
        warp_fb = max(warp_fb, u.perlin_feedback);
    }
    if (u.ripple_intensity > 0.0) {
        uv    = ripple_warp(uv);
        uv_fb = ripple_warp(uv_fb);
        warp_fb = max(warp_fb, u.ripple_feedback);
    }
    if (u.polar_intensity > 0.0) {
        uv    = polar_warp(uv);
        uv_fb = polar_warp(uv_fb);
        warp_fb = max(warp_fb, u.polar_feedback);
    }

    uv    = clamp(uv,    0.0, 1.0);
    uv_fb = clamp(uv_fb, 0.0, 1.0);

    if (u.pixelate_intensity > 0.0) {
        uv    = pixelate_uv(uv);
        uv_fb = pixelate_uv(uv_fb);
        warp_fb = max(warp_fb, u.pixelate_feedback);
    }
    if (u.kaleidoscope_intensity > 0.0) {
        uv    = kaleidoscope_uv(uv);
        uv_fb = kaleidoscope_uv(uv_fb);
        warp_fb = max(warp_fb, u.kaleidoscope_feedback);
    }

    // Primary texture sample. Glitch/chromatic blend using their own feedback;
    // plain sample blends based on the composite warp feedback.
    vec4 color;
    if (u.glitch_intensity > 0.0)
        color = apply_glitch(uv, uv_fb);
    else if (u.chromatic_intensity > 0.0)
        color = apply_chromatic(uv, uv_fb);
    else
        color = mix(texture(u_image, uv), texture(u_feedback, uv_fb), warp_fb);

    // Color effects. For each effect, if its feedback > 0, blend the accumulated
    // color toward the previous frame before applying the effect.
#define FB_BLEND(fb_param) \
    if ((fb_param) > 0.0) color.rgb = mix(color.rgb, texture(u_feedback, uv_fb).rgb, (fb_param));

    if (u.hue_shift_intensity    > 0.0) { FB_BLEND(u.hue_shift_feedback)    color.rgb = hue_shift(color.rgb); }
    if (u.saturation_intensity   > 0.0) { FB_BLEND(u.saturation_feedback)   color.rgb = apply_saturation(color.rgb); }
    if (u.brightness_intensity   > 0.0) { FB_BLEND(u.brightness_feedback)   color.rgb = apply_brightness(color.rgb); }
    if (u.posterize_intensity    > 0.0) { FB_BLEND(u.posterize_feedback)     color.rgb = apply_posterize(color.rgb); }
    if (u.solarize_intensity     > 0.0) { FB_BLEND(u.solarize_feedback)      color.rgb = apply_solarize(color.rgb); }
    if (u.duotone_intensity      > 0.0) { FB_BLEND(u.duotone_feedback)       color.rgb = apply_duotone(color.rgb); }
    if (u.channel_swap_intensity > 0.0) { FB_BLEND(u.channel_swap_feedback)  color.rgb = apply_channel_swap(color.rgb); }
    if (u.glow_intensity         > 0.0) { FB_BLEND(u.glow_feedback)          color.rgb = apply_glow(color.rgb, uv, uv_fb); }
    if (u.edge_glow_intensity    > 0.0) { FB_BLEND(u.edge_glow_feedback)     color.rgb = apply_edge_glow(color.rgb, uv, uv_fb); }
    if (u.emboss_intensity       > 0.0) { FB_BLEND(u.emboss_feedback)        color.rgb = apply_emboss(color.rgb, uv, uv_fb); }
    if (u.morph_intensity        > 0.0) { FB_BLEND(u.morph_feedback)         color.rgb = apply_morph(color.rgb, uv, uv_fb); }
    if (u.scanline_intensity     > 0.0) { FB_BLEND(u.scanline_feedback)      color.rgb = apply_scanlines(color.rgb, uv); }
    if (u.vignette_intensity     > 0.0) { FB_BLEND(u.vignette_feedback)      color.rgb = apply_vignette(color.rgb, uv); }
    if (u.film_grain_intensity   > 0.0) { FB_BLEND(u.film_grain_feedback)    color.rgb = apply_film_grain(color.rgb, uv); }

#undef FB_BLEND

    // Motion trails: blend current output with decayed previous frame.
    // fb_uv maps frag_uv (viewport-local 0-1) back into the full scene_texture_
    // coordinate space where the previous frame's content was stored.
    if (u.feedback_intensity > 0.0) {
        vec3  prev  = texture(u_feedback, fb_uv).rgb;
        float decay = 1.0 - u.feedback_intensity * (0.05 + u.rms * 0.4);
        color.rgb   = mix(color.rgb, prev * decay, u.feedback_intensity * 0.9);
    }

    out_color = color;
}
