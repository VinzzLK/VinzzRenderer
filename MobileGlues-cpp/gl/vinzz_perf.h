#pragma once
// VinzzRenderer - vinzz_perf.h
// REAL optimizations - cross-platform (Android + iOS)
#include "../gles/loader.h"
#include "../config/settings.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unordered_map>
#include <unordered_set>
#include <string>

// ── Platform logging ─────────────────────────────────────────
#ifdef __ANDROID__
#  include <android/log.h>
#  include <dlfcn.h>
#  include <unistd.h>
#  define VLOG(...) __android_log_print(ANDROID_LOG_INFO, "VinzzPerf", __VA_ARGS__)
#else
#  define VLOG(...) ((void)0)
#endif

// ============================================
// GL STATE CACHE
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
inline void vinzz_scissor(int x, int y, int w, int h) {
    if (g_vs.sc_x==x && g_vs.sc_y==y && g_vs.sc_w==w && g_vs.sc_h==h) return;
    g_vs.sc_x=x; g_vs.sc_y=y; g_vs.sc_w=w; g_vs.sc_h=h;
    GLES.glScissor(x,y,w,h);
}
inline void vinzz_enable(uint32_t cap, bool en) {
    if      (cap==0x0B44){if(g_vs.scissor_test==en)return;g_vs.scissor_test=en;}
    else if (cap==0x0BE2){if(g_vs.blend_en==en)return;g_vs.blend_en=en;}
    else if (cap==0x0B71){if(g_vs.depth_test==en)return;g_vs.depth_test=en;}
    else if (cap==0x0B90){if(g_vs.stencil_test==en)return;g_vs.stencil_test=en;}
    else if (cap==0x0BD0){if(g_vs.dither_en==en)return;g_vs.dither_en=en;}
    if (en) GLES.glEnable(cap); else GLES.glDisable(cap);
}
inline void vinzz_bind_vao(uint32_t id) {
    if (g_vs.vao==id) return; g_vs.vao=id; GLES.glBindVertexArray(id);
}
inline void vinzz_depth_mask(bool v) {
    if (g_vs.depth_mask==v) return; g_vs.depth_mask=v;
    GLES.glDepthMask(v?GL_TRUE:GL_FALSE);
}
inline void vinzz_depth_func(uint32_t f) {
    if (g_vs.depth_func==f) return; g_vs.depth_func=f; GLES.glDepthFunc(f);
}
inline void vinzz_blend_func(uint32_t src, uint32_t dst) {
    if (g_vs.blend_src_rgb==src && g_vs.blend_dst_rgb==dst) return;
    g_vs.blend_src_rgb=src; g_vs.blend_dst_rgb=dst; GLES.glBlendFunc(src,dst);
}
inline void vinzz_cull_face(uint32_t mode) {
    if (g_vs.cull_face==mode) return; g_vs.cull_face=mode; GLES.glCullFace(mode);
}
inline void vinzz_polygon_offset(float factor, float units) {
    static float pf=0,pu=0;
    if (pf==factor && pu==units) return;
    pf=factor; pu=units; GLES.glPolygonOffset(factor,units);
}

// ============================================
// FBO STATUS CACHE
// ============================================
extern std::unordered_map<uint32_t,uint32_t> g_fbo_status_cache;
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
// SKIP TINY DRAWS
// ============================================
inline bool vinzz_should_skip_draw(int count) {
    return global_settings.vinzz_skip_small_draws && count < 6;
}

// ============================================
// UNIFORM CACHE
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
    g_uloc_cache[key] = loc; return loc;
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
}

// ============================================
// SODIUM MULTIDRAW
// ============================================
typedef void (*PFNGLMULTIDRAWELEMENTSEXT)(uint32_t,const int*,uint32_t,
                                          const void* const*,int);
extern PFNGLMULTIDRAWELEMENTSEXT pfn_MultiDrawElements;
extern bool g_has_multidraw;

inline void vinzz_multi_draw_elements(uint32_t mode, const int* counts,
                                      uint32_t type,
                                      const void* const* indices, int dc) {
    if (global_settings.vinzz_sodium_mode && g_has_multidraw
        && pfn_MultiDrawElements) {
        pfn_MultiDrawElements(mode, counts, type, indices, dc);
    } else {
        for (int i=0;i<dc;i++)
            if (counts[i]>0) GLES.glDrawElements(mode,counts[i],type,indices[i]);
    }
}

