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

#include <string.h>
#include "static-api.h"


//  This translation unit is compiled into cm-pkcs12 itself, so it references
//  the provider's own local entry points (defined in main-cm-pkcs12.cpp).
//  No cross-module dllimport / visibility games are needed.
#ifdef __cplusplus
extern "C" {
#endif

CM_ERROR provider_info (CM_JSON_PCHAR* providerInfo);
CM_ERROR provider_init (CM_JSON_PCHAR providerParams);
CM_ERROR provider_deinit (void);
CM_ERROR provider_open (const char* fileName, uint32_t openMode, const CM_JSON_PCHAR openParams, CM_SESSION_API** session);
CM_ERROR provider_close (CM_SESSION_API* session);
void block_free (void* ptr);
void bytearray_free (CM_BYTEARRAY* ba);


void cm_pkcs12_get_static_api (CM_PROVIDER_API* api)
{
    if (!api) return;

    memset(api, 0, sizeof(*api));
    //  required API
    api->info           = provider_info;
    api->init           = provider_init;
    api->deinit         = provider_deinit;
    api->open           = provider_open;
    api->close          = provider_close;
    api->block_free     = block_free;
    api->bytearray_free = bytearray_free;
    //  optional API not implemented by cm-pkcs12 (left as nullptr):
    //  list_storages, storage_info, format
}

#ifdef __cplusplus
}
#endif
