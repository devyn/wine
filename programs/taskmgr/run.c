/*
 *  ReactOS Task Manager
 *
 *  run.c
 *
 *  Copyright (C) 1999 - 2001  Brian Palmer  <brianp@reactos.org>
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
    
#define WIN32_LEAN_AND_MEAN    /* Exclude rarely-used stuff from Windows headers */
#include <windows.h>
#include <commctrl.h>
#include <stdlib.h>
#include <memory.h>
#include <tchar.h>
#include <stdio.h>
    
#include "taskmgr.h"

typedef	void (WINAPI *RUNFILEDLG)(
HWND    hwndOwner, 
HICON   hIcon, 
LPCSTR  lpstrDirectory, 
LPCSTR  lpstrTitle, 
LPCSTR  lpstrDescription,
UINT    uFlags); 

/*
 * Flags for RunFileDlg
 */

#define	RFF_NOBROWSE		0x01	/* Removes the browse button. */
#define	RFF_NODEFAULT		0x02	/* No default item selected. */
#define	RFF_CALCDIRECTORY	0x04	/* Calculates the working directory from the file name. */
#define	RFF_NOLABEL			0x08	/* Removes the edit box label. */
#define	RFF_NOSEPARATEMEM	0x20	/* Removes the Separate Memory Space check box (Windows NT only). */

void TaskManager_OnFileNew(void)
{
    HMODULE            hShell32;
    RUNFILEDLG        RunFileDlg;
    OSVERSIONINFO    versionInfo;

    hShell32 = LoadLibrary(_T("SHELL32.DLL"));
    RunFileDlg = (RUNFILEDLG)(FARPROC)GetProcAddress(hShell32, (char*)((long)0x3D));

    /* Show "Run..." dialog */
    if (RunFileDlg)
    {
        HICON hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_TASKMANAGER));
        versionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx(&versionInfo);

        if (versionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
        {
            WCHAR wTitle[64];
            LoadStringW(GetModuleHandleW(NULL), IDS_RUNDLG_CAPTION, wTitle, 64);
            RunFileDlg(hMainWnd, hIcon, NULL, (LPCSTR)wTitle, NULL, RFF_CALCDIRECTORY);
        }
        else
        {
            char szTitle[64];
            LoadStringA(GetModuleHandleW(NULL), IDS_RUNDLG_CAPTION, szTitle, 64);
            RunFileDlg(hMainWnd, hIcon, NULL, szTitle, NULL, RFF_CALCDIRECTORY);
        }
    }

    FreeLibrary(hShell32);
}
