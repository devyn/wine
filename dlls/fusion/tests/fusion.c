/*
 * Copyright 2008 James Hawkins
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

#include <windows.h>
#include <fusion.h>

#include "wine/test.h"

static HMODULE hmscoree;

static HRESULT (WINAPI *pGetCachePath)(ASM_CACHE_FLAGS dwCacheFlags,
                                       LPWSTR pwzCachePath, PDWORD pcchPath);
static HRESULT (WINAPI *pLoadLibraryShim)(LPCWSTR szDllName, LPCWSTR szVersion,
                                          LPVOID pvReserved, HMODULE *phModDll);
static HRESULT (WINAPI *pGetCORVersion)(LPWSTR pbuffer, DWORD cchBuffer,
                                        DWORD *dwLength);

static CHAR string1[MAX_PATH], string2[MAX_PATH];

#define ok_w2(format, szString1, szString2) \
\
    if (lstrcmpW(szString1, szString2) != 0) \
    { \
        WideCharToMultiByte(CP_ACP, 0, szString1, -1, string1, MAX_PATH, NULL, NULL); \
        WideCharToMultiByte(CP_ACP, 0, szString2, -1, string2, MAX_PATH, NULL, NULL); \
        ok(0, format, string1, string2); \
    }

static BOOL init_functionpointers(void)
{
    HRESULT hr;
    HMODULE hfusion;

    static const WCHAR szFusion[] = {'f','u','s','i','o','n','.','d','l','l',0};

    hmscoree = LoadLibraryA("mscoree.dll");
    if (!hmscoree)
    {
        skip("mscoree.dll not available\n");
        return FALSE;
    }

    pLoadLibraryShim = (void *)GetProcAddress(hmscoree, "LoadLibraryShim");
    if (!pLoadLibraryShim)
    {
        skip("LoadLibraryShim not available\n");
        FreeLibrary(hmscoree);
        return FALSE;
    }

    pGetCORVersion = (void *)GetProcAddress(hmscoree, "GetCORVersion");

    hr = pLoadLibraryShim(szFusion, NULL, NULL, &hfusion);
    if (FAILED(hr))
    {
        skip("fusion.dll not available\n");
        FreeLibrary(hmscoree);
        return FALSE;
    }

    pGetCachePath = (void *)GetProcAddress(hfusion, "GetCachePath");
    return TRUE;
}

static void test_GetCachePath(void)
{
    WCHAR cachepath[MAX_PATH];
    WCHAR version[MAX_PATH];
    WCHAR path[MAX_PATH];
    DWORD size;
    HRESULT hr;

    static const WCHAR backslash[] = {'\\',0};
    static const WCHAR nochange[] = {'n','o','c','h','a','n','g','e',0};
    static const WCHAR assembly[] = {'a','s','s','e','m','b','l','y',0};
    static const WCHAR gac[] = {'G','A','C',0};
    static const WCHAR nativeimg[] = {
        'N','a','t','i','v','e','I','m','a','g','e','s','_',0};
    static const WCHAR zapfmt[] = {
        '%','s','\\','%','s','\\','%','s','%','s','_','3','2',0};

    if (!pGetCachePath)
    {
        skip("GetCachePath not implemented\n");
        return;
    }

    GetWindowsDirectoryW(cachepath, MAX_PATH);
    lstrcatW(cachepath, backslash);
    lstrcatW(cachepath, assembly);
    lstrcatW(cachepath, backslash);
    lstrcatW(cachepath, gac);

    /* NULL pwzCachePath, pcchPath is 0 */
    size = 0;
    hr = pGetCachePath(ASM_CACHE_GAC, NULL, &size);
    ok(hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER),
       "Expected HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), got %08x\n", hr);
    ok(size == lstrlenW(cachepath) + 1,
       "Expected %d, got %d\n", lstrlenW(cachepath) + 1, size);

    /* NULL pwszCachePath, pcchPath is MAX_PATH */
    size = MAX_PATH;
    hr = pGetCachePath(ASM_CACHE_GAC, NULL, &size);
    ok(hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER),
       "Expected HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), got %08x\n", hr);
    ok(size == lstrlenW(cachepath) + 1,
       "Expected %d, got %d\n", lstrlenW(cachepath) + 1, size);

    /* both pwszCachePath and pcchPath NULL */
    hr = pGetCachePath(ASM_CACHE_GAC, NULL, NULL);
    ok(hr == E_INVALIDARG, "Expected E_INVALIDARG, got %08x\n", hr);

    /* NULL pcchPath */
    lstrcpyW(path, nochange);
    hr = pGetCachePath(ASM_CACHE_GAC, path, NULL);
    ok(hr == E_INVALIDARG, "Expected E_INVALIDARG, got %08x\n", hr);
    ok_w2("Expected \"%s\",  got \"%s\"\n", nochange, path);

    /* get the cache path */
    lstrcpyW(path, nochange);
    size = MAX_PATH;
    hr = pGetCachePath(ASM_CACHE_GAC, path, &size);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok_w2("Expected \"%s\",  got \"%s\"\n", cachepath, path);

    /* pcchPath has no room for NULL terminator */
    lstrcpyW(path, nochange);
    size = lstrlenW(cachepath);
    hr = pGetCachePath(ASM_CACHE_GAC, path, &size);
    ok(hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER),
       "Expected HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), got %08x\n", hr);
    ok_w2("Expected \"%s\",  got \"%s\"\n", nochange, path);

    if (pGetCORVersion)
    {
        pGetCORVersion(version, MAX_PATH, &size);
        GetWindowsDirectoryW(path, MAX_PATH);
        wsprintfW(cachepath, zapfmt, path, assembly, nativeimg, version);

        /* ASM_CACHE_ZAP */
        lstrcpyW(path, nochange);
        size = MAX_PATH;
        hr = pGetCachePath(ASM_CACHE_ZAP, path, &size);
        ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
        ok_w2("Expected \"%s\",  got \"%s\"\n", cachepath, path);
    }

    GetWindowsDirectoryW(cachepath, MAX_PATH);
    lstrcatW(cachepath, backslash);
    lstrcatW(cachepath, assembly);

    /* ASM_CACHE_ROOT */
    lstrcpyW(path, nochange);
    size = MAX_PATH;
    hr = pGetCachePath(ASM_CACHE_ROOT, path, &size);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok_w2("Expected \"%s\",  got \"%s\"\n", cachepath, path);

    /* two flags at once */
    lstrcpyW(path, nochange);
    size = MAX_PATH;
    hr = pGetCachePath(ASM_CACHE_GAC | ASM_CACHE_ROOT, path, &size);
    ok(hr == E_INVALIDARG, "Expected E_INVALIDARG, got %08x\n", hr);
    ok_w2("Expected \"%s\",  got \"%s\"\n", nochange, path);
}

START_TEST(fusion)
{
    if (!init_functionpointers())
        return;

    test_GetCachePath();

    FreeLibrary(hmscoree);
}
