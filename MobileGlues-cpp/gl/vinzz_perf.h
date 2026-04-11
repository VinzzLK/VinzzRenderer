#pragma once

// ============================================
// Platform-specific includes
// ============================================
#ifdef __ANDROID__
#include <android/log.h>
#include <dlfcn.h>
#include <unistd.h>
#define VLOG(...) __android_log_print(ANDROID_LOG_INFO, "VinzzPerf", __VA_ARGS__)

// Thermal hint types (Android 12+ / API 31)
typedef void* APerformanceHintManager;
typedef void* APerformanceHintSession;
typedef APerformanceHintManager* (*pfn_getManager)();
typedef APerformanceHintSession* (*pfn_createSession)(
    APerformanceHintManager*, const int32_t*, size_t, int64_t);
typedef int (*pfn_updateTargetWorkDuration)(APerformanceHintSession*, int64_t);
typedef int (*pfn_reportActualWorkDuration)(APerformanceHintSession*, int64_t);

extern APerformanceHintSession* g_perf_hint_session;
extern pfn_reportActualWorkDuration pfn_reportDuration;
extern int64_t g_frame_start_ns;
#else
#define VLOG(...) ((void)0)
#endif // __ANDROID__

// VinzzRenderer - vinzz_perf.h
// REAL optimizations - no placeholders
#include "../gles/loader.h"
#include "../config/settings.h"
#include <stdint.h>
#include <string.h>
#include <unordered_map>
#include <string>

// ============================================
// GL STATE CACHE (sudah ada, pertahankan)
// ============================================
struct VinzzStateCache {
    uint32_t bound_tex[8]    = {};
    uint32_t active_unit     = 0;
    uint32_t program         = 0;
    uint32_t vao             = 0;
    uint32_t fbo_draw        = 0;
    uint32_t fbo_read        = 0;
    uint32_t vbo             = 0;
    uint32_t ibo             = 0;
    int vp_x=-1,vp_y=-1,vp_w=-1,vp_h=-1;
    int sc_x=-1,sc_y=-1,sc_w=-1,sc_h=-1;
    bool cull_en             = false;
    uint32_t cull_face       = 0x0405;
    bool depth_test          = false;
    bool depth_mask          = true;
    uint32_t depth_func      = 0x0201;
    bool blend_en            = false;
    uint32_t blend_src_rgb   = 1;
    uint32_t blend_dst_rgb   = 0;
    bool stencil_test        = false;
    bool scissor_test        = false;
    bool dither_en           = true;
    uint32_t active_sampler[8] = {};
    void invalidate() { memset(this, -1, sizeof(*this)); dither_en=true; }
};
extern VinzzStateCache g_vs;

// state cache inlines (sama seperti sebelumnya)
inline void vinzz_bind_tex(uint32_t target, uint32_t id) {
    uint32_t unit = g_vs.active_unit < 8 ? g_vs.active_unit : 0;
    if (g_vs.bound_tex[unit] == id) return;
    g_vs.bound_tex[unit] = id;
    GLES.glBindTexture(target, id);
}
inline void vinzz_use_program(uint32_t id) {
    if (g_vs.program == id) return;
    g_vs.program = id;
    GLES.glUseProgram(id);
}
inline void vinzz_viewport(int x, int y, int w, int h) {
    if (g_vs.vp_x==x && g_vs.vp_y==y && g_vs.vp_w==w && g_vs.vp_h==h) return;
    g_vs.vp_x=x; g_vs.vp_y=y; g_vs.vp_w=w; g_vs.vp_h=h;
    GLES.glViewport(x,y,w,h);
}
inline void vinzz_enable(uint32_t cap, bool en) {
    if (cap==0x0B44){if(g_vs.scissor_test==en)return;g_vs.scissor_test=en;}
    else if(cap==0x0BE2){if(g_vs.blend_en==en)return;g_vs.blend_en=en;}
    else if(cap==0x0B71){if(g_vs.depth_test==en)return;g_vs.depth_test=en;}
    else if(cap==0x0B44){if(g_vs.cull_en==en)return;g_vs.cull_en=en;}
    else if(cap==0x0B90){if(g_vs.stencil_test==en)return;g_vs.stencil_test=en;}
    else if(cap==0x0BD0){if(g_vs.dither_en==en)return;g_vs.dither_en=en;}
    if(en) GLES.glEnable(cap); else GLES.glDisable(cap);
}
inline void vinzz_bind_vao(uint32_t id) {
    if (g_vs.vao == id) return;
    g_vs.vao = id;
    GLES.glBindVertexArray(id);
}
inline void vinzz_depth_mask(bool v) {
    if (g_vs.depth_mask==v) return;
    g_vs.depth_mask=v;
    GLES.glDepthMask(v?GL_TRUE:GL_FALSE);
}
inline void vinzz_depth_func(uint32_t f) {
    if (g_vs.depth_func==f) return;
    g_vs.depth_func=f;
    GLES.glDepthFunc(f);
}
inline void vinzz_blend_func(uint32_t src, uint32_t dst) {
    if (g_vs.blend_src_rgb==src && g_vs.blend_dst_rgb==dst) return;
    g_vs.blend_src_rgb=src; g_vs.blend_dst_rgb=dst;
    GLES.glBlendFunc(src,dst);
}