// ============================================
// QCOM TILING
// ============================================
typedef void (*PFNGLSTARTTILINGQCOM)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
typedef void (*PFNGLENDTILINGQCOM)(uint32_t);
extern PFNGLSTARTTILINGQCOM pfn_StartTiling;
extern PFNGLENDTILINGQCOM   pfn_EndTiling;
extern bool g_has_qcom_tiling;

inline void vinzz_begin_tiling(uint32_t x,uint32_t y,uint32_t w,uint32_t h) {
    if (!global_settings.vinzz_qcom_tiling || !g_has_qcom_tiling) return;
    pfn_StartTiling(x,y,w,h,0x00000001|0x00000100);
}
inline void vinzz_end_tiling() {
    if (!global_settings.vinzz_qcom_tiling || !g_has_qcom_tiling) return;
    pfn_EndTiling(0x00000001|0x00000100);
}

// ============================================
// FENCE SYNC POOL
// ============================================
#define VFENCE_SIZE 4
extern void* g_fence_pool[VFENCE_SIZE];
extern int   g_fence_idx;

inline void* vinzz_acquire_fence() {
    if (!global_settings.vinzz_fence_pool)
        return (void*)GLES.glFenceSync(0x9117,0);
    int slot = g_fence_idx % VFENCE_SIZE;
    if (g_fence_pool[slot]) {
        GLES.glClientWaitSync((GLsync)g_fence_pool[slot],0,0);
        GLES.glDeleteSync((GLsync)g_fence_pool[slot]);
    }
    void* f = (void*)GLES.glFenceSync(0x9117,0);
    g_fence_pool[slot]=f;
    g_fence_idx=(g_fence_idx+1)%VFENCE_SIZE;
    return f;
}
inline bool vinzz_fence_ready(void* f) {
    if (!f) return true;
    int val=0;
    GLES.glGetSynciv((GLsync)f,0x9113,1,nullptr,&val);
    return val==0x9119;
}

// ============================================
// DISABLE DISJOINT TIMER
// ============================================
extern bool g_disjoint_checked_this_frame;
inline bool vinzz_should_check_disjoint() {
    if (global_settings.vinzz_disjoint_timer_off) return false;
    if (g_disjoint_checked_this_frame) return false;
    g_disjoint_checked_this_frame = true;
    return true;
}

// ============================================
// SMART DEPTH INVALIDATION
// ============================================
inline void vinzz_invalidate_depth(uint32_t fbo) {
    if (!global_settings.vinzz_smart_depth_invalidation) return;
    uint32_t prev=g_vs.fbo_draw;
    if (prev!=fbo) GLES.glBindFramebuffer(0x8D40,fbo);
    uint32_t att[2]={0x8D00,0x8D20};
    GLES.glInvalidateFramebuffer(0x8D40,2,(GLenum*)att);
    if (prev!=fbo) GLES.glBindFramebuffer(0x8D40,prev);
    vinzz_invalidate_fbo_cache(fbo);
}

// ============================================
// COLOR BUFFER INVALIDATION
// ============================================
inline void vinzz_invalidate_color(uint32_t fbo) {
    if (!global_settings.vinzz_color_invalidate) return;
    uint32_t prev=g_vs.fbo_draw;
    if (prev!=fbo) GLES.glBindFramebuffer(0x8D40,fbo);
    uint32_t att=0x8CE0;
    GLES.glInvalidateFramebuffer(0x8D40,1,(GLenum*)&att);
    if (prev!=fbo) GLES.glBindFramebuffer(0x8D40,prev);
    vinzz_invalidate_fbo_cache(fbo);
}

// ============================================
// ANISOTROPIC
// ============================================
extern float g_max_aniso;
inline void vinzz_apply_aniso_to_current_tex(uint32_t target) {
    float level=(float)global_settings.vinzz_anisotropic_level;
    if (level<=1.0f) return;
    if (g_max_aniso==0.0f)
        GLES.glGetFloatv(0x84FF,&g_max_aniso);
    if (level>g_max_aniso) level=g_max_aniso;
    GLES.glTexParameterf(target,0x84FE,level);
}

