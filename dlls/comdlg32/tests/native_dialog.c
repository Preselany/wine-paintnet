/* Native chooser transport and IFileDialog lifecycle. LGPL-2.1-or-later. */
#define COBJMACROS
#define CONST_VTABLE
#include "shlobj.h"
#include "wine/test.h"

static IFileDialogEvents events, observer;
static IFileDialog *active;
static HWND owner;
static unsigned int accepted, types, overwrites, selection_count, observed;
static BOOL veto, refuse;

static HRESULT WINAPI event_query(IFileDialogEvents *iface, REFIID iid, void **out)
{
    *out = NULL;
    if (!IsEqualIID(iid, &IID_IUnknown) && !IsEqualIID(iid, &IID_IFileDialogEvents)) return E_NOINTERFACE;
    *out = iface;
    return S_OK;
}
static ULONG WINAPI event_addref(IFileDialogEvents *iface) { return 2; }
static ULONG WINAPI event_release(IFileDialogEvents *iface) { return 1; }
static HRESULT WINAPI file_ok(IFileDialogEvents *iface, IFileDialog *dialog)
{
    IShellItem *item = NULL;
    WCHAR *name = NULL;
    HRESULT hr;
    accepted++;
    ok(!IsWindowEnabled(owner), "Owner enabled inside OnFileOk.\n");
    hr = IFileDialog_GetResult(dialog, &item);
    ok(hr == (selection_count > 1 ? E_FAIL : S_OK), "GetResult %#lx.\n", hr);
    if (item) IShellItem_Release(item);
    hr = IFileDialog_GetFileName(dialog, &name);
    ok(hr == S_OK && name && name[0], "GetFileName %#lx, %s.\n", hr, wine_dbgstr_w(name));
    CoTaskMemFree(name);
    if (veto && accepted == 1)
    {
        IFileDialog_SetFileName(dialog, L"retry");
        return S_FALSE;
    }
    return S_OK;
}
static HRESULT WINAPI folder_changing(IFileDialogEvents *iface, IFileDialog *dialog, IShellItem *folder) { return S_OK; }
static HRESULT WINAPI folder_changed(IFileDialogEvents *iface, IFileDialog *dialog) { return S_OK; }
static HRESULT WINAPI selection_changed(IFileDialogEvents *iface, IFileDialog *dialog) { return S_OK; }
static HRESULT WINAPI share_violation(IFileDialogEvents *iface, IFileDialog *dialog, IShellItem *item,
        FDE_SHAREVIOLATION_RESPONSE *response) { return E_NOTIMPL; }
static HRESULT WINAPI type_changed(IFileDialogEvents *iface, IFileDialog *dialog) { types++; return S_OK; }
static HRESULT WINAPI overwrite(IFileDialogEvents *iface, IFileDialog *dialog, IShellItem *item,
        FDE_OVERWRITE_RESPONSE *response)
{
    overwrites++;
    if (refuse)
    {
        *response = FDEOR_REFUSE;
        IFileDialog_SetFileName(dialog, L"retry");
    }
    return S_OK;
}
static HRESULT WINAPI observer_ok(IFileDialogEvents *iface, IFileDialog *dialog)
{
    observed++;
    return S_OK;
}
static const IFileDialogEventsVtbl observer_vtbl = {
    event_query, event_addref, event_release, observer_ok, folder_changing, folder_changed,
    selection_changed, share_violation, type_changed, overwrite
};
static const IFileDialogEventsVtbl events_vtbl = {
    event_query, event_addref, event_release, file_ok, folder_changing, folder_changed,
    selection_changed, share_violation, type_changed, overwrite
};