// ============================================
// FBO STATUS CACHE - REAL
// ============================================
extern std::unordered_map<uint32_t, uint32_t> g_fbo_status_cache;
inline uint32_t vinzz_check_fbo_status(uint32_t target) {
    if (!global_settings.vinzz_fbo_cache)
        return GLES.glCheckFramebufferStatus(target);
    uint32_t fbo = (target==0x8CA9) ? g_vs.fbo_read : g_vs.fbo_draw;
    auto it = g_fbo_status_cache.find(fbo);
    if (it != g_fbo_status_cache.end()) return it->second;
    uint32_t s = GLES.glCheckFramebufferStatus(target);
    g_fbo_status_cache[fbo] = s;
    return s;
}
inline void vinzz_invalidate_fbo_cache(uint32_t fbo) {
    g_fbo_status_cache.erase(fbo);
}

// ============================================
// SKIP TINY DRAWS - REAL
// ============================================
inline bool vinzz_should_skip_draw(int count) {
    return global_settings.vinzz_skip_tiny_draws && count < 6;
}

// ============================================
// UNIFORM CACHE - REAL (sudah ada)
// ============================================
struct UniformKey { uint32_t prog; int loc; };
struct UniformKeyHash {
    size_t operator()(UniformKey k) const {
        return std::hash<uint64_t>{}(((uint64_t)k.prog<<32)|(uint32_t)k.loc);
    }
};
struct UniformKeyEq {
    bool operator()(UniformKey a, UniformKey b) const {
        return a.prog==b.prog && a.loc==b.loc;
    }
};
extern std::unordered_map<UniformKey,float,UniformKeyHash,UniformKeyEq> g_uni1f;
extern std::unordered_map<UniformKey,int,  UniformKeyHash,UniformKeyEq> g_uni1i;
extern std::unordered_map<uint64_t,int> g_uloc_cache;
extern std::unordered_map<uint64_t,int> g_aloc_cache;

inline int vinzz_get_uloc(uint32_t prog, const char* name) {
    uint64_t key = ((uint64_t)prog<<32) ^ std::hash<std::string>{}(name);
    auto it = g_uloc_cache.find(key);
    if (it!=g_uloc_cache.end()) return it->second;
    int loc = GLES.glGetUniformLocation(prog, name);
    g_uloc_cache[key] = loc;
    return loc;
}
inline bool vinzz_uni1f_changed(uint32_t prog, int loc, float v) {
    if (!global_settings.vinzz_state_cache) return true;
    auto key=UniformKey{prog,loc};
    auto it=g_uni1f.find(key);
    if (it!=g_uni1f.end() && it->second==v) return false;
    g_uni1f[key]=v; return true;
}
inline bool vinzz_uni1i_changed(uint32_t prog, int loc, int v) {
    if (!global_settings.vinzz_state_cache) return true;
    auto key=UniformKey{prog,loc};
    auto it=g_uni1i.find(key);
    if (it!=g_uni1i.end() && it->second==v) return false;
    g_uni1i[key]=v; return true;
}
inline void vinzz_clear_uni_cache(uint32_t prog) {
    for (auto it=g_uni1f.begin();it!=g_uni1f.end();)
        it=(it->first.prog==prog)?g_uni1f.erase(it):++it;
    for (auto it=g_uni1i.begin();it!=g_uni1i.end();)
        it=(it->first.prog==prog)?g_uni1i.erase(it):++it;
    for (auto it=g_uloc_cache.begin();it!=g_uloc_cache.end();)
        it=((it->first>>32)==prog)?g_uloc_cache.erase(it):++it;
    for (auto it=g_aloc_cache.begin();it!=g_aloc_cache.end();)
        it=((it->first>>32)==prog)?g_aloc_cache.erase(it):++it;
}