// ============================================
// ASTC PREFER
// ============================================
extern bool g_has_astc;
inline bool vinzz_prefer_astc() {
    return global_settings.vinzz_astc_prefer && g_has_astc;
}
inline uint32_t vinzz_pick_tex_fmt(uint32_t fmt) {
    if (!vinzz_prefer_astc()) return fmt;
    if (fmt==0x8058||fmt==0x8C43) return 0x93B4; // RGBA8/SRGB8→ASTC6x6
    return fmt;
}

// ============================================
// AGGRESSIVE SHADER CACHE
// ============================================
inline bool vinzz_load_program_binary(uint32_t prog, uint64_t hash) {
#ifndef __ANDROID__
    return false;
#else
    if (!global_settings.vinzz_shader_cache_aggressive) return false;
    char path[256];
    snprintf(path,sizeof(path),"/sdcard/MG/shadercache/%016llx.bin",
             (unsigned long long)hash);
    FILE* f=fopen(path,"rb"); if (!f) return false;
    int fmt=0,len=0;
    fread(&fmt,4,1,f); fread(&len,4,1,f);
    void* buf=malloc(len); fread(buf,1,len,f); fclose(f);
    GLES.glProgramBinary(prog,(GLenum)fmt,buf,len);
    free(buf);
    int ok=0; GLES.glGetProgramiv(prog,GL_LINK_STATUS,&ok);
    return ok==GL_TRUE;
#endif
}
inline void vinzz_save_program_binary(uint32_t prog, uint64_t hash) {
#ifndef __ANDROID__
    return;
#else
    if (!global_settings.vinzz_shader_cache_aggressive) return;
    mkdir("/sdcard/MG/shadercache",0755);
    int len=0;
    GLES.glGetProgramiv(prog,0x8741,&len);
    if (len<=0) return;
    void* buf=malloc(len); int fmt=0;
    GLES.glGetProgramBinary(prog,len,nullptr,(GLenum*)&fmt,buf);
    char path[256];
    snprintf(path,sizeof(path),"/sdcard/MG/shadercache/%016llx.bin",
             (unsigned long long)hash);
    FILE* f=fopen(path,"wb");
    if (f){fwrite(&fmt,4,1,f);fwrite(&len,4,1,f);fwrite(buf,1,len,f);fclose(f);}
    free(buf);
#endif
}

// ============================================
// EARLY FRAGMENT TEST / MEDIUMP / REDUCE PRECISION
// ============================================
inline std::string vinzz_inject_early_frag(const std::string& src) {
    // LRZ FIX: always-on untuk Adreno 650, optional via setting untuk yang lain
    extern int g_is_adreno_650;
    if (!global_settings.vinzz_mediump_fragment && !g_is_adreno_650) return src;
    // Jangan inject kalau ada discard — discard MEMBUNUH LRZ
    if (src.find("discard")!=std::string::npos) return src;
    // Jangan inject kalau ada gl_FragDepth write — depth RAW membunuh LRZ
    if (src.find("gl_FragDepth")!=std::string::npos) return src;
    // Jangan inject kalau sudah ada (prevent duplicate)
    if (src.find("early_fragment_tests")!=std::string::npos) return src;
    size_t pos=src.find('\n');
    if (pos==std::string::npos) return src;
    return src.substr(0,pos+1)+"layout(early_fragment_tests) in;\n"+src.substr(pos+1);
}
inline std::string vinzz_inject_mediump(const std::string& src) {
    if (!global_settings.vinzz_mediump_fragment) return src;
    if (src.find("precision highp float")!=std::string::npos) return src;
    size_t pos=src.find('\n');
    if (pos==std::string::npos) return src;
    return src.substr(0,pos+1)+"precision mediump float;\nprecision mediump int;\n"+src.substr(pos+1);
}
inline std::string vinzz_reduce_precision(const std::string& src) {
    if (!global_settings.vinzz_reduce_precision) return src;
    std::string out=src; size_t p=0;
    while ((p=out.find("uniform highp float",p))!=std::string::npos) {
        out.replace(p,19,"uniform mediump float"); p+=21;
    }
    return out;
}

