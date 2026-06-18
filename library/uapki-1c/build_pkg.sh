#!/usr/bin/env bash
#
# Builds the self-contained UAPKI 1C:Enterprise native AddIn for the available
# architectures and packages them into a 1C AddIn bundle (Manifest.xml + the
# per-arch binaries, zipped).
#
# Usage:
#   ./build_pkg.sh                 # build for the host OS (Windows: x64 + x86)
#   ./build_pkg.sh --no-build      # only (re)package binaries already in ./pkg
#
# Result: ./uapki-1c-<version>.zip  containing Manifest.xml and the binaries.
#
# The build is fully static (uapki/uapkic/uapkif/cm-pkcs12 linked in; on Windows
# the static CRT and the vendored static libcurl too), so each binary is a single
# self-contained file with no external runtime dependencies.

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LIBROOT="$(cd "${HERE}/.." && pwd)"
VERSION="$(sed -n 's/^set(UAPKI_1C_VERSION \([0-9.]*\)).*/\1/p' "${HERE}/CMakeLists.txt" | head -1)"
VERSION="${VERSION:-1.0.0}"

PKG_DIR="${HERE}/pkg"
CMAKE="${CMAKE:-cmake}"

COMMON_FLAGS=(
    -DUAPKI_BUILD_1C_ADDIN=ON
    -DUAPKI_LIBS_TYPE=STATIC
    -DUAPKI_CM_PKCS12_LIB_TYPE=STATIC
    -DUAPKI_DISABLE_COPY=ON
)

# Collect a freshly built artifact (named <base>) from a build dir into ./pkg.
collect () {
    local build_dir="$1" base="$2"
    local found
    found="$(find "${build_dir}" -name "${base}" -type f 2>/dev/null | head -1 || true)"
    if [ -n "${found}" ]; then
        cp -f "${found}" "${PKG_DIR}/${base}"
        echo "  collected ${base}"
    else
        echo "  WARN: ${base} not found under ${build_dir}"
    fi
}

build_windows () {
    # x64
    echo "== building Windows x86_64 =="
    "${CMAKE}" -G "Visual Studio 17 2022" -A x64 -S "${LIBROOT}" -B "${LIBROOT}/build-1c-x64" \
        "${COMMON_FLAGS[@]}" \
        "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded\$<\$<CONFIG:Debug>:Debug>"
    "${CMAKE}" --build "${LIBROOT}/build-1c-x64" --config Release --target uapki-1c
    collect "${LIBROOT}/build-1c-x64" "uapki-1cWin64.dll"

    # x86
    echo "== building Windows i386 =="
    "${CMAKE}" -G "Visual Studio 17 2022" -A Win32 -S "${LIBROOT}" -B "${LIBROOT}/build-1c-x86" \
        "${COMMON_FLAGS[@]}" \
        "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded\$<\$<CONFIG:Debug>:Debug>"
    "${CMAKE}" --build "${LIBROOT}/build-1c-x86" --config Release --target uapki-1c
    collect "${LIBROOT}/build-1c-x86" "uapki-1cWin32.dll"
}

build_unix () {
    local arch_flag="$1" out_name="$2" build_dir="$3"
    echo "== building ${out_name} =="
    "${CMAKE}" -S "${LIBROOT}" -B "${build_dir}" "${COMMON_FLAGS[@]}" ${arch_flag} -DCMAKE_BUILD_TYPE=Release
    "${CMAKE}" --build "${build_dir}" --config Release --target uapki-1c -j 4
    collect "${build_dir}" "${out_name}"
}

generate_manifest () {
    local manifest="${PKG_DIR}/Manifest.xml"
    {
        echo '<?xml version="1.0" encoding="UTF-8"?>'
        echo '<bundle xmlns="http://v8.1c.ru/8.2/addin/bundle">'
        [ -f "${PKG_DIR}/uapki-1cWin32.dll" ]      && echo '  <component os="Windows" path="uapki-1cWin32.dll" type="native" arch="i386"/>'
        [ -f "${PKG_DIR}/uapki-1cWin64.dll" ]      && echo '  <component os="Windows" path="uapki-1cWin64.dll" type="native" arch="x86_64"/>'
        [ -f "${PKG_DIR}/libuapki-1cLin64.so" ]    && echo '  <component os="Linux" path="libuapki-1cLin64.so" type="native" arch="x86_64"/>'
        [ -f "${PKG_DIR}/libuapki-1cLinARM64.so" ] && echo '  <component os="Linux" path="libuapki-1cLinARM64.so" type="native" arch="ARM64"/>'
        [ -f "${PKG_DIR}/libuapki-1cMac.dylib" ]   && echo '  <component os="MacOS" path="libuapki-1cMac.dylib" type="native" arch="x86_64"/>'
        echo '</bundle>'
    } > "${manifest}"
    echo "  wrote Manifest.xml"

    # Validate against the SDK schema if xmllint and the schema are available.
    local xsd="${HERE}/MANIFEST.xsd"
    if command -v xmllint >/dev/null 2>&1 && [ -f "${xsd}" ]; then
        if xmllint --noout --schema "${xsd}" "${manifest}" 2>/dev/null; then
            echo "  Manifest.xml validates against MANIFEST.xsd"
        else
            echo "  WARN: Manifest.xml did not validate against MANIFEST.xsd"
        fi
    fi
}

main () {
    mkdir -p "${PKG_DIR}"

    if [ "${1:-}" != "--no-build" ]; then
        case "$(uname -s)" in
            MINGW*|MSYS*|CYGWIN*) build_windows ;;
            Linux)  build_unix "-DUAPKI_CMAKE_ARCH=x64" "libuapki-1cLin64.so" "${LIBROOT}/build-1c-lin64" ;;
            Darwin) build_unix "-DCMAKE_OSX_ARCHITECTURES=x86_64" "libuapki-1cMac.dylib" "${LIBROOT}/build-1c-mac" ;;
            *) echo "Unsupported host OS: $(uname -s)"; exit 1 ;;
        esac
    fi

    generate_manifest

    local zip="${HERE}/uapki-1c-${VERSION}.zip"
    rm -f "${zip}"
    ( cd "${PKG_DIR}" && zip -j -X "${zip}" Manifest.xml ./*.dll ./*.so ./*.dylib 2>/dev/null || \
      ( cd "${PKG_DIR}" && zip -j -X "${zip}" Manifest.xml $(ls ./*.dll ./*.so ./*.dylib 2>/dev/null) ) )
    echo "Package: ${zip}"
    unzip -l "${zip}" || true
}

main "$@"
