/*
 * MIME OLE International interface
 *
 * Copyright 2008 Huw Davies for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define COBJMACROS
#define NONAMELESSUNION

#include <stdarg.h>
#include <stdio.h>

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winnls.h"
#include "objbase.h"
#include "ole2.h"
#include "mimeole.h"
#include "mlang.h"

#include "wine/list.h"
#include "wine/debug.h"

#include "inetcomm_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(inetcomm);

typedef struct
{
    struct list entry;
    INETCSETINFO cs_info;
} charset_entry;

typedef struct
{
    const IMimeInternationalVtbl *lpVtbl;
    LONG refs;
    CRITICAL_SECTION cs;

    struct list charsets;
    LONG next_charset_handle;
    HCHARSET default_charset;
} internat;

static inline internat *impl_from_IMimeInternational( IMimeInternational *iface )
{
    return (internat *)((char*)iface - FIELD_OFFSET(internat, lpVtbl));
}

static inline HRESULT get_mlang(IMultiLanguage **ml)
{
    return CoCreateInstance(&CLSID_CMultiLanguage, NULL,  CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER,
                            &IID_IMultiLanguage, (void **)ml);
}

static HRESULT WINAPI MimeInternat_QueryInterface( IMimeInternational *iface, REFIID riid, LPVOID *ppobj )
{
    if (IsEqualGUID(riid, &IID_IUnknown) ||
        IsEqualGUID(riid, &IID_IMimeInternational))
    {
        IMimeInternational_AddRef( iface );
        *ppobj = iface;
        return S_OK;
    }

    FIXME("interface %s not implemented\n", debugstr_guid(riid));
    return E_NOINTERFACE;
}

static ULONG WINAPI MimeInternat_AddRef( IMimeInternational *iface )
{
    internat *This = impl_from_IMimeInternational( iface );
    return InterlockedIncrement(&This->refs);
}

static ULONG WINAPI MimeInternat_Release( IMimeInternational *iface )
{
    internat *This = impl_from_IMimeInternational( iface );
    ULONG refs;

    refs = InterlockedDecrement(&This->refs);
    if (!refs)
    {
        charset_entry *charset, *cursor2;

        LIST_FOR_EACH_ENTRY_SAFE(charset, cursor2, &This->charsets, charset_entry, entry)
        {
            list_remove(&charset->entry);
            HeapFree(GetProcessHeap(), 0, charset);
        }
        HeapFree(GetProcessHeap(), 0, This);
    }

    return refs;
}

static HRESULT WINAPI MimeInternat_SetDefaultCharset(IMimeInternational *iface, HCHARSET hCharset)
{
    internat *This = impl_from_IMimeInternational( iface );

    TRACE("(%p)->(%p)\n", iface, hCharset);

    if(hCharset == NULL) return E_INVALIDARG;
    /* FIXME check hCharset is valid */

    InterlockedExchangePointer(&This->default_charset, hCharset);

    return S_OK;
}

static HRESULT WINAPI MimeInternat_GetDefaultCharset(IMimeInternational *iface, LPHCHARSET phCharset)
{
    internat *This = impl_from_IMimeInternational( iface );
    HRESULT hr = S_OK;

    TRACE("(%p)->(%p)\n", iface, phCharset);

    if(This->default_charset == NULL)
    {
        HCHARSET hcs;
        hr = IMimeInternational_GetCodePageCharset(iface, GetACP(), CHARSET_BODY, &hcs);
        if(SUCCEEDED(hr))
            InterlockedCompareExchangePointer(&This->default_charset, hcs, NULL);
    }
    *phCharset = This->default_charset;

    return hr;
}

static HRESULT mlang_getcodepageinfo(UINT cp, MIMECPINFO *mlang_cp_info)
{
    HRESULT hr;
    IMultiLanguage *ml;

    hr = get_mlang(&ml);

    if(SUCCEEDED(hr))
    {
        hr = IMultiLanguage_GetCodePageInfo(ml, cp, mlang_cp_info);
        IMultiLanguage_Release(ml);
    }
    return hr;
}

