#include <windows.h>
#include <psapi.h>
#include <gl/GL.h>

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <iterator>
#include <vector>

namespace {

HMODULE selfModule = nullptr;

using GlSelectBufferFn = void (APIENTRY*)(GLsizei, GLuint*);
using GlRenderModeFn = GLint (APIENTRY*)(GLenum);
using GlInitNamesFn = void (APIENTRY*)();
using GlPushNameFn = void (APIENTRY*)(GLuint);
using GlPopNameFn = void (APIENTRY*)();
using GlLoadNameFn = void (APIENTRY*)(GLuint);
using GlBeginFn = void (APIENTRY*)(GLenum);
using GlEndFn = void (APIENTRY*)();
using GlCallListFn = void (APIENTRY*)(GLuint);
using GlDrawArraysFn = void (APIENTRY*)(GLenum, GLint, GLsizei);
using GlDrawElementsFn = void (APIENTRY*)(GLenum, GLsizei, GLenum, const GLvoid*);
using GlGetStringFn = const GLubyte* (APIENTRY*)(GLenum);
using WglGetProcAddressFn = PROC (WINAPI*)(LPCSTR);
using GlIsEnabledFn = GLboolean (APIENTRY*)(GLenum);
using GlEnableFn = void (APIENTRY*)(GLenum);
using GlDisableFn = void (APIENTRY*)(GLenum);
using GlGetBooleanvFn = void (APIENTRY*)(GLenum, GLboolean*);
using GlGetIntegervFn = void (APIENTRY*)(GLenum, GLint*);
using GlColorMaskFn = void (APIENTRY*)(GLboolean, GLboolean, GLboolean, GLboolean);
using GlDepthMaskFn = void (APIENTRY*)(GLboolean);
using GlStencilMaskFn = void (APIENTRY*)(GLuint);
using GlGenQueriesFn = void (APIENTRY*)(GLsizei, GLuint*);
using GlDeleteQueriesFn = void (APIENTRY*)(GLsizei, const GLuint*);
using GlBeginQueryFn = void (APIENTRY*)(GLenum, GLuint);
using GlEndQueryFn = void (APIENTRY*)(GLenum);
using GlGetQueryObjectuivFn = void (APIENTRY*)(GLuint, GLenum, GLuint*);

GlSelectBufferFn realGlSelectBuffer = nullptr;
GlRenderModeFn realGlRenderMode = nullptr;
GlInitNamesFn realGlInitNames = nullptr;
GlPushNameFn realGlPushName = nullptr;
GlPopNameFn realGlPopName = nullptr;
GlLoadNameFn realGlLoadName = nullptr;
GlBeginFn realGlBegin = nullptr;
GlEndFn realGlEnd = nullptr;
GlCallListFn realGlCallList = nullptr;
GlDrawArraysFn realGlDrawArrays = nullptr;
GlDrawElementsFn realGlDrawElements = nullptr;
GlGetStringFn realGlGetString = nullptr;
WglGetProcAddressFn realWglGetProcAddress = nullptr;
GlIsEnabledFn realGlIsEnabled = nullptr;
GlEnableFn realGlEnable = nullptr;
GlDisableFn realGlDisable = nullptr;
GlGetBooleanvFn realGlGetBooleanv = nullptr;
GlGetIntegervFn realGlGetIntegerv = nullptr;
GlColorMaskFn realGlColorMask = nullptr;
GlDepthMaskFn realGlDepthMask = nullptr;
GlStencilMaskFn realGlStencilMask = nullptr;
GlGenQueriesFn glGenQueries = nullptr;
GlDeleteQueriesFn glDeleteQueries = nullptr;
GlBeginQueryFn glBeginQuery = nullptr;
GlEndQueryFn glEndQuery = nullptr;
GlGetQueryObjectuivFn glGetQueryObjectuiv = nullptr;

CRITICAL_SECTION logLock;
std::atomic<bool> initialized{false};
std::atomic<bool> initializationStarted{false};
std::atomic<unsigned long long> emulatedPasses{0};
thread_local GLenum trackedMode = GL_RENDER;
thread_local GLuint* trackedBuffer = nullptr;
thread_local GLsizei trackedBufferSize = 0;
thread_local std::vector<GLuint> nameStack;
std::atomic<bool> capabilitiesLogged{false};
bool fixModeEnabled = false;
bool verboseLogging = true;
char logFilePath[MAX_PATH]{};
constexpr GLenum GL_SAMPLES_PASSED_ARB_VALUE = 0x8914;
constexpr GLenum GL_QUERY_RESULT_ARB_VALUE = 0x8866;

struct PendingQuery {
    GLuint id = 0;
    std::vector<GLuint> names;
};

struct SavedState {
    GLboolean colorMask[4]{};
    GLboolean depthMask = GL_TRUE;
    GLint stencilMask = ~0;
    GLboolean depthTest = GL_FALSE;
};

thread_local bool emulatingSelection = false;
thread_local GLuint activeQuery = 0;
thread_local std::vector<PendingQuery> pendingQueries;
thread_local std::vector<GLuint> queryPool;
thread_local SavedState savedState;

void SiblingPath(const char* filename, char (&output)[MAX_PATH]) {
    GetModuleFileNameA(selfModule, output, MAX_PATH);
    char* slash = std::strrchr(output, '\\');
    if (slash)
        strcpy_s(slash + 1, MAX_PATH - static_cast<size_t>(slash + 1 - output), filename);
}

void ConfigurePaths() {
    char enablePath[MAX_PATH]{};
    char verbosePath[MAX_PATH]{};
    SiblingPath("XSISelectFix.log", logFilePath);
    SiblingPath("XSISelectFix.enable", enablePath);
    SiblingPath("XSISelectFix.verbose", verbosePath);
    fixModeEnabled = GetFileAttributesA(enablePath) != INVALID_FILE_ATTRIBUTES;
    verboseLogging = !fixModeEnabled ||
        GetFileAttributesA(verbosePath) != INVALID_FILE_ATTRIBUTES;
}

void Log(const char* format, ...) {
    EnterCriticalSection(&logLock);
    FILE* file = nullptr;
    fopen_s(&file, logFilePath, "a");
    if (file) {
        SYSTEMTIME time{};
        GetLocalTime(&time);
        std::fprintf(file, "[%02u:%02u:%02u.%03u tid=%lu] ", time.wHour, time.wMinute,
                     time.wSecond, time.wMilliseconds, GetCurrentThreadId());
        va_list args;
        va_start(args, format);
        std::vfprintf(file, format, args);
        va_end(args);
        std::fputc('\n', file);
        std::fclose(file);
    }
    LeaveCriticalSection(&logLock);
}

void LogSelectionBuffer(GLint hitCount) {
    if (hitCount <= 0 || !trackedBuffer || trackedBufferSize <= 0) return;
    const GLuint* cursor = trackedBuffer;
    const GLuint* end = trackedBuffer + trackedBufferSize;
    for (GLint hit = 0; hit < hitCount && cursor + 3 <= end; ++hit) {
        const GLuint count = cursor[0];
        const GLuint minDepth = cursor[1];
        const GLuint maxDepth = cursor[2];
        if (cursor + 3 + count > end) {
            Log("hit[%d] malformed: names=%u exceeds buffer", hit, count);
            return;
        }
        std::string names;
        for (GLuint i = 0; i < count; ++i) {
            char item[32]{};
            std::snprintf(item, sizeof(item), "%s%08X", i ? "," : "", cursor[3 + i]);
            names += item;
        }
        Log("hit[%d] min=%08X max=%08X names=[%s]", hit, minDepth, maxDepth, names.c_str());
        cursor += 3 + count;
    }
}

bool EnsureQueryFunctions() {
    if (glGenQueries && glDeleteQueries && glBeginQuery && glEndQuery && glGetQueryObjectuiv)
        return true;
    glGenQueries = reinterpret_cast<GlGenQueriesFn>(realWglGetProcAddress("glGenQueriesARB"));
    glDeleteQueries = reinterpret_cast<GlDeleteQueriesFn>(realWglGetProcAddress("glDeleteQueriesARB"));
    glBeginQuery = reinterpret_cast<GlBeginQueryFn>(realWglGetProcAddress("glBeginQueryARB"));
    glEndQuery = reinterpret_cast<GlEndQueryFn>(realWglGetProcAddress("glEndQueryARB"));
    glGetQueryObjectuiv = reinterpret_cast<GlGetQueryObjectuivFn>(
        realWglGetProcAddress("glGetQueryObjectuivARB"));
    return glGenQueries && glDeleteQueries && glBeginQuery && glEndQuery && glGetQueryObjectuiv;
}

void EndActiveQuery() {
    if (!activeQuery) return;
    glEndQuery(GL_SAMPLES_PASSED_ARB_VALUE);
    activeQuery = 0;
}

void StartQueryForCurrentNames() {
    EndActiveQuery();
    if (!emulatingSelection || nameStack.empty() || nameStack.back() == 0) return;
    PendingQuery query;
    query.names = nameStack;
    const size_t poolIndex = pendingQueries.size();
    if (poolIndex >= queryPool.size()) {
        GLuint id = 0;
        glGenQueries(1, &id);
        if (id) queryPool.push_back(id);
    }
    if (poolIndex < queryPool.size()) query.id = queryPool[poolIndex];
    if (!query.id) return;
    pendingQueries.push_back(query);
    activeQuery = query.id;
    glBeginQuery(GL_SAMPLES_PASSED_ARB_VALUE, activeQuery);
}

void BeginEmulatedSelection() {
    pendingQueries.clear();
    activeQuery = 0;
    nameStack.clear();
    realGlGetBooleanv(GL_COLOR_WRITEMASK, savedState.colorMask);
    realGlGetBooleanv(GL_DEPTH_WRITEMASK, &savedState.depthMask);
    realGlGetIntegerv(GL_STENCIL_WRITEMASK, &savedState.stencilMask);
    savedState.depthTest = realGlIsEnabled(GL_DEPTH_TEST);
    realGlColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    realGlDepthMask(GL_FALSE);
    realGlStencilMask(0);
    realGlDisable(GL_DEPTH_TEST);
    emulatingSelection = true;
    trackedMode = GL_SELECT;
    if (verboseLogging) Log("emulation begin");
}

GLint EndEmulatedSelection() {
    EndActiveQuery();
    realGlColorMask(savedState.colorMask[0], savedState.colorMask[1], savedState.colorMask[2],
                    savedState.colorMask[3]);
    realGlDepthMask(savedState.depthMask);
    realGlStencilMask(static_cast<GLuint>(savedState.stencilMask));
    if (savedState.depthTest) realGlEnable(GL_DEPTH_TEST); else realGlDisable(GL_DEPTH_TEST);

    GLint hitCount = 0;
    bool overflow = false;
    GLuint* cursor = trackedBuffer;
    const GLuint* end = trackedBuffer && trackedBufferSize > 0
        ? trackedBuffer + trackedBufferSize : nullptr;
    for (const auto& query : pendingQueries) {
        GLuint samples = 0;
        glGetQueryObjectuiv(query.id, GL_QUERY_RESULT_ARB_VALUE, &samples);
        const GLuint finalName = query.names.empty() ? 0 : query.names.back();
        if (verboseLogging) Log("query name=%08X samples=%u", finalName, samples);
        if (!samples) continue;
        if (!cursor || cursor + 3 + query.names.size() > end) {
            overflow = true;
            continue;
        }
        const GLuint depth = 0xF0000000u - (std::min)(samples, 0x0FFFFFFFu);
        *cursor++ = static_cast<GLuint>(query.names.size());
        *cursor++ = depth;
        *cursor++ = depth;
        for (GLuint name : query.names) *cursor++ = name;
        ++hitCount;
    }
    pendingQueries.clear();
    emulatingSelection = false;
    trackedMode = GL_RENDER;
    const auto pass = ++emulatedPasses;
    if (overflow) {
        Log("selection buffer overflow on pass=%llu", pass);
        return -1;
    }
    if (verboseLogging) Log("emulation end synthesizedHits=%d", hitCount);
    else if (pass % 1000 == 0) Log("summary emulatedPasses=%llu lastHits=%d", pass, hitCount);
    return hitCount;
}

void APIENTRY HookGlSelectBuffer(GLsizei size, GLuint* buffer) {
    trackedBufferSize = size;
    trackedBuffer = buffer;
    if (verboseLogging) Log("glSelectBuffer size=%d buffer=%p", size, buffer);
    realGlSelectBuffer(size, buffer);
}

GLint APIENTRY HookGlRenderMode(GLenum mode) {
    if (fixModeEnabled && mode == GL_SELECT && trackedMode == GL_RENDER && EnsureQueryFunctions()) {
        BeginEmulatedSelection();
        return 0;
    }
    if (fixModeEnabled && mode == GL_RENDER && emulatingSelection) {
        return EndEmulatedSelection();
    }
    const GLenum previousMode = trackedMode;
    const GLint result = realGlRenderMode(mode);
    trackedMode = mode;
    if (verboseLogging)
        Log("glRenderMode %04X -> result=%d (previous=%04X)", mode, result, previousMode);
    if (mode == GL_SELECT && !capabilitiesLogged.exchange(true)) {
        const char* extensions = reinterpret_cast<const char*>(realGlGetString(GL_EXTENSIONS));
        const bool arbQuery = extensions && std::strstr(extensions, "GL_ARB_occlusion_query");
        const bool nvQuery = extensions && std::strstr(extensions, "GL_NV_occlusion_query");
        Log("capabilities ARB_occlusion_query=%d NV_occlusion_query=%d GenQueriesARB=%p",
            arbQuery ? 1 : 0, nvQuery ? 1 : 0, realWglGetProcAddress("glGenQueriesARB"));
    }
    if (previousMode == GL_SELECT && mode == GL_RENDER) LogSelectionBuffer(result);
    return result;
}

void APIENTRY HookGlInitNames() {
    if (trackedMode == GL_SELECT) {
        nameStack.clear();
        if (verboseLogging) Log("glInitNames");
    }
    if (!emulatingSelection) realGlInitNames();
}

void APIENTRY HookGlPushName(GLuint name) {
    if (trackedMode == GL_SELECT) {
        nameStack.push_back(name);
        if (verboseLogging) Log("glPushName %08X depth=%zu", name, nameStack.size());
    }
    if (!emulatingSelection) realGlPushName(name);
}

void APIENTRY HookGlPopName() {
    if (trackedMode == GL_SELECT) {
        if (emulatingSelection) EndActiveQuery();
        if (!nameStack.empty()) nameStack.pop_back();
        if (verboseLogging) Log("glPopName depth=%zu", nameStack.size());
    }
    if (!emulatingSelection) realGlPopName();
}

void APIENTRY HookGlLoadName(GLuint name) {
    if (trackedMode == GL_SELECT) {
        if (emulatingSelection) EndActiveQuery();
        if (!nameStack.empty()) nameStack.back() = name;
        if (verboseLogging) Log("glLoadName %08X", name);
        if (emulatingSelection) StartQueryForCurrentNames();
    }
    if (!emulatingSelection) realGlLoadName(name);
}

void APIENTRY HookGlBegin(GLenum mode) {
    if (trackedMode == GL_SELECT && verboseLogging) Log("glBegin mode=%04X", mode);
    realGlBegin(mode);
}

void APIENTRY HookGlEnd() {
    if (trackedMode == GL_SELECT && verboseLogging) Log("glEnd");
    realGlEnd();
}

void APIENTRY HookGlCallList(GLuint list) {
    if (trackedMode == GL_SELECT && verboseLogging) Log("glCallList list=%u", list);
    realGlCallList(list);
}

void APIENTRY HookGlDrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (trackedMode == GL_SELECT && verboseLogging)
        Log("glDrawArrays mode=%04X first=%d count=%d", mode, first, count);
    realGlDrawArrays(mode, first, count);
}

