#include "settings.h"
// VinzzRenderer GPU globals
int g_is_adreno_650 = 0;
int g_is_adreno_6xx = 0;

// VinzzRenderer: default settings init
static void vinzz_init_defaults() {
    global_settings.vinzz_sodium_mode = true;
    global_settings.vinzz_vulkan_mode = false;
    global_settings.vinzz_no_throttle = false;
    global_settings.vinzz_fast_hints = false;
    global_settings.vinzz_disable_dither = false;
    global_settings.vinzz_mediump_fragment = false;
    global_settings.vinzz_early_z = true;  // Adreno LRZ effective
    global_settings.vinzz_skip_small_draws = false;
    global_settings.vinzz_state_cache = false;
    global_settings.vinzz_batch_uniforms = false;
    global_settings.vinzz_anisotropic_level = 1;
    global_settings.vinzz_mip_bias = 0.0f;
    global_settings.vinzz_astc_prefer = false;
    global_settings.vinzz_tex_cache = false;
    global_settings.vinzz_smart_invalidate = false;
    global_settings.vinzz_color_invalidate = false;
    global_settings.vinzz_fbo_cache = false;
    global_settings.vinzz_glsl_pragma_opt = false;
    global_settings.vinzz_reduce_precision = false;
    global_settings.vinzz_shader_cache_aggressive = false;
    global_settings.vinzz_persistent_vbo = false;
    global_settings.vinzz_index_reuse = false;
    global_settings.vinzz_multidraw_sodium = false;
    global_settings.vinzz_qcom_tiling = false;
    global_settings.vinzz_fence_pool = false;
    global_settings.vinzz_disjoint_timer_off = false;

    // Batch 2 defaults
    global_settings.vinzz_gpu_pipeline_flush = false;
    global_settings.vinzz_early_fragment_test = false;
    global_settings.vinzz_primitive_restart = false;
    global_settings.vinzz_geometry_cache = false;
    global_settings.vinzz_instanced_arrays = false;
    global_settings.vinzz_ubo_cache = false;
    global_settings.vinzz_ssbo_prefer = false;
    global_settings.vinzz_buffer_storage = false;
    global_settings.vinzz_coherent_map = false;
    global_settings.vinzz_client_wait_zero = false;
    global_settings.vinzz_sparse_texture = false;
    global_settings.vinzz_texture_swizzle = false;
    global_settings.vinzz_compressed_upload = false;
    global_settings.vinzz_tex_storage = true;
    global_settings.vinzz_max_texture_size_cap = false;
    global_settings.vinzz_binary_shader_cache = false;
    global_settings.vinzz_program_pipeline = false;
    global_settings.vinzz_uniform_cache = true;
    global_settings.vinzz_skip_uniform_noop = true;
    global_settings.vinzz_link_cache = false;
    global_settings.vinzz_scissor_cull = false;
    global_settings.vinzz_depth_clamp = false;
    global_settings.vinzz_polygon_offset_fix = false;
    global_settings.vinzz_blend_equation_cache = true;
    global_settings.vinzz_stencil_mask_cache = true;
    global_settings.vinzz_accurate_vsync = false;
    global_settings.vinzz_frame_pacing = false;
    global_settings.vinzz_triple_buffer = false;
    global_settings.vinzz_swap_interval = 1;
    global_settings.vinzz_fps_cap_enable = false;
    global_settings.vinzz_fps_cap_value = 120;
    global_settings.vinzz_adreno_gmem_save = false;
    global_settings.vinzz_adreno_ubwc = false;
    global_settings.vinzz_adreno_perfcounter = false;
    global_settings.vinzz_adreno_lrz = false;
    global_settings.vinzz_adreno_binning = false;
    global_settings.vinzz_sodium_chunk_cache = false;
    global_settings.vinzz_sodium_translucent = false;
    global_settings.vinzz_sodium_region_cull = false;
    global_settings.vinzz_iris_compat_strict = true;
    global_settings.vinzz_vertex_mediump  = false;
    global_settings.vinzz_fp16_varyings   = false;
    global_settings.vinzz_invariant_strip = false;
    global_settings.vinzz_precise_strip   = false;
    // New 5 features – all ON by default
    global_settings.vinzz_buffer_streaming = true;
    global_settings.vinzz_cpu_preprep      = true;
    global_settings.vinzz_denoiser         = true;
    global_settings.vinzz_async_shader     = true;
    global_settings.vinzz_pipeline_cache   = true;
    global_settings.vinzz_shader_complexity_gate = true;
    global_settings.vinzz_compute_protect         = true;
    // Shader Optimization Features
    global_settings.vinzz_persistent_binary_cache = true;
    global_settings.vinzz_precision_guard         = true;
    global_settings.vinzz_dead_code_elim          = true;
    global_settings.vinzz_uniform_batching        = true;
    global_settings.vinzz_texture_norm            = true;
    global_settings.vinzz_gmem_resolve_opt        = true;
    global_settings.vinzz_subgroup_opt            = true;
    global_settings.vinzz_subgroup_hint           = 64;
}