static HRESULT WINAPI MimeInternat_GetCodePageCharset(IMimeInternational *iface, CODEPAGEID cpiCodePage,
                                                      CHARSETTYPE ctCsetType,
                                                      LPHCHARSET phCharset)
{
    HRESULT hr;
    MIMECPINFO mlang_cp_info;

    TRACE("(%p)->(%d, %d, %p)\n", iface, cpiCodePage, ctCsetType, phCharset);

    *phCharset = NULL;

    if(ctCsetType < CHARSET_BODY || ctCsetType > CHARSET_WEB)
        return MIME_E_INVALID_CHARSET_TYPE;

    hr = mlang_getcodepageinfo(cpiCodePage, &mlang_cp_info);
    if(SUCCEEDED(hr))
    {
        const WCHAR *charset_nameW = NULL;
        char *charset_name;
        DWORD len;

        switch(ctCsetType)
        {
        case CHARSET_BODY:
            charset_nameW = mlang_cp_info.wszBodyCharset;
            break;
        case CHARSET_HEADER:
            charset_nameW = mlang_cp_info.wszHeaderCharset;
            break;
        case CHARSET_WEB:
            charset_nameW = mlang_cp_info.wszWebCharset;
            break;
        }
        len = WideCharToMultiByte(CP_ACP, 0, charset_nameW, -1, NULL, 0, NULL, NULL);
        charset_name = HeapAlloc(GetProcessHeap(), 0, len);
        WideCharToMultiByte(CP_ACP, 0, charset_nameW, -1, charset_name, len, NULL, NULL);
        hr = IMimeInternational_FindCharset(iface, charset_name, phCharset);
        HeapFree(GetProcessHeap(), 0, charset_name);
    }
    return hr;
}

static HRESULT mlang_getcsetinfo(const char *charset, MIMECSETINFO *mlang_info)
{
    DWORD len = MultiByteToWideChar(CP_ACP, 0, charset, -1, NULL, 0);
    BSTR bstr = SysAllocStringLen(NULL, len - 1);
    HRESULT hr;
    IMultiLanguage *ml;

    MultiByteToWideChar(CP_ACP, 0, charset, -1, bstr, len);

    hr = get_mlang(&ml);

    if(SUCCEEDED(hr))
    {
        hr = IMultiLanguage_GetCharsetInfo(ml, bstr, mlang_info);
        IMultiLanguage_Release(ml);
    }
    SysFreeString(bstr);
    if(FAILED(hr)) hr = MIME_E_NOT_FOUND;
    return hr;
}

static HCHARSET add_charset(struct list *list, MIMECSETINFO *mlang_info, HCHARSET handle)
{
    charset_entry *charset = HeapAlloc(GetProcessHeap(), 0, sizeof(*charset));

    WideCharToMultiByte(CP_ACP, 0, mlang_info->wszCharset, -1,
                        charset->cs_info.szName, sizeof(charset->cs_info.szName), NULL, NULL);
    charset->cs_info.cpiWindows = mlang_info->uiCodePage;
    charset->cs_info.cpiInternet = mlang_info->uiInternetEncoding;
    charset->cs_info.hCharset = handle;
    charset->cs_info.dwReserved1 = 0;
    list_add_head(list, &charset->entry);

    return charset->cs_info.hCharset;
}

static HRESULT WINAPI MimeInternat_FindCharset(IMimeInternational *iface, LPCSTR pszCharset,
                                               LPHCHARSET phCharset)
{
    internat *This = impl_from_IMimeInternational( iface );
    HRESULT hr = MIME_E_NOT_FOUND;
    charset_entry *charset;

    TRACE("(%p)->(%s, %p)\n", iface, debugstr_a(pszCharset), phCharset);

    *phCharset = NULL;

    EnterCriticalSection(&This->cs);

    LIST_FOR_EACH_ENTRY(charset, &This->charsets, charset_entry, entry)
    {
        if(!strcmp(charset->cs_info.szName, pszCharset))
        {
            *phCharset = charset->cs_info.hCharset;
            hr = S_OK;
            break;
        }
    }

    if(hr == MIME_E_NOT_FOUND)
    {
        MIMECSETINFO mlang_info;

        LeaveCriticalSection(&This->cs);
        hr = mlang_getcsetinfo(pszCharset, &mlang_info);
        EnterCriticalSection(&This->cs);

        if(SUCCEEDED(hr))
            *phCharset = add_charset(&This->charsets, &mlang_info,
                                     (HCHARSET)InterlockedIncrement(&This->next_charset_handle));
    }

    LeaveCriticalSection(&This->cs);
    return hr;
}

static HRESULT WINAPI MimeInternat_GetCharsetInfo(IMimeInternational *iface, HCHARSET hCharset,
                                                  LPINETCSETINFO pCsetInfo)
{
    internat *This = impl_from_IMimeInternational( iface );
    HRESULT hr = MIME_E_INVALID_HANDLE;
    charset_entry *charset;

    TRACE("(%p)->(%p, %p)\n", iface, hCharset, pCsetInfo);

    EnterCriticalSection(&This->cs);

    LIST_FOR_EACH_ENTRY(charset, &This->charsets, charset_entry, entry)
    {
        if(charset->cs_info.hCharset ==  hCharset)
        {
            *pCsetInfo = charset->cs_info;
            hr = S_OK;
            break;
        }
    }

    LeaveCriticalSection(&This->cs);

    return hr;
}

