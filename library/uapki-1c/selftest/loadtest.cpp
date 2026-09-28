/*
 *  uapki-1c-loadtest (Unix only): loads the built AddIn the way the 1C platform
 *  does — dlopen() + dlsym() of the plain C entry points — then drives the Uapki
 *  object through the IComponentBase interface: FindMethod("Process") and
 *  CallAsFunc with a VERSION request.
 *
 *  Usage: uapki-1c-loadtest /path/to/libuapki-1cLin64.so
 */

#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "ComponentBase.h"
#include "IMemoryManager.h"

typedef long (*GetClassObject_f)(const WCHAR_T*, IComponentBase**);
typedef long (*DestroyObject_f)(IComponentBase**);
typedef const WCHAR_T* (*GetClassNames_f)(void);

class MallocMemory : public IMemoryManager {
public:
    bool ADDIN_API AllocMemory (void** pMemory, unsigned long ulCountByte) override {
        *pMemory = malloc(ulCountByte);
        return *pMemory != nullptr;
    }
    void ADDIN_API FreeMemory (void** pMemory) override {
        free(*pMemory);
        *pMemory = nullptr;
    }
};

static int failures = 0;
static void check (bool ok, const char* what)
{
    printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static std::string narrow (const WCHAR_T* ws, size_t n)
{
    std::string s;
    for (size_t i = 0; i < n && ws[i]; i++) s += (ws[i] < 0x80) ? (char)ws[i] : '?';
    return s;
}

int main (int argc, char** argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <addin.so>\n", argv[0]); return 2; }

    void* h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    check(h != nullptr, "dlopen the AddIn");
    if (!h) { fprintf(stderr, "  %s\n", dlerror()); return 1; }

    const char* names[] = { "GetClassObject", "DestroyObject", "GetClassNames",
                            "SetPlatformCapabilities", "GetAttachType" };
    for (const char* n : names) {
        std::string what = std::string("dlsym ") + n;
        check(dlsym(h, n) != nullptr, what.c_str());
    }
    GetClassObject_f getObj = (GetClassObject_f)dlsym(h, "GetClassObject");
    DestroyObject_f destroyObj = (DestroyObject_f)dlsym(h, "DestroyObject");
    GetClassNames_f getNames = (GetClassNames_f)dlsym(h, "GetClassNames");
    if (!getObj || !destroyObj || !getNames) return 1;

    const WCHAR_T* cls = getNames();
    check(cls && narrow(cls, 64) == "Uapki", "GetClassNames == \"Uapki\"");

    IComponentBase* obj = nullptr;
    check(getObj(cls, &obj) != 0 && obj != nullptr, "GetClassObject returns an instance");
    if (!obj) return 1;

    MallocMemory mem;
    check(obj->setMemManager(&mem), "setMemManager");

    const long meth = obj->FindMethod(u"Process");
    check(meth >= 0, "FindMethod(\"Process\")");

    static const WCHAR_T req[] = u"{\"method\":\"VERSION\"}";
    tVariant param;
    memset(&param, 0, sizeof(param));
    TV_VT(&param) = VTYPE_PWSTR;
    param.pwstrVal = (WCHAR_T*)req;
    param.wstrLen = (uint32_t)(sizeof(req) / sizeof(WCHAR_T) - 1);
    tVariant ret;
    memset(&ret, 0, sizeof(ret));
    const bool called = obj->CallAsFunc(meth, &ret, &param, 1);
    check(called && TV_VT(&ret) == VTYPE_PWSTR, "CallAsFunc(Process, VERSION) returns a string");
    if (called && TV_VT(&ret) == VTYPE_PWSTR) {
        const std::string resp = narrow(ret.pwstrVal, ret.wstrLen);
        printf("  VERSION -> %s\n", resp.c_str());
        check(resp.find("\"errorCode\":0") != std::string::npos, "VERSION errorCode == 0");
        mem.FreeMemory((void**)&ret.pwstrVal);
    }

    check(destroyObj(&obj) == 0 && obj == nullptr, "DestroyObject");
    dlclose(h);

    printf(failures ? "\n%d FAILURE(S)\n" : "\nALL CHECKS PASSED\n", failures);
    return failures ? 1 : 0;
}
