#pragma once

#include <cstddef>

namespace kaos::reactor {

struct EffectParams {
    float zoom_intensity{0.0f};
    float wave_intensity{0.0f};
    float hue_shift_intensity{0.0f};
    float glitch_intensity{0.0f};
    float glow_intensity{0.0f};
    float displacement_intensity{0.0f};
    float perlin_intensity{0.0f};
    float warp_scale{1.0f};
    float brightness_intensity{0.0f};
    float saturation_intensity{0.0f};
    float vignette_intensity{0.0f};
    float chromatic_intensity{0.0f};
    float film_grain_intensity{0.0f};
    float pixelate_intensity{0.0f};
    float edge_glow_intensity{0.0f};
    float morph_intensity{0.0f};
    float barrel_intensity{0.0f};
    float kaleidoscope_intensity{0.0f};
    float polar_intensity{0.0f};
    float ripple_intensity{0.0f};
    float posterize_intensity{0.0f};
    float solarize_intensity{0.0f};
    float channel_swap_intensity{0.0f};
    float duotone_intensity{0.0f};
    float emboss_intensity{0.0f};
    float scanline_intensity{0.0f};
    float feedback_intensity{0.0f};

    float zoom_feedback{0.0f};
    float wave_feedback{0.0f};
    float hue_shift_feedback{0.0f};
    float glitch_feedback{0.0f};
    float glow_feedback{0.0f};
    float displacement_feedback{0.0f};
    float perlin_feedback{0.0f};
    float brightness_feedback{0.0f};
    float saturation_feedback{0.0f};
    float vignette_feedback{0.0f};
    float chromatic_feedback{0.0f};
    float film_grain_feedback{0.0f};
    float pixelate_feedback{0.0f};
    float edge_glow_feedback{0.0f};
    float morph_feedback{0.0f};
    float barrel_feedback{0.0f};
    float kaleidoscope_feedback{0.0f};
    float polar_feedback{0.0f};
    float ripple_feedback{0.0f};
    float posterize_feedback{0.0f};
    float solarize_feedback{0.0f};
    float channel_swap_feedback{0.0f};
    float duotone_feedback{0.0f};
    float emboss_feedback{0.0f};
    float scanline_feedback{0.0f};
};

// Mirror of the GLSL std140 UBO in image.frag — layout must match exactly.
struct EffectUBO {
    float rms{0.f};
    float bass{0.f};
    float mid{0.f};
    float treble{0.f};
    float spectral_centroid{0.f};
    float spectral_flux{0.f};
    float time{0.f};
    float beat{0.f};
    float onset{0.f};
    float zoom_intensity{0.f};
    float wave_intensity{0.f};
    float hue_shift_intensity{0.f};
    float glitch_intensity{0.f};
    float glow_intensity{0.f};
    float displacement_intensity{0.f};
    float perlin_intensity{0.f};
    float image_ar{1.f};    // source image width / height
    float viewport_ar{1.f}; // export resolution width / height
    float pan_x{0.f};
    float pan_y{0.f};
    float zoom_scale{1.f};
    float warp_scale{1.f};
    float brightness_intensity{0.f};
    float saturation_intensity{0.f};
    float vignette_intensity{0.f};
    float chromatic_intensity{0.f};
    float film_grain_intensity{0.f};
    float pixelate_intensity{0.f};
    float edge_glow_intensity{0.f};
    float morph_intensity{0.f};
    float barrel_intensity{0.f};
    float kaleidoscope_intensity{0.f};
    float polar_intensity{0.f};
    float ripple_intensity{0.f};
    float posterize_intensity{0.f};
    float solarize_intensity{0.f};
    float channel_swap_intensity{0.f};
    float duotone_intensity{0.f};
    float emboss_intensity{0.f};
    float scanline_intensity{0.f};
    float feedback_intensity{0.f};
    float cinematic_zoom{1.f};
    float cinematic_pan_x{0.f};
    float cinematic_pan_y{0.f};
    // Normalized viewport rect within scene_texture_ for feedback UV correction.
    float fb_uv_offset_x{0.f};
    float fb_uv_offset_y{0.f};
    float fb_uv_scale_x{1.f};
    float fb_uv_scale_y{1.f};
    // Per-effect feedback blend (0 = base image, 1 = previous frame).
    float zoom_feedback{0.f};
    float wave_feedback{0.f};
    float hue_shift_feedback{0.f};
    float glitch_feedback{0.f};
    float glow_feedback{0.f};
    float displacement_feedback{0.f};
    float perlin_feedback{0.f};
    float brightness_feedback{0.f};
    float saturation_feedback{0.f};
    float vignette_feedback{0.f};
    float chromatic_feedback{0.f};
    float film_grain_feedback{0.f};
    float pixelate_feedback{0.f};
    float edge_glow_feedback{0.f};
    float morph_feedback{0.f};
    float barrel_feedback{0.f};
    float kaleidoscope_feedback{0.f};
    float polar_feedback{0.f};
    float ripple_feedback{0.f};
    float posterize_feedback{0.f};
    float solarize_feedback{0.f};
    float channel_swap_feedback{0.f};
    float duotone_feedback{0.f};
    float emboss_feedback{0.f};
    float scanline_feedback{0.f};
    // Padding to reach 304 bytes (19 × 16, required by std140).
    float reserved0{0.f};
    float reserved1{0.f};
    float reserved2{0.f};
};
static_assert(sizeof(EffectUBO) == 304, "EffectUBO size must match GLSL std140 layout");

} // namespace kaos::reactor
