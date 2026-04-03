// MobileGlues - config/settings.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#ifndef MOBILEGLUES_PLUGIN_SETTINGS_H
#define MOBILEGLUES_PLUGIN_SETTINGS_H

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>

#if !defined(__APPLE__)
#include <__stddef_size_t.h>
#else
typedef unsigned long size_t;
#endif

#define DEFAULT_GL_VERSION 40

enum class multidraw_mode_t : int {
    Auto = 0,
    PreferIndirect,
    PreferBaseVertex,
    PreferMultidrawIndirect,
    DrawElements,
    Compute,
    MaxValue
};

enum class AngleConfig : int {
    DisableIfPossible = 0,
    EnableIfPossible = 1,
    ForceDisable = 2,
    ForceEnable = 3
};

enum class AngleMode : int {
    Disabled = 0,
    Enabled = 1
};

enum class IgnoreErrorLevel : int {
    None = 0,
    Partial = 1,
    Full = 2
};

enum class NoErrorConfig : int {
    Auto = 0,
    Disable = 1,
    Level1 = 2,
    Level2 = 3
};

enum class AngleDepthClearFixMode : int {
    Disabled = 0,
    Mode1 = 1,
    Mode2 = 2,
    MaxValue
};

enum class HideMGEnvLevel : int {
    Disabled = 0,
    Level1 = 1, // Hide MG extensions and randomise OpenGL version/renderer,
    MaxValue
};

struct Version {
    int Major{0};
    int Minor{0};
    int Patch{0};

    Version() = default;

    Version(int major, int minor, int patch) : Major(major), Minor(minor), Patch(patch) {}

    Version(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\n\r");
        size_t end = str.find_last_not_of(" \t\n\r");
        if (start == std::string::npos) {
            return;
        }
        std::string s = str.substr(start, end - start + 1);

        std::vector<int> parts;
        std::istringstream iss(s);
        std::string token;
        while (std::getline(iss, token, '.') && parts.size() < 3) {
            try {
                parts.push_back(std::stoi(token));
            }
            catch (...) {
                parts.push_back(0);
            }
        }
        while (parts.size() < 3) {
            parts.push_back(0);
        }
        Major = parts[0];
        Minor = parts[1];
        Patch = parts[2];
    }

    explicit Version(int code) {
        if (code < 0) code = -code;
        std::string s = std::to_string(code);
        for (size_t i = 0; i < 3; ++i) {
            int digit = 0;
            if (i < s.size() && std::isdigit(s[i])) {
                digit = s[i] - '0';
            }
            switch (i) {
            case 0:
                Major = digit;
                break;
            case 1:
                Minor = digit;
                break;
            case 2:
                Patch = digit;
                break;
            }
        }
    }

    std::string toString() const {
        std::ostringstream oss;
        oss << Major << '.' << Minor << '.' << Patch;
        return oss.str();
    }

    int toInt(int digit_count = 3) const {
        switch (digit_count) {
        case 1:
            return Major;
        case 2:
            return Major * 10 + Minor;
        default:
            return Major * 100 + Minor * 10 + Patch;
        }
    }

    bool isEmpty() const { return Major == 0 && Minor == 0 && Patch == 0; }
};

typedef enum class FSR1_Quality_Preset : int { // may be useless
    Disabled = 0,
    UltraQuality, // 1
    Quality,      // 2
    Balanced,     // 3
    Performance,  // 4
    MaxValue      // 5
};

struct global_settings_t {
    AngleMode angle;
    IgnoreErrorLevel ignore_error;
    bool ext_compute_shader;
    bool ext_timer_query;
    bool ext_direct_state_access;
    bool buffer_coherent_as_flush;
    size_t max_glsl_cache_size;
    multidraw_mode_t multidraw_mode;
    AngleDepthClearFixMode angle_depth_clear_fix_mode;
    Version custom_gl_version;
    FSR1_Quality_Preset fsr1_setting;
    HideMGEnvLevel hide_mg_env_level;

    // VinzzRenderer 25 Optimizations Settings
    // Rendering mode
    bool vinzz_sodium_mode;        // Sodium-optimized mode
    bool vinzz_vulkan_mode;        // Vulkan-optimized mode
    // Performance
    bool vinzz_no_throttle;        // Disable thermal throttle hints
    bool vinzz_fast_hints;         // GL_FASTEST for all hints
    bool vinzz_disable_dither;     // Disable dithering
    bool vinzz_mediump_fragment;   // Use mediump in fragment shaders
    bool vinzz_early_z;            // Early fragment test hint
    bool vinzz_skip_small_draws;   // Skip draws < 6 vertices
    bool vinzz_state_cache;        // Cache GL state to skip redundant calls
    bool vinzz_batch_uniforms;     // Batch uniform uploads
    // Texture
    int vinzz_anisotropic_level;   // 1,2,4,8,16x anisotropic
    float vinzz_mip_bias;          // Mipmap LOD bias (-1.0 to 1.0)
    bool vinzz_astc_prefer;        // Prefer ASTC compression
    bool vinzz_tex_cache;          // Texture bind cache
    // Framebuffer
    bool vinzz_smart_invalidate;   // Smart depth/stencil invalidation
    bool vinzz_color_invalidate;   // Also invalidate color when safe
    bool vinzz_fbo_cache;          // Cache framebuffer status checks
    // Shader
    bool vinzz_glsl_pragma_opt;    // Inject #pragma optimize(on)
    bool vinzz_reduce_precision;   // Reduce non-critical precision
    bool vinzz_shader_cache_aggressive; // More aggressive shader caching
    // Draw/Buffer
    bool vinzz_persistent_vbo;     // Persistent VBO mapping
    bool vinzz_index_reuse;        // Index buffer reuse detection
    bool vinzz_multidraw_sodium;   // Sodium-specific multidraw tuning
    // Advanced
    bool vinzz_qcom_tiling;        // QCOM tiled rendering proper
    bool vinzz_fence_pool;         // Fence sync pooling
    bool vinzz_disjoint_timer_off; // Disable disjoint timer (saves overhead)
};

extern global_settings_t global_settings;

void init_settings();
void init_settings_post();
std::string dump_settings_string(std::string prefix = "");
void set_multidraw_setting();

#endif // MOBILEGLUES_PLUGIN_SETTINGS_H

// VinzzRenderer: Global GPU flags (set after init_settings)
extern int g_is_adreno_650;
extern int g_is_adreno_6xx;
