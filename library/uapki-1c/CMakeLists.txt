cmake_minimum_required(VERSION 3.17)

project(uapki-1c LANGUAGES C CXX)
set(UAPKI_1C_VERSION 1.0.0)
message(STATUS "Project: uapki-1c, version: " ${UAPKI_1C_VERSION})

#  The AddIn is a self-contained single binary: it statically links
#  uapki/uapkic/uapkif/cm-pkcs12. In particular it references
#  cm_pkcs12_get_static_api, which is only linkable when cm-pkcs12 is STATIC.
if (NOT (DEFINED UAPKI_LIBS_TYPE AND UAPKI_LIBS_TYPE STREQUAL STATIC))
    message(FATAL_ERROR "uapki-1c requires -DUAPKI_LIBS_TYPE=STATIC")
endif ()
if (NOT (DEFINED UAPKI_CM_PKCS12_LIB_TYPE AND UAPKI_CM_PKCS12_LIB_TYPE STREQUAL STATIC))
    message(FATAL_ERROR "uapki-1c requires -DUAPKI_CM_PKCS12_LIB_TYPE=STATIC")
endif ()

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
    message(STATUS "Build type not specified: Use Release by default")
endif (NOT CMAKE_BUILD_TYPE)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_SKIP_BUILD_RPATH OFF)
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
set(CMAKE_INSTALL_RPATH "\$ORIGIN")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if (${WIN32})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif ()

set(PATH_PRJ ${CMAKE_CURRENT_SOURCE_DIR})
set(PATH_COMMON_CMAPI ${PATH_PRJ}/../common/cm-api)
set(PATH_COMMON_LOADERS ${PATH_PRJ}/../common/loaders)
set(PATH_CMPKCS12_SRC ${PATH_PRJ}/../cm-pkcs12/src)
set(PATH_UAPKI_INCLUDE ${PATH_PRJ}/../uapki/include)

add_library(uapki-1c SHARED
    uapki-1c.cpp
    addin-exports.cpp
    static-providers.cpp
    str-convert.cpp
    dllmain.cpp
)

target_include_directories(uapki-1c PRIVATE
    ${PATH_PRJ}/include-1c
    ${PATH_COMMON_CMAPI}
    ${PATH_COMMON_LOADERS}
    ${PATH_CMPKCS12_SRC}
    ${PATH_UAPKI_INCLUDE}
)

#  Output file name per OS/arch (1C bundle convention).
if (${WIN32})
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set_target_properties(uapki-1c PROPERTIES OUTPUT_NAME "uapki-1cWin64")
    else ()
        set_target_properties(uapki-1c PROPERTIES OUTPUT_NAME "uapki-1cWin32")
    endif ()
elseif (${APPLE})
    set_target_properties(uapki-1c PROPERTIES OUTPUT_NAME "uapki-1cMac")
elseif (${UNIX})
    set(_addin_arch "Lin64")
    if (DEFINED UAPKI_CMAKE_ARCH)
        if (UAPKI_CMAKE_ARCH STREQUAL "arm64")
            set(_addin_arch "LinARM64")
        elseif (UAPKI_CMAKE_ARCH STREQUAL "armv7")
            set(_addin_arch "LinARM")
        elseif (UAPKI_CMAKE_ARCH STREQUAL "x64")
            set(_addin_arch "Lin64")
        endif ()
    endif ()
    set_target_properties(uapki-1c PROPERTIES OUTPUT_NAME "uapki-1c${_addin_arch}")
endif ()

#  MSVC: read the (UTF-8) sources as UTF-8 so the Russian aliases are correct,
#  and export only the five AddIn entry points via the module-definition file.
if (MSVC)
    target_compile_options(uapki-1c PRIVATE /utf-8)
    target_compile_definitions(uapki-1c PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(uapki-1c PROPERTIES LINK_FLAGS "/DEF:\"${PATH_PRJ}/uapki-1c.def\"")
endif ()

if (NOT WIN32)
    set_target_properties(uapki-1c PROPERTIES LINK_FLAGS_RELEASE -s)
endif ()

#  Statically embed the whole UAPKI stack into this one binary.
target_link_libraries(uapki-1c PRIVATE uapki uapkic uapkif cm-pkcs12)
if (${UNIX})
    target_link_libraries(uapki-1c PRIVATE dl)
endif ()
if (${APPLE})
    target_link_libraries(uapki-1c PRIVATE iconv)
endif ()


if (NOT UAPKI_DISABLE_COPY)
    add_custom_command(TARGET uapki-1c POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:uapki-1c> ../out/
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:uapki-1c> ../../${OUT_DIR}/
    )
endif ()


#  Optional standalone verification harness: links the same static stack the
#  AddIn does, registers cm-pkcs12 as a built-in provider, and round-trips
#  process() over the library/test fixtures. Not part of the shipped bundle.
option(UAPKI_BUILD_1C_SELFTEST "Build the uapki-1c verification harness" OFF)
if (UAPKI_BUILD_1C_SELFTEST)
    add_executable(uapki-1c-selftest
        selftest/selftest.cpp
        str-convert.cpp
    )
    target_include_directories(uapki-1c-selftest PRIVATE
        ${PATH_PRJ}
        ${PATH_COMMON_CMAPI}
        ${PATH_COMMON_LOADERS}
        ${PATH_CMPKCS12_SRC}
    )
    if (MSVC)
        target_compile_options(uapki-1c-selftest PRIVATE /utf-8)
        target_compile_definitions(uapki-1c-selftest PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif ()
    target_link_libraries(uapki-1c-selftest PRIVATE uapki uapkic uapkif cm-pkcs12)
    if (${UNIX})
        target_link_libraries(uapki-1c-selftest PRIVATE dl)
    endif ()
    if (${APPLE})
        target_link_libraries(uapki-1c-selftest PRIVATE iconv)
    endif ()
endif ()
