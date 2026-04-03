
// VinzzRenderer - vinzz_perf.cpp
#include "vinzz_perf.h"
#include "../gles/loader.h"
#include "../config/settings.h"

VinzzStateCache g_vstate;

void vinzz_perf_init() {
    g_vstate.reset();
    g_use_tex_storage = (g_gles_caps.major >= 3);
    g_spo_supported = false; // Check extension if needed
    
    if (global_settings.vinzz_fast_hints) {
        vinzz_adreno650_init();
    }
    
    LOG_V("[VinzzRenderer] Performance system initialized - 50 optimizations active")
    LOG_V("[VinzzRenderer] State cache: ON | Uniform cache: ON | Sampler pool: ON")
    LOG_V("[VinzzRenderer] Buffer unsync map: ON | Fence pool: ON | Mip clamp: ON")
}

void vinzz_perf_frame_begin() {
    vinzz_reset_draw_count();
    g_flush_defer = 0;
}

void vinzz_perf_frame_end() {
    // Log draw call stats occasionally
    static int frame = 0;
    if (++frame % 600 == 0) {
        LOG_V("[VinzzRenderer] Draw calls/frame: %d", g_draw_call_frame_count)
    }
}
