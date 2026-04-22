// MobileGlues - gl/program.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include <regex.h>
#include "GL/glext.h"
#include "GLES3/gl32.h"
#include "log.h"
#include "shader.h"
#include "program.h"
#include <regex>
#include <cstring>
#include <iostream>
#include "../config/settings.h"
#include <ankerl/unordered_dense.h>
#include "drawing.h"
#include "vinzz_perf.h"  // VinzzRenderer LRZ
// VINZZ_BINARY_CACHE
#include <fstream>
#include <sstream>
#include <sys/stat.h>

// VinzzRenderer: Persistent Program Binary Cache
// Simpan hasil kompilasi GL program ke disk biar tidak compile ulang tiap sesi
namespace vinzz_prog_cache {

static std::string get_cache_dir() {
    // Pakai MG_DIR_PATH yang sudah diinit settings
    const char* mg = getenv("MG_DIR_PATH");
    std::string base = mg ? mg : "/sdcard/MG";
    return base + "/prog_cache";
}

static std::string get_cache_path(GLuint program) {
    // Hash sederhana dari program ID + timestamp pertama compile
    // Gunakan program ID sebagai key (unik per session, tapi dikombinasi dengan shader hash)
    std::ostringstream ss;
    ss << get_cache_dir() << "/prog_" << program << ".bin";
    return ss.str();
}

static bool ensure_dir(const std::string& dir) {
    struct stat st;
    if (stat(dir.c_str(), &st) == 0) return true;
    return mkdir(dir.c_str(), 0755) == 0;
}

static bool save(GLuint program) {
    if (!global_settings.vinzz_persistent_binary_cache) return false;
    GLint bin_len = 0;
    GLES.glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &bin_len);
    if (bin_len <= 0) return false;

    std::vector<uint8_t> buf(bin_len);
    GLenum fmt = 0;
    GLES.glGetProgramBinary(program, bin_len, nullptr, &fmt, buf.data());

    if (!ensure_dir(get_cache_dir())) return false;

    std::string path = get_cache_path(program);
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    // Header: magic + format
    uint32_t magic = 0x56425043; // 'VBPC'
    f.write(reinterpret_cast<char*>(&magic), 4);
    f.write(reinterpret_cast<char*>(&fmt), sizeof(fmt));
    f.write(reinterpret_cast<char*>(buf.data()), bin_len);
    return true;
}

static bool load(GLuint program) {
    if (!global_settings.vinzz_persistent_binary_cache) return false;
    std::string path = get_cache_path(program);
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    uint32_t magic = 0;
    f.read(reinterpret_cast<char*>(&magic), 4);
    if (magic != 0x56425043) return false;

    GLenum fmt = 0;
    f.read(reinterpret_cast<char*>(&fmt), sizeof(fmt));

    std::string bin((std::istreambuf_iterator<char>(f)), {});
    if (bin.empty()) return false;

    GLES.glProgramBinary(program, fmt,
        reinterpret_cast<const void*>(bin.data()),
        static_cast<GLsizei>(bin.size()));

    GLint status = 0;
    GLES.glGetProgramiv(program, GL_LINK_STATUS, &status);
    return status == GL_TRUE;
}

} // namespace vinzz_prog_cache

#define DEBUG 0

extern UnorderedMap<GLuint, bool> shader_map_is_sampler_buffer_emulated;
UnorderedMap<GLuint, bool> program_map_is_sampler_buffer_emulated;

extern UnorderedMap<GLuint, bool> shader_map_is_atomic_counter_emulated;
UnorderedMap<GLuint, bool> program_map_is_atomic_counter_emulated;

enum class ShouldGenerateFSState : int {
    Never = 0,
    Maybe = 1,
    Unknown = 2
};

UnorderedMap<GLuint, ShouldGenerateFSState> program_map_should_generate_fs;

char* updateLayoutLocation(const char* esslSource, GLuint color, const char* name) {
    std::string shaderCode(esslSource);

    std::string pattern = std::string(R"((layout\s*$[^)]*location\s*=\s*\d+[^)]*$\s*)?)") +
                          R"(out\s+((?:highp|mediump|lowp|\w+\s+)*\w+)\s+)" + name + R"(\s*;)";

    std::string replacement = "layout (location = " + std::to_string(color) + ") out $2 " + name + ";";

    std::regex reg(pattern);
    std::string modifiedCode = std::regex_replace(shaderCode, reg, replacement);

    char* result = new char[modifiedCode.size() + 1];
    strcpy(result, modifiedCode.c_str());
    return result;
}

