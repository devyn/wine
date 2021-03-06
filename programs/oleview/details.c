/*
 * OleView (details.c)
 *
 * Copyright 2006 Piotr Caban
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

#include "main.h"

DETAILS details;
static const WCHAR wszAppID[] = { 'A','p','p','I','D','\0' };
static const WCHAR wszCLSID[] = { 'C','L','S','I','D','\0' };
static const WCHAR wszProgID[] = { 'P','r','o','g','I','D','\0' };
static const WCHAR wszProxyStubClsid32[] = 
    { 'P','r','o','x','y','S','t','u','b','C','l','s','i','d','3','2','\0' };
static const WCHAR wszTypeLib[] = { 'T','y','p','e','L','i','b','\0' };

static void CreateRegRec(HKEY hKey, HTREEITEM parent, WCHAR *wszKeyName, BOOL addings)
{
    int i=0, j, retEnum;
    HKEY hCurKey;
    DWORD lenName, lenData, valType;
    WCHAR wszName[MAX_LOAD_STRING];
    WCHAR wszData[MAX_LOAD_STRING];
    WCHAR wszTree[MAX_LOAD_STRING];
    const WCHAR wszBinary[] = { '%','0','2','X',' ','\0' };
    const WCHAR wszDots[] = { '.','.','.','\0' };
    const WCHAR wszFormat1[] = { '%','s',' ','[','%','s',']',' ','=',' ','%','s','\0' };
    const WCHAR wszFormat2[] = { '%','s',' ','=',' ','%','s','\0' };
    TVINSERTSTRUCT tvis;
    HTREEITEM addPlace = parent;

    U(tvis).item.mask = TVIF_TEXT;
    U(tvis).item.cchTextMax = MAX_LOAD_STRING;
    U(tvis).item.pszText = wszTree;
    tvis.hInsertAfter = (HTREEITEM)TVI_LAST;
    tvis.hParent = parent;

    while(TRUE)
    {
        lenName = sizeof(WCHAR[MAX_LOAD_STRING])/sizeof(WCHAR);
        lenData = sizeof(WCHAR[MAX_LOAD_STRING]);

        retEnum = RegEnumValue(hKey, i, wszName, &lenName,
                NULL, &valType, (LPBYTE)wszData, &lenData);

        if(retEnum != ERROR_SUCCESS)
        {
            if(!i && lstrlenW(wszKeyName) > 1)
            {
                U(tvis).item.pszText = (LPWSTR)wszKeyName;
                addPlace = TreeView_InsertItem(details.hReg, &tvis);
                U(tvis).item.pszText = wszTree;
            }
            break;
        }

        if(valType == REG_BINARY)
        {
            WCHAR wszBuf[MAX_LOAD_STRING];

            for(j=0; j<MAX_LOAD_STRING/3-1; j++)
                wsprintfW(&wszBuf[3*j], wszBinary, (int)((unsigned char)wszData[j]));
            wszBuf[(lenData*3>=MAX_LOAD_STRING ? MAX_LOAD_STRING-1 : lenData*3)] = '\0';
            lstrcpyW(wszData, wszBuf);
            lstrcpyW(&wszData[MAX_LOAD_STRING-5], wszDots);
        }

        if(lenName) wsprintfW(wszTree, wszFormat1, wszKeyName, wszName, wszData);
        else wsprintfW(wszTree, wszFormat2, wszKeyName, wszData);

        addPlace = TreeView_InsertItem(details.hReg, &tvis);

        if(addings && !memcmp(wszName, wszAppID, sizeof(WCHAR[6])))
        {
            lstrcpyW(wszTree, wszName);
            memmove(&wszData[6], wszData, sizeof(WCHAR[MAX_LOAD_STRING-6]));
            lstrcpyW(wszData, wszCLSID);
            wszData[5] = '\\';

            if(RegOpenKey(HKEY_CLASSES_ROOT, wszData, &hCurKey) != ERROR_SUCCESS)
            {
                i++;
                continue;
            }

            tvis.hParent = TVI_ROOT;
            tvis.hParent = TreeView_InsertItem(details.hReg, &tvis);

            lenName = sizeof(WCHAR[MAX_LOAD_STRING]);

            RegQueryValue(hCurKey, NULL, wszName, (LONG *)&lenName);
            RegCloseKey(hCurKey);

            wsprintfW(wszTree, wszFormat2, &wszData[6], wszName);

            SendMessage(details.hReg, TVM_INSERTITEM, 0, (LPARAM)&tvis);
            SendMessage(details.hReg, TVM_EXPAND, TVE_EXPAND, (LPARAM)tvis.hParent);

            tvis.hParent = parent;
        }
        i++;
    }

    i=-1;
    lenName = sizeof(WCHAR[MAX_LOAD_STRING]);

    while(TRUE)
    {
        i++;

        if(RegEnumKey(hKey, i, wszName, lenName) != ERROR_SUCCESS) break;

        if(RegOpenKey(hKey, wszName, &hCurKey) != ERROR_SUCCESS) continue;

        CreateRegRec(hCurKey, addPlace, wszName, addings);
        SendMessage(details.hReg, TVM_EXPAND, TVE_EXPAND, (LPARAM)addPlace);

        if(addings && !memcmp(wszName, wszProgID, sizeof(WCHAR[7])))
        {
            lenData = sizeof(WCHAR[MAX_LOAD_STRING]);

            RegQueryValue(hCurKey, NULL, wszData, (LONG *)&lenData);
            RegCloseKey(hCurKey);

            if(RegOpenKey(HKEY_CLASSES_ROOT, wszData, &hCurKey) != ERROR_SUCCESS)
                continue;
            CreateRegRec(hCurKey, TVI_ROOT, wszData, FALSE);
        }
        else if(addings && !memcmp(wszName, wszProxyStubClsid32, sizeof(WCHAR[17])))
        {
            lenData = sizeof(WCHAR[MAX_LOAD_STRING]);

            RegQueryValue(hCurKey, NULL, wszData, (LONG *)&lenData);
            RegCloseKey(hCurKey);

            RegOpenKey(HKEY_CLASSES_ROOT, wszCLSID, &hCurKey);

            lenName = sizeof(WCHAR[MAX_LOAD_STRING]);
            RegQueryValue(hCurKey, NULL, wszName, (LONG *)&lenName);

            tvis.hParent = TVI_ROOT;
            wsprintfW(wszTree, wszFormat2, wszCLSID, wszName);
            tvis.hParent = TreeView_InsertItem(details.hReg, &tvis);

            RegCloseKey(hCurKey);

            memmove(&wszData[6], wszData, sizeof(WCHAR[lenData]));
            memcpy(wszData, wszCLSID, sizeof(WCHAR[6]));
            wszData[5] = '\\';

            RegOpenKey(HKEY_CLASSES_ROOT, wszData, &hCurKey);

            CreateRegRec(hCurKey, tvis.hParent, &wszData[6], FALSE);

            SendMessage(details.hReg, TVM_EXPAND, TVE_EXPAND, (LPARAM)tvis.hParent);
            tvis.hParent = parent;
        }
        else if(addings && !memcmp(wszName, wszTypeLib, sizeof(WCHAR[8])))
        {
            lenData = sizeof(WCHAR[MAX_LOAD_STRING]);

            RegQueryValue(hCurKey, NULL, wszData, (LONG *)&lenData);
            RegCloseKey(hCurKey);

            RegOpenKey(HKEY_CLASSES_ROOT, wszTypeLib, &hCurKey);

            lenName = sizeof(WCHAR[MAX_LOAD_STRING]);
            RegQueryValue(hCurKey, NULL, wszName, (LONG *)&lenName);

            tvis.hParent = TVI_ROOT;
            wsprintfW(wszTree, wszFormat2, wszTypeLib, wszName);
            tvis.hParent = TreeView_InsertItem(details.hReg, &tvis);

            RegCloseKey(hCurKey);

            memmove(&wszData[8], wszData, sizeof(WCHAR[lenData]));
            memcpy(wszData, wszTypeLib, sizeof(WCHAR[8]));
            wszData[7] = '\\';
            RegOpenKey(HKEY_CLASSES_ROOT, wszData, &hCurKey);

            CreateRegRec(hCurKey, tvis.hParent, &wszData[8], FALSE);

            SendMessage(details.hReg, TVM_EXPAND, TVE_EXPAND, (LPARAM)tvis.hParent);
            tvis.hParent = parent;
        }
        RegCloseKey(hCurKey);
    }
}

static void CreateReg(WCHAR *buffer)
{
    HKEY hKey;
    DWORD lenBuffer=-1, lastLenBuffer, lenTree;
    WCHAR *path;
    WCHAR wszTree[MAX_LOAD_STRING];
    TVINSERTSTRUCT tvis;
    HTREEITEM addPlace = TVI_ROOT;

    U(tvis).item.mask = TVIF_TEXT;
    U(tvis).item.cchTextMax = MAX_LOAD_STRING;
    U(tvis).item.pszText = wszTree;
    tvis.hInsertAfter = (HTREEITEM)TVI_LAST;
    tvis.hParent = TVI_ROOT;

    path = buffer;
    while(TRUE)
    {
        while(*path != '\\' && *path != '\0') path += 1;

        if(*path == '\\')
        {
            *path = '\0';

            if(RegOpenKey(HKEY_CLASSES_ROOT, (LPWSTR)buffer, &hKey) != ERROR_SUCCESS)
                return;

            lastLenBuffer = lenBuffer+1;
            lenBuffer = lstrlenW(buffer);
            *path = '\\';
            path += 1;

            lenTree = sizeof(WCHAR[MAX_LOAD_STRING]);

            if(RegQueryValue(hKey, NULL, wszTree, (LONG *)&lenTree) == ERROR_SUCCESS)
            {
                memmove(&wszTree[lenBuffer-lastLenBuffer+3], wszTree,
                        sizeof(WCHAR[lenTree]));
                memcpy(wszTree, &buffer[lastLenBuffer],
                        sizeof(WCHAR[lenBuffer-lastLenBuffer]));

                if(lenTree == 1) wszTree[lenBuffer-lastLenBuffer] = '\0';
                else
                {
                    wszTree[lenBuffer-lastLenBuffer] = ' ';
                    wszTree[lenBuffer-lastLenBuffer+1] = '=';
                    wszTree[lenBuffer-lastLenBuffer+2] = ' ';
                }

                addPlace = TreeView_InsertItem(details.hReg, &tvis);
            }

            tvis.hParent = addPlace;
            RegCloseKey(hKey);
        }
        else break;
    }

    if(RegOpenKey(HKEY_CLASSES_ROOT, (LPWSTR)buffer, &hKey) != ERROR_SUCCESS) return;

    CreateRegRec(hKey, addPlace, (LPWSTR)&buffer[lenBuffer+1], TRUE);

    RegCloseKey(hKey);

    SendMessage(details.hReg, TVM_EXPAND, TVE_EXPAND, (LPARAM)addPlace);
    SendMessage(details.hReg, TVM_ENSUREVISIBLE, 0, (LPARAM)addPlace);
}

void RefreshDetails(HTREEITEM item)
{
    TVITEM tvi;
    WCHAR wszBuf[MAX_LOAD_STRING];
    WCHAR wszStaticText[MAX_LOAD_STRING];
    const WCHAR wszFormat[] = { '%','s','\n','%','s','\0' };
    BOOL show;

    memset(&tvi, 0, sizeof(TVITEM));
    memset(&wszStaticText, 0, sizeof(WCHAR[MAX_LOAD_STRING]));
    tvi.mask = TVIF_TEXT;
    tvi.hItem = item;
    tvi.pszText = wszBuf;
    tvi.cchTextMax = MAX_LOAD_STRING;
    SendMessage(globals.hTree, TVM_GETITEM, 0, (LPARAM)&tvi);

    if(tvi.lParam)
        wsprintfW(wszStaticText, wszFormat, tvi.pszText, ((ITEM_INFO *)tvi.lParam)->clsid);
    else lstrcpyW(wszStaticText, tvi.pszText);

    SetWindowText(details.hStatic, wszStaticText);

    SendMessage(details.hTab, TCM_SETCURSEL, 0, 0);

    if(tvi.lParam && ((ITEM_INFO *)tvi.lParam)->cFlag & SHOWALL)
    {
        if(TabCtrl_GetItemCount(details.hTab) == 1)
        {
            TCITEM tci;
            memset(&tci, 0, sizeof(TCITEM));
            tci.mask = TCIF_TEXT;
            tci.pszText = wszBuf;
            tci.cchTextMax = sizeof(wszBuf)/sizeof(wszBuf[0]);

            LoadString(globals.hMainInst, IDS_TAB_IMPL,
                    wszBuf, sizeof(wszBuf)/sizeof(wszBuf[0]));
            SendMessage(details.hTab, TCM_INSERTITEM, 1, (LPARAM)&tci);

            LoadString(globals.hMainInst, IDS_TAB_ACTIV,
                    wszBuf, sizeof(wszBuf)/sizeof(wszBuf[0]));
            SendMessage(details.hTab, TCM_INSERTITEM, 2, (LPARAM)&tci);
        }
    }
    else
    {
        SendMessage(details.hTab, TCM_DELETEITEM, 2, 0);
        SendMessage(details.hTab, TCM_DELETEITEM, 1, 0);
    }

    show = CreateRegPath(item, wszBuf, MAX_LOAD_STRING);
    ShowWindow(details.hTab, show ? SW_SHOW : SW_HIDE);

    /* FIXME Next line deals with TreeView_EnsureVisible bug */
    SendMessage(details.hReg, TVM_ENSUREVISIBLE, 0,
            SendMessage(details.hReg, TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)TVI_ROOT));
    SendMessage(details.hReg, TVM_DELETEITEM, 0, (LPARAM)TVI_ROOT);
    if(show) CreateReg(wszBuf);
}