// ============================================
// NO THERMAL THROTTLE (Android 12+ only)
// ============================================
#ifdef __ANDROID__
typedef void* APerformanceHintManager;
typedef void* APerformanceHintSession;
typedef APerformanceHintManager* (*pfn_getManager)();
typedef APerformanceHintSession* (*pfn_createSession)(
    APerformanceHintManager*,const int32_t*,size_t,int64_t);
typedef int (*pfn_updateTargetWorkDuration)(APerformanceHintSession*,int64_t);
typedef int (*pfn_reportActualWorkDuration)(APerformanceHintSession*,int64_t);

extern APerformanceHintSession*      g_perf_hint_session;
extern pfn_reportActualWorkDuration  pfn_reportDuration;
extern int64_t                       g_frame_start_ns;
#endif // __ANDROID__

inline void vinzz_thermal_frame_begin() {
#ifdef __ANDROID__
    if (!global_settings.vinzz_no_thermal_throttle || !g_perf_hint_session) return;
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);
    g_frame_start_ns=(int64_t)ts.tv_sec*1000000000LL+ts.tv_nsec;
#endif
}
inline void vinzz_thermal_frame_end() {
#ifdef __ANDROID__
    if (!global_settings.vinzz_no_thermal_throttle
        || !g_perf_hint_session || !pfn_reportDuration) return;
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);
    int64_t now=(int64_t)ts.tv_sec*1000000000LL+ts.tv_nsec;
    pfn_reportDuration(g_perf_hint_session,now-g_frame_start_ns);
#endif
}

// ============================================
// INDEX BUFFER REUSE
// ============================================
#define IB_POOL_SIZE 8
struct IBPoolEntry { uint32_t id; int size; bool used; };
extern IBPoolEntry g_ib_pool[IB_POOL_SIZE];

inline uint32_t vinzz_get_ib(int byte_size) {
    if (!global_settings.vinzz_index_reuse) {
        uint32_t id; GLES.glGenBuffers(1,(GLuint*)&id); return id;
    }
    for (int i=0;i<IB_POOL_SIZE;i++)
        if (!g_ib_pool[i].used && g_ib_pool[i].size>=byte_size) {
            g_ib_pool[i].used=true; return g_ib_pool[i].id;
        }
    uint32_t id; GLES.glGenBuffers(1,(GLuint*)&id); return id;
}
inline void vinzz_return_ib(uint32_t id, int byte_size) {
    if (!global_settings.vinzz_index_reuse) {
        GLES.glDeleteBuffers(1,(const GLuint*)&id); return;
    }
    for (int i=0;i<IB_POOL_SIZE;i++)
        if (g_ib_pool[i].id==id) { g_ib_pool[i].used=false; return; }
    for (int i=0;i<IB_POOL_SIZE;i++)
        if (!g_ib_pool[i].id) { g_ib_pool[i]={id,byte_size,false}; return; }
    GLES.glDeleteBuffers(1,(const GLuint*)&id);
}


// ============================================
// LRZ (Low Resolution Z) TRACKER — Adreno 650
//
// LRZ = hardware hierarchical Z pada Adreno.
// Aktif  → GPU cull occluded fragment sebelum rasterize (hemat GPU banyak).
// Nonaktif → full rasterize semua fragment termasuk yang occluded.
//
// LRZ MATI kalau:
//   1. Fragment shader pakai `discard`
//   2. Fragment shader nulis gl_FragDepth (depth read-after-write)
//   3. glDepthFunc = GL_ALWAYS / GL_NEVER / GL_NOTEQUAL
//   4. Depth test disabled
//
// Solusi yang diimplementasi di sini:
//   - Track setiap FS yang punya discard/gl_FragDepth saat glShaderSource
//   - Register ke g_lrz_kill_programs saat glLinkProgram
//   - early_fragment_tests auto-inject untuk FS yang AMAN (patch 2)
//   - Log tiap draw yang kill LRZ via VLOG
// ============================================
extern std::unordered_set<uint32_t> g_lrz_kill_programs;
extern bool g_lrz_cur_fs_has_discard;
extern bool g_lrz_cur_fs_has_fragdepth;

