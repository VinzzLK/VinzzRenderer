// VinzzRenderer - gl/vinzz_newfeatures.cpp
#include "vinzz_newfeatures.h"
#include "log.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>
#ifdef __ANDROID__
#  include <dlfcn.h>
#  include <sys/stat.h>
#endif

// ═══════════════════════════════════════════════════════════════
// FEATURE 1: BUFFER STREAMING
// ═══════════════════════════════════════════════════════════════
static PFNGLBUFFERSTORAGEEXTPROC pfn_BufferStorageEXT = nullptr;
static bool g_has_buffer_storage = false;
static std::unordered_map<GLuint, VinzzPersistentBuf> g_pers_bufs;
static std::mutex g_pers_mutex;

static GLuint get_bound_buf(GLenum target) {
    GLenum q = GL_ARRAY_BUFFER_BINDING;
    if (target==GL_ELEMENT_ARRAY_BUFFER) q=GL_ELEMENT_ARRAY_BUFFER_BINDING;
    else if (target==GL_UNIFORM_BUFFER)  q=GL_UNIFORM_BUFFER_BINDING;
    GLint id=0; GLES.glGetIntegerv(q,&id); return (GLuint)id;
}
void vinzz_buffer_streaming_init() {
    if (!global_settings.vinzz_buffer_streaming) return;
    const char* ext=(const char*)GLES.glGetString(GL_EXTENSIONS);
    if (ext && strstr(ext,"GL_EXT_buffer_storage")) {
#ifdef __ANDROID__
        pfn_BufferStorageEXT=(PFNGLBUFFERSTORAGEEXTPROC)eglGetProcAddress("glBufferStorageEXT");
#endif
        g_has_buffer_storage=(pfn_BufferStorageEXT!=nullptr);
    }
    VNLOG("[BufferStream] %s", g_has_buffer_storage?"OK":"fallback to normal");
}
bool vinzz_bufferdata_hook(GLenum target,GLsizeiptr size,const void* data,GLenum usage) {
    if (!global_settings.vinzz_buffer_streaming||!g_has_buffer_storage) return false;
    if (usage!=GL_DYNAMIC_DRAW&&usage!=GL_STREAM_DRAW) return false;
    if (size<=0||size>(64*1024*1024)) return false;
    GLuint id=get_bound_buf(target); if (!id) return false;
    std::lock_guard<std::mutex> lk(g_pers_mutex);
    auto it=g_pers_bufs.find(id);
    if (it!=g_pers_bufs.end()&&it->second.valid
        &&it->second.size==(size_t)size&&it->second.target==target) {
        if (data) memcpy(it->second.ptr,data,(size_t)size); return true;
    }
    if (it!=g_pers_bufs.end()&&it->second.valid) {
        GLES.glUnmapBuffer(target); it->second.valid=false;
    }
    GLbitfield flags=GL_MAP_WRITE_BIT_EXT|GL_MAP_PERSISTENT_BIT_EXT|GL_MAP_COHERENT_BIT_EXT;
    pfn_BufferStorageEXT(target,size,nullptr,flags);
    void* ptr=GLES.glMapBufferRange(target,0,(GLsizeiptr)size,flags);
    if (!ptr) { VNLOG("[BufferStream] MapBufferRange failed buf=%u",id); return false; }
    if (data) memcpy(ptr,data,(size_t)size);
    VinzzPersistentBuf pb; pb.id=id;pb.ptr=ptr;pb.size=(size_t)size;pb.target=target;pb.valid=true;
    g_pers_bufs[id]=pb; return true;
}
bool vinzz_buffersubdata_hook(GLenum target,GLintptr offset,GLsizeiptr size,const void* data) {
    if (!global_settings.vinzz_buffer_streaming) return false;
    GLuint id=get_bound_buf(target); if (!id) return false;
    std::lock_guard<std::mutex> lk(g_pers_mutex);
    auto it=g_pers_bufs.find(id);
    if (it==g_pers_bufs.end()||!it->second.valid) return false;
    if (offset+size>(GLsizeiptr)it->second.size) return false;
    memcpy((char*)it->second.ptr+offset,data,(size_t)size); return true;
}
void vinzz_deletebuffer_hook(GLuint id) {
    if (!global_settings.vinzz_buffer_streaming) return;
    std::lock_guard<std::mutex> lk(g_pers_mutex);
    auto it=g_pers_bufs.find(id);
    if (it!=g_pers_bufs.end()&&it->second.valid){it->second.valid=false;g_pers_bufs.erase(it);}
}

// ═══════════════════════════════════════════════════════════════
// FEATURE 2: CPU PRE-PREPARATION PIPELINE
// ═══════════════════════════════════════════════════════════════
static std::vector<VinzzDrawCmd> g_draw_queue;
static std::mutex g_draw_mutex;
static bool g_preprep_active=false;
static int  g_max_queue=512;

