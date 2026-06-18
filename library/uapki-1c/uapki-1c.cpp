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

#include "uapki-1c.h"
#include "str-convert.h"
#include <cstring>


//  UAPKI's entire public surface. Declared plainly here (rather than via
//  uapki-export.h) so the AddIn links the statically-linked uapki cleanly,
//  with no __declspec(dllimport) decoration.
extern "C" char* process (const char* request);
extern "C" void  json_free (char* buf);


using namespace uapki1c;


//  Component (wrapper) version, exposed via the read-only "Version" property.
//  The underlying UAPKI version is available through Process({"method":"VERSION"}).
static const char16_t  UAPKI_1C_VERSION[] = u"1.0.0";
static const char16_t  ERR_SOURCE[]       = u"AddIn.UAPKI";

//  Property / method names. Index 0 = international (English), 1 = Russian.
//  The Cyrillic aliases are plain u"" literals (this source is UTF-8); the
//  build tells MSVC so with /utf-8 (GCC/Clang handle UTF-8 source natively).
static const char16_t* const PROP_NAMES[2][Uapki1C::ePropLast] = {
    { u"Version", u"LastError" },
    { u"Версия",  u"ПоследняяОшибка" }
};
static const char16_t* const METHOD_NAMES[2][Uapki1C::eMethLast] = {
    { u"Process" },
    { u"Обработать" }
};


//  -- small UTF-16 helpers (no dependency on platform wide-char functions) -----

static size_t u16len (const char16_t* s)
{
    size_t n = 0;
    if (s) { while (s[n]) ++n; }
    return n;
}

//  Case-folding for ASCII and Russian Cyrillic, so method/property lookups are
//  case-insensitive the way BSL identifiers are.
static char16_t u16fold (char16_t c)
{
    if (c >= u'A' && c <= u'Z') return (char16_t)(c + 0x20);            //  ASCII
    if (c >= 0x0410 && c <= 0x042F) return (char16_t)(c + 0x20);        //  А..Я -> а..я
    if (c == 0x0401) return 0x0451;                                     //  Ё -> ё
    return c;
}

static bool u16iequals (const char16_t* a, const char16_t* b)
{
    if (!a || !b) return false;
    for (; *a && *b; ++a, ++b) {
        if (u16fold(*a) != u16fold(*b)) return false;
    }
    return (*a == 0) && (*b == 0);
}


//  ---------------------------------------------------------------------------//

Uapki1C::Uapki1C (void)
    : m_iConnect(nullptr)
    , m_iMemory(nullptr)
{
}

Uapki1C::~Uapki1C ()
{
}

//  IInitDoneBase
//  ---------------------------------------------------------------------------//
bool Uapki1C::Init (void* disp)
{
    m_iConnect = (IAddInDefBase*)disp;
    return (m_iConnect != nullptr);
}

bool Uapki1C::setMemManager (void* mem)
{
    m_iMemory = (IMemoryManager*)mem;
    return (m_iMemory != nullptr);
}

long Uapki1C::GetInfo (void)
{
    //  2.0 component technology version.
    return 2000;
}

void Uapki1C::Done (void)
{
}

//  ILanguageExtenderBase
//  ---------------------------------------------------------------------------//
bool Uapki1C::RegisterExtensionAs (WCHAR_T** wsExtensionName)
{
    static const char16_t name[] = u"Uapki";
    if (!m_iMemory || !wsExtensionName) return false;

    const size_t bytes = (u16len(name) + 1) * sizeof(WCHAR_T);
    if (!m_iMemory->AllocMemory((void**)wsExtensionName, (unsigned long)bytes)) return false;
    memcpy(*wsExtensionName, name, bytes);
    return true;
}

long Uapki1C::GetNProps (void)
{
    return ePropLast;
}

long Uapki1C::FindProp (const WCHAR_T* wsPropName)
{
    for (long i = 0; i < ePropLast; i++) {
        if (u16iequals(wsPropName, PROP_NAMES[0][i]) || u16iequals(wsPropName, PROP_NAMES[1][i])) {
            return i;
        }
    }
    return -1;
}

const WCHAR_T* Uapki1C::GetPropName (long lPropNum, long lPropAlias)
{
    if ((lPropNum < 0) || (lPropNum >= ePropLast)) return nullptr;
    if ((lPropAlias != 0) && (lPropAlias != 1)) return nullptr;
    return allocName(PROP_NAMES[lPropAlias][lPropNum]);
}

bool Uapki1C::GetPropVal (const long lPropNum, tVariant* pvarPropVal)
{
    switch (lPropNum) {
    case ePropVersion:
        return returnU16String(pvarPropVal, std::u16string(UAPKI_1C_VERSION));
    case ePropLastError:
        return returnU16String(pvarPropVal, m_lastError);
    default:
        return false;
    }
}

bool Uapki1C::SetPropVal (const long /*lPropNum*/, tVariant* /*varPropVal*/)
{
    //  Both properties are read-only.
    return false;
}

bool Uapki1C::IsPropReadable (const long lPropNum)
{
    return (lPropNum == ePropVersion) || (lPropNum == ePropLastError);
}

bool Uapki1C::IsPropWritable (const long /*lPropNum*/)
{
    return false;
}

long Uapki1C::GetNMethods (void)
{
    return eMethLast;
}

