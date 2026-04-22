#include "vinzz_perf.h"
#include "vinzz_newfeatures.h"
// MobileGlues - gl/shader.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include <cctype>
#include "shader.h"

#include <GL/gl.h>
#include "log.h"
#include "program.h"
#include "../gles/loader.h"
#include "../includes.h"
#include "glsl/glsl_for_es.h"
#include "../config/settings.h"
#include "FSR1/FSR1.h"

#define DEBUG 0

struct shader_t shaderInfo;

UnorderedMap<GLuint, bool> shader_map_is_sampler_buffer_emulated;
UnorderedMap<GLuint, bool> shader_map_is_atomic_counter_emulated;

bool can_run_essl3(unsigned int esversion, const char* glsl) {
    if (strncmp(glsl, "#version 100", 12) == 0) {
        return true;
    }

    unsigned int glsl_version = 0;
    if (strncmp(glsl, "#version 300 es", 15) == 0) {
        glsl_version = 300;
    } else if (strncmp(glsl, "#version 310 es", 15) == 0) {
        glsl_version = 310;
    } else if (strncmp(glsl, "#version 320 es", 15) == 0) {
        glsl_version = 320;
    } else {
        return false;
    }
    return esversion >= glsl_version;
}

bool is_direct_shader(const char* glsl) {
    bool es3_ability = can_run_essl3(hardware->es_version, glsl);
    return es3_ability;
}

bool check_if_sampler_buffer_used(std::string str) {
    return str.find("samplerBuffer") != std::string::npos;
}


// VinzzRenderer: shader injection