void vinzz_cpu_preprep_init() {
    if (!global_settings.vinzz_cpu_preprep) return;
    g_draw_queue.reserve(g_max_queue); g_preprep_active=true;
    VNLOG("[CPUPrePrep] active");
}
void vinzz_cpu_preprep_frame_begin() {
    if (!g_preprep_active) return;
    std::lock_guard<std::mutex> lk(g_draw_mutex); g_draw_queue.clear();
}
bool vinzz_cpu_preprep_active() { return g_preprep_active; }
void vinzz_cpu_preprep_enqueue(VinzzDrawCmd& cmd) {
    if (!g_preprep_active) return;
    GLint cp=0,ct=0,cv=0;
    GLES.glGetIntegerv(GL_CURRENT_PROGRAM,&cp);
    GLES.glGetIntegerv(GL_TEXTURE_BINDING_2D,&ct);
    GLES.glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&cv);
    cmd.prog=(GLuint)cp;cmd.tex0=(GLuint)ct;cmd.vao=(GLuint)cv;cmd.valid=true;
    std::lock_guard<std::mutex> lk(g_draw_mutex);
    if ((int)g_draw_queue.size()<g_max_queue) g_draw_queue.push_back(cmd);
}
void vinzz_cpu_preprep_flush() {
    if (!g_preprep_active) return;
    std::lock_guard<std::mutex> lk(g_draw_mutex);
    if (g_draw_queue.empty()) return;
    std::stable_sort(g_draw_queue.begin(),g_draw_queue.end(),
        [](const VinzzDrawCmd& a,const VinzzDrawCmd& b){
            return a.prog!=b.prog?a.prog<b.prog:a.tex0<b.tex0;});
    GLuint lp=0xFFFFFFFF,lt=0xFFFFFFFF,lv=0xFFFFFFFF;
    for (auto& c:g_draw_queue){
        if(!c.valid) continue;
        if(c.vao!=lv){GLES.glBindVertexArray(c.vao);lv=c.vao;}
        if(c.prog!=lp){GLES.glUseProgram(c.prog);lp=c.prog;}
        if(c.tex0!=lt){GLES.glBindTexture(GL_TEXTURE_2D,c.tex0);lt=c.tex0;}
        if(c.indexed) GLES.glDrawElements(c.mode,c.count,c.type,(const void*)c.indices);
        else GLES.glDrawArrays(c.mode,(GLint)c.indices,c.count);
    }
    g_draw_queue.clear();
}

// ═══════════════════════════════════════════════════════════════
// FEATURE 3: SHADER DENOISER (A-Trous Wavelet)
// ═══════════════════════════════════════════════════════════════
float g_denoiser_strength=0.5f;

static const char* DEN_VERT=R"GLSL(
#version 320 es
out vec2 vUV;
void main(){
    vec2 p=vec2((gl_VertexID&1)*2-1,(gl_VertexID>>1)*2-1);
    vUV=p*0.5+0.5; gl_Position=vec4(p,0.0,1.0);
}
)GLSL";

static const char* DEN_FRAG=R"GLSL(
#version 320 es
precision mediump float;
in vec2 vUV; out vec4 fragColor;
uniform sampler2D uInput; uniform vec2 uTexelSize; uniform float uStrength;
const float K[25]=float[25](1.,4.,7.,4.,1.,4.,16.,26.,16.,4.,7.,26.,41.,26.,7.,4.,16.,26.,16.,4.,1.,4.,7.,4.,1.);
const float K_SUM=273.;
void main(){
    vec4 cen=texture(uInput,vUV);
    float cL=dot(cen.rgb,vec3(0.2126,0.7152,0.0722));
    float var=0.;
    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){
        float l=dot(texture(uInput,vUV+vec2(float(dx),float(dy))*uTexelSize).rgb,vec3(0.2126,0.7152,0.0722));
        float d=l-cL;var+=d*d;}
    var/=9.;
    if(var<0.001||uStrength<0.01){fragColor=cen;return;}
    vec4 acc=vec4(0.);float wT=0.;int ki=0;
    for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx){
        vec4 s=texture(uInput,vUV+vec2(float(dx),float(dy))*uTexelSize);
        float sL=dot(s.rgb,vec3(0.2126,0.7152,0.0722));
        float wd=abs(sL-cL);
        float w=(K[ki]/K_SUM)*exp(-wd*wd/(2.*0.1*0.1));
        acc+=s*w;wT+=w;++ki;}
    fragColor=mix(cen,acc/max(wT,0.0001),uStrength);
}
)GLSL";

static GLuint g_dp=0,g_df=0,g_dt=0,g_dv=0;
static GLint g_duInput=-1,g_duTexel=-1,g_duStr=-1;
static int g_dw=0,g_dh=0; static bool g_dready=false;