void glBindFragDataLocation(GLuint program, GLuint color, const GLchar* name) {
    LOG()
    LOG_D("glBindFragDataLocation(%d, %d, %s)", program, color, name)

    if (strlen(name) > 8 && strncmp(name, "outColor", 8) == 0) {
        const char* numberStr = name + 8;
        bool isNumber = true;
        for (int i = 0; numberStr[i] != '\0'; ++i) {
            if (!isdigit(numberStr[i])) {
                isNumber = false;
                break;
            }
        }

        if (isNumber) {
            unsigned int extractedColor = static_cast<unsigned int>(std::stoul(numberStr));
            if (extractedColor == color) {
                // outColor was bound in glsl process. exit now
                LOG_D("Find outColor* with color *, skipping")
                return;
            }
        }
    }

    char* origin_glsl = nullptr;
    if (shaderInfo.frag_data_changed) {
        size_t glslLen = strlen(shaderInfo.frag_data_changed_converted) + 1;
        origin_glsl = (char*)malloc(glslLen);
        if (origin_glsl == nullptr) {
            LOG_E("Memory reallocation failed for frag_data_changed_converted\n")
            return;
        }
        strcpy(origin_glsl, shaderInfo.frag_data_changed_converted);
    } else {
        size_t glslLen = shaderInfo.converted.length() + 1;
        origin_glsl = (char*)malloc(glslLen);
        if (origin_glsl == nullptr) {
            LOG_E("Memory reallocation failed for converted\n")
            return;
        }
        strcpy(origin_glsl, shaderInfo.converted.c_str());
    }

    char* result_glsl = updateLayoutLocation(origin_glsl, color, name);
    free(origin_glsl);

    shaderInfo.frag_data_changed_converted = result_glsl;
    shaderInfo.frag_data_changed = 1;
}

static std::string DefaultFSSource;
static unsigned CurrentDefaultFSSourceVersion = 0; // the version (hardware->es_version) may change during runtime

void GenerateDefaultFSSource() {
    if (CurrentDefaultFSSourceVersion != hardware->es_version) {
        CurrentDefaultFSSourceVersion = hardware->es_version;
        std::ostringstream ss;
        ss << "#version " << CurrentDefaultFSSourceVersion << " es\n";
        ss << "precision mediump float;\n\n";
        ss << "out vec4 fragColor;\n\n";
        ss << "void main() {\n";
        ss << "    fragColor = vec4(1.0, 1.0, 1.0, 1.0);\n";
        ss << "}\n";

        DefaultFSSource = ss.str();
    }
}

static UnorderedMap<unsigned, GLuint> DefaultFSMap; // essl version <-> shader id
void glLinkProgram(GLuint program) {
    LOG()

    LOG_D("glLinkProgram(%d)", program)
    if (!shaderInfo.converted.empty() && shaderInfo.frag_data_changed) {
        GLES.glShaderSource(shaderInfo.id, 1, (const GLchar* const*)&shaderInfo.frag_data_changed_converted, nullptr);
        GLES.glCompileShader(shaderInfo.id);
        GLint status = 0;
        GLES.glGetShaderiv(shaderInfo.id, GL_COMPILE_STATUS, &status);
        if (status != GL_TRUE) {
            char tmp[500];
            GLES.glGetShaderInfoLog(shaderInfo.id, 500, nullptr, tmp);
            LOG_E("Failed to compile patched shader, log:\n%s", tmp)
        }
        GLES.glDetachShader(program, shaderInfo.id);
        GLES.glAttachShader(program, shaderInfo.id);
        CHECK_GL_ERROR
    }
    shaderInfo.id = 0;
    shaderInfo.converted = "";
    shaderInfo.frag_data_changed_converted = nullptr;
    shaderInfo.frag_data_changed = 0;

    // Generate defaut fragment shader if needed
    if (program_map_should_generate_fs[program] == ShouldGenerateFSState::Maybe) {
        GenerateDefaultFSSource();
        GLuint& default_fs = DefaultFSMap[CurrentDefaultFSSourceVersion];
        if (!default_fs) {
            default_fs = GLES.glCreateShader(GL_FRAGMENT_SHADER);
            const char* src = DefaultFSSource.c_str();
            GLES.glShaderSource(default_fs, 1, &src, nullptr);

            GLES.glCompileShader(default_fs);

            GLint success = 0;
            GLES.glGetShaderiv(default_fs, GL_COMPILE_STATUS, &success);
            if (!success) {
                GLint logLength = 0;
                GLES.glGetShaderiv(default_fs, GL_INFO_LOG_LENGTH, &logLength);
                std::vector<char> log(logLength);
                GLES.glGetShaderInfoLog(default_fs, logLength, nullptr, log.data());
                LOG_E("Default fragment shader compile error for program %u :\n%s\n", program, log.data());
                GLES.glDeleteShader(default_fs);
                default_fs = 0;
            }
        }

        if (default_fs) {
            LOG_D("Try to attach missing default FS for program %u...", program);
            GLES.glAttachShader(program, default_fs);
        }
    }

    GLES.glLinkProgram(program);
    // VinzzRenderer: save binary cache setelah link sukses
    { GLint _ls = 0; GLES.glGetProgramiv(program, GL_LINK_STATUS, &_ls);
      if (_ls) vinzz_prog_cache::save(program); }
    // VinzzRenderer LRZ: register program ini ke kill list kalau FS-nya
    // punya discard atau gl_FragDepth write (dicatat saat glShaderSource).
    vinzz_lrz_register(program);

    CHECK_GL_ERROR
}

