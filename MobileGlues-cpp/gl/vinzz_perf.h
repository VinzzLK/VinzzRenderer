#pragma once
// VinzzRenderer - vinzz_perf.h
// 50 Performance Optimizations for Adreno 650 / Snapdragon 870
#include <stdint.h>
#include <string.h>
#include <unordered_map>
#include <string>
#include <vector>

// ============================================
// STATE CACHE
// ============================================
struct VinzzStateCache {
    uint32_t tex_bound[32] = {0};
    uint32_t active_tex_unit = 0;
    uint32_t program = 0;
    uint32_t vao = 0;
    uint32_t vbo = 0;
    uint32_t ebo = 0;
    uint32_t ubo[16] = {0};
    int vp_x=-1, vp_y=-1, vp_w=-1, vp_h=-1;
    int sc_x=-1, sc_y=-1, sc_w=-1, sc_h=-1;
    bool cmask_r=true,cmask_g=true,cmask_b=true,cmask_a=true;
    bool depth_mask=true;
    uint32_t cull_face=0x0405;
    bool cull_enabled=false;
    bool blend_enabled=false;
    uint32_t blend_src=0x0302,blend_dst=0x0303;
    bool depth_test=false;
    uint32_t depth_func=0x0201;
    bool stencil_test=false;
    bool poly_offset=false;
    float poly_offset_factor=0.0f, poly_offset_units=0.0f;
    bool dirty=false;
    void reset() { memset(this, 0, sizeof(*this)); dirty=true; }
};

extern VinzzStateCache g_vstate;

// Draw call tracking
extern int g_draw_call_count;
extern int g_draw_call_frame_count;
inline void vinzz_count_draw() { g_draw_call_count++; }

// Memory barrier flag
extern bool g_needs_barrier;
inline void vinzz_mark_barrier_needed() { g_needs_barrier = true; }

// Uniform location cache
extern std::unordered_map<uint64_t, int> g_uniform_loc_cache;
extern std::unordered_map<uint64_t, float> g_uniform_float_cache;
extern std::unordered_map<uint64_t, uint64_t> g_matrix_hash_cache;
extern std::unordered_map<uint64_t, int> g_attrib_cache;

// Buffer state
extern bool g_use_tex_storage;
extern bool g_spo_supported;
extern bool g_shaders_warmed;
extern bool g_needs_barrier;

// Sampler pool
struct VinzzSamplerDesc {
    uint32_t min_filter,mag_filter,wrap_s,wrap_t;
    float max_aniso;
    uint32_t sampler_id;
};
extern std::vector<VinzzSamplerDesc> g_sampler_pool;

// Fence pool
extern uint32_t g_fence_pool[8];
extern int g_fence_pool_size;

// Flush defer
extern int g_flush_defer;

// Opt 10: Optimal index type selection
inline uint32_t vinzz_optimal_index_type(int vertex_count) {
    return (vertex_count < 65536) ? 0x1403u : 0x1405u;
}

// Opt 15: LOD bias calc
inline float vinzz_calc_lod_bias(float view_distance) {
    return (view_distance > 8.0f) ? 0.5f : -0.5f;
}

// Opt 34: Uniform float dedup
inline bool vinzz_uniform_float_changed(uint32_t prog, int loc, float v) {
    uint64_t key = ((uint64_t)prog << 32) | (uint32_t)loc;
    auto it = g_uniform_float_cache.find(key);
    if (it != g_uniform_float_cache.end() && it->second == v) return false;
    g_uniform_float_cache[key] = v;
    return true;
}

// Opt 35: Matrix hash dedup
inline bool vinzz_matrix_changed(uint32_t prog, int loc, const float* m) {
    uint64_t h = 0;
    for (int i=0;i<16;i++) h ^= std::hash<float>{}(m[i]) << (i*2);
    uint64_t key = ((uint64_t)prog << 32) | (uint32_t)loc;
    auto it = g_matrix_hash_cache.find(key);
    if (it != g_matrix_hash_cache.end() && it->second == h) return false;
    g_matrix_hash_cache[key] = h;
    return true;
}

// Functions implemented in vinzz_perf.cpp
void vinzz_perf_init();
void vinzz_perf_frame_begin();
void vinzz_perf_frame_end();
void vinzz_reset_draw_count();
void vinzz_emit_barrier_if_needed(uint32_t bits);
int  vinzz_get_uniform(uint32_t prog, const char* name);
int  vinzz_get_attrib(uint32_t prog, const char* name);
void vinzz_clear_uniform_cache(uint32_t prog);
uint32_t vinzz_get_sampler(uint32_t min_f, uint32_t mag_f, uint32_t ws, uint32_t wt, float aniso);
void vinzz_track_deleted_tex(uint32_t id);
void vinzz_clamp_mip_levels(uint32_t target, int max_level);
void* vinzz_get_fence();
void* vinzz_map_write_unsync(uint32_t target, int offset, int length);
void vinzz_orphan_buffer(uint32_t target, int size);
void vinzz_fast_clear(uint32_t mask);
void vinzz_adreno650_init();
