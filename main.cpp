#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include "LauncherWindow.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // COM is needed by ShellExecuteW (URL handling) and SHBrowseForFolderW
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"JosephsDeckMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        CoUninitialize();
        return 0;
    }

    LauncherWindow launcher;
    if (!launcher.Create(hInstance)) {
        MessageBoxW(nullptr, L"Failed to create launcher window.", L"Joseph's Deck", MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    launcher.Show();

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CloseHandle(hMutex);
    CoUninitialize();
    return 0;
}
