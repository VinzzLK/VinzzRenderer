#pragma once
// VinzzRenderer - gl/vinzz_newfeatures.h
// 5 New Features: Buffer Streaming, CPU PrePrep, Denoiser, Async Shader, Pipeline Cache

#include "../gles/loader.h"
#include "../config/settings.h"
#include <string>
#include <unordered_map>
#include <vector>

#ifdef __ANDROID__
#  include <android/log.h>
#  define VNLOG(...) __android_log_print(ANDROID_LOG_INFO, "VinzzNew", __VA_ARGS__)
#else
#  define VNLOG(...) ((void)0)
#endif

// ============================================================
// FEATURE 1: BUFFER STREAMING (GL_EXT_buffer_storage)
// ============================================================
#ifndef GL_MAP_PERSISTENT_BIT_EXT
#  define GL_MAP_PERSISTENT_BIT_EXT   0x0040
#  define GL_MAP_COHERENT_BIT_EXT     0x0080
#  define GL_MAP_WRITE_BIT_EXT        0x0002
#endif
typedef void (GL_APIENTRYP PFNGLBUFFERSTORAGEEXTPROC)(
    GLenum target, GLsizeiptr size, const void* data, GLbitfield flags);
struct VinzzPersistentBuf {
    GLuint id=0; void* ptr=nullptr; size_t size=0; GLenum target=0; bool valid=false;
};
void vinzz_buffer_streaming_init();
bool vinzz_bufferdata_hook(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
bool vinzz_buffersubdata_hook(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
void vinzz_deletebuffer_hook(GLuint id);

// ============================================================
// FEATURE 2: CPU PRE-PREPARATION PIPELINE
// ============================================================
struct VinzzDrawCmd {
    GLenum mode=GL_TRIANGLES; GLsizei count=0; GLenum type=GL_UNSIGNED_INT;
    intptr_t indices=0; GLuint prog=0; GLuint tex0=0; GLuint vao=0;
    bool indexed=true; bool valid=false;
};
void vinzz_cpu_preprep_init();
void vinzz_cpu_preprep_frame_begin();
void vinzz_cpu_preprep_enqueue(VinzzDrawCmd& cmd);
void vinzz_cpu_preprep_flush();
bool vinzz_cpu_preprep_active();

// ============================================================
// FEATURE 3: SHADER DENOISER POST-PROCESS
// ============================================================
void  vinzz_denoiser_init();
void  vinzz_denoiser_apply();
void  vinzz_denoiser_cleanup();
extern float g_denoiser_strength;

// ============================================================
// FEATURE 4: ASYNC SHADER COMPILE
// ============================================================
void vinzz_async_shader_init();
void vinzz_async_compile(GLuint shader);
void vinzz_async_link(GLuint program);
void vinzz_async_shader_wait(GLuint shader);
void vinzz_async_program_wait(GLuint program);

// ============================================================
// FEATURE 5: PIPELINE BINARY CACHE
// ============================================================
void vinzz_pipeline_cache_init(const char* cache_dir);
bool vinzz_pipeline_cache_load(GLuint prog, const std::string& key);
void vinzz_pipeline_cache_save(GLuint prog, const std::string& key);
void vinzz_pipeline_cache_clear_all();
