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

#ifndef UAPKI_1C_H
#define UAPKI_1C_H


#include "ComponentBase.h"
#include "AddInDefBase.h"
#include "IMemoryManager.h"
#include <string>


//  1C:Enterprise native component that exposes UAPKI through a thin JSON
//  passthrough. One class "Uapki" with:
//    - method  Process(jsonRequest) -> jsonResponse   (mirrors uapki process())
//    - property Version   (read-only)  - the component version string
//    - property LastError (read-only)  - last transport/marshaling error, if any
class Uapki1C : public IComponentBase
{
public:
    enum Props {
        ePropVersion = 0,
        ePropLastError,
        ePropLast       //  always last
    };

    enum Methods {
        eMethProcess = 0,
        eMethLast       //  always last
    };

    Uapki1C (void);
    virtual ~Uapki1C ();

    //  IInitDoneBase
    virtual bool ADDIN_API Init (void* disp) override;
    virtual bool ADDIN_API setMemManager (void* mem) override;
    virtual long ADDIN_API GetInfo () override;
    virtual void ADDIN_API Done () override;

    //  ILanguageExtenderBase
    virtual bool ADDIN_API RegisterExtensionAs (WCHAR_T** wsExtensionName) override;
    virtual long ADDIN_API GetNProps () override;
    virtual long ADDIN_API FindProp (const WCHAR_T* wsPropName) override;
    virtual const WCHAR_T* ADDIN_API GetPropName (long lPropNum, long lPropAlias) override;
    virtual bool ADDIN_API GetPropVal (const long lPropNum, tVariant* pvarPropVal) override;
    virtual bool ADDIN_API SetPropVal (const long lPropNum, tVariant* varPropVal) override;
    virtual bool ADDIN_API IsPropReadable (const long lPropNum) override;
    virtual bool ADDIN_API IsPropWritable (const long lPropNum) override;
    virtual long ADDIN_API GetNMethods () override;
    virtual long ADDIN_API FindMethod (const WCHAR_T* wsMethodName) override;
    virtual const WCHAR_T* ADDIN_API GetMethodName (const long lMethodNum, const long lMethodAlias) override;
    virtual long ADDIN_API GetNParams (const long lMethodNum) override;
    virtual bool ADDIN_API GetParamDefValue (const long lMethodNum, const long lParamNum, tVariant* pvarParamDefValue) override;
    virtual bool ADDIN_API HasRetVal (const long lMethodNum) override;
    virtual bool ADDIN_API CallAsProc (const long lMethodNum, tVariant* paParams, const long lSizeArray) override;
    virtual bool ADDIN_API CallAsFunc (const long lMethodNum, tVariant* pvarRetValue, tVariant* paParams, const long lSizeArray) override;

    //  LocaleBase
    virtual void ADDIN_API SetLocale (const WCHAR_T* loc) override;

    //  UserLanguageBase
    virtual void ADDIN_API SetUserInterfaceLanguageCode (const WCHAR_T* lang) override;

private:
    //  Runs uapki process() on the (single) string parameter. Returns RET_OK
    //  and fills outResponse (UTF-8) on success; otherwise sets LastError.
    bool invokeProcess (tVariant* paParams, const long lSizeArray, std::string& outResponse);

    //  Allocates a 1C-owned UTF-16 string into dst (VTYPE_PWSTR).
    bool returnU16String (tVariant* dst, const std::u16string& s);
    bool returnUtf8String (tVariant* dst, const std::string& utf8);
    //  Allocates a 1C-owned copy of a name (for GetPropName/GetMethodName).
    WCHAR_T* allocName (const char16_t* name);

    void setLastError (const std::string& utf8);
    void addError (uint16_t wcode, const char16_t* descr, long scode);

    IAddInDefBase*   m_iConnect;
    IMemoryManager*  m_iMemory;
    std::u16string   m_lastError;   //  UTF-16
    std::u16string   m_userLang;    //  UTF-16
};

#endif
