// VinzzRenderer - vinzz_perf.cpp
// Implementation of 50 Adreno 650 optimizations
#include "vinzz_perf.h"
#include "../gles/loader.h"
#include "../config/settings.h"
#include "log.h"

// ============================================
// GLOBAL STATE
// ============================================
VinzzStateCache g_vstate;
int g_draw_call_count = 0;
int g_draw_call_frame_count = 0;
bool g_needs_barrier = false;
bool g_use_tex_storage = false;
bool g_spo_supported = false;
bool g_shaders_warmed = false;
int g_flush_defer = 0;

std::unordered_map<uint64_t, int> g_uniform_loc_cache;
std::unordered_map<uint64_t, float> g_uniform_float_cache;
std::unordered_map<uint64_t, uint64_t> g_matrix_hash_cache;
std::unordered_map<uint64_t, int> g_attrib_cache;
std::vector<VinzzSamplerDesc> g_sampler_pool;

uint32_t g_fence_pool[8] = {0};
int g_fence_pool_size = 0;

static uint32_t g_last_tex_deleted[64] = {0};
static int g_tex_del_idx = 0;
static uint32_t g_double_buf[2] = {0,0};
static int g_double_buf_idx = 0;

// ============================================
// OPT 17: Barrier emit
// ============================================
void vinzz_emit_barrier_if_needed(uint32_t bits) {
    if (g_needs_barrier) {
        GLES.glMemoryBarrier(bits);
        g_needs_barrier = false;
    }
}

// ============================================
// OPT 18: Uniform location cache
// ============================================
int vinzz_get_uniform(uint32_t prog, const char* name) {
    uint64_t key = ((uint64_t)prog << 32) ^ std::hash<std::string>{}(name);
    auto it = g_uniform_loc_cache.find(key);
    if (it != g_uniform_loc_cache.end()) return it->second;
    int loc = GLES.glGetUniformLocation(prog, name);
    g_uniform_loc_cache[key] = loc;
    return loc;
}

void vinzz_clear_uniform_cache(uint32_t prog) {
    for (auto it = g_uniform_loc_cache.begin(); it != g_uniform_loc_cache.end();) {
        if ((it->first >> 32) == prog) it = g_uniform_loc_cache.erase(it);
        else ++it;
    }
    for (auto it = g_uniform_float_cache.begin(); it != g_uniform_float_cache.end();) {
        if ((it->first >> 32) == prog) it = g_uniform_float_cache.erase(it);
        else ++it;
    }
    for (auto it = g_matrix_hash_cache.begin(); it != g_matrix_hash_cache.end();) {
        if ((it->first >> 32) == prog) it = g_matrix_hash_cache.erase(it);
        else ++it;
    }
}

// ============================================
// OPT 19: Attrib location cache
// ============================================
int vinzz_get_attrib(uint32_t prog, const char* name) {
    uint64_t key = ((uint64_t)prog << 32) ^ std::hash<std::string>{}(name);
    auto it = g_attrib_cache.find(key);
    if (it != g_attrib_cache.end()) return it->second;
    int loc = GLES.glGetAttribLocation(prog, name);
    g_attrib_cache[key] = loc;
    return loc;
}

// ============================================
// OPT 20: Sampler object pool
// ============================================
uint32_t vinzz_get_sampler(uint32_t min_f, uint32_t mag_f, uint32_t ws, uint32_t wt, float aniso) {
    for (auto& s : g_sampler_pool) {
        if (s.min_filter==min_f && s.mag_filter==mag_f &&
            s.wrap_s==ws && s.wrap_t==wt && s.max_aniso==aniso)
            return s.sampler_id;
    }
    uint32_t sid = 0;
    GLES.glGenSamplers(1, &sid);
    GLES.glSamplerParameteri(sid, 0x2800, mag_f);
    GLES.glSamplerParameteri(sid, 0x2801, min_f);
    GLES.glSamplerParameteri(sid, 0x2802, ws);
    GLES.glSamplerParameteri(sid, 0x2803, wt);
    if (aniso > 1.0f) GLES.glSamplerParameterf(sid, 0x84FE, aniso);
    g_sampler_pool.push_back({min_f, mag_f, ws, wt, aniso, sid});
    return sid;
}

// ============================================
// OPT 21-25: Adreno 650 init hints
// ============================================
void vinzz_adreno650_init() {
    GLES.glEnable(0x0B44);        // GL_SCISSOR_TEST - tiled fast clear
    GLES.glHint(0x0C54, 0x1101);  // GL_FOG_HINT, GL_FASTEST
    GLES.glHint(0x0C52, 0x1101);  // GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST
    GLES.glDisable(0x0BD0);       // GL_DITHER off
}

// ============================================
// OPT 29: Fast clear (skip stencil on default FBO)
// ============================================
extern GLuint current_draw_fbo;
void vinzz_fast_clear(uint32_t mask) {
    if ((mask & 0x0400) && current_draw_fbo == 0) {
        GLES.glClear(mask & ~0x0400u);
        return;
    }
    GLES.glClear(mask);
}

// ============================================
// OPT 11: Buffer orphaning
// ============================================
void vinzz_orphan_buffer(uint32_t target, int size) {
    GLES.glBufferData(target, size, nullptr, 0x88E0); // GL_STREAM_DRAW
}

// ============================================
// OPT 30: Track deleted textures
// ============================================
void vinzz_track_deleted_tex(uint32_t id) {
    g_last_tex_deleted[g_tex_del_idx++ & 63] = id;
}

// ============================================
// OPT 43: Clamp mip levels
// ============================================
void vinzz_clamp_mip_levels(uint32_t target, int max_level) {
    GLES.glTexParameteri(target, 0x813D, max_level);
}

// ============================================
// OPT 46: Fence pool
// ============================================
void* vinzz_get_fence() {
    if (g_fence_pool_size > 0)
        return (void*)(uintptr_t)g_fence_pool[--g_fence_pool_size];
    return nullptr;
}

// ============================================
// OPT 50: Unsynchronized buffer map
// ============================================
void* vinzz_map_write_unsync(uint32_t target, int offset, int length) {
    return GLES.glMapBufferRange(target, offset, length,
        0x0002 | 0x0020 | 0x0040);
}

// ============================================
// FRAME MANAGEMENT
// ============================================
void vinzz_reset_draw_count() {
    g_draw_call_frame_count = g_draw_call_count;
    g_draw_call_count = 0;
}

void vinzz_perf_init() {
    g_vstate.reset();
    g_use_tex_storage = (g_gles_caps.major >= 3);
    if (global_settings.vinzz_fast_hints) {
        vinzz_adreno650_init();
    }
    /* VinzzRenderer: 50 optimizations initialized! */
    /* VinzzRenderer: Adreno 650 Snapdragon 870 - Max FPS mode */
}

void vinzz_perf_frame_begin() {
    vinzz_reset_draw_count();
    g_flush_defer = 0;
}

void vinzz_perf_frame_end() {
    static int frame = 0;
    if (++frame % 600 == 0) {
        /* VinzzRenderer: Draw calls/frame: %d */
    }
}