// MobileGlues - config/settings.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include "config.h"
#include "../gl/log.h"
#include "../gl/envvars.h"
#include "gpu_utils.h"
#include "../gl/getter.h"

#define DEBUG 0

global_settings_t global_settings;

void init_settings() {
    vinzz_init_defaults();
#if defined(__APPLE__)
    global_settings.angle = AngleMode::Disabled;
    global_settings.ignore_error = IgnoreErrorLevel::Partial;
    global_settings.ext_compute_shader = false;
    global_settings.max_glsl_cache_size = 30 * 1024 * 1024;
    global_settings.multidraw_mode = multidraw_mode_t::DrawElements;
    global_settings.angle_depth_clear_fix_mode = AngleDepthClearFixMode::Disabled;
    global_settings.ext_direct_state_access = true;
    global_settings.custom_gl_version = {0, 0, 0}; // will go default
    global_settings.fsr1_setting = FSR1_Quality_Preset::Disabled;
    global_settings.hide_mg_env_level = HideMGEnvLevel::Disabled;

#else

    int success = initialized;
    if (!success) {
        success = config_refresh();
        if (!success) {
            LOG_V("Failed to load config. Use default config.")
        }
    }

    AngleConfig angleConfig =
        success ? static_cast<AngleConfig>(config_get_int("enableANGLE")) : AngleConfig::DisableIfPossible;
    NoErrorConfig noErrorConfig =
        success ? static_cast<NoErrorConfig>(config_get_int("enableNoError")) : NoErrorConfig::Auto;
    bool enableExtComputeShader = success ? (config_get_int("enableExtComputeShader") != 0) : true;  // VinzzFix: default ON
    bool enableExtTimerQuery = success ? (config_get_int("enableExtTimerQuery") > 0) : false;
    bool enableExtDirectStateAccess = success ? (config_get_int("enableExtDirectStateAccess") > 0) : false;
    AngleDepthClearFixMode angleDepthClearFixMode =
        success ? static_cast<AngleDepthClearFixMode>(config_get_int("angleDepthClearFixMode"))
                : AngleDepthClearFixMode::Disabled;
    int customGLVersionInt = success ? config_get_int("customGLVersion") : DEFAULT_GL_VERSION;
    FSR1_Quality_Preset fsr1Setting =
        success ? static_cast<FSR1_Quality_Preset>(config_get_int("fsr1Setting")) : FSR1_Quality_Preset::Disabled;
    HideMGEnvLevel hideMGEnvLevel =
        success ? static_cast<HideMGEnvLevel>(config_get_int("hideMGEnvLevel")) : HideMGEnvLevel::Disabled;

    if (customGLVersionInt < 0) {
        customGLVersionInt = 0;
    }

    size_t maxGlslCacheSize = 0;
    if (config_get_int("maxGlslCacheSize") > 0) {
        maxGlslCacheSize = success ? config_get_int("maxGlslCacheSize") * 1024 * 1024 : 0;
    }

    if (static_cast<int>(angleConfig) < 0 || static_cast<int>(angleConfig) > 3) {
        angleConfig = AngleConfig::EnableIfPossible;
    }
    if (static_cast<int>(noErrorConfig) < 0 || static_cast<int>(noErrorConfig) > 3) {
        noErrorConfig = NoErrorConfig::Auto;
    }
    if (static_cast<int>(angleDepthClearFixMode) < 0 ||
        static_cast<int>(angleDepthClearFixMode) >= static_cast<int>(AngleDepthClearFixMode::MaxValue)) {
        angleDepthClearFixMode = AngleDepthClearFixMode::Disabled;
    }
    if (customGLVersionInt > 46) {
        customGLVersionInt = 46;
    } else if (customGLVersionInt < 32 && customGLVersionInt != 0) {
        customGLVersionInt = 32;
    } else if (customGLVersionInt > 33 && customGLVersionInt < 40) {
        customGLVersionInt = 33;
    } else if (customGLVersionInt == 0) {
        customGLVersionInt = DEFAULT_GL_VERSION;
    }
    if (static_cast<int>(fsr1Setting) < 0 ||
        static_cast<int>(fsr1Setting) >= static_cast<int>(FSR1_Quality_Preset::MaxValue)) {
        fsr1Setting = FSR1_Quality_Preset::Disabled;
    }
    if (static_cast<int>(hideMGEnvLevel) < 0 ||
        static_cast<int>(hideMGEnvLevel) >= static_cast<int>(HideMGEnvLevel::MaxValue)) {
        hideMGEnvLevel = HideMGEnvLevel::Disabled;
    }

    Version customGLVersion(customGLVersionInt);

    int isInPluginApp = 0;
    GetEnvVarInt("MG_PLUGIN_STATUS", &isInPluginApp, 0);
    int fclVersion = 0;
    GetEnvVarInt("FCL_VERSION_CODE", &fclVersion, 0);
    int zlVersion = 0;
    GetEnvVarInt("ZALITH_VERSION_CODE", &zlVersion, 0);
    int pgwVersion = 0;
    GetEnvVarInt("PGW_VERSION_CODE", &pgwVersion, 0);

    LOG_V("MG_DIR_PATH = %s", mg_directory_path ? mg_directory_path : "(default)")

    if (isInPluginApp == 0 && fclVersion == 0 && zlVersion == 0 && pgwVersion == 0 && !is_custom_mg_dir) {
        LOG_V("Unsupported launcher detected, force using default config.")
        angleConfig = AngleConfig::DisableIfPossible;
        noErrorConfig = NoErrorConfig::Auto;
        enableExtTimerQuery = true;
        enableExtDirectStateAccess = true;
        maxGlslCacheSize = 0;
        angleDepthClearFixMode = AngleDepthClearFixMode::Disabled;
        fsr1Setting = FSR1_Quality_Preset::Disabled;
        hideMGEnvLevel = HideMGEnvLevel::Disabled;
    }

    AngleMode finalAngleMode = AngleMode::Disabled;
    std::string gpuString = getGPUInfo();
    const char* gpu_cstr = gpuString.c_str();
    LOG_D("GPU: %s", gpu_cstr ? gpu_cstr : "(unknown)")

    int hasVk12 = hasVulkan12();
    int isQcom = isAdreno(gpu_cstr);
    int is730 = isAdreno730(gpu_cstr);
    int is740 = isAdreno740(gpu_cstr);
    int is830 = isAdreno830(gpu_cstr);
    int is650 = isAdreno650(gpu_cstr);
    int is6xx = isAdreno6xx(gpu_cstr);
    g_is_adreno_650 = is650;
    g_is_adreno_6xx = is6xx;
    bool isANGLESupported = checkIfANGLESupported(gpu_cstr);

    LOG_D("Has Vulkan 1.2? = %s", hasVk12 ? "true" : "false")
    LOG_D("Is Adreno? = %s", isQcom ? "true" : "false")
    LOG_D("Is Adreno 730? = %s", is730 ? "true" : "false")
    LOG_D("Is Adreno 740? = %s", is740 ? "true" : "false")
    LOG_D("Is Adreno 830? = %s", is830 ? "true" : "false")
    LOG_D("Is Adreno 650? = %s", is650 ? "true" : "false")
    LOG_D("Is Adreno 6xx? = %s", is6xx ? "true" : "false")
    LOG_D("Is ANGLE supported? = %s", isANGLESupported ? "true" : "false")

    switch (angleConfig) {
    case AngleConfig::ForceDisable:
        finalAngleMode = AngleMode::Disabled;
        LOG_D("ANGLE: Force disabled");
        break;

    case AngleConfig::ForceEnable:
        finalAngleMode = AngleMode::Enabled;
        LOG_D("ANGLE: Force enabled");
        break;

    case AngleConfig::EnableIfPossible: {
        finalAngleMode = isANGLESupported ? AngleMode::Enabled : AngleMode::Disabled;
        LOG_D("ANGLE: Conditionally %s", (finalAngleMode == AngleMode::Enabled) ? "enabled" : "disabled");
        break;
    }

    case AngleConfig::DisableIfPossible:
    default:
        finalAngleMode = AngleMode::Disabled;
        break;
    }

    global_settings.angle = finalAngleMode;
    LOG_D("Final ANGLE setting: %d", static_cast<int>(global_settings.angle))
    global_settings.buffer_coherent_as_flush = (global_settings.angle == AngleMode::Disabled);

    if (global_settings.angle == AngleMode::Enabled) {
        // setenv("LIBGL_GLES", "libGLESv2_angle.so", 1);
        // setenv("LIBGL_EGL", "libEGL_angle.so", 1);
    }

    switch (noErrorConfig) {
    case NoErrorConfig::Level1:
        global_settings.ignore_error = IgnoreErrorLevel::Partial;
        LOG_D("Error ignoring: Level 1 (Partial)");
        break;

    case NoErrorConfig::Level2:
        global_settings.ignore_error = IgnoreErrorLevel::Full;
        LOG_D("Error ignoring: Level 2 (Full)");
        break;

    case NoErrorConfig::Auto:
    case NoErrorConfig::Disable:
    default:
        global_settings.ignore_error = IgnoreErrorLevel::None;
        LOG_D("Error ignoring: Disabled");
        break;
    }

    // VinzzRenderer: Read user-configurable vinzz settings from config
    if (success) {
        global_settings.vinzz_no_throttle         = config_get_int("vinzz_no_throttle") > 0;
        global_settings.vinzz_fast_hints          = config_get_int("vinzz_fast_hints") > 0;
        global_settings.vinzz_disable_dither      = config_get_int("vinzz_disable_dither") > 0;
        global_settings.vinzz_skip_small_draws    = config_get_int("vinzz_skip_small_draws") > 0;
        global_settings.vinzz_state_cache         = config_get_int("vinzz_state_cache") > 0;
        global_settings.vinzz_fbo_cache           = config_get_int("vinzz_fbo_cache") > 0;
        global_settings.vinzz_smart_invalidate    = config_get_int("vinzz_smart_invalidate") > 0;
        global_settings.vinzz_color_invalidate    = config_get_int("vinzz_color_invalidate") > 0;
        global_settings.vinzz_qcom_tiling         = config_get_int("vinzz_qcom_tiling") > 0;
        global_settings.vinzz_multidraw_sodium    = config_get_int("vinzz_multidraw_sodium") > 0;
        global_settings.vinzz_shader_cache_aggressive = config_get_int("vinzz_shader_cache_aggressive") > 0;
        global_settings.vinzz_fence_pool          = config_get_int("vinzz_fence_pool") > 0;
        global_settings.vinzz_disjoint_timer_off  = config_get_int("vinzz_disjoint_timer_off") > 0;
        global_settings.vinzz_sodium_mode         = config_get_int("vinzz_sodium_mode") > 0;
        global_settings.vinzz_tex_cache           = config_get_int("vinzz_tex_cache") > 0;
        global_settings.vinzz_persistent_vbo      = config_get_int("vinzz_persistent_vbo") > 0;
        global_settings.vinzz_index_reuse         = config_get_int("vinzz_index_reuse") > 0;
        global_settings.vinzz_batch_uniforms      = config_get_int("vinzz_batch_uniforms") > 0;
        global_settings.vinzz_early_z             = config_get_int("vinzz_early_z") > 0;
        global_settings.vinzz_glsl_pragma_opt     = config_get_int("vinzz_glsl_pragma_opt") > 0;
        global_settings.vinzz_reduce_precision    = config_get_int("vinzz_reduce_precision") > 0;
        global_settings.vinzz_mediump_fragment    = config_get_int("vinzz_mediump_fragment") > 0;
        global_settings.vinzz_astc_prefer         = config_get_int("vinzz_astc_prefer") > 0;
        global_settings.vinzz_vertex_mediump  = config_get_int("vinzz_vertex_mediump") > 0;
        global_settings.vinzz_fp16_varyings   = config_get_int("vinzz_fp16_varyings") > 0;
        global_settings.vinzz_invariant_strip = config_get_int("vinzz_invariant_strip") > 0;
        global_settings.vinzz_precise_strip   = config_get_int("vinzz_precise_strip") > 0;
        global_settings.vinzz_adreno_lrz = config_get_int("vinzz_lrz") > 0;
        int aniso = config_get_int("vinzz_anisotropic_level");
        if (aniso >= 1 && aniso <= 16) global_settings.vinzz_anisotropic_level = aniso;
        global_settings.vinzz_buffer_streaming = config_get_int("vinzz_buffer_streaming") != 0;
        global_settings.vinzz_cpu_preprep      = config_get_int("vinzz_cpu_preprep") != 0;
        global_settings.vinzz_denoiser         = config_get_int("vinzz_denoiser") != 0;
        global_settings.vinzz_async_shader     = config_get_int("vinzz_async_shader") != 0;
        global_settings.vinzz_pipeline_cache   = config_get_int("vinzz_pipeline_cache") != 0;
        global_settings.vinzz_shader_complexity_gate = config_get_int("vinzz_shader_complexity_gate") != 0;
        global_settings.vinzz_compute_protect         = config_get_int("vinzz_compute_protect") != 0;
        global_settings.vinzz_persistent_binary_cache = config_get_int("vinzz_persistent_binary_cache") != 0;
        global_settings.vinzz_precision_guard         = config_get_int("vinzz_precision_guard") != 0;
        global_settings.vinzz_dead_code_elim          = config_get_int("vinzz_dead_code_elim") != 0;
        global_settings.vinzz_uniform_batching        = config_get_int("vinzz_uniform_batching") != 0;
        global_settings.vinzz_texture_norm            = config_get_int("vinzz_texture_norm") != 0;
        global_settings.vinzz_gmem_resolve_opt        = config_get_int("vinzz_gmem_resolve_opt") != 0;
        global_settings.vinzz_subgroup_opt            = config_get_int("vinzz_subgroup_opt") != 0;
        int mip_bias_int = config_get_int("vinzz_mip_bias_x10");
        if (mip_bias_int >= -10 && mip_bias_int <= 10)
            global_settings.vinzz_mip_bias = mip_bias_int / 10.0f;
    }

    global_settings.ext_compute_shader = enableExtComputeShader;
    global_settings.ext_timer_query = enableExtTimerQuery;
    global_settings.ext_direct_state_access = enableExtDirectStateAccess;
    global_settings.max_glsl_cache_size = maxGlslCacheSize;
    global_settings.angle_depth_clear_fix_mode = angleDepthClearFixMode;
    global_settings.custom_gl_version = customGLVersion;
    global_settings.fsr1_setting = fsr1Setting;
    global_settings.hide_mg_env_level = hideMGEnvLevel;

    // VinzzRenderer: Adreno 650 auto-optimization
    if (is650) {
        LOG_V("[VinzzRenderer] Adreno 650 detected! Applying optimizations...")
        // ANGLE works well on Adreno 650 (unlike 730/740)
        if (angleConfig == AngleConfig::DisableIfPossible) {
            finalAngleMode = AngleMode::Enabled;
            global_settings.angle = AngleMode::Enabled;
            LOG_V("[VinzzRenderer] Adreno 650: ANGLE auto-enabled")
        }
        // Larger GLSL cache for better shader reload perf
        if (global_settings.max_glsl_cache_size == 0) {
            global_settings.max_glsl_cache_size = 100 * 1024 * 1024; // 100MB
            LOG_V("[VinzzRenderer] Adreno 650: GLSL cache set to 100MB")
        }
        // Enable DSA for Adreno 650
        global_settings.ext_direct_state_access = true;
        LOG_V("[VinzzRenderer] Adreno 650: DSA enabled")
        // Force 100MB GLSL cache regardless of user setting
        global_settings.max_glsl_cache_size = 100 * 1024 * 1024;
        LOG_V("[VinzzRenderer] Adreno 650: GLSL cache forced to 100MB")
        // Force MultiDraw Indirect (best for Adreno 650)
        // Will be finalized in init_settings_post
        LOG_V("[VinzzRenderer] Adreno 650: all optimizations applied")
        // Enable all 25 VinzzRenderer optimizations for Adreno 650
        global_settings.vinzz_no_throttle = true;
        global_settings.vinzz_fast_hints = true;
        global_settings.vinzz_disable_dither = true;
        global_settings.vinzz_mediump_fragment = false; // keep highp for Iris compat
        global_settings.vinzz_early_z = true;
        global_settings.vinzz_skip_small_draws = false; // may break Sodium
        global_settings.vinzz_state_cache = true;
        global_settings.vinzz_batch_uniforms = true;
        global_settings.vinzz_anisotropic_level = 16; // 16x max for Adreno 650
        global_settings.vinzz_mip_bias = -1.0f; // max sharpness for Adreno 650
        global_settings.vinzz_astc_prefer = true;
        global_settings.vinzz_tex_cache = true;
        global_settings.vinzz_smart_invalidate = true;
        global_settings.vinzz_color_invalidate = false; // safe=false by default
        global_settings.vinzz_fbo_cache = true;
        global_settings.vinzz_glsl_pragma_opt = true;
        global_settings.vinzz_reduce_precision = false; // keep for Iris compat
        global_settings.vinzz_shader_cache_aggressive = true;
        global_settings.vinzz_persistent_vbo = true;
        global_settings.vinzz_index_reuse = true;
        global_settings.vinzz_multidraw_sodium = true;
        global_settings.vinzz_qcom_tiling = true;
        global_settings.vinzz_fence_pool = true;
        global_settings.vinzz_disjoint_timer_off = true;
        // Mode defaults
        global_settings.vinzz_sodium_mode = true;
        global_settings.vinzz_vulkan_mode = false;
        LOG_V("[VinzzRenderer] All 25+7 Adreno 650 optimizations ACTIVE")
        // Batch 2: 40 advanced optimizations for Adreno 650
        global_settings.vinzz_gpu_pipeline_flush = true;
        global_settings.vinzz_early_fragment_test = true;
        global_settings.vinzz_primitive_restart = true;
        global_settings.vinzz_geometry_cache = true;
        global_settings.vinzz_ubo_cache = true;
        global_settings.vinzz_buffer_storage = true;
        global_settings.vinzz_coherent_map = true;
        global_settings.vinzz_client_wait_zero = true;
        global_settings.vinzz_texture_swizzle = true;
        global_settings.vinzz_tex_storage = true;
        global_settings.vinzz_binary_shader_cache = true;
        global_settings.vinzz_uniform_cache = true;
        global_settings.vinzz_skip_uniform_noop = true;
        global_settings.vinzz_link_cache = true;
        global_settings.vinzz_scissor_cull = true;
        global_settings.vinzz_blend_equation_cache = true;
        global_settings.vinzz_stencil_mask_cache = true;
        global_settings.vinzz_accurate_vsync = true;
        global_settings.vinzz_triple_buffer = true;
        global_settings.vinzz_swap_interval = 0;
        global_settings.vinzz_adreno_gmem_save = true;
        global_settings.vinzz_adreno_perfcounter = true;
        global_settings.vinzz_adreno_lrz = true;
        global_settings.vinzz_adreno_binning = true;
        global_settings.vinzz_sodium_chunk_cache = true;
        global_settings.vinzz_sodium_translucent = true;
        global_settings.vinzz_sodium_region_cull = true;
        global_settings.vinzz_iris_compat_strict = true;
        LOG_V("[VinzzRenderer] Batch 2: 40 Advanced Adreno 650 opts ACTIVE")
        global_settings.vinzz_buffer_streaming = true;
        global_settings.vinzz_cpu_preprep      = true;
        global_settings.vinzz_denoiser         = true;
        global_settings.vinzz_async_shader     = true;
        global_settings.vinzz_pipeline_cache   = true;
        LOG_V("[VinzzRenderer] New 5 Features ACTIVE on Adreno 650")
    }
#endif

    LOG_V("[VinzzRenderer] Setting: enableAngle                 = %s",
          global_settings.angle == AngleMode::Enabled ? "true" : "false")
    LOG_V("[VinzzRenderer] Setting: ignoreError                 = %i", static_cast<int>(global_settings.ignore_error))
    LOG_V("[VinzzRenderer] Setting: enableExtComputeShader      = %s",
          global_settings.ext_compute_shader ? "true" : "false")
    LOG_V("[VinzzRenderer] Setting: enableExtTimerQuery         = %s", global_settings.ext_timer_query ? "true" : "false")
    LOG_V("[VinzzRenderer] Setting: enableExtDirectStateAccess  = %s",
          global_settings.ext_direct_state_access ? "true" : "false")
    LOG_V("[VinzzRenderer] Setting: maxGlslCacheSize            = %i",
          static_cast<int>(global_settings.max_glsl_cache_size / 1024 / 1024))
    LOG_V("[VinzzRenderer] Setting: angleDepthClearFixMode      = %i",
          static_cast<int>(global_settings.angle_depth_clear_fix_mode))
    LOG_V("[VinzzRenderer] Setting: bufferCoherentAsFlush       = %i",
          static_cast<int>(global_settings.buffer_coherent_as_flush))
    if (global_settings.custom_gl_version.isEmpty()) {
        LOG_V("[VinzzRenderer] Setting: customGLVersion             = (default)");
    } else {
        LOG_V("[VinzzRenderer] Setting: customGLVersion             = %s",
              global_settings.custom_gl_version.toString().c_str());
    }
    LOG_V("[VinzzRenderer] Setting: fsr1Setting                 = %i", static_cast<int>(global_settings.fsr1_setting))
    LOG_V("[VinzzRenderer] Setting: hideMGEnvLevel              = %i",
          static_cast<int>(global_settings.hide_mg_env_level))

    GLVersion =
        global_settings.custom_gl_version.isEmpty() ? Version(DEFAULT_GL_VERSION) : global_settings.custom_gl_version;
}