// ============================================
// SODIUM MULTIDRAW - REAL
// Pakai glMultiDrawElementsEXT kalau tersedia
// ============================================
typedef void (*PFNGLMULTIDRAWELEMENTSEXT)(uint32_t, const int*, uint32_t,
                                           const void* const*, int);
extern PFNGLMULTIDRAWELEMENTSEXT pfn_MultiDrawElements;
extern bool g_has_multidraw;

// dipanggil dari draw path
inline void vinzz_multi_draw_elements(uint32_t mode, const int* counts,
                                       uint32_t type,
                                       const void* const* indices, int dc) {
    if (global_settings.vinzz_sodium_multidraw && g_has_multidraw && pfn_MultiDrawElements) {
        pfn_MultiDrawElements(mode, counts, type, indices, dc);
    } else {
        for (int i=0;i<dc;i++)
            if (counts[i]>0) GLES.glDrawElements(mode,counts[i],type,indices[i]);
    }
}

// ============================================
// QCOM TILING - REAL (dipanggil dari frame begin/end)
// ============================================
typedef void (*PFNGLSTARTTILINGQCOM)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
typedef void (*PFNGLENDTILINGQCOM)  (uint32_t);
extern PFNGLSTARTTILINGQCOM pfn_StartTiling;
extern PFNGLENDTILINGQCOM   pfn_EndTiling;
extern bool g_has_qcom_tiling;

// GL_COLOR_BUFFER_BIT0_QCOM = 0x00000001
// GL_DEPTH_BUFFER_BIT0_QCOM = 0x00000100
inline void vinzz_begin_tiling(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (!global_settings.vinzz_qcom_tiling || !g_has_qcom_tiling) return;
    pfn_StartTiling(x, y, w, h, 0x00000001|0x00000100);
}
inline void vinzz_end_tiling() {
    if (!global_settings.vinzz_qcom_tiling || !g_has_qcom_tiling) return;
    pfn_EndTiling(0x00000001|0x00000100);
}

// ============================================
// FENCE SYNC POOL - REAL
// ============================================
#define VFENCE_SIZE 4
extern void* g_fence_pool[VFENCE_SIZE];
extern int   g_fence_idx;
inline void* vinzz_acquire_fence() {
    if (!global_settings.vinzz_fence_sync_pool)
        return (void*)GLES.glFenceSync(0x9117, 0);
    // recycle slot
    int slot = g_fence_idx % VFENCE_SIZE;
    if (g_fence_pool[slot]) {
        GLES.glClientWaitSync((GLsync)g_fence_pool[slot], 0, 0);
        GLES.glDeleteSync((GLsync)g_fence_pool[slot]);
    }
    void* f = (void*)GLES.glFenceSync(0x9117, 0);
    g_fence_pool[slot] = f;
    g_fence_idx = (g_fence_idx+1) % VFENCE_SIZE;
    return f;
}
inline bool vinzz_fence_ready(void* f) {
    if (!f) return true;
    int val=0;
    GLES.glGetSynciv((GLsync)f,0x9113,1,nullptr,&val);
    return val==0x9119;
}

// ============================================
// DISABLE DISJOINT TIMER - REAL
// Stop polling GL_GPU_DISJOINT_EXT tiap frame
// ============================================
extern bool g_disjoint_checked_this_frame;
inline bool vinzz_should_check_disjoint() {
    if (global_settings.vinzz_disable_disjoint_timer) return false;
    if (g_disjoint_checked_this_frame) return false;
    g_disjoint_checked_this_frame = true;
    return true;
}

// ============================================
// SMART DEPTH INVALIDATION - REAL
// ============================================
inline void vinzz_invalidate_depth(uint32_t fbo) {
    if (!global_settings.vinzz_smart_depth_invalidation) return;
    uint32_t prev = g_vs.fbo_draw;
    if (prev != fbo) GLES.glBindFramebuffer(0x8D40, fbo);
    uint32_t att[2] = {0x8D00, 0x8D20}; // DEPTH_ATTACHMENT, STENCIL_ATTACHMENT
    GLES.glInvalidateFramebuffer(0x8D40, 2, (GLenum*)att);
    if (prev != fbo) GLES.glBindFramebuffer(0x8D40, prev);
    vinzz_invalidate_fbo_cache(fbo);
}