long Uapki1C::FindMethod (const WCHAR_T* wsMethodName)
{
    for (long i = 0; i < eMethLast; i++) {
        if (u16iequals(wsMethodName, METHOD_NAMES[0][i]) || u16iequals(wsMethodName, METHOD_NAMES[1][i])) {
            return i;
        }
    }
    return -1;
}

const WCHAR_T* Uapki1C::GetMethodName (const long lMethodNum, const long lMethodAlias)
{
    if ((lMethodNum < 0) || (lMethodNum >= eMethLast)) return nullptr;
    if ((lMethodAlias != 0) && (lMethodAlias != 1)) return nullptr;
    return allocName(METHOD_NAMES[lMethodAlias][lMethodNum]);
}

long Uapki1C::GetNParams (const long lMethodNum)
{
    switch (lMethodNum) {
    case eMethProcess:
        return 1;
    default:
        return 0;
    }
}

bool Uapki1C::GetParamDefValue (const long /*lMethodNum*/, const long /*lParamNum*/, tVariant* pvarParamDefValue)
{
    if (pvarParamDefValue) {
        TV_VT(pvarParamDefValue) = VTYPE_EMPTY;
    }
    //  No default values; the request parameter is required.
    return false;
}

bool Uapki1C::HasRetVal (const long lMethodNum)
{
    return (lMethodNum == eMethProcess);
}

bool Uapki1C::CallAsProc (const long lMethodNum, tVariant* paParams, const long lSizeArray)
{
    if (lMethodNum != eMethProcess) return false;

    std::string response;
    return invokeProcess(paParams, lSizeArray, response);
}

bool Uapki1C::CallAsFunc (const long lMethodNum, tVariant* pvarRetValue, tVariant* paParams, const long lSizeArray)
{
    if (lMethodNum != eMethProcess) return false;

    std::string response;
    if (!invokeProcess(paParams, lSizeArray, response)) return false;

    return returnUtf8String(pvarRetValue, response);
}

//  LocaleBase
//  ---------------------------------------------------------------------------//
void Uapki1C::SetLocale (const WCHAR_T* /*loc*/)
{
}

//  UserLanguageBase
//  ---------------------------------------------------------------------------//
void Uapki1C::SetUserInterfaceLanguageCode (const WCHAR_T* lang)
{
    m_userLang.assign(lang ? lang : u"");
}

//  ---------------------------------------------------------------------------//
//  internals
//  ---------------------------------------------------------------------------//
bool Uapki1C::invokeProcess (tVariant* paParams, const long lSizeArray, std::string& outResponse)
{
    m_lastError.clear();

    if ((lSizeArray < 1) || !paParams) {
        addError(ADDIN_E_VERY_IMPORTANT, u"Process: missing request parameter", -1);
        return false;
    }

    std::string request;
    tVariant* p0 = &paParams[0];
    switch (TV_VT(p0)) {
    case VTYPE_PWSTR:
        if (p0->pwstrVal) request = utf16ToUtf8(p0->pwstrVal, p0->wstrLen);
        break;
    case VTYPE_PSTR:
        if (p0->pstrVal) request.assign(p0->pstrVal, p0->strLen);
        break;
    default:
        addError(ADDIN_E_VERY_IMPORTANT, u"Process: request parameter must be a string", -1);
        return false;
    }

    char* resp = process(request.c_str());
    if (!resp) {
        addError(ADDIN_E_VERY_IMPORTANT, u"Process: uapki returned no response", -1);
        return false;
    }

    outResponse.assign(resp);
    json_free(resp);
    return true;
}

bool Uapki1C::returnU16String (tVariant* dst, const std::u16string& s)
{
    if (!dst) return false;
    if (!m_iMemory) {
        TV_VT(dst) = VTYPE_EMPTY;
        return false;
    }

    const size_t n = s.size();
    WCHAR_T* buf = nullptr;
    if (!m_iMemory->AllocMemory((void**)&buf, (unsigned long)((n + 1) * sizeof(WCHAR_T)))) {
        TV_VT(dst) = VTYPE_EMPTY;
        return false;
    }

    if (n) memcpy(buf, s.data(), n * sizeof(WCHAR_T));
    buf[n] = 0;

    TV_VT(dst) = VTYPE_PWSTR;
    dst->pwstrVal = buf;
    dst->wstrLen = (uint32_t)n;
    return true;
}

bool Uapki1C::returnUtf8String (tVariant* dst, const std::string& utf8)
{
    return returnU16String(dst, utf8ToUtf16(utf8));
}

WCHAR_T* Uapki1C::allocName (const char16_t* name)
{
    if (!m_iMemory || !name) return nullptr;

    const size_t bytes = (u16len(name) + 1) * sizeof(WCHAR_T);
    WCHAR_T* buf = nullptr;
    if (!m_iMemory->AllocMemory((void**)&buf, (unsigned long)bytes)) return nullptr;
    memcpy(buf, name, bytes);
    return buf;
}

void Uapki1C::setLastError (const std::string& utf8)
{
    m_lastError = utf8ToUtf16(utf8);
}

void Uapki1C::addError (uint16_t wcode, const char16_t* descr, long scode)
{
    m_lastError.assign(descr ? descr : u"");
    if (m_iConnect) {
        m_iConnect->AddError(wcode, ERR_SOURCE, descr, scode);
    }
}
