/*
 * Copyright 2008 Henri Verbeet for CodeWeavers
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
 *
 */

#include "config.h"
#include "wine/port.h"

#include "dxgi_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dxgi);

/* IUnknown methods */

static HRESULT STDMETHODCALLTYPE dxgi_swapchain_QueryInterface(IDXGISwapChain *iface, REFIID riid, void **object)
{
    TRACE("iface %p, riid %s, object %p\n", iface, debugstr_guid(riid), object);

    if (IsEqualGUID(riid, &IID_IUnknown)
            || IsEqualGUID(riid, &IID_IDXGIObject)
            || IsEqualGUID(riid, &IID_IDXGIDeviceSubObject)
            || IsEqualGUID(riid, &IID_IDXGISwapChain))
    {
        IUnknown_AddRef(iface);
        *object = iface;
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE\n", debugstr_guid(riid));

    *object = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE dxgi_swapchain_AddRef(IDXGISwapChain *iface)
{
    struct dxgi_swapchain *This = (struct dxgi_swapchain *)iface;
    ULONG refcount = InterlockedIncrement(&This->refcount);

    TRACE("%p increasing refcount to %u\n", This, refcount);

    return refcount;
}

static ULONG STDMETHODCALLTYPE dxgi_swapchain_Release(IDXGISwapChain *iface)
{
    struct dxgi_swapchain *This = (struct dxgi_swapchain *)iface;
    ULONG refcount = InterlockedDecrement(&This->refcount);

    TRACE("%p decreasing refcount to %u\n", This, refcount);

    if (!refcount)
    {
        HeapFree(GetProcessHeap(), 0, This);
    }

    return refcount;
}

/* IDXGIObject methods */

static HRESULT STDMETHODCALLTYPE dxgi_swapchain_SetPrivateData(IDXGISwapChain *iface,
        REFGUID guid, UINT data_size, const void *data)
{
    FIXME("iface %p, guid %s, data_size %u, data %p stub!\n", iface, debugstr_guid(guid), data_size, data);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_swapchain_SetPrivateDataInterface(IDXGISwapChain *iface,
        REFGUID guid, const IUnknown *object)
{
    FIXME("iface %p, guid %s, object %p stub!\n", iface, debugstr_guid(guid), object);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_swapchain_GetPrivateData(IDXGISwapChain *iface,
        REFGUID guid, UINT *data_size, void *data)
{
    FIXME("iface %p, guid %s, data_size %p, data %p stub!\n", iface, debugstr_guid(guid), data_size, data);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_swapchain_GetParent(IDXGISwapChain *iface, REFIID riid, void **parent)
{
    FIXME("iface %p, riid %s, parent %p stub!\n", iface, debugstr_guid(riid), parent);

    return E_NOTIMPL;
}

/* IDXGIDeviceSubObject methods */

static HRESULT STDMETHODCALLTYPE dxgi_swapchain_GetDevice(IDXGISwapChain *iface, REFIID riid, void **device)
{
    FIXME("iface %p, riid %s, device %p stub!\n", iface, debugstr_guid(riid), device);

    return E_NOTIMPL;
}

/* IDXGISwapChain methods */

static HRESULT STDMETHODCALLTYPE dxgi_swapchain_Present(IDXGISwapChain *iface, UINT sync_interval, UINT flags)
{
    FIXME("iface %p, sync_interval %u, flags %#x stub!\n", iface, sync_interval, flags);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_swapchain_GetBuffer(IDXGISwapChain *iface,
        UINT buffer_idx, REFIID riid, void **surface)
{
    FIXME("iface %p, buffer_idx %u, riid %s, surface %p stub!\n",
            iface, buffer_idx, debugstr_guid(riid), surface);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_swapchain_SetFullscreenState(IDXGISwapChain *iface,
        BOOL fullscreen, IDXGIOutput *target)
{
    FIXME("iface %p, fullscreen %u, target %p stub!\n", iface, fullscreen, target);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_swapchain_GetFullscreenState(IDXGISwapChain *iface,
        BOOL *fullscreen, IDXGIOutput **target)
{
    FIXME("iface %p, fullscreen %p, target %p stub!\n", iface, fullscreen, target);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_swapchain_GetDesc(IDXGISwapChain *iface, DXGI_SWAP_CHAIN_DESC *desc)
{
    FIXME("iface %p, desc %p stub!\n", iface, desc);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_swapchain_ResizeBuffers(IDXGISwapChain *iface,
        UINT buffer_count, UINT width, UINT height, DXGI_FORMAT format, UINT flags)
{
    FIXME("iface %p, buffer_count %u, width %u, height %u, format %s, flags %#x stub!\n",
            iface, buffer_count, width, height, debug_dxgi_format(format), flags);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_swapchain_ResizeTarget(IDXGISwapChain *iface,
        const DXGI_MODE_DESC *target_mode_desc)
{
    FIXME("iface %p, target_mode_desc %p stub!\n", iface, target_mode_desc);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_swapchain_GetContainingOutput(IDXGISwapChain *iface, IDXGIOutput **output)
{
    FIXME("iface %p, output %p stub!\n", iface, output);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_swapchain_GetFrameStatistics(IDXGISwapChain *iface, DXGI_FRAME_STATISTICS *stats)
{
    FIXME("iface %p, stats %p stub!\n", iface, stats);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_swapchain_GetLastPresentCount(IDXGISwapChain *iface, UINT *last_present_count)
{
    FIXME("iface %p, last_present_count %p stub!\n", iface, last_present_count);

    return E_NOTIMPL;
}

const struct IDXGISwapChainVtbl dxgi_swapchain_vtbl =
{
    /* IUnknown methods */
    dxgi_swapchain_QueryInterface,
    dxgi_swapchain_AddRef,
    dxgi_swapchain_Release,
    /* IDXGIObject methods */
    dxgi_swapchain_SetPrivateData,
    dxgi_swapchain_SetPrivateDataInterface,
    dxgi_swapchain_GetPrivateData,
    dxgi_swapchain_GetParent,
    /* IDXGIDeviceSubObject methods */
    dxgi_swapchain_GetDevice,
    /* IDXGISwapChain methods */
    dxgi_swapchain_Present,
    dxgi_swapchain_GetBuffer,
    dxgi_swapchain_SetFullscreenState,
    dxgi_swapchain_GetFullscreenState,
    dxgi_swapchain_GetDesc,
    dxgi_swapchain_ResizeBuffers,
    dxgi_swapchain_ResizeTarget,
    dxgi_swapchain_GetContainingOutput,
    dxgi_swapchain_GetFrameStatistics,
    dxgi_swapchain_GetLastPresentCount,
};
