/*
 * POP3 Transport
 *
 * Copyright 2008 Hans Leidekker for CodeWeavers
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

#include <stdarg.h>
#include <stdio.h>

#include "windef.h"
#include "winbase.h"
#include "winnt.h"
#include "winuser.h"
#include "objbase.h"
#include "mimeole.h"
#include "wine/debug.h"

#include "inetcomm_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(inetcomm);

typedef struct
{
    InternetTransport InetTransport;
    ULONG refs;
} POP3Transport;

static HRESULT WINAPI POP3Transport_QueryInterface(IPOP3Transport *iface, REFIID riid, void **ppv)
{
    TRACE("(%s, %p)\n", debugstr_guid(riid), ppv);

    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IInternetTransport) ||
        IsEqualIID(riid, &IID_IPOP3Transport))
    {
        *ppv = iface;
        IUnknown_AddRef(iface);
        return S_OK;
    }
    *ppv = NULL;
    FIXME("no interface for %s\n", debugstr_guid(riid));
    return E_NOINTERFACE;
}

static ULONG WINAPI POP3Transport_AddRef(IPOP3Transport *iface)
{
    POP3Transport *This = (POP3Transport *)iface;
    return InterlockedIncrement((LONG *)&This->refs);
}

static ULONG WINAPI POP3Transport_Release(IPOP3Transport *iface)
{
    POP3Transport *This = (POP3Transport *)iface;
    ULONG refs = InterlockedDecrement((LONG *)&This->refs);
    if (!refs)
    {
        TRACE("destroying %p\n", This);
        if (This->InetTransport.Status != IXP_DISCONNECTED)
            InternetTransport_DropConnection(&This->InetTransport);
        if (This->InetTransport.pCallback) ITransportCallback_Release(This->InetTransport.pCallback);
        HeapFree(GetProcessHeap(), 0, This);
    }
    return refs;
}

static HRESULT WINAPI POP3Transport_GetServerInfo(IPOP3Transport *iface,
    LPINETSERVER pInetServer)
{
    POP3Transport *This = (POP3Transport *)iface;

    TRACE("(%p)\n", pInetServer);
    return InternetTransport_GetServerInfo(&This->InetTransport, pInetServer);
}

static IXPTYPE WINAPI POP3Transport_GetIXPType(IPOP3Transport *iface)
{
    TRACE("()\n");
    return IXP_POP3;
}

static HRESULT WINAPI POP3Transport_IsState(IPOP3Transport *iface, IXPISSTATE isstate)
{
    FIXME("(%u)\n", isstate);
    return E_NOTIMPL;
}

static HRESULT WINAPI POP3Transport_InetServerFromAccount(
    IPOP3Transport *iface, IImnAccount *pAccount, LPINETSERVER pInetServer)
{
    POP3Transport *This = (POP3Transport *)iface;

    TRACE("(%p, %p)\n", pAccount, pInetServer);
    return InternetTransport_InetServerFromAccount(&This->InetTransport, pAccount, pInetServer);
}

static HRESULT WINAPI POP3Transport_Connect(IPOP3Transport *iface,
    LPINETSERVER pInetServer, boolean fAuthenticate, boolean fCommandLogging)
{
    POP3Transport *This = (POP3Transport *)iface;
    HRESULT hr;

    TRACE("(%p, %s, %s)\n", pInetServer, fAuthenticate ? "TRUE" : "FALSE", fCommandLogging ? "TRUE" : "FALSE");

    hr = InternetTransport_Connect(&This->InetTransport, pInetServer, fAuthenticate, fCommandLogging);

    FIXME("continue state machine here\n");
    return hr;
}

static HRESULT WINAPI POP3Transport_HandsOffCallback(IPOP3Transport *iface)
{
    POP3Transport *This = (POP3Transport *)iface;

    TRACE("()\n");
    return InternetTransport_HandsOffCallback(&This->InetTransport);
}

static HRESULT WINAPI POP3Transport_Disconnect(IPOP3Transport *iface)
{
    FIXME("()\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI POP3Transport_DropConnection(IPOP3Transport *iface)
{
    POP3Transport *This = (POP3Transport *)iface;

    TRACE("()\n");
    return InternetTransport_DropConnection(&This->InetTransport);
}

static HRESULT WINAPI POP3Transport_GetStatus(IPOP3Transport *iface,
    IXPSTATUS *pCurrentStatus)
{
    POP3Transport *This = (POP3Transport *)iface;

    TRACE("()\n");
    return InternetTransport_GetStatus(&This->InetTransport, pCurrentStatus);
}

static HRESULT WINAPI POP3Transport_InitNew(IPOP3Transport *iface,
    LPSTR pszLogFilePath, IPOP3Callback *pCallback)
{
    POP3Transport *This = (POP3Transport *)iface;

    TRACE("(%s, %p)\n", debugstr_a(pszLogFilePath), pCallback);

    if (!pCallback)
        return E_INVALIDARG;

    if (pszLogFilePath)
        FIXME("not using log file of %s, use Wine debug logging instead\n",
            debugstr_a(pszLogFilePath));

    IPOP3Callback_AddRef(pCallback);
    This->InetTransport.pCallback = (ITransportCallback *)pCallback;
    This->InetTransport.fInitialised = TRUE;

    return S_OK;
}

static HRESULT WINAPI POP3Transport_MarkItem(IPOP3Transport *iface, POP3MARKTYPE marktype,
    DWORD dwPopId, boolean fMarked)
{
    FIXME("(%u, %u, %d)\n", marktype, dwPopId, fMarked);
    return E_NOTIMPL;
}

static HRESULT WINAPI POP3Transport_CommandAUTH(IPOP3Transport *iface, LPSTR pszAuthType)
{
    FIXME("(%s)\n", pszAuthType);
    return E_NOTIMPL;
}

static HRESULT WINAPI POP3Transport_CommandUSER(IPOP3Transport *iface, LPSTR username)
{
    FIXME("(%s)\n", username);
    return E_NOTIMPL;
}

static HRESULT WINAPI POP3Transport_CommandPASS(IPOP3Transport *iface, LPSTR password)
{
    FIXME("(%s)\n", password);
    return E_NOTIMPL;
}

static HRESULT WINAPI POP3Transport_CommandLIST(
    IPOP3Transport *iface, POP3CMDTYPE cmdtype, DWORD dwPopId)
{
    FIXME("(%u, %u)\n", cmdtype, dwPopId);
    return E_NOTIMPL;
}

static HRESULT WINAPI POP3Transport_CommandTOP(
    IPOP3Transport *iface, POP3CMDTYPE cmdtype, DWORD dwPopId, DWORD cPreviewLines)
{
    FIXME("(%u, %u, %u)\n", cmdtype, dwPopId, cPreviewLines);
    return E_NOTIMPL;
}

static HRESULT WINAPI POP3Transport_CommandQUIT(IPOP3Transport *iface)
{
    FIXME("()\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI POP3Transport_CommandSTAT(IPOP3Transport *iface)
{
    FIXME("()\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI POP3Transport_CommandNOOP(IPOP3Transport *iface)
{
    FIXME("()\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI POP3Transport_CommandRSET(IPOP3Transport *iface)
{
    FIXME("()\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI POP3Transport_CommandUIDL(
    IPOP3Transport *iface, POP3CMDTYPE cmdtype, DWORD dwPopId)
{
    FIXME("(%u, %u)\n", cmdtype, dwPopId);
    return E_NOTIMPL;
}

static HRESULT WINAPI POP3Transport_CommandDELE(
    IPOP3Transport *iface, POP3CMDTYPE cmdtype, DWORD dwPopId)
{
    FIXME("(%u, %u)\n", cmdtype, dwPopId);
    return E_NOTIMPL;
}

static HRESULT WINAPI POP3Transport_CommandRETR(
    IPOP3Transport *iface, POP3CMDTYPE cmdtype, DWORD dwPopId)
{
    FIXME("(%u, %u)\n", cmdtype, dwPopId);
    return E_NOTIMPL;
}

static const IPOP3TransportVtbl POP3TransportVtbl =
{
    POP3Transport_QueryInterface,
    POP3Transport_AddRef,
    POP3Transport_Release,
    POP3Transport_GetServerInfo,
    POP3Transport_GetIXPType,
    POP3Transport_IsState,
    POP3Transport_InetServerFromAccount,
    POP3Transport_Connect,
    POP3Transport_HandsOffCallback,
    POP3Transport_Disconnect,
    POP3Transport_DropConnection,
    POP3Transport_GetStatus,
    POP3Transport_InitNew,
    POP3Transport_MarkItem,
    POP3Transport_CommandAUTH,
    POP3Transport_CommandUSER,
    POP3Transport_CommandPASS,
    POP3Transport_CommandLIST,
    POP3Transport_CommandTOP,
    POP3Transport_CommandQUIT,
    POP3Transport_CommandSTAT,
    POP3Transport_CommandNOOP,
    POP3Transport_CommandRSET,
    POP3Transport_CommandUIDL,
    POP3Transport_CommandDELE,
    POP3Transport_CommandRETR
};

HRESULT WINAPI CreatePOP3Transport(IPOP3Transport **ppTransport)
{
    HRESULT hr;
    POP3Transport *This = HeapAlloc(GetProcessHeap(), 0, sizeof(*This));
    if (!This)
        return E_OUTOFMEMORY;

    This->InetTransport.u.vtblPOP3 = &POP3TransportVtbl;
    This->refs = 0;
    hr = InternetTransport_Init(&This->InetTransport);
    if (FAILED(hr))
    {
        HeapFree(GetProcessHeap(), 0, This);
        return hr;
    }

    *ppTransport = (IPOP3Transport *)&This->InetTransport.u.vtblPOP3;
    IPOP3Transport_AddRef(*ppTransport);

    return S_OK;
}

static HRESULT WINAPI POP3TransportCF_QueryInterface(LPCLASSFACTORY iface,
    REFIID riid, LPVOID *ppv)
{
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IClassFactory))
    {
        *ppv = iface;
        IUnknown_AddRef(iface);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG WINAPI POP3TransportCF_AddRef(LPCLASSFACTORY iface)
{
    return 2; /* non-heap based object */
}