void set_multidraw_setting() { // should be called after init_gles_target()
    multidraw_mode_t multidrawMode = static_cast<multidraw_mode_t>(config_get_int("multidrawMode"));
    if (static_cast<int>(multidrawMode) == -1) {
        multidrawMode = multidraw_mode_t::Auto;
    }
    if (static_cast<int>(multidrawMode) < 0 ||
        static_cast<int>(multidrawMode) >= static_cast<int>(multidraw_mode_t::MaxValue)) {
        multidrawMode = multidraw_mode_t::Auto;
    }
    global_settings.multidraw_mode = multidrawMode;
    std::string draw_mode_str;
    switch (global_settings.multidraw_mode) {
    case multidraw_mode_t::PreferIndirect:
        draw_mode_str = "Indirect";
        break;
    case multidraw_mode_t::PreferBaseVertex:
        draw_mode_str = "Unroll";
        break;
    case multidraw_mode_t::PreferMultidrawIndirect:
        draw_mode_str = "Multidraw indirect";
        break;
    case multidraw_mode_t::DrawElements:
        draw_mode_str = "DrawElements";
        break;
    case multidraw_mode_t::Compute:
        draw_mode_str = "Compute";
        break;
    case multidraw_mode_t::Auto:
        draw_mode_str = "Auto";
        break;
    default:
        draw_mode_str = "(Unknown)";
        global_settings.multidraw_mode = multidraw_mode_t::Auto;
        break;
    }
    LOG_V("[VinzzRenderer] Setting: multidrawMode               = %s", draw_mode_str.c_str())
}

