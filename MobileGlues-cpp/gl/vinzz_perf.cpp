// VinzzRenderer - vinzz_perf.cpp
#include "vinzz_perf.h"
#include "vinzz_newfeatures.h"
#include "log.h"

VinzzStateCache g_vs;
std::unordered_map<UniformKey,float,UniformKeyHash,UniformKeyEq> g_uni1f;
std::unordered_map<UniformKey,int,  UniformKeyHash,UniformKeyEq> g_uni1i;
std::unordered_map<uint64_t,int> g_uloc_cache;
std::unordered_map<uint64_t,int> g_aloc_cache;
std::unordered_map<uint32_t,uint32_t> g_fbo_status_cache;

PFNGLMULTIDRAWELEMENTSEXT pfn_MultiDrawElements = nullptr;
bool g_has_multidraw  = false;

PFNGLSTARTTILINGQCOM pfn_StartTiling = nullptr;
PFNGLENDTILINGQCOM   pfn_EndTiling   = nullptr;
bool g_has_qcom_tiling = false;

void* g_fence_pool[VFENCE_SIZE] = {};
int   g_fence_idx = 0;

bool g_disjoint_checked_this_frame = false;
float g_max_aniso = 0.0f;
bool  g_has_astc  = false;

// LRZ tracker storage — Adreno 650
std::unordered_set<uint32_t> g_lrz_kill_programs;
bool g_lrz_cur_fs_has_discard   = false;
bool g_lrz_cur_fs_has_fragdepth = false;

IBPoolEntry g_ib_pool[IB_POOL_SIZE] = {};

#ifdef __ANDROID__
APerformanceHintSession*     g_perf_hint_session = nullptr;
pfn_reportActualWorkDuration pfn_reportDuration  = nullptr;
int64_t                      g_frame_start_ns    = 0;
#endif

extern int g_is_adreno_650;

static void init_extensions() {
    const char* ext=(const char*)GLES.glGetString(GL_EXTENSIONS);
    if (!ext) return;
    if (strstr(ext,"GL_EXT_multi_draw_arrays")) {
#ifdef __ANDROID__
        pfn_MultiDrawElements=(PFNGLMULTIDRAWELEMENTSEXT)
            eglGetProcAddress("glMultiDrawElementsEXT");
#endif
        g_has_multidraw = pfn_MultiDrawElements!=nullptr;
        if (g_has_multidraw) VLOG("MultiDraw: OK");
    }
    if (strstr(ext,"GL_QCOM_tiled_rendering")) {
#ifdef __ANDROID__
        pfn_StartTiling=(PFNGLSTARTTILINGQCOM)eglGetProcAddress("glStartTilingQCOM");
        pfn_EndTiling  =(PFNGLENDTILINGQCOM)  eglGetProcAddress("glEndTilingQCOM");
#endif
        g_has_qcom_tiling = pfn_StartTiling && pfn_EndTiling;
        if (g_has_qcom_tiling) VLOG("QCOM Tiling: OK");
    }
    if (strstr(ext,"GL_KHR_texture_compression_astc_ldr")) {
        g_has_astc=true; VLOG("ASTC: OK");
    }
}

static void init_thermal_hint() {
#ifndef __ANDROID__
    return;
#else
    void* lib=dlopen("libandroid.so",RTLD_NOW|RTLD_LOCAL);
    if (!lib) return;
    auto getManager  =(pfn_getManager)  dlsym(lib,"APerformanceHint_getManager");
    auto createSess  =(pfn_createSession)dlsym(lib,"APerformanceHint_createSession");
    auto setTarget   =(pfn_updateTargetWorkDuration)
                        dlsym(lib,"APerformanceHint_updateTargetWorkDuration");
    pfn_reportDuration=(pfn_reportActualWorkDuration)
                        dlsym(lib,"APerformanceHint_reportActualWorkDuration");
    if (!getManager||!createSess||!setTarget||!pfn_reportDuration) return;
    APerformanceHintManager* mgr=getManager();
    if (!mgr) return;
    int32_t tids[1]={(int32_t)gettid()};
    g_perf_hint_session=createSess(mgr,tids,1,16666667LL);
    if (g_perf_hint_session) {
        setTarget(g_perf_hint_session,16666667LL);
        VLOG("Thermal hint session: OK");
    }
#endif
}

void vinzz_perf_init() {
    g_vs.invalidate();
    memset(g_ib_pool,0,sizeof(g_ib_pool));
    memset(g_fence_pool,0,sizeof(g_fence_pool));
    g_fbo_status_cache.clear();
    g_uni1f.clear(); g_uni1i.clear();
    g_uloc_cache.clear(); g_aloc_cache.clear();

    init_extensions();

    if (global_settings.vinzz_fast_hints) {
        GLES.glHint(0x8B8B,0x1101);
        GLES.glHint(0x8192,0x1101);
    }
    if (global_settings.vinzz_disable_dither)
        GLES.glDisable(0x0BD0);

    if (global_settings.vinzz_no_thermal_throttle)
        init_thermal_hint();

    vinzz_buffer_streaming_init();
    vinzz_cpu_preprep_init();
    vinzz_denoiser_init();
    vinzz_async_shader_init();
    vinzz_pipeline_cache_init("/sdcard/MG/pipeline_cache");
    VLOG("VinzzPerf init done");
}

void vinzz_perf_frame_begin() {
    g_disjoint_checked_this_frame=false;
    vinzz_thermal_frame_begin();
}

void vinzz_perf_frame_end() {
    vinzz_end_tiling();
    vinzz_thermal_frame_end();
}
