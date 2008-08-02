/*
 * Test suite for TaskScheduler interface
 *
 * Copyright (C) 2008 Google (Roy Shea)
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

#include "initguid.h"
#include "mstask.h"
#include "wine/test.h"

static ITaskScheduler *test_task_scheduler;

static void test_NewWorkItem(void)
{
    HRESULT hres;
    ITask *task;
    const WCHAR task_name[] = {'T', 'e', 's', 't', 'i', 'n', 'g', 0};
    GUID GUID_BAD;

    /* Initialize a GUID that will not be a recognized CLSID or a IID */
    CoCreateGuid(&GUID_BAD);

    /* Create TaskScheduler */
    hres = CoCreateInstance(&CLSID_CTaskScheduler, NULL, CLSCTX_INPROC_SERVER,
            &IID_ITaskScheduler, (void **) &test_task_scheduler);
    ok(hres == S_OK, "CTaskScheduler CoCreateInstance failed: %08x\n", hres);
    if (hres != S_OK)
    {
        skip("Failed to create task scheduler.  Skipping tests.\n");
        return;
    }

    /* Test basic task creation */
    hres = ITaskScheduler_NewWorkItem(test_task_scheduler, task_name,
            &CLSID_CTask, &IID_ITask, (IUnknown**)&task);
    todo_wine ok(hres == S_OK, "NewNetworkItem failed: %08x\n", hres);
    if (hres == S_OK)
        ITask_Release(task);

    /* Task creation attempt using invalid work item class ID */
    hres = ITaskScheduler_NewWorkItem(test_task_scheduler, task_name,
            &GUID_BAD, &IID_ITask, (IUnknown**)&task);
    todo_wine ok(hres == CLASS_E_CLASSNOTAVAILABLE,
            "Expected CLASS_E_CLASSNOTAVAILABLE: %08x\n", hres);

    /* Task creation attempt using invalid interface ID */
    hres = ITaskScheduler_NewWorkItem(test_task_scheduler, task_name,
            &CLSID_CTask, &GUID_BAD, (IUnknown**)&task);
    todo_wine ok(hres == E_NOINTERFACE, "Expected E_NOINTERFACE: %08x\n", hres);

    /* Task creation attempt using invalid work item class and interface ID */
    hres = ITaskScheduler_NewWorkItem(test_task_scheduler, task_name,
            &GUID_BAD, &GUID_BAD, (IUnknown**)&task);
    todo_wine ok(hres == CLASS_E_CLASSNOTAVAILABLE,
            "Expected CLASS_E_CLASSNOTAVAILABLE: %08x\n", hres);

    ITaskScheduler_Release(test_task_scheduler);
    return;
}

START_TEST(task_scheduler)
{
    CoInitialize(NULL);
    test_NewWorkItem();
    CoUninitialize();
}