// Dipanggil di glShaderSource, khusus fragment shader
// Catat apakah FS ini punya discard atau gl_FragDepth write
inline void vinzz_lrz_note_frag(const std::string& src) {
    extern int g_is_adreno_650;
    if (!g_is_adreno_650) return;
    g_lrz_cur_fs_has_discard   = (src.find("discard")      != std::string::npos);
    g_lrz_cur_fs_has_fragdepth = (src.find("gl_FragDepth") != std::string::npos);
    if (g_lrz_cur_fs_has_discard || g_lrz_cur_fs_has_fragdepth) {
        VLOG("LRZ NOTE FS: discard=%d fragDepth=%d",
             (int)g_lrz_cur_fs_has_discard, (int)g_lrz_cur_fs_has_fragdepth);
    }
}

// Dipanggil setelah glLinkProgram berhasil
// Daftarkan program ke kill list kalau FS-nya punya discard/fragDepth
inline void vinzz_lrz_register(uint32_t prog) {
    extern int g_is_adreno_650;
    if (!g_is_adreno_650) return;
    if (g_lrz_cur_fs_has_discard || g_lrz_cur_fs_has_fragdepth) {
        g_lrz_kill_programs.insert(prog);
        VLOG("LRZ REGISTER KILL: prog=%u discard=%d fragDepth=%d",
             prog, (int)g_lrz_cur_fs_has_discard, (int)g_lrz_cur_fs_has_fragdepth);
    } else {
        g_lrz_kill_programs.erase(prog);
    }
    // Reset untuk program berikutnya
    g_lrz_cur_fs_has_discard   = false;
    g_lrz_cur_fs_has_fragdepth = false;
}

// Unregister saat glDeleteProgram
inline void vinzz_lrz_unregister(uint32_t prog) {
    g_lrz_kill_programs.erase(prog);
}

// True kalau program ini TIDAK akan membunuh LRZ
inline bool vinzz_lrz_safe(uint32_t prog) {
    return g_lrz_kill_programs.find(prog) == g_lrz_kill_programs.end();
}

// Depth func yang kill LRZ:
//   GL_ALWAYS=0x0207, GL_NEVER=0x0200, GL_NOTEQUAL=0x0205
inline bool vinzz_lrz_depth_ok(uint32_t func) {
    return func != 0x0207 && func != 0x0200 && func != 0x0205;
}

// Dipanggil sebelum setiap draw call — log LRZ killers
// (tidak memblokir draw, hanya tracking untuk debug via adb logcat -s VinzzPerf)
inline void vinzz_lrz_draw_check(uint32_t prog) {
    extern int g_is_adreno_650;
    if (!g_is_adreno_650) return;
    // Program check
    static uint32_t s_last_bad_prog = 0xFFFFFFFFu;
    if (!vinzz_lrz_safe(prog) && s_last_bad_prog != prog) {
        VLOG("LRZ KILL draw: prog=%u (has discard or gl_FragDepth write)", prog);
        s_last_bad_prog = prog;
    }
    // Depth func check via cached state
    if (!vinzz_lrz_depth_ok(g_vs.depth_func)) {
        static uint32_t s_last_bad_df = 0u;
        if (s_last_bad_df != g_vs.depth_func) {
            VLOG("LRZ KILL draw: depth_func=0x%04X (ALWAYS/NEVER/NOTEQUAL)",
                 g_vs.depth_func);
            s_last_bad_df = g_vs.depth_func;
        }
    }
}


// ============================================
// VERTEX SHADER OPTIMIZER — Adreno 650
//
// VS jalan di binning pass + render pass (2x).
// Ngurangin VS cost = efek 2x di Adreno TBDR.
//
// vinzz_vertex_mediump:
//   Inject `precision mediump float/int` ke VS.
//   Ngurangin register count → SP occupancy naik.
//   Explicit highp di shader tetap override.
//
// vinzz_fp16_varyings:
//   Ganti out/in highp varyings → mediump.
//   Ngurangin interpolation bandwidth rasterizer.
//   Default OFF — bisa visual artifact di beberapa shader.
//
// vinzz_invariant_strip:
//   Hapus keyword `invariant`.
//   invariant paksa tile sync antar draw call — SANGAT
//   mahal di TBDR. Aman dihapus untuk non-shadow pass.
//
// vinzz_precise_strip:
//   Hapus keyword `precise`.
//   precise blokir fused MAD/FMA. Menghapusnya = GPU
//   bebas pakai instruction fusion → math lebih cepat.
// ============================================