static ULONG WINAPI POP3TransportCF_Release(LPCLASSFACTORY iface)
{
    return 1; /* non-heap based object */
}

static HRESULT WINAPI POP3TransportCF_CreateInstance(LPCLASSFACTORY iface,
    LPUNKNOWN pUnk, REFIID riid, LPVOID *ppv)
{
    HRESULT hr;
    IPOP3Transport *pPop3Transport;

    TRACE("(%p, %s, %p)\n", pUnk, debugstr_guid(riid), ppv);

    *ppv = NULL;

    if (pUnk)
        return CLASS_E_NOAGGREGATION;

    hr = CreatePOP3Transport(&pPop3Transport);
    if (FAILED(hr))
        return hr;

    hr = IPOP3Transport_QueryInterface(pPop3Transport, riid, ppv);
    IPOP3Transport_Release(pPop3Transport);

    return hr;
}

static HRESULT WINAPI POP3TransportCF_LockServer(LPCLASSFACTORY iface, BOOL fLock)
{
    FIXME("(%d)\n",fLock);
    return S_OK;
}

static const IClassFactoryVtbl POP3TransportCFVtbl =
{
    POP3TransportCF_QueryInterface,
    POP3TransportCF_AddRef,
    POP3TransportCF_Release,
    POP3TransportCF_CreateInstance,
    POP3TransportCF_LockServer
};
static const IClassFactoryVtbl *POP3TransportCF = &POP3TransportCFVtbl;

HRESULT POP3TransportCF_Create(REFIID riid, LPVOID *ppv)
{
    return IClassFactory_QueryInterface((IClassFactory *)&POP3TransportCF, riid, ppv);
}