static void CALLBACK close_timer(HWND hwnd, UINT msg, UINT_PTR id, DWORD ticks)
{
    IOleWindow *window;
    HWND handle = NULL;
    HRESULT hr;
    KillTimer(hwnd, id);
    ok(!IsWindowEnabled(owner), "Owner not disabled during Show.\n");
    hr = IFileDialog_QueryInterface(active, &IID_IOleWindow, (void **)&window);
    ok(hr == S_OK, "IOleWindow %#lx.\n", hr);
    if (SUCCEEDED(hr))
    {
        hr = IOleWindow_GetWindow(window, &handle);
        ok(hr == S_OK && IsWindow(handle) && handle != owner, "GetWindow %#lx, %p.\n", hr, handle);
        IOleWindow_Release(window);
    }
    hr = IFileDialog_Close(active, E_ABORT);
    ok(hr == S_OK, "Close %#lx.\n", hr);
}

static void run_dialog(const WCHAR *directory, const WCHAR *mode, BOOL save, DWORD options,
        HRESULT expected, const WCHAR *expected_name, unsigned int expected_count)
{
    static const COMDLG_FILTERSPEC filters[] = {{L"Paint.NET", L"*.pdn"}, {L"PNG", L"*.png"}};
    IShellItem *folder = NULL, *item = NULL;
    IFileOpenDialog *open;
    IShellItemArray *array;
    WCHAR *path, full[MAX_PATH];
    DWORD cookie, observer_cookie = 0, count;
    UINT index;
    HRESULT hr;
    ULONGLONG start;

    accepted = types = overwrites = observed = 0;
    selection_count = expected_count;
    hr = CoCreateInstance(save ? &CLSID_FileSaveDialog : &CLSID_FileOpenDialog, NULL,
            CLSCTX_INPROC_SERVER, &IID_IFileDialog, (void **)&active);
    ok(hr == S_OK, "Create %#lx.\n", hr);
    if (FAILED(hr)) return;
    hr = SHCreateItemFromParsingName(directory, NULL, &IID_IShellItem, (void **)&folder);
    ok(hr == S_OK, "Folder %#lx.\n", hr);
    if (folder) { IFileDialog_SetFolder(active, folder); IShellItem_Release(folder); }
    IFileDialog_SetFileTypes(active, ARRAY_SIZE(filters), filters);
    IFileDialog_SetFileTypeIndex(active, 1);
    IFileDialog_SetOptions(active, options | FOS_FORCEFILESYSTEM);
    IFileDialog_SetTitle(active, mode);
    IFileDialog_Advise(active, &events, &cookie);
    if (veto) IFileDialog_Advise(active, &observer, &observer_cookie);
    if (!lstrcmpW(mode, L"wait")) SetTimer(owner, 1, 200, close_timer);
    start = GetTickCount64();
    hr = IFileDialog_Show(active, owner);
    ok(hr == expected, "%s: Show %#lx, expected %#lx.\n", wine_dbgstr_w(mode), hr, expected);
    ok(IsWindow(owner) && IsWindowEnabled(owner), "%s: Owner damaged/disabled.\n", wine_dbgstr_w(mode));
    if (!lstrcmpW(mode, L"wait"))
        ok(GetTickCount64() - start < 5000, "Close did not cancel the native process promptly.\n");
    if (SUCCEEDED(expected))
    {
        hr = IFileDialog_GetFileTypeIndex(active, &index);
        ok(hr == S_OK && index == 2, "Filter %#lx, %u.\n", hr, index);
        ok(types == (veto ? 4 : 2), "Unexpected type notifications: %u.\n", types);
        if (veto) ok(observed == 1, "A later listener received %u OnFileOk calls despite the veto.\n", observed);
        ok(accepted == (veto ? 2 : 1), "OnFileOk called %u times.\n", accepted);
        if (refuse) ok(overwrites == 1, "OnOverwrite called %u times.\n", overwrites);
        if (expected_count == 1)
        {
            hr = IFileDialog_GetResult(active, &item);
            ok(hr == S_OK, "Result %#lx.\n", hr);
        }
        if (!save)
        {
            IFileDialog_QueryInterface(active, &IID_IFileOpenDialog, (void **)&open);
            hr = IFileOpenDialog_GetResults(open, &array);
            ok(hr == S_OK, "GetResults %#lx.\n", hr);
            if (SUCCEEDED(hr))
            {
                IShellItemArray_GetCount(array, &count);
                ok(count == expected_count, "Count %lu, expected %u.\n", count, expected_count);
                if (!item) IShellItemArray_GetItemAt(array, 0, &item);
                IShellItemArray_Release(array);
            }
            IFileOpenDialog_Release(open);
        }
        if (item)
        {
            hr = IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &path);
            ok(hr == S_OK, "Path %#lx.\n", hr);
            if (SUCCEEDED(hr))
            {
                swprintf(full, ARRAY_SIZE(full), L"%s\\%s", directory, expected_name);
                ok(!lstrcmpiW(path, full), "Path %s, expected %s.\n", wine_dbgstr_w(path), wine_dbgstr_w(full));
                CoTaskMemFree(path);
            }
            IShellItem_Release(item);
        }
    }
    if (observer_cookie) IFileDialog_Unadvise(active, observer_cookie);
    IFileDialog_Unadvise(active, cookie);
    IFileDialog_Release(active);
    active = NULL;
}