void APIENTRY HookGlDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices) {
    if (trackedMode == GL_SELECT && verboseLogging)
        Log("glDrawElements mode=%04X count=%d type=%04X indices=%p", mode, count, type, indices);
    realGlDrawElements(mode, count, type, indices);
}

struct HookSpec {
    const char* name;
    void* replacement;
};

bool PatchModule(HMODULE module, const HookSpec* hooks, size_t hookCount) {
    auto* base = reinterpret_cast<unsigned char*>(module);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!directory.VirtualAddress) return false;

    bool patched = false;
    auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress);
    for (; descriptor->Name; ++descriptor) {
        const char* dllName = reinterpret_cast<const char*>(base + descriptor->Name);
        if (_stricmp(dllName, "opengl32.dll") != 0) continue;
        if (!descriptor->OriginalFirstThunk) continue;
        auto* original = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->OriginalFirstThunk);
        auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->FirstThunk);
        for (; original->u1.AddressOfData; ++original, ++thunk) {
            if (IMAGE_SNAP_BY_ORDINAL(original->u1.Ordinal)) continue;
            auto* imported = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + original->u1.AddressOfData);
            const char* functionName = reinterpret_cast<const char*>(imported->Name);
            for (size_t i = 0; i < hookCount; ++i) {
                if (std::strcmp(functionName, hooks[i].name) != 0) continue;
                DWORD oldProtect = 0;
                if (VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &oldProtect)) {
                    thunk->u1.Function = reinterpret_cast<ULONG_PTR>(hooks[i].replacement);
                    DWORD ignored = 0;
                    VirtualProtect(&thunk->u1.Function, sizeof(void*), oldProtect, &ignored);
                    FlushInstructionCache(GetCurrentProcess(), &thunk->u1.Function, sizeof(void*));
                    patched = true;
                }
            }
        }
    }
    return patched;
}