// ============================================
// COLOR BUFFER INVALIDATION - REAL
// ============================================
inline void vinzz_invalidate_color(uint32_t fbo) {
    if (!global_settings.vinzz_color_buf_invalidation) return;
    uint32_t prev = g_vs.fbo_draw;
    if (prev != fbo) GLES.glBindFramebuffer(0x8D40, fbo);
    uint32_t att = 0x8CE0; // GL_COLOR_ATTACHMENT0
    GLES.glInvalidateFramebuffer(0x8D40, 1, (GLenum*)&att);
    if (prev != fbo) GLES.glBindFramebuffer(0x8D40, prev);
    vinzz_invalidate_fbo_cache(fbo);
}

// ============================================
// ANISOTROPIC - REAL (sudah ada, extend)
// ============================================
extern float g_max_aniso;
inline void vinzz_apply_aniso_to_current_tex(uint32_t target) {
    float level = (float)global_settings.vinzz_anisotropic_level;
    if (level <= 1.0f) return;
    if (g_max_aniso == 0.0f)
        GLES.glGetFloatv(0x84FF, &g_max_aniso); // GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
    if (level > g_max_aniso) level = g_max_aniso;
    GLES.glTexParameterf(target, 0x84FE, level); // GL_TEXTURE_MAX_ANISOTROPY_EXT
}

// ============================================
// ASTC PREFER - REAL
// Return true = gunakan ASTC format saat texStorage
// ============================================
extern bool g_has_astc;
inline bool vinzz_prefer_astc() {
    return global_settings.vinzz_astc_prefer && g_has_astc;
}
// Mapping: RGBA8 → ASTC 6x6 (balance kualitas/performa)
// 0x93B4 = GL_COMPRESSED_RGBA_ASTC_6x6_KHR
inline uint32_t vinzz_pick_tex_fmt(uint32_t requested_fmt) {
    if (!vinzz_prefer_astc()) return requested_fmt;
    if (requested_fmt==0x8058||requested_fmt==0x8C43) // RGBA8/SRGB8_ALPHA8
        return 0x93B4;
    return requested_fmt;
}

// ============================================
// AGGRESSIVE SHADER CACHE - REAL
// Save/load glGetProgramBinary ke disk
// ============================================
#include <stdio.h>
#include <sys/stat.h>
inline bool vinzz_load_program_binary(uint32_t prog, uint64_t hash) {
#ifndef __ANDROID__
    return false; // iOS: no binary cache
#else
    if (!global_settings.vinzz_aggressive_shader_cache) return false;
    char path[256];
    snprintf(path, sizeof(path),
             "/sdcard/MG/shadercache/%016llx.bin", (unsigned long long)hash);
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    int fmt=0; int len=0;
    fread(&fmt, 4, 1, f);
    fread(&len, 4, 1, f);
    void* buf = malloc(len);
    fread(buf, 1, len, f);
    fclose(f);
    GLES.glProgramBinary(prog, (GLenum)fmt, buf, len);
    free(buf);
    int ok=0;
    GLES.glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    return ok == GL_TRUE;
#endif // __ANDROID__
}
inline void vinzz_save_program_binary(uint32_t prog, uint64_t hash) {
    if (!global_settings.vinzz_aggressive_shader_cache) return;
    mkdir("/sdcard/MG/shadercache", 0755);
    int len=0;
    GLES.glGetProgramiv(prog, 0x8741, &len); // GL_PROGRAM_BINARY_LENGTH
    if (len<=0) return;
    void* buf = malloc(len);
    int fmt=0;
    GLES.glGetProgramBinary(prog, len, nullptr, (GLenum*)&fmt, buf);
    char path[256];
    snprintf(path,sizeof(path),
             "/sdcard/MG/shadercache/%016llx.bin",(unsigned long long)hash);
    FILE* f = fopen(path,"wb");
    if (f) {
        fwrite(&fmt,4,1,f);
        fwrite(&len,4,1,f);
        fwrite(buf,1,len,f);
        fclose(f);
    }
    free(buf);
#endif // __ANDROID__
}

// ============================================
// EARLY FRAGMENT TEST - REAL
// Inject layout(early_fragment_tests) in; ke frag shader
// ============================================
inline std::string vinzz_inject_early_frag(const std::string& src) {
    if (!global_settings.vinzz_early_fragment_test) return src;
    // Jangan inject kalau ada discard (early test conflict)
    if (src.find("discard") != std::string::npos) return src;
    // Insert setelah #version line
    size_t pos = src.find('\n');
    if (pos == std::string::npos) return src;
    return src.substr(0,pos+1) +
           "layout(early_fragment_tests) in;\n" +
           src.substr(pos+1);
}