void init_settings_post() {
    bool multidraw = g_gles_caps.GL_EXT_multi_draw_indirect;
    bool basevertex = g_gles_caps.GL_EXT_draw_elements_base_vertex || g_gles_caps.GL_OES_draw_elements_base_vertex ||
                      (g_gles_caps.major == 3 && g_gles_caps.minor >= 2) || (g_gles_caps.major > 3);
    bool indirect = (g_gles_caps.major == 3 && g_gles_caps.minor >= 1) || (g_gles_caps.major > 3);

    switch (global_settings.multidraw_mode) {
    case multidraw_mode_t::PreferIndirect:
        LOG_V("multidrawMode = PreferIndirect")
        if (indirect) {
            global_settings.multidraw_mode = multidraw_mode_t::PreferIndirect;
            LOG_V("    -> Indirect (OK)")
        } else if (basevertex) {
            global_settings.multidraw_mode = multidraw_mode_t::PreferBaseVertex;
            LOG_V("    -> BaseVertex (Preferred not supported, falling back)")
        } else {
            global_settings.multidraw_mode = multidraw_mode_t::DrawElements;
            LOG_V("    -> DrawElements (Preferred not supported, falling back)")
        }
        break;
    case multidraw_mode_t::PreferBaseVertex:
        LOG_V("multidrawMode = PreferBaseVertex")
        if (basevertex) {
            global_settings.multidraw_mode = multidraw_mode_t::PreferBaseVertex;
            LOG_V("    -> BaseVertex (OK)")
        } else if (multidraw) {
            global_settings.multidraw_mode = multidraw_mode_t::PreferMultidrawIndirect;
            LOG_V("    -> MultidrawIndirect (Preferred not supported, falling back)")
        } else if (indirect) {
            global_settings.multidraw_mode = multidraw_mode_t::PreferIndirect;
            LOG_V("    -> Indirect (Preferred not supported, falling back)")
        } else {
            global_settings.multidraw_mode = multidraw_mode_t::DrawElements;
            LOG_V("    -> DrawElements (Preferred not supported, falling back)")
        }
        break;
    case multidraw_mode_t::DrawElements:
        LOG_V("multidrawMode = DrawElements")
        global_settings.multidraw_mode = multidraw_mode_t::DrawElements;
        LOG_V("    -> DrawElements (OK)")
        break;
    case multidraw_mode_t::Compute:
        LOG_V("multidrawMode = Compute")
        global_settings.multidraw_mode = multidraw_mode_t::Compute;
        LOG_V("    -> Compute (OK)")
        break;
    case multidraw_mode_t::Auto:
    default:
        LOG_V("multidrawMode = Auto")
        if (multidraw) {
            global_settings.multidraw_mode = multidraw_mode_t::PreferMultidrawIndirect;
            LOG_V("    -> MultidrawIndirect (Auto detected)")
        } else if (indirect) {
            global_settings.multidraw_mode = multidraw_mode_t::PreferIndirect;
            LOG_V("    -> Indirect (Auto detected)")
        } else if (basevertex) {
            global_settings.multidraw_mode = multidraw_mode_t::PreferBaseVertex;
            LOG_V("    -> BaseVertex (Auto detected)")
        } else {
            global_settings.multidraw_mode = multidraw_mode_t::DrawElements;
            LOG_V("    -> DrawElements (Auto detected)")
        }
        break;
    }
}