DWORD WINAPI InitializeHooks(void*) {
    InitializeCriticalSection(&logLock);
    ConfigurePaths();
    // A self-installed Softimage plug-in can be discovered before the viewport's
    // OpenGL modules have finished loading.  Wait off the loader thread so normal
    // startup can use this DLL without an external injector.
    HMODULE openGl = nullptr;
    HMODULE graphicOgl = nullptr;
    for (unsigned attempt = 0; attempt < 240; ++attempt) {
        openGl = GetModuleHandleW(L"opengl32.dll");
        graphicOgl = GetModuleHandleW(L"graphicogl.dll");
        if (openGl && graphicOgl) break;
        Sleep(250);
    }
    if (!openGl || !graphicOgl) {
        Log("XSISelectFix startup timed out waiting for opengl32.dll/graphicogl.dll");
        return 1;
    }
    realGlSelectBuffer = reinterpret_cast<GlSelectBufferFn>(GetProcAddress(openGl, "glSelectBuffer"));
    realGlRenderMode = reinterpret_cast<GlRenderModeFn>(GetProcAddress(openGl, "glRenderMode"));
    realGlInitNames = reinterpret_cast<GlInitNamesFn>(GetProcAddress(openGl, "glInitNames"));
    realGlPushName = reinterpret_cast<GlPushNameFn>(GetProcAddress(openGl, "glPushName"));
    realGlPopName = reinterpret_cast<GlPopNameFn>(GetProcAddress(openGl, "glPopName"));
    realGlLoadName = reinterpret_cast<GlLoadNameFn>(GetProcAddress(openGl, "glLoadName"));
    realGlBegin = reinterpret_cast<GlBeginFn>(GetProcAddress(openGl, "glBegin"));
    realGlEnd = reinterpret_cast<GlEndFn>(GetProcAddress(openGl, "glEnd"));
    realGlCallList = reinterpret_cast<GlCallListFn>(GetProcAddress(openGl, "glCallList"));
    realGlDrawArrays = reinterpret_cast<GlDrawArraysFn>(GetProcAddress(openGl, "glDrawArrays"));
    realGlDrawElements = reinterpret_cast<GlDrawElementsFn>(GetProcAddress(openGl, "glDrawElements"));
    realGlGetString = reinterpret_cast<GlGetStringFn>(GetProcAddress(openGl, "glGetString"));
    realWglGetProcAddress = reinterpret_cast<WglGetProcAddressFn>(
        GetProcAddress(openGl, "wglGetProcAddress"));
    realGlIsEnabled = reinterpret_cast<GlIsEnabledFn>(GetProcAddress(openGl, "glIsEnabled"));
    realGlEnable = reinterpret_cast<GlEnableFn>(GetProcAddress(openGl, "glEnable"));
    realGlDisable = reinterpret_cast<GlDisableFn>(GetProcAddress(openGl, "glDisable"));
    realGlGetBooleanv = reinterpret_cast<GlGetBooleanvFn>(GetProcAddress(openGl, "glGetBooleanv"));
    realGlGetIntegerv = reinterpret_cast<GlGetIntegervFn>(GetProcAddress(openGl, "glGetIntegerv"));
    realGlColorMask = reinterpret_cast<GlColorMaskFn>(GetProcAddress(openGl, "glColorMask"));
    realGlDepthMask = reinterpret_cast<GlDepthMaskFn>(GetProcAddress(openGl, "glDepthMask"));
    realGlStencilMask = reinterpret_cast<GlStencilMaskFn>(GetProcAddress(openGl, "glStencilMask"));
    if (!realGlSelectBuffer || !realGlRenderMode || !realGlInitNames || !realGlPushName ||
        !realGlPopName || !realGlLoadName || !realGlBegin || !realGlEnd || !realGlCallList ||
        !realGlDrawArrays || !realGlDrawElements || !realGlGetString || !realWglGetProcAddress ||
        !realGlIsEnabled || !realGlEnable || !realGlDisable || !realGlGetBooleanv ||
        !realGlGetIntegerv || !realGlColorMask || !realGlDepthMask || !realGlStencilMask)
        return 2;

    const HookSpec hooks[] = {
        {"glSelectBuffer", reinterpret_cast<void*>(&HookGlSelectBuffer)},
        {"glRenderMode", reinterpret_cast<void*>(&HookGlRenderMode)},
        {"glInitNames", reinterpret_cast<void*>(&HookGlInitNames)},
        {"glPushName", reinterpret_cast<void*>(&HookGlPushName)},
        {"glPopName", reinterpret_cast<void*>(&HookGlPopName)},
        {"glLoadName", reinterpret_cast<void*>(&HookGlLoadName)},
        {"glBegin", reinterpret_cast<void*>(&HookGlBegin)},
        {"glEnd", reinterpret_cast<void*>(&HookGlEnd)},
        {"glCallList", reinterpret_cast<void*>(&HookGlCallList)},
        {"glDrawArrays", reinterpret_cast<void*>(&HookGlDrawArrays)},
        {"glDrawElements", reinterpret_cast<void*>(&HookGlDrawElements)},
    };

    HMODULE modules[2048]{};
    DWORD bytesNeeded = 0;
    unsigned patchedModules = 0;
    if (EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &bytesNeeded)) {
        const size_t count = std::min<size_t>(bytesNeeded / sizeof(HMODULE), std::size(modules));
        for (size_t i = 0; i < count; ++i) {
            if (PatchModule(modules[i], hooks, std::size(hooks))) ++patchedModules;
        }
    }
    initialized = true;
    Log("XSISelectFix loaded; patchedModules=%u fixMode=%d verbose=%d", patchedModules,
        fixModeEnabled ? 1 : 0, verboseLogging ? 1 : 0);
    return patchedModules ? 0 : 3;
}

}  // namespace

extern "C" __declspec(dllexport) BOOL WINAPI XSISelectFixInitialize() {
    if (initialized.load()) return TRUE;

    bool expected = false;
    if (!initializationStarted.compare_exchange_strong(expected, true)) {
        // A previous explicit initialization request already started the
        // worker. It is safe for repeated loader callbacks to return success.
        return TRUE;
    }

    const HANDLE thread = CreateThread(nullptr, 0, InitializeHooks, nullptr, 0, nullptr);
    if (!thread) {
        initializationStarted = false;
        return FALSE;
    }
    CloseHandle(thread);
    return TRUE;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void*) {
    if (reason == DLL_PROCESS_ATTACH) {
        selfModule = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