static void CreateTabCtrl(HWND hWnd)
{
    TCITEM tci;
    WCHAR buffer[MAX_LOAD_STRING];

    memset(&tci, 0, sizeof(TCITEM));
    tci.mask = TCIF_TEXT;
    tci.pszText = buffer;
    tci.cchTextMax = sizeof(buffer)/sizeof(buffer[0]);

    details.hTab = CreateWindow(WC_TABCONTROL, NULL, WS_CHILD|WS_VISIBLE,
            0, 0, 0, 0, hWnd, (HMENU)TAB_WINDOW, globals.hMainInst, NULL);
    ShowWindow(details.hTab, SW_HIDE);

    LoadString(globals.hMainInst, IDS_TAB_REG, buffer, sizeof(buffer)/sizeof(buffer[0]));
    SendMessage(details.hTab, TCM_INSERTITEM, 0, (LPARAM)&tci);

    details.hReg = CreateWindowEx(WS_EX_CLIENTEDGE, WC_TREEVIEW, NULL,
            WS_CHILD|WS_VISIBLE|TVS_HASLINES,
            0, 0, 0, 0, details.hTab, NULL, globals.hMainInst, NULL);
}

static LRESULT CALLBACK DetailsProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    int sel;

    switch(uMsg)
    {
        case WM_CREATE:
            {
                const WCHAR wszStatic[] = { 'S','t','a','t','i','c','\0' };

                details.hStatic = CreateWindow(wszStatic, NULL, WS_CHILD|WS_VISIBLE,
                        0, 0, 0, 0, hWnd, NULL, globals.hMainInst, NULL);
                CreateTabCtrl(hWnd);
            }
            break;
        case WM_SIZE:
            MoveWindow(details.hStatic, 0, 0, LOWORD(lParam), 40, TRUE);
            MoveWindow(details.hTab, 3, 40, LOWORD(lParam)-6, HIWORD(lParam)-43, TRUE);
            MoveWindow(details.hReg, 10, 34, LOWORD(lParam)-26,
                    HIWORD(lParam)-87, TRUE);
            break;
        case WM_NOTIFY:
            if((int)wParam != TAB_WINDOW) break;
            switch(((LPNMHDR)lParam)->code)
            {
                case TCN_SELCHANGE:
                    ShowWindow(details.hReg, SW_HIDE);
                    sel = TabCtrl_GetCurSel(details.hTab);

                    if(sel==0) ShowWindow(details.hReg, SW_SHOW);
                    break;
            }
            break;
        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

HWND CreateDetailsWindow(HINSTANCE hInst)
{
    WNDCLASS wcd;
    const WCHAR wszDetailsClass[] = { 'D','E','T','A','I','L','S','\0' };
    
    memset(&wcd, 0, sizeof(WNDCLASS));
    wcd.lpfnWndProc = DetailsProc;
    wcd.lpszClassName = wszDetailsClass;
    wcd.hbrBackground = (HBRUSH)COLOR_WINDOW;

    if(!RegisterClass(&wcd)) return NULL;

    globals.hDetails = CreateWindowEx(WS_EX_CLIENTEDGE, wszDetailsClass, NULL,
            WS_CHILD|WS_VISIBLE, 0, 0, 0, 0, globals.hPaneWnd, NULL, hInst, NULL);

    return globals.hDetails;
}