std::string dump_settings_string(std::string prefix) {
    std::stringstream ss;

    ss << prefix << "Angle: " << (global_settings.angle == AngleMode::Enabled ? "Enabled" : "Disabled") << "\n";
    ss << prefix << "IgnoreError: ";
    switch (global_settings.ignore_error) {
    case IgnoreErrorLevel::None:
        ss << "None";
        break;
    case IgnoreErrorLevel::Partial:
        ss << "Partial";
        break;
    case IgnoreErrorLevel::Full:
        ss << "Full";
        break;
    }
    ss << "\n";

    ss << prefix << "ExtComputeShader: " << (global_settings.ext_compute_shader ? "True" : "False") << "\n";
    ss << prefix << "ExtTimerQuery: " << (global_settings.ext_timer_query ? "True" : "False") << "\n";
    ss << prefix << "ExtDirectStateAccess: " << (global_settings.ext_direct_state_access ? "True" : "False") << "\n";
    ss << prefix << "MaxGlslCacheSize: " << (global_settings.max_glsl_cache_size / 1024 / 1024) << "MB\n";

    ss << prefix << "MultidrawMode: ";
    switch (global_settings.multidraw_mode) {
    case multidraw_mode_t::Auto:
        ss << "Auto";
        break;
    case multidraw_mode_t::PreferIndirect:
        ss << "Indirect (glDrawElementsIndirect)";
        break;
    case multidraw_mode_t::PreferBaseVertex:
        ss << "BaseVertex (glDrawElementsBaseVertex)";
        break;
    case multidraw_mode_t::PreferMultidrawIndirect:
        ss << "MultidrawIndirect (glMultiDrawElementsIndirect)";
        break;
    case multidraw_mode_t::DrawElements:
        ss << "DrawElements (glDrawElements with per-draw CPU rebase)";
        break;
    case multidraw_mode_t::Compute:
        ss << "Compute (glDrawElements with compute-shader rebase)";
        break;
    default:
        ss << "Unknown";
        break;
    }
    ss << "\n";

    ss << prefix << "AngleDepthClearFixMode: "
       << (global_settings.angle_depth_clear_fix_mode == AngleDepthClearFixMode::Disabled ? "Disabled" : "Enabled")
       << "\n";

    ss << prefix << "BufferCoherentAsFlush: " << (global_settings.buffer_coherent_as_flush ? "True" : "False") << "\n";

    ss << prefix << "CustomGLVersion: "
       << ((GLVersion.toInt(2) == DEFAULT_GL_VERSION) ? "(Default)" : std::to_string(GLVersion.toInt(2))) << "\n";

    ss << prefix << "Fsr1Setting: ";

    switch (global_settings.fsr1_setting) {
    case FSR1_Quality_Preset::Disabled:
        ss << "Disabled";
        break;
    case FSR1_Quality_Preset::UltraQuality:
        ss << "UltraQuality";
        break;
    case FSR1_Quality_Preset::Quality:
        ss << "Quality";
        break;
    case FSR1_Quality_Preset::Balanced:
        ss << "Balanced";
        break;
    case FSR1_Quality_Preset::Performance:
        ss << "Performance";
        break;
    default:
        ss << "Unknown";
        break;
    }
    ss << "\n";

    ss << prefix << "HideMGEnvLevel: "
       << ((global_settings.hide_mg_env_level == HideMGEnvLevel::Disabled)
               ? "Disabled"
               : std::to_string(static_cast<int>(global_settings.hide_mg_env_level)));

    ss << "\n";

    return ss.str();
}