static GLuint cs(GLenum type,const char* src){
    GLuint s=GLES.glCreateShader(type);GLES.glShaderSource(s,1,&src,nullptr);
    GLES.glCompileShader(s);GLint ok=0;GLES.glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){char b[512];GLES.glGetShaderInfoLog(s,512,nullptr,b);VNLOG("[Den] %s",b);GLES.glDeleteShader(s);return 0;}
    return s;
}
void vinzz_denoiser_init(){
    if(!global_settings.vinzz_denoiser) return;
    GLuint vs=cs(GL_VERTEX_SHADER,DEN_VERT),fs=cs(GL_FRAGMENT_SHADER,DEN_FRAG);
    if(!vs||!fs){GLES.glDeleteShader(vs);GLES.glDeleteShader(fs);return;}
    g_dp=GLES.glCreateProgram();
    GLES.glAttachShader(g_dp,vs);GLES.glAttachShader(g_dp,fs);GLES.glLinkProgram(g_dp);
    GLES.glDeleteShader(vs);GLES.glDeleteShader(fs);
    GLint ok=0;GLES.glGetProgramiv(g_dp,GL_LINK_STATUS,&ok);
    if(!ok){char b[512];GLES.glGetProgramInfoLog(g_dp,512,nullptr,b);VNLOG("[Den] link: %s",b);GLES.glDeleteProgram(g_dp);g_dp=0;return;}
    g_duInput=GLES.glGetUniformLocation(g_dp,"uInput");
    g_duTexel=GLES.glGetUniformLocation(g_dp,"uTexelSize");
    g_duStr  =GLES.glGetUniformLocation(g_dp,"uStrength");
    GLES.glGenVertexArrays(1,&g_dv);
    VNLOG("[Denoiser] Init OK"); g_dready=true;
}
void vinzz_denoiser_apply(){
    if(!global_settings.vinzz_denoiser||!g_dready||g_denoiser_strength<=0.f) return;
    GLint vp[4]={};GLES.glGetIntegerv(GL_VIEWPORT,vp);int w=vp[2],h=vp[3];if(w<=0||h<=0) return;
    if(w!=g_dw||h!=g_dh){
        if(g_df){GLES.glDeleteFramebuffers(1,&g_df);g_df=0;}
        if(g_dt){GLES.glDeleteTextures(1,&g_dt);g_dt=0;}
        g_dw=w;g_dh=h;
        GLES.glGenTextures(1,&g_dt);GLES.glBindTexture(GL_TEXTURE_2D,g_dt);
        GLES.glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,nullptr);
        GLES.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        GLES.glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        GLES.glBindTexture(GL_TEXTURE_2D,0);
        GLES.glGenFramebuffers(1,&g_df);GLES.glBindFramebuffer(GL_FRAMEBUFFER,g_df);
        GLES.glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,g_dt,0);
        GLES.glBindFramebuffer(GL_FRAMEBUFFER,0);
    }
    GLint pp=0,pf=0,pv=0,pt=0;
    GLES.glGetIntegerv(GL_CURRENT_PROGRAM,&pp);GLES.glGetIntegerv(GL_FRAMEBUFFER_BINDING,&pf);
    GLES.glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&pv);GLES.glGetIntegerv(GL_TEXTURE_BINDING_2D,&pt);
    GLES.glBindFramebuffer(GL_READ_FRAMEBUFFER,0);GLES.glBindFramebuffer(GL_DRAW_FRAMEBUFFER,g_df);
    GLES.glBlitFramebuffer(0,0,w,h,0,0,w,h,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    GLES.glBindFramebuffer(GL_FRAMEBUFFER,0);
    GLES.glUseProgram(g_dp);GLES.glActiveTexture(GL_TEXTURE0);GLES.glBindTexture(GL_TEXTURE_2D,g_dt);
    GLES.glUniform1i(g_duInput,0);GLES.glUniform2f(g_duTexel,1.f/w,1.f/h);
    GLES.glUniform1f(g_duStr,g_denoiser_strength);
    GLES.glBindVertexArray(g_dv);GLES.glDisable(GL_DEPTH_TEST);GLES.glDisable(GL_BLEND);
    GLES.glDrawArrays(GL_TRIANGLES,0,3);
    GLES.glUseProgram((GLuint)pp);GLES.glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)pf);
    GLES.glBindVertexArray((GLuint)pv);GLES.glBindTexture(GL_TEXTURE_2D,(GLuint)pt);
}
void vinzz_denoiser_cleanup(){
    if(g_df){GLES.glDeleteFramebuffers(1,&g_df);g_df=0;}
    if(g_dt){GLES.glDeleteTextures(1,&g_dt);g_dt=0;}
    if(g_dp){GLES.glDeleteProgram(g_dp);g_dp=0;}
    if(g_dv){GLES.glDeleteVertexArrays(1,&g_dv);g_dv=0;}
    g_dready=false;
}

