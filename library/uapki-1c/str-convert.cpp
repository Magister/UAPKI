/*
 * Copyright (c) 2021, The UAPKI Project Authors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "str-convert.h"

#if defined(_WIN32) || defined(__WINDOWS__)
#include <windows.h>
#else
#include <iconv.h>
#include <cstdlib>
#include <cstring>
#endif


namespace uapki1c {


#if defined(_WIN32) || defined(__WINDOWS__)

std::u16string utf8ToUtf16 (const std::string& in)
{
    if (in.empty()) return std::u16string();

    const int wlen = MultiByteToWideChar(CP_UTF8, 0, in.data(), (int)in.size(), nullptr, 0);
    if (wlen <= 0) return std::u16string();

    std::u16string out((size_t)wlen, u'\0');
    //  wchar_t is UTF-16LE on Windows, bit-for-bit identical to char16_t.
    MultiByteToWideChar(CP_UTF8, 0, in.data(), (int)in.size(), (wchar_t*)&out[0], wlen);
    return out;
}

std::string utf16ToUtf8 (const char16_t* in, std::size_t len)
{
    if (!in || (len == 0)) return std::string();

    const int blen = WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)in, (int)len, nullptr, 0, nullptr, nullptr);
    if (blen <= 0) return std::string();

    std::string out((size_t)blen, '\0');
    WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)in, (int)len, &out[0], blen, nullptr, nullptr);
    return out;
}

#else   //  POSIX (Linux / macOS) via iconv

static std::string iconvConvert (const char* fromCode, const char* toCode, const char* in, std::size_t inBytes)
{
    if (!in || (inBytes == 0)) return std::string();

    iconv_t cd = iconv_open(toCode, fromCode);
    if (cd == (iconv_t)-1) return std::string();

    //  Worst case UTF-8<->UTF-16 expansion stays within 2x + headroom.
    std::size_t outCap = inBytes * 2 + 4;
    std::string out(outCap, '\0');

    char* inPtr = const_cast<char*>(in);
    std::size_t inLeft = inBytes;
    char* outPtr = &out[0];
    std::size_t outLeft = outCap;

    const std::size_t rc = iconv(cd, &inPtr, &inLeft, &outPtr, &outLeft);
    iconv_close(cd);

    if (rc == (std::size_t)-1) return std::string();

    out.resize(outCap - outLeft);
    return out;
}

std::u16string utf8ToUtf16 (const std::string& in)
{
    const std::string bytes = iconvConvert("UTF-8", "UTF-16LE", in.data(), in.size());
    std::u16string out(bytes.size() / sizeof(char16_t), u'\0');
    if (!out.empty()) {
        std::memcpy(&out[0], bytes.data(), out.size() * sizeof(char16_t));
    }
    return out;
}

std::string utf16ToUtf8 (const char16_t* in, std::size_t len)
{
    return iconvConvert("UTF-16LE", "UTF-8", (const char*)in, len * sizeof(char16_t));
}

#endif


}   //  namespace uapki1c
