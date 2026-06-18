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

#include <cstdint>
#include <string>
#include "ComponentBase.h"
#include "uapki-1c.h"
#include "static-providers.h"


//  The one class this component registers (BSL: New("AddIn.<name>.Uapki")).
static const WCHAR_T   g_classNames[] = u"Uapki";
static std::u16string  s_classNames(g_classNames);


//  These five C functions are the platform's entry points into a native AddIn.
//  On Windows they are exported via uapki-1c.def; on Unix via default visibility.

long GetClassObject (const WCHAR_T* /*wsName*/, IComponentBase** pInterface)
{
    if (!pInterface) return 0;
    if (!*pInterface) {
        //  Make the statically-linked cm-pkcs12 provider available before any
        //  INIT request reaches uapki via process().
        uapki1c_register_builtin_providers();
        *pInterface = new Uapki1C;
        return (long)(intptr_t)(*pInterface);
    }
    return 0;
}

long DestroyObject (IComponentBase** pIntf)
{
    if (!pIntf || !*pIntf) return -1;

    delete *pIntf;
    *pIntf = nullptr;
    return 0;
}

const WCHAR_T* GetClassNames (void)
{
    return s_classNames.c_str();
}

AppCapabilities SetPlatformCapabilities (const AppCapabilities /*capabilities*/)
{
    //  Report the highest capability level we understand.
    return eAppCapabilitiesLast;
}

AttachType GetAttachType (void)
{
    //  File-key crypto works in or out of process; let 1C choose (default since 8.3.21).
    return eCanAttachAny;
}