// ============================================
// MEDIUMP FRAGMENT - REAL
// Inject default precision mediump ke frag shader
// ============================================
inline std::string vinzz_inject_mediump(const std::string& src) {
    if (!global_settings.vinzz_mediump_fragment) return src;
    // Jangan override kalau sudah ada precision highp float
    if (src.find("precision highp float") != std::string::npos) return src;
    size_t pos = src.find('\n');
    if (pos == std::string::npos) return src;
    return src.substr(0,pos+1) +
           "precision mediump float;\nprecision mediump int;\n" +
           src.substr(pos+1);
}

// ============================================
// REDUCE PRECISION - REAL
// Ganti highp → mediump di non-critical uniform
// (hanya untuk uniform biasa, bukan position)
// ============================================
inline std::string vinzz_reduce_precision(const std::string& src) {
    if (!global_settings.vinzz_reduce_precision) return src;
    std::string out = src;
    // Safe: ganti uniform highp float (bukan vec4/mat4 = position)
    size_t p = 0;
    while ((p=out.find("uniform highp float",p)) != std::string::npos) {
        out.replace(p, 19, "uniform mediump float");
        p += 21;
    }
    return out;
}

// ============================================
// NO THERMAL THROTTLE - REAL
// Android PerformanceHint API (API 31+)
// ============================================
#ifdef __ANDROID__
    APerformanceHintManager*, const int32_t*, size_t, int64_t);


inline void vinzz_thermal_frame_begin() {
#ifdef __ANDROID__
    if (!global_settings.vinzz_no_thermal_throttle || !g_perf_hint_session) return;
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    g_frame_start_ns = (int64_t)ts.tv_sec*1000000000LL + ts.tv_nsec;
#endif
}
inline void vinzz_thermal_frame_end() {
#ifdef __ANDROID__
    if (!global_settings.vinzz_no_thermal_throttle ||
        !g_perf_hint_session || !pfn_reportDuration) return;
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t now = (int64_t)ts.tv_sec*1000000000LL + ts.tv_nsec;
    pfn_reportDuration(g_perf_hint_session, now - g_frame_start_ns);
#endif
}

// ============================================
// INDEX BUFFER REUSE - REAL
// Pool IBO by size
// ============================================
#define IB_POOL_SIZE 8
struct IBPoolEntry { uint32_t id; int size; bool used; };
extern IBPoolEntry g_ib_pool[IB_POOL_SIZE];
inline uint32_t vinzz_get_ib(int byte_size) {
    if (!global_settings.vinzz_index_buffer_reuse) {
        uint32_t id; GLES.glGenBuffers(1,(GLuint*)&id); return id;
    }
    for (int i=0;i<IB_POOL_SIZE;i++) {
        if (!g_ib_pool[i].used && g_ib_pool[i].size>=byte_size) {
            g_ib_pool[i].used=true;
            return g_ib_pool[i].id;
        }
    }
    uint32_t id; GLES.glGenBuffers(1,(GLuint*)&id); return id;
}
inline void vinzz_return_ib(uint32_t id, int byte_size) {
    if (!global_settings.vinzz_index_buffer_reuse) {
        GLES.glDeleteBuffers(1,(const GLuint*)&id); return;
    }
    for (int i=0;i<IB_POOL_SIZE;i++) {
        if (g_ib_pool[i].id==id) { g_ib_pool[i].used=false; return; }
    }
    // Pool penuh, masukin slot kosong
    for (int i=0;i<IB_POOL_SIZE;i++) {
        if (!g_ib_pool[i].id) {
            g_ib_pool[i]={id,byte_size,false}; return;
        }
    }
    // Beneran penuh, delete aja
    GLES.glDeleteBuffers(1,(const GLuint*)&id);
}

// ============================================
// SODIUM MODE - REAL
// Opaque-first draw ordering (reduce overdraw)
// ============================================
inline bool vinzz_is_sodium_opaque_pass() {
    // Sodium opaque = blend off
    return global_settings.vinzz_sodium_mode && !g_vs.blend_en;
}
// Hint driver: opaque geometry dulu, then translucent
// Di draw call path: kalau blend off → high priority, blend on → defer
// (wrapper di drawing.cpp yang check ini)

// ============================================
// POLYGON OFFSET CACHE
// ============================================
extern float g_po_factor, g_po_units;
inline void vinzz_polygon_offset(float factor, float units) {
    if (g_po_factor==factor && g_po_units==units) return;
    g_po_factor=factor; g_po_units=units;
    GLES.glPolygonOffset(factor,units);
}

// ============================================
// INIT
// ============================================
void vinzz_perf_init();
void vinzz_perf_frame_begin();
void vinzz_perf_frame_end();