// OPT-2: Precision Promotion Guard
// Downgrade highp → mediump untuk variable yang aman di Adreno 650
// (koordinat texture, color output, fog — tidak butuh double precision)
inline std::string vinzz_precision_guard_apply(const std::string& src) {
    if (!global_settings.vinzz_precision_guard) return src;

    std::string out = src;
    // Safe to downgrade: texture coords, color outputs, varying interpolants
    // Tidak downgrade: position, depth, matrix operations
    // Strategi: ganti "highp" di output/varying/sampler declarations
    size_t pos = 0;
    while ((pos = out.find("highp ", pos)) != std::string::npos) {
        // Cek konteks: jangan downgrade gl_Position, depth, mat
        size_t line_start = out.rfind('
', pos);
        if (line_start == std::string::npos) line_start = 0;
        std::string line_ctx = out.substr(line_start, pos - line_start + 50);

        bool is_position = line_ctx.find("gl_Position") != std::string::npos
                        || line_ctx.find("gl_FragDepth") != std::string::npos
                        || line_ctx.find("mat") != std::string::npos
                        || line_ctx.find("uniform") != std::string::npos;
        if (!is_position) {
            out.replace(pos, 6, "mediump");
        } else {
            pos += 6;
        }
    }
    return out;
}

// OPT-3: Dead Code Elimination
// Strip #if 0 ... #endif blocks yang tidak pernah compile
inline std::string vinzz_dead_code_elim(const std::string& src) {
    if (!global_settings.vinzz_dead_code_elim) return src;

    std::string out;
    out.reserve(src.size());
    size_t pos = 0;
    while (pos < src.size()) {
        // Cari "#if 0" block
        size_t if0 = src.find("#if 0", pos);
        if (if0 == std::string::npos) {
            out += src.substr(pos);
            break;
        }
        // Copy konten sebelum #if 0
        out += src.substr(pos, if0 - pos);
        // Skip sampai matching #endif
        int depth = 1;
        size_t cur = if0 + 5;
        while (cur < src.size() && depth > 0) {
            size_t ifn  = src.find("#if", cur);
            size_t endn = src.find("#endif", cur);
            if (endn == std::string::npos) { cur = src.size(); break; }
            if (ifn != std::string::npos && ifn < endn) {
                depth++; cur = ifn + 3;
            } else {
                depth--; cur = endn + 6;
            }
        }
        pos = cur;
    }
    return out;
}
static std::string vinzz_process_frag_shader(const std::string& src) {
    std::string out = src;
    out = vinzz_inject_early_frag(out);
    out = vinzz_inject_mediump(out);
    out = vinzz_reduce_precision(out);
    return out;
}

void glShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length) {
    LOG()
    shaderInfo.id = 0;
    shaderInfo.converted = "";
    shaderInfo.frag_data_changed = 0;
    size_t l = 0;
    for (int i = 0; i < count; i++)
        l += (length && length[i] >= 0) ? length[i] : strlen(string[i]);
    std::string glsl_src, essl_src;
    glsl_src.reserve(l + 1);
    if (length) {
        for (int i = 0; i < count; i++) {
            if (length[i] >= 0)
                glsl_src += std::string_view(string[i], length[i]);
            else
                glsl_src += string[i];
        }
    } else {
        for (int i = 0; i < count; i++) {
            glsl_src += string[i];
        }
    }

    bool is_sampler_buffer_emulated = hardware->emulate_texture_buffer && check_if_sampler_buffer_used(glsl_src);

    if (is_direct_shader(glsl_src.c_str())) {
        LOG_D("[INFO] [Shader] Direct shader source: ")
        LOG_D("%s", glsl_src.c_str())
        essl_src = glsl_src;
    } else {
        int glsl_version = getGLSLVersion(glsl_src.c_str());
        LOG_D("[INFO] [Shader] Shader source: ")
        LOG_D("%s", glsl_src.c_str())
        GLint shaderType;
        GLES.glGetShaderiv(shader, GL_SHADER_TYPE, &shaderType);
        int return_code = 0;
        essl_src = GLSLtoGLSLES(glsl_src.c_str(), shaderType, hardware->es_version, glsl_version, return_code);
        if (return_code == 1) { // atomicCounterEmulated
            shader_map_is_atomic_counter_emulated[shader] = true;
            LOG_D("[INFO] [Shader] Atomic counter emulated in shader %d", shader)
        }

        if (essl_src.empty()) {
            LOG_E("Failed to convert shader %d.", shader)
            return;
        }
        LOG_D("\n[INFO] [Shader] Converted Shader source: \n%s", essl_src.c_str())
    }
    if (!essl_src.empty()) {
        // COMPUTE_GUARD + COMPLEXITY_CHECK
        // FIX: Compute shader (Iris deferred, raytracing dll) TIDAK disentuh vinzz inject
        // NEW: Shader complexity check — skip heavy opts jika shader terlalu kompleks
        {
            GLint _lrz_shader_type = 0;
            GLES.glGetShaderiv(shader, GL_SHADER_TYPE, &_lrz_shader_type);

            // Guard: jangan sentuh compute shader sama sekali
            bool _is_compute = (_lrz_shader_type == GL_COMPUTE_SHADER);

            // Complexity check: hitung panjang shader
            // Shader > 8000 char = kompleks, skip injection yang berat
            bool _is_heavy = (essl_src.size() > 8000);

            if (!_is_compute && _lrz_shader_type == GL_FRAGMENT_SHADER) {
                // OPT-2+3: Precision guard + dead code elim
                essl_src = vinzz_precision_guard_apply(essl_src);
                essl_src = vinzz_dead_code_elim(essl_src);
                essl_src = vinzz_process_frag_shader(essl_src);
                vinzz_lrz_note_frag(essl_src);
                // Strip invariant/precise hanya untuk shader ringan-medium
                if (!_is_heavy) {
                    essl_src = vinzz_strip_invariant(essl_src);
                    essl_src = vinzz_strip_precise(essl_src);
                } else {
                    // Shader berat: hanya strip invariant (aman), skip precise strip
                    essl_src = vinzz_strip_invariant(essl_src);
                }
            } else if (!_is_compute && _lrz_shader_type == GL_VERTEX_SHADER) {
                essl_src = vinzz_precision_guard_apply(essl_src);
                essl_src = vinzz_dead_code_elim(essl_src);
                essl_src = vinzz_process_vert_shader(essl_src);
                if (!_is_heavy) {
                    essl_src = vinzz_inject_mediump_varyings(essl_src);
                    essl_src = vinzz_strip_invariant(essl_src);
                    essl_src = vinzz_strip_precise(essl_src);
                } else {
                    essl_src = vinzz_strip_invariant(essl_src);
                }
            }
            // Compute shader: pass-through tanpa modifikasi apapun
        }
        shaderInfo.id = shader;
        shaderInfo.converted = essl_src;
        const char* s[] = {essl_src.c_str()};
        GLES.glShaderSource(shader, count, s, nullptr);
        if (hardware->emulate_texture_buffer)
            shader_map_is_sampler_buffer_emulated[shader] = is_sampler_buffer_emulated;
    } else
        LOG_E("Failed to convert glsl.")
    CHECK_GL_ERROR
}

void glGetShaderiv(GLuint shader, GLenum pname, GLint* params) {
    LOG()
    GLES.glGetShaderiv(shader, pname, params);
    if (global_settings.ignore_error >= IgnoreErrorLevel::Partial && pname == GL_COMPILE_STATUS && !*params) {
        GLchar infoLog[512];
        GLES.glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        LOG_W_FORCE("Shader %d compilation failed: \n%s", shader, infoLog)
        LOG_W_FORCE("Now try to cheat.")
        *params = GL_TRUE;
    }
    CHECK_GL_ERROR
}

GLuint glCreateShader(GLenum shaderType) {
    if (global_settings.fsr1_setting != FSR1_Quality_Preset::Disabled && !fsrInitialized) {
        InitFSRResources();
    }

    LOG()
    LOG_D("glCreateShader(%s)", glEnumToString(shaderType))
    GLuint shader = GLES.glCreateShader(shaderType);
    if (shader != 0 && hardware->emulate_texture_buffer) shader_map_is_sampler_buffer_emulated[shader] = false;
    CHECK_GL_ERROR
    return shader;
}