// ═══════════════════════════════════════════════════════════════
// FEATURE 4: ASYNC SHADER COMPILE (GL_KHR_parallel_shader_compile)
// ═══════════════════════════════════════════════════════════════
#ifndef GL_COMPLETION_STATUS_KHR
#  define GL_COMPLETION_STATUS_KHR 0x91B1
#endif
typedef void (GL_APIENTRYP PFNGLMAXSHADERCOMPILERTHREADSKHRPROC)(GLuint count);
static PFNGLMAXSHADERCOMPILERTHREADSKHRPROC pfn_MaxSCT=nullptr;
static bool g_has_pc=false;

void vinzz_async_shader_init(){
    if(!global_settings.vinzz_async_shader) return;
    const char* ext=(const char*)GLES.glGetString(GL_EXTENSIONS);
    if(ext&&strstr(ext,"GL_KHR_parallel_shader_compile")){
#ifdef __ANDROID__
        pfn_MaxSCT=(PFNGLMAXSHADERCOMPILERTHREADSKHRPROC)eglGetProcAddress("glMaxShaderCompilerThreadsKHR");
#endif
        g_has_pc=(pfn_MaxSCT!=nullptr);
    }
    if(g_has_pc){pfn_MaxSCT(0xFFFFFFFFu);VNLOG("[AsyncShader] OK - unlimited threads");}
    else VNLOG("[AsyncShader] GL_KHR_parallel_shader_compile not available");
}
void vinzz_async_compile(GLuint s){GLES.glCompileShader(s);}
void vinzz_async_link(GLuint p){GLES.glLinkProgram(p);}
void vinzz_async_shader_wait(GLuint s){
    if(!g_has_pc) return;
    GLint done=GL_FALSE;int t=0;
    while(!done&&t<5000){GLES.glGetShaderiv(s,GL_COMPLETION_STATUS_KHR,&done);if(!done){++t;std::this_thread::yield();}}
}
void vinzz_async_program_wait(GLuint p){
    if(!g_has_pc) return;
    GLint done=GL_FALSE;int t=0;
    while(!done&&t<5000){GLES.glGetProgramiv(p,GL_COMPLETION_STATUS_KHR,&done);if(!done){++t;std::this_thread::yield();}}
}

// ═══════════════════════════════════════════════════════════════
// FEATURE 5: PIPELINE BINARY CACHE
// ═══════════════════════════════════════════════════════════════
static std::string g_cdir; static bool g_pcready=false;

static std::string cpath(const std::string& k){
    std::string s=k; for(char& c:s) if(c=='/'||c=='\\'||c==':') c='_';
    return g_cdir+"/"+s+".bin";
}
void vinzz_pipeline_cache_init(const char* d){
    if(!global_settings.vinzz_pipeline_cache||!d) return;
    g_cdir=d;
#ifdef __ANDROID__
    mkdir(g_cdir.c_str(),0755);
#endif
    g_pcready=true; VNLOG("[PipelineCache] dir: %s",d);
}
bool vinzz_pipeline_cache_load(GLuint prog,const std::string& key){
    if(!g_pcready) return false;
    FILE* f=fopen(cpath(key).c_str(),"rb"); if(!f) return false;
    fseek(f,0,SEEK_END);long sz=ftell(f);rewind(f);
    if(sz<8){fclose(f);return false;}
    GLenum fmt=0;fread(&fmt,4,1,f);
    long dl=sz-4;std::vector<uint8_t> buf(dl);fread(buf.data(),1,(size_t)dl,f);fclose(f);
    GLES.glProgramBinary(prog,fmt,buf.data(),(GLsizei)dl);
    GLint ok=0;GLES.glGetProgramiv(prog,GL_LINK_STATUS,&ok);
    if(ok){VNLOG("[PipelineCache] HIT %s",key.c_str());return true;}
    remove(cpath(key).c_str()); return false;
}
void vinzz_pipeline_cache_save(GLuint prog,const std::string& key){
    if(!g_pcready) return;
    GLint bl=0;GLES.glGetProgramiv(prog,GL_PROGRAM_BINARY_LENGTH,&bl);if(bl<=0) return;
    std::vector<uint8_t> buf((size_t)bl);GLenum fmt=0;GLsizei wr=0;
    GLES.glGetProgramBinary(prog,bl,&wr,&fmt,buf.data());if(wr<=0) return;
    FILE* f=fopen(cpath(key).c_str(),"wb");if(!f) return;
    fwrite(&fmt,4,1,f);fwrite(buf.data(),1,(size_t)wr,f);fclose(f);
    VNLOG("[PipelineCache] SAVE %s len=%d",key.c_str(),wr);
}
void vinzz_pipeline_cache_clear_all(){ VNLOG("[PipelineCache] clear_all"); }