static HRESULT WINAPI MimeInternat_GetCodePageInfo(IMimeInternational *iface, CODEPAGEID cpiCodePage,
                                                   LPCODEPAGEINFO pCodePageInfo)
{
    FIXME("stub\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI MimeInternat_CanConvertCodePages(IMimeInternational *iface, CODEPAGEID cpiSource,
                                                       CODEPAGEID cpiDest)
{
    HRESULT hr;
    IMultiLanguage *ml;

    TRACE("(%p)->(%d, %d)\n", iface, cpiSource, cpiDest);

    /* Could call mlang.IsConvertINetStringAvailable() to avoid the COM overhead if need be. */

    hr = get_mlang(&ml);
    if(SUCCEEDED(hr))
    {
        hr = IMultiLanguage_IsConvertible(ml, cpiSource, cpiDest);
        IMultiLanguage_Release(ml);
    }

    return hr;
}

static HRESULT WINAPI MimeInternat_DecodeHeader(IMimeInternational *iface, HCHARSET hCharset,
                                                LPCSTR pszData,
                                                LPPROPVARIANT pDecoded,
                                                LPRFC1522INFO pRfc1522Info)
{
    FIXME("stub\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI MimeInternat_EncodeHeader(IMimeInternational *iface, HCHARSET hCharset,
                                                LPPROPVARIANT pData,
                                                LPSTR *ppszEncoded,
                                                LPRFC1522INFO pRfc1522Info)
{
    FIXME("stub\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI MimeInternat_ConvertBuffer(IMimeInternational *iface, CODEPAGEID cpiSource,
                                                 CODEPAGEID cpiDest,
                                                 LPBLOB pIn,
                                                 LPBLOB pOut,
                                                 ULONG *pcbRead)
{
    FIXME("stub\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI MimeInternat_ConvertString(IMimeInternational *iface, CODEPAGEID cpiSource,
                                                 CODEPAGEID cpiDest,
                                                 LPPROPVARIANT pIn,
                                                 LPPROPVARIANT pOut)
{
    FIXME("stub\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI MimeInternat_MLANG_ConvertInetReset(IMimeInternational *iface)
{
    FIXME("stub\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI MimeInternat_MLANG_ConvertInetString(IMimeInternational *iface, CODEPAGEID cpiSource,
                                                           CODEPAGEID cpiDest,
                                                           LPCSTR pSource,
                                                           int *pnSizeOfSource,
                                                           LPSTR pDestination,
                                                           int *pnDstSize)
{
    FIXME("stub\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI MimeInternat_Rfc1522Decode(IMimeInternational *iface, LPCSTR pszValue,
                                                 LPCSTR pszCharset,
                                                 ULONG cchmax,
                                                 LPSTR *ppszDecoded)
{
    FIXME("stub\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI MimeInternat_Rfc1522Encode(IMimeInternational *iface, LPCSTR pszValue,
                                                 HCHARSET hCharset,
                                                 LPSTR *ppszEncoded)
{
    FIXME("stub\n");
    return E_NOTIMPL;
}

static IMimeInternationalVtbl mime_internat_vtbl =
{
    MimeInternat_QueryInterface,
    MimeInternat_AddRef,
    MimeInternat_Release,
    MimeInternat_SetDefaultCharset,
    MimeInternat_GetDefaultCharset,
    MimeInternat_GetCodePageCharset,
    MimeInternat_FindCharset,
    MimeInternat_GetCharsetInfo,
    MimeInternat_GetCodePageInfo,
    MimeInternat_CanConvertCodePages,
    MimeInternat_DecodeHeader,
    MimeInternat_EncodeHeader,
    MimeInternat_ConvertBuffer,
    MimeInternat_ConvertString,
    MimeInternat_MLANG_ConvertInetReset,
    MimeInternat_MLANG_ConvertInetString,
    MimeInternat_Rfc1522Decode,
    MimeInternat_Rfc1522Encode
};

static internat *global_internat;

HRESULT MimeInternational_Construct(IMimeInternational **internat)
{
    global_internat = HeapAlloc(GetProcessHeap(), 0, sizeof(*global_internat));
    global_internat->lpVtbl = &mime_internat_vtbl;
    global_internat->refs = 0;
    InitializeCriticalSection(&global_internat->cs);

    list_init(&global_internat->charsets);
    global_internat->next_charset_handle = 0;
    global_internat->default_charset = NULL;

    *internat = (IMimeInternational*)&global_internat->lpVtbl;

    IMimeInternational_AddRef(*internat);
    return S_OK;
}

HRESULT WINAPI MimeOleGetInternat(IMimeInternational **internat)
{
    TRACE("(%p)\n", internat);

    *internat = (IMimeInternational *)&global_internat->lpVtbl;
    IMimeInternational_AddRef(*internat);
    return S_OK;
}