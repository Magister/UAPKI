# UAPKI — 1C:Enterprise native AddIn

A self-contained 1C:Enterprise **native AddIn** that exposes the full UAPKI
PKI/crypto surface (sign, verify, hash, certificate/CRL/OCSP validation,
file-key operations, …) to BSL code.

Everything is bundled into **one binary per platform/arch** — `uapki`, `uapkic`,
`uapkif` and the `cm-pkcs12` file-key provider are statically linked in (on
Windows the static CRT and the vendored static libcurl too). There is nothing
else to install or copy next to it.

## The API surface

The component registers one class, `Uapki`, with a single method plus two
read-only properties. It is a thin 1:1 passthrough to UAPKI's `process()` JSON
API, so the complete UAPKI feature set is available on day one and never drifts.

| Member | Kind | Description |
|--------|------|-------------|
| `Process(jsonRequest)` | function → string | Sends a UAPKI JSON request, returns the JSON response. |
| `Version` | property (read-only) | The component (wrapper) version. |
| `LastError` | property (read-only) | Last transport/marshaling error, empty on success. |

Russian aliases: `Обработать`, `Версия`, `ПоследняяОшибка`.

The request/response JSON is exactly UAPKI's — see `library/test/data/*.json` for
ready-made fixtures (INIT, DIGEST, SIGN, VERIFY, key operations, …).

## BSL usage

```bsl
// Attach the bundle (.zip produced by build_pkg.sh) and instantiate the class.
ПодключитьВнешнююКомпоненту("...путь/uapki-1c.zip", "UAPKI", ТипВнешнейКомпоненты.Native);
Crypto = Новый("AddIn.UAPKI.Uapki");

// VERSION
Ответ = Crypto.Process("{""method"":""VERSION""}");

// INIT — note "lib":"cm-pkcs12" resolves to the *built-in* provider; no
// cm-pkcs12.dll/.so needs to exist on disk.
ЗапросINIT =
"{""method"":""INIT"",""parameters"":{
   ""cmProviders"":{""dir"":"""",""allowedProviders"":[{""lib"":""cm-pkcs12""}]},
   ""certCache"":{""path"":""certs/"",""trustedCerts"":[]},
   ""crlCache"":{""path"":""crls/""},
   ""offline"":false}}";
Ответ = Crypto.Process(ЗапросINIT);

// Open a PKCS#12 key, select it, sign:
Crypto.Process("{""method"":""OPEN"",""parameters"":{""storage"":""key.p12"",""password"":""...""}}");
Crypto.Process("{""method"":""SELECT_KEY"",""parameters"":{""id"":""<keyId>""}}");
Подпись = Crypto.Process("{""method"":""SIGN"",""parameters"":{ ... }}");

Если Не ПустаяСтрока(Crypto.LastError) Тогда
    Сообщить("UAPKI: " + Crypto.LastError);
КонецЕсли;
```

`Process` returns the raw UAPKI JSON; parse it in BSL. UAPKI signals operation
results through the `errorCode` field of that JSON — `LastError` is only set for
transport-level problems (bad parameter, no response).

## Building

The component is opt-in and requires a fully static build:

```bash
cmake -S library -B build \
  -DUAPKI_BUILD_1C_ADDIN=ON \
  -DUAPKI_LIBS_TYPE=STATIC -DUAPKI_CM_PKCS12_LIB_TYPE=STATIC \
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded        # Windows: static CRT
cmake --build build --config Release --target uapki-1c
```

Output names follow the 1C bundle convention:

| OS / arch | file |
|-----------|------|
| Windows x86_64 | `uapki-1cWin64.dll` |
| Windows i386 | `uapki-1cWin32.dll` |
| Linux x86_64 | `libuapki-1cLin64.so` |
| Linux ARM64 | `libuapki-1cLinARM64.so` |
| macOS x86_64 | `libuapki-1cMac.dylib` |

### Packaging

`./build_pkg.sh` builds the host-platform binaries (on Windows: both x64 and
x86), generates `Manifest.xml`, validates it against `MANIFEST.xsd`, and zips a
ready-to-attach 1C AddIn bundle.

### Verification harness

Configure additionally with `-DUAPKI_BUILD_1C_SELFTEST=ON` to build
`uapki-1c-selftest`, a standalone program that registers cm-pkcs12 as a built-in
provider and round-trips `process()` (VERSION/DIGEST/INIT/OPEN/SIGN/VERIFY)
against the `library/test/data` fixtures — proving the single binary works with
no external provider on disk. Run it from the data dir:

```bash
./uapki-1c-selftest library/test/data
```

## How the single binary works

UAPKI normally `dlopen()`s a `cm-pkcs12` shared object. Here, `cm-pkcs12` is
linked in statically and registers itself as a **built-in provider** before the
first INIT:

- `cm_pkcs12_get_static_api()` (in cm-pkcs12) fills a `CM_PROVIDER_API` from the
  provider's own entry points;
- `CmLoader::registerStaticProvider("cm-pkcs12", api)` records it;
- `CmLoader::load("cm-pkcs12")` returns the built-in API instead of `dlopen`ing.

INIT still names `"lib":"cm-pkcs12"` exactly as before — it now resolves to the
embedded copy.

## Notes

- **Offline mode**: pass `"offline":true` to INIT to bypass all network access
  (OCSP/CRL/TSP); offline sign/verify needs no connectivity.
- **Isolation**: `GetAttachType()` returns `eCanAttachAny`, so 1C may run the
  component out-of-process (its default for native components since 8.3.21).
  File-key crypto works fine isolated.
- The vendored 1C SDK headers live in `include-1c/` (from the 8.3.23 SDK,
  `WCHAR_T` = `char16_t`). On Linux, building requires `uuid/uuid.h`
  (`uuid-dev` / `libuuid-devel`).
