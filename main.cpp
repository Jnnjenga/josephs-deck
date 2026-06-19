#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <gdiplus.h>
#include "LauncherWindow.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    Gdiplus::GdiplusStartupInput gdipInput;
    ULONG_PTR gdipToken = 0;
    Gdiplus::GdiplusStartup(&gdipToken, &gdipInput, nullptr);

    // COM is needed by ShellExecuteW (URL handling) and SHBrowseForFolderW
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"JosephsDeckMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Toggle existing instance (show if hidden, hide if visible)
        UINT toggleMsg = RegisterWindowMessageW(L"JosephsDeckToggle");
        HWND existing  = FindWindowW(L"JosephsDeckLauncher", nullptr);
        if (existing && toggleMsg) PostMessageW(existing, toggleMsg, 0, 0);
        CloseHandle(hMutex);
        CoUninitialize();
        Gdiplus::GdiplusShutdown(gdipToken);
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
    Gdiplus::GdiplusShutdown(gdipToken);
    return 0;
}
