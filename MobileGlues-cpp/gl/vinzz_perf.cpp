// VinzzRenderer - vinzz_perf.cpp
#include "vinzz_perf.h"
#include "log.h"
#include <time.h>
#include <dlfcn.h>

// globals
VinzzStateCache g_vs;
std::unordered_map<UniformKey,float,UniformKeyHash,UniformKeyEq> g_uni1f;
std::unordered_map<UniformKey,int,  UniformKeyHash,UniformKeyEq> g_uni1i;
std::unordered_map<uint64_t,int> g_uloc_cache;
std::unordered_map<uint64_t,int> g_aloc_cache;
std::unordered_map<uint32_t,uint32_t> g_fbo_status_cache;

// multidraw
PFNGLMULTIDRAWELEMENTSEXT pfn_MultiDrawElements = nullptr;
bool g_has_multidraw = false;

// QCOM tiling
PFNGLSTARTTILINGQCOM pfn_StartTiling = nullptr;
PFNGLENDTILINGQCOM   pfn_EndTiling   = nullptr;
bool g_has_qcom_tiling = false;

// fence pool
void* g_fence_pool[VFENCE_SIZE] = {};
int   g_fence_idx = 0;

// disjoint
bool g_disjoint_checked_this_frame = false;

// aniso
float g_max_aniso = 0.0f;

// ASTC
bool g_has_astc = false;

// thermal
#ifdef __ANDROID__
APerformanceHintSession* g_perf_hint_session = nullptr;
pfn_reportActualWorkDuration pfn_reportDuration = nullptr;
int64_t g_frame_start_ns = 0;
#endif

// IB pool
IBPoolEntry g_ib_pool[IB_POOL_SIZE] = {};

// polygon offset
float g_po_factor=0, g_po_units=0;

extern int g_is_adreno_650;

static void init_extensions() {
    const char* ext = (const char*)GLES.glGetString(GL_EXTENSIONS);
    if (!ext) return;

    // MultiDraw
    if (strstr(ext,"GL_EXT_multi_draw_arrays")) {
#ifdef __ANDROID__
        pfn_MultiDrawElements = (PFNGLMULTIDRAWELEMENTSEXT)
            eglGetProcAddress("glMultiDrawElementsEXT");
#endif
        g_has_multidraw = pfn_MultiDrawElements != nullptr;
        if (g_has_multidraw) VLOG("MultiDrawElements: OK");
    }

    // QCOM Tiling
    if (strstr(ext,"GL_QCOM_tiled_rendering")) {
#ifdef __ANDROID__
        pfn_StartTiling = (PFNGLSTARTTILINGQCOM)
            eglGetProcAddress("glStartTilingQCOM");
        pfn_EndTiling = (PFNGLENDTILINGQCOM)
            eglGetProcAddress("glEndTilingQCOM");
#endif
        g_has_qcom_tiling = pfn_StartTiling && pfn_EndTiling;
        if (g_has_qcom_tiling) VLOG("QCOM Tiling: OK");
    }

    // ASTC
    if (strstr(ext,"GL_KHR_texture_compression_astc_ldr")) {
        g_has_astc = true;
        VLOG("ASTC: OK");
    }
}

static void init_thermal_hint() {
#ifndef __ANDROID__
    return;
#else
    void* lib = dlopen("libandroid.so", RTLD_NOW|RTLD_LOCAL);
    if (!lib) return;

    auto getManager = (pfn_getManager)
        dlsym(lib,"APerformanceHint_getManager");
    auto createSession = (pfn_createSession)
        dlsym(lib,"APerformanceHint_createSession");
    auto setTarget = (pfn_updateTargetWorkDuration)
        dlsym(lib,"APerformanceHint_updateTargetWorkDuration");
    pfn_reportDuration = (pfn_reportActualWorkDuration)
        dlsym(lib,"APerformanceHint_reportActualWorkDuration");

    if (!getManager||!createSession||!setTarget||!pfn_reportDuration) return;

    APerformanceHintManager* mgr = getManager();
    if (!mgr) return;

    // tid = render thread
    int32_t tids[1] = {(int32_t)gettid()};
    // target = 16.6ms (60fps) in nanoseconds
    g_perf_hint_session = createSession(mgr, tids, 1, 16666667LL);
    if (g_perf_hint_session) {
        setTarget(g_perf_hint_session, 16666667LL);
        VLOG("Thermal PerformanceHint session: OK");
    }
#endif // __ANDROID__
}

void vinzz_perf_init() {
    g_vs.invalidate();
    memset(g_ib_pool, 0, sizeof(g_ib_pool));
    memset(g_fence_pool, 0, sizeof(g_fence_pool));
    g_fbo_status_cache.clear();
    g_uni1f.clear(); g_uni1i.clear();
    g_uloc_cache.clear(); g_aloc_cache.clear();

    init_extensions();

    if (global_settings.vinzz_gl_fast_hints) {
        GLES.glHint(0x8B8B, 0x1101); // derivative FASTEST
        GLES.glHint(0x8192, 0x1101); // mipmap FASTEST
        VLOG("GL hints: FASTEST");
    }
    if (global_settings.vinzz_disable_dithering) {
        GLES.glDisable(0x0BD0);
        VLOG("Dither: OFF");
    }

    float aniso = (float)global_settings.vinzz_anisotropic_level;
    if (aniso < 1.0f) aniso = 1.0f;
    if (aniso > 16.0f) aniso = 16.0f;

    if (global_settings.vinzz_no_thermal_throttle)
        init_thermal_hint();

    VLOG("VinzzRenderer init done");
}

void vinzz_perf_frame_begin() {
    g_disjoint_checked_this_frame = false;
    vinzz_thermal_frame_begin();
    // QCOM tiling dipanggil setelah tahu ukuran surface
    // (caller yang tahu w/h, panggil vinzz_begin_tiling(0,0,w,h))
}

void vinzz_perf_frame_end() {
    vinzz_end_tiling();
    vinzz_thermal_frame_end();
}