void glGetProgramiv(GLuint program, GLenum pname, GLint* params) {
    LOG()
    GLES.glGetProgramiv(program, pname, params);
    if (global_settings.ignore_error >= IgnoreErrorLevel::Partial &&
        (pname == GL_LINK_STATUS || pname == GL_VALIDATE_STATUS) && !*params) {
        GLchar infoLog[512];
        GLES.glGetProgramInfoLog(program, 512, nullptr, infoLog);

        LOG_W_FORCE("Program %d linking failed: \n%s", program, infoLog);
        LOG_W_FORCE("Now try to cheat.");
        *params = GL_TRUE;
    }
    CHECK_GL_ERROR
}

void glUseProgram(GLuint program) {
    LOG()
    LOG_D("glUseProgram(%d)", program)
    if (program != gl_state->current_program) {
        gl_state->current_program = program;
        GLES.glUseProgram(program);
        CHECK_GL_ERROR
    }
}

void glAttachShader(GLuint program, GLuint shader) {
    LOG()
    LOG_D("glAttachShader(%u, %u)", program, shader)
    if (hardware->emulate_texture_buffer && shader_map_is_sampler_buffer_emulated[shader])
        program_map_is_sampler_buffer_emulated[program] = true;
    if (shader_map_is_atomic_counter_emulated[shader]) {
        program_map_is_atomic_counter_emulated[program] = true;
        LOG_D("Shader %d is atomic counter emulated, setting program %d to atomic counter emulated", shader, program)
    }

    GLint type = 0;
    GLES.glGetShaderiv(shader, GL_SHADER_TYPE, &type);
    auto& should_gen_fs_map = program_map_should_generate_fs;
    if (type == GL_FRAGMENT_SHADER) {
        should_gen_fs_map[program] = ShouldGenerateFSState::Never;
    } else if (type == GL_VERTEX_SHADER) {
        auto it = should_gen_fs_map.find(program);
        if (it == should_gen_fs_map.end() || should_gen_fs_map[program] != ShouldGenerateFSState::Never) {
            should_gen_fs_map[program] = ShouldGenerateFSState::Maybe;
        }
    }

    GLES.glAttachShader(program, shader);
    CHECK_GL_ERROR
}

extern UnorderedMap<GLuint, SamplerInfo> g_samplerCacheForSamplerBuffer;

GLuint glCreateProgram() {
    LOG()
    LOG_D("glCreateProgram")
    GLuint program = GLES.glCreateProgram();
    if (hardware->emulate_texture_buffer) {
        program_map_is_sampler_buffer_emulated[program] = false;
        if (g_samplerCacheForSamplerBuffer.find(program) != g_samplerCacheForSamplerBuffer.end()) {
            g_samplerCacheForSamplerBuffer.erase(program);
        }
    }
    program_map_is_atomic_counter_emulated[program] = false;
    program_map_should_generate_fs[program] = ShouldGenerateFSState::Unknown;

    CHECK_GL_ERROR
    return program;
}