inline std::string vinzz_process_vert_shader(const std::string& src) {
    if (!global_settings.vinzz_vertex_mediump) return src;
    if (src.find("precision mediump float") != std::string::npos) return src;
    if (src.find("precision highp float")   != std::string::npos) return src;
    size_t pos = src.find('\n');
    if (pos == std::string::npos) return src;
    return src.substr(0, pos+1)
           + "precision mediump float;\nprecision mediump int;\n"
           + src.substr(pos+1);
}

inline std::string vinzz_inject_mediump_varyings(const std::string& src) {
    if (!global_settings.vinzz_fp16_varyings) return src;
    std::string r = src;
    for (size_t p = 0; (p = r.find("out highp ", p)) != std::string::npos; )
        { r.replace(p, 10, "out mediump "); p += 12; }
    for (size_t p = 0; (p = r.find("in highp ",  p)) != std::string::npos; )
        { r.replace(p,  9, "in mediump ");  p += 11; }
    return r;
}

inline std::string vinzz_strip_invariant(const std::string& src) {
    if (!global_settings.vinzz_invariant_strip) return src;
    std::string r = src;
    for (size_t p = 0; (p = r.find("invariant ", p)) != std::string::npos; )
        r.erase(p, 10);
    return r;
}

inline std::string vinzz_strip_precise(const std::string& src) {
    if (!global_settings.vinzz_precise_strip) return src;
    std::string r = src;
    for (size_t p = 0; (p = r.find("precise ", p)) != std::string::npos; )
        r.erase(p, 8);
    return r;
}


// ============================================
// VULKAN MODE — Adreno 650 (VulkanMod support)
//
// Saat VulkanMod aktif, rendering bypass OpenGL
// sepenuhnya. MobileGlues harus:
//   1. Skip semua GL state cache (tidak relevan)
//   2. Skip early_fragment_tests injection
//   3. Skip mediump/precision injection
//   4. Skip QCOM tiling hints (konflik dengan Vulkan WSI)
//   5. Minimal overhead passthrough mode
//
// vinzz_vulkan_mode_active() cek apakah mode aktif.
// Dipanggil di hot-path sebelum optimisasi GL.
// ============================================
inline bool vinzz_vulkan_mode_active() {
    return global_settings.vinzz_vulkan_mode != 0;
}

// Inject Vulkan-friendly JVM flags via properties
// Fix: NoSuchFieldError sun.misc.Unsafe UNSAFE di VkAndroidSurfaceCreateInfoKHR
// Dipanggil saat init kalau vinzz_vulkan_lwjgl_patch aktif
inline void vinzz_apply_vulkan_jvm_hints() {
    if (!global_settings.vinzz_vulkan_mode) return;
    if (!global_settings.vinzz_vulkan_lwjgl_patch) return;
    // Set system properties untuk LWJGL Android compatibility
    // Ini dibaca LWJGL saat load natives
    setenv("LWJGL_EXTRACT_DIR", "/data/local/tmp", 0);
    setenv("ORG_LWJGL_SYSTEM_SHAREDLIBRARYEXTRACTPATH", "/data/local/tmp", 0);
    setenv("VK_ICD_FILENAMES", "", 0);  // Use Android's built-in Vulkan ICD
    setenv("VK_LAYER_PATH", "", 0);     // Disable validation layers path (use built-in)
    // Disable Vulkan validation layers untuk performa
    if (global_settings.vinzz_vulkan_disable_validation) {
        setenv("VK_INSTANCE_LAYERS", "", 1);  // Clear validation layers
    }
}

// ============================================
// INIT
// ============================================
void vinzz_perf_init();
void vinzz_perf_frame_begin();
void vinzz_perf_frame_end();