START_TEST(native_dialog)
{
    WCHAR env[MAX_PATH], directory[MAX_PATH], path[MAX_PATH];
    static const WCHAR *names[] = {L"native \x03b1 one.png", L"native \x03b2 two.png"};
    HANDLE file;
    unsigned int i;
    if (!GetEnvironmentVariableW(L"WINE_NATIVE_DIALOG_TEST", env, ARRAY_SIZE(env)))
    {
        win_skip("Requires paintnet/test-native-dialog.sh and its deterministic transport peer.\n");
        return;
    }
    CoInitialize(NULL);
    events.lpVtbl = &events_vtbl;
    observer.lpVtbl = &observer_vtbl;
    owner = CreateWindowW(L"Static", L"Native dialog test owner", WS_OVERLAPPEDWINDOW,
            0, 0, 100, 100, NULL, NULL, GetModuleHandleW(NULL), NULL);
    ok(owner != NULL, "CreateWindow failed.\n");
    GetTempPathW(ARRAY_SIZE(path), path);
    swprintf(directory, ARRAY_SIZE(directory), L"%snative-dialog-%lu", path, GetCurrentProcessId());
    CreateDirectoryW(directory, NULL);
    for (i = 0; i < ARRAY_SIZE(names); i++)
    {
        swprintf(path, ARRAY_SIZE(path), L"%s\\%s", directory, names[i]);
        file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
        ok(file != INVALID_HANDLE_VALUE, "Creating %s failed.\n", wine_dbgstr_w(path));
        CloseHandle(file);
    }
    run_dialog(directory, L"cancel", FALSE, 0, HRESULT_FROM_WIN32(ERROR_CANCELLED), NULL, 0);
    run_dialog(directory, L"bad_cancel", FALSE, 0, E_FAIL, NULL, 0);
    run_dialog(directory, L"bad_index", FALSE, 0, E_FAIL, NULL, 0);
    run_dialog(directory, L"bad_path", FALSE, FOS_ALLOWMULTISELECT, E_FAIL, NULL, 0);
    run_dialog(directory, L"open", FALSE, FOS_FILEMUSTEXIST, S_OK, names[0], 1);
    run_dialog(directory, L"multi", FALSE, FOS_ALLOWMULTISELECT, S_OK, names[0], 2);
    run_dialog(directory, L"save", TRUE, 0, S_OK, L"native \x03b3 saved.png", 1);
    veto = TRUE;
    run_dialog(directory, L"veto", FALSE, 0, S_OK, names[1], 1);
    veto = FALSE;
    refuse = TRUE;
    run_dialog(directory, L"overwrite", TRUE, FOS_OVERWRITEPROMPT, S_OK, L"native \x03b3 saved.png", 1);
    refuse = FALSE;
    run_dialog(directory, L"wait", FALSE, 0, E_ABORT, NULL, 0);
    for (i = 0; i < ARRAY_SIZE(names); i++)
    {
        swprintf(path, ARRAY_SIZE(path), L"%s\\%s", directory, names[i]);
        DeleteFileW(path);
    }
    RemoveDirectoryW(directory);
    DestroyWindow(owner);
    CoUninitialize();
}
