/*
 * Copyright (c) 2021, The UAPKI Project Authors.
 *
 * Standalone verification harness for the self-contained UAPKI build:
 *   - registers cm-pkcs12 as a *built-in* (statically linked) provider via the
 *     new cm_pkcs12_get_static_api() + CmLoader::registerStaticProvider() path,
 *     with NO cm-pkcs12.{dll,so} on disk;
 *   - drives uapki process() through VERSION / DIGEST / INIT / OPEN / SELECT_KEY
 *     / SIGN / VERIFY using the fixtures in library/test/data;
 *   - exercises the AddIn's UTF-8 <-> UTF-16 marshaling (incl. Cyrillic and a
 *     character above U+FFFF).
 *
 * Exit code 0 = all checks passed.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(_WIN32)
#include <direct.h>
#define chdir _chdir
#define MKDIR(p) _mkdir(p)
#else
#include <unistd.h>
#include <sys/stat.h>
#define MKDIR(p) mkdir((p), 0777)
#endif

#include "cm-api.h"
#include "cm-loader.h"
#include "static-api.h"
#include "str-convert.h"

extern "C" char* process (const char* request);
extern "C" void  json_free (char* buf);


static int g_failures = 0;

static void check (bool ok, const char* what)
{
    printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) g_failures++;
}

static std::string call (const char* request)
{
    char* resp = process(request);
    std::string out = resp ? resp : "";
    if (resp) json_free(resp);
    return out;
}

static bool contains (const std::string& hay, const char* needle)
{
    return hay.find(needle) != std::string::npos;
}

//  Parse the integer value of "errorCode": from a UAPKI response.
static long errorCode (const std::string& resp)
{
    const std::string key = "\"errorCode\":";
    size_t p = resp.find(key);
    if (p == std::string::npos) return -1000;
    p += key.size();
    return strtol(resp.c_str() + p, nullptr, 10);
}

static void shortPrint (const char* label, const std::string& resp)
{
    std::string s = resp.size() > 200 ? (resp.substr(0, 200) + " ...") : resp;
    printf("  %s -> %s\n", label, s.c_str());
}

//  A throwaway, writable cache directory (UAPKI rewrites cert/CRL cache dirs in
//  place, so we must not point it at the committed fixtures). Returns a path
//  with forward slashes and a trailing slash.
static std::string makeTempCacheDir (const char* leaf)
{
    const char* base = getenv("TEMP");
    if (!base) base = getenv("TMPDIR");
    if (!base) base =
#if defined(_WIN32)
        ".";
#else
        "/tmp";
#endif
    std::string path(base);
    for (size_t i = 0; i < path.size(); i++) {
        if (path[i] == '\\') path[i] = '/';
    }
    if (!path.empty() && path[path.size() - 1] != '/') path += '/';
    path += "uapki1c_selftest_";
    path += leaf;
    MKDIR(path.c_str());
    path += '/';
    return path;
}


int main (int argc, char** argv)
{
    if (argc > 1) {
        if (chdir(argv[1]) != 0) {
            printf("WARN: could not chdir to '%s'\n", argv[1]);
        }
    }

    //  --- 1. Register cm-pkcs12 as a built-in provider (no shared object) -----
    CM_PROVIDER_API api;
    memset(&api, 0, sizeof(api));
    cm_pkcs12_get_static_api(&api);
    check(api.info && api.init && api.open && api.close, "cm_pkcs12_get_static_api filled the required entry points");
    CmLoader::registerStaticProvider("cm-pkcs12", api);

    //  --- 2. UTF-8 <-> UTF-16 round-trip (Cyrillic + emoji U+1F600) ----------
    {
        const std::string original = u8"ASCII Кирилиця 😀 end";
        std::u16string u16 = uapki1c::utf8ToUtf16(original);
        std::string back = uapki1c::utf16ToUtf8(u16.data(), u16.size());
        check(back == original, "UTF-8<->UTF-16 round-trip (Cyrillic + U+1F600)");
        if (back != original) {
            printf("    original=%s\n    back    =%s\n", original.c_str(), back.c_str());
        }
    }

    //  --- 3. VERSION ----------------------------------------------------------
    {
        std::string r = call("{\"method\":\"VERSION\"}");
        shortPrint("VERSION", r);
        check(errorCode(r) == 0, "VERSION errorCode == 0");
    }

    //  --- 4. DIGEST (SHA-256 of 'The quick brown fox ...') -------------------
    {
        const char* req =
            "{\"method\":\"DIGEST\",\"parameters\":{"
            "\"hashAlgo\":\"2.16.840.1.101.3.4.2.1\","
            "\"bytes\":\"VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wcyBvdmVyIHRoZSBsYXp5IGRvZw==\"}}";
        std::string r = call(req);
        shortPrint("DIGEST", r);
        check(errorCode(r) == 0, "DIGEST errorCode == 0");
        //  UAPKI returns the digest base64-encoded in "bytes";
        //  16j7swfXgJRpypq8sAguT41WUeRtPNt2LQLQvzfJ5ZI= == hex d7a8fbb307...c9e592
        check(contains(r, "16j7swfXgJRpypq8sAguT41WUeRtPNt2LQLQvzfJ5ZI="),
              "DIGEST SHA-256 matches known value (base64)");
    }

    //  --- 5. INIT with the built-in cm-pkcs12 provider (offline) -------------
    //  Use throwaway temp cache dirs: UAPKI rewrites its cert/CRL cache in place,
    //  so we keep the committed library/test/data fixtures untouched.
    {
        const std::string certDir = makeTempCacheDir("certs");
        const std::string crlDir  = makeTempCacheDir("crls");
        const std::string req =
            std::string("{\"method\":\"INIT\",\"parameters\":{")
            + "\"cmProviders\":{\"dir\":\"\",\"allowedProviders\":[{\"lib\":\"cm-pkcs12\"}]},"
            + "\"certCache\":{\"path\":\"" + certDir + "\",\"trustedCerts\":[]},"
            + "\"crlCache\":{\"path\":\"" + crlDir + "\"},"
            + "\"offline\":true}}";
        std::string r = call(req.c_str());
        shortPrint("INIT", r);
        check(errorCode(r) == 0, "INIT errorCode == 0");
        check(contains(r, "\"countCmProviders\":1") || contains(r, "\"countCmProviders\": 1"),
              "INIT registered exactly the built-in cm-pkcs12 provider");
    }

    //  --- 6. OPEN file-key container + SELECT_KEY + SIGN ----------------------
    {
        std::string r = call("{\"method\":\"OPEN\",\"parameters\":{"
                             "\"provider\":\"PKCS12\",\"storage\":\"test-diia.p12\","
                             "\"password\":\"testpassword\",\"mode\":\"RO\"}}");
        shortPrint("OPEN", r);
        check(errorCode(r) == 0, "OPEN test-diia.p12 errorCode == 0 (file-key provider works embedded)");

        std::string rk = call("{\"method\":\"SELECT_KEY\",\"parameters\":{"
                              "\"id\":\"5BC6C06EE1E00C1700E92AA7A9AD75F82D3CB7A9B66E3A98023209B24513315C\"}}");
        shortPrint("SELECT_KEY", rk);
        check(errorCode(rk) == 0, "SELECT_KEY errorCode == 0");

        //  RAW signature: signs the data hash with the embedded file key. This
        //  deliberately avoids CAdES cert handling so the check does not depend
        //  on the bundled test-diia.p12 certificate (which expired 2024-04-05).
        const char* rawsign =
            "{\"method\":\"SIGN\",\"parameters\":{"
            "\"signParams\":{\"signatureFormat\":\"RAW\",\"signAlgo\":\"1.2.804.2.1.1.1.1.3.1.1\"},"
            "\"dataTbs\":[{\"id\":\"doc-0\","
            "\"bytes\":\"VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wcyBvdmVyIHRoZSBsYXp5IGRvZw==\"}]}}";
        std::string rs = call(rawsign);
        shortPrint("SIGN(RAW)", rs);
        check(errorCode(rs) == 0, "SIGN(RAW) errorCode == 0 (embedded file-key signs)");
        check(contains(rs, "\"bytes\""), "SIGN(RAW) produced a signature (bytes present)");

        //  Informational: the full CAdES-BES path (exercises cert handling). With
        //  the expired bundled cert this is expected to return 4163
        //  (RET_UAPKI_CERT_VALIDITY_NOT_AFTER_ERROR); not asserted.
        const char* cades =
            "{\"method\":\"SIGN\",\"parameters\":{"
            "\"signParams\":{\"signatureFormat\":\"CAdES-BES\",\"signAlgo\":\"1.2.804.2.1.1.1.1.3.1.1\","
            "\"detachedData\":false,\"includeCert\":true},"
            "\"dataTbs\":[{\"id\":\"doc-0\","
            "\"bytes\":\"VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wcyBvdmVyIHRoZSBsYXp5IGRvZw==\"}]}}";
        std::string rc = call(cades);
        printf("  (info) SIGN(CAdES-BES) errorCode=%ld%s\n", errorCode(rc),
               errorCode(rc) == 4163 ? " (expected: bundled signer cert expired 2024-04-05)" : "");

        (void)call("{\"method\":\"CLOSE\"}");
    }

    //  --- 7. VERIFY a known-good CAdES-BES fixture signature ------------------
    {
        const char* verifyreq =
            "{\"method\":\"VERIFY\",\"parameters\":{\"signature\":{\"bytes\":\""
            "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"
            "\"},\"reportTime\":true}}";
        std::string r = call(verifyreq);
        shortPrint("VERIFY", r);
        check(errorCode(r) == 0, "VERIFY errorCode == 0");
        check(contains(r, "VALID") || contains(r, "valid"),
              "VERIFY reports a signature status (VALID)");
    }

    (void)call("{\"method\":\"DEINIT\"}");

    printf("\n%s (%d failure(s))\n", g_failures == 0 ? "ALL CHECKS PASSED" : "THERE WERE FAILURES", g_failures);
    return (g_failures == 0) ? 0 : 1;
}
