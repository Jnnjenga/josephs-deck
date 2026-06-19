#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <string>
#include "EditDialog.h"

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

// Child control IDs
enum {
    ID_EDIT_NAME        = 101,
    ID_COMBO_TYPE       = 102,
    ID_EDIT_TARGET      = 103,
    ID_BTN_BROWSE       = 104,
    ID_EDIT_ICON        = 105,
    ID_BTN_BROWSE_ICON  = 106,
    ID_BTN_OK           = IDOK,
    ID_BTN_CANCEL       = IDCANCEL,
};

static const wchar_t* TYPE_LABELS[] = {
    L"application", L"website", L"folder", L"file", L"command"
};

static ShortcutType TypeFromIndex(int i) {
    switch (i) {
        case 0: return ShortcutType::Application;
        case 1: return ShortcutType::Website;
        case 2: return ShortcutType::Folder;
        case 3: return ShortcutType::File;
        case 4: return ShortcutType::Command;
        default: return ShortcutType::Application;
    }
}

static int IndexFromType(ShortcutType t) {
    switch (t) {
        case ShortcutType::Application: return 0;
        case ShortcutType::Website:     return 1;
        case ShortcutType::Folder:      return 2;
        case ShortcutType::File:        return 3;
        case ShortcutType::Command:     return 4;
        default:                        return 0;
    }
}

struct DlgState {
    const Shortcut* src;
    EditResult      result;
    bool            running;
    HWND            hBrowse;
    HWND            hBrowseIcon;
    HWND            hTypeCombo;
};

static void UpdateBrowseState(DlgState* s) {
    int sel = (int)SendMessageW(s->hTypeCombo, CB_GETCURSEL, 0, 0);
    ShortcutType t = TypeFromIndex(sel < 0 ? 0 : sel);
    bool canBrowse = (t == ShortcutType::Application ||
                      t == ShortcutType::File        ||
                      t == ShortcutType::Folder);
    EnableWindow(s->hBrowse, canBrowse ? TRUE : FALSE);
}

static LRESULT CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DlgState* s = (DlgState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
        s = (DlgState*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)s);

        HFONT font = CreateFontW(15, 0, 0, 0, FW_NORMAL, 0, 0, 0,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

        auto MkLabel = [&](const wchar_t* t, int x, int y, int w, int ht) {
            HWND ctrl = CreateWindowExW(0, L"STATIC", t, WS_CHILD | WS_VISIBLE | SS_RIGHT,
                x, y, w, ht, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
            SendMessageW(ctrl, WM_SETFONT, (WPARAM)font, TRUE);
        };
        auto MkEdit = [&](int id, const wchar_t* t, int x, int y, int w, int ht) -> HWND {
            HWND ctrl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", t,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                x, y, w, ht, hwnd, (HMENU)(UINT_PTR)id,
                GetModuleHandleW(nullptr), nullptr);
            SendMessageW(ctrl, WM_SETFONT, (WPARAM)font, TRUE);
            return ctrl;
        };
        auto MkButton = [&](int id, const wchar_t* t, int x, int y, int w, int ht) -> HWND {
            HWND ctrl = CreateWindowExW(0, L"BUTTON", t,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                x, y, w, ht, hwnd, (HMENU)(UINT_PTR)id,
                GetModuleHandleW(nullptr), nullptr);
            SendMessageW(ctrl, WM_SETFONT, (WPARAM)font, TRUE);
            return ctrl;
        };

        // Row layout — label column left-aligned right-justified, controls start at IX
        const int LX = 10, LW = 64, IX = 80, IH = 26;
        const int EW = 300;

        MkLabel(L"Name:",   LX, 18,  LW, 22);
        MkEdit(ID_EDIT_NAME, s->src->name.c_str(), IX, 16, EW, IH);

        MkLabel(L"Type:",   LX, 58,  LW, 22);
        HWND combo = CreateWindowExW(0, L"COMBOBOX", nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            IX, 56, 160, 130, hwnd, (HMENU)ID_COMBO_TYPE,
            GetModuleHandleW(nullptr), nullptr);
        SendMessageW(combo, WM_SETFONT, (WPARAM)font, TRUE);
        for (auto lbl : TYPE_LABELS)
            SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)lbl);
        SendMessageW(combo, CB_SETCURSEL, IndexFromType(s->src->type), 0);
        s->hTypeCombo = combo;

        MkLabel(L"Target:", LX, 98,  LW, 22);
        MkEdit(ID_EDIT_TARGET, s->src->target.c_str(), IX, 96, EW - 96, IH);
        s->hBrowse = MkButton(ID_BTN_BROWSE, L"Browse...", IX + EW - 92, 96, 92, IH);

        MkLabel(L"Icon:",   LX, 134, LW, 22);
        MkEdit(ID_EDIT_ICON, s->src->iconPath.c_str(), IX, 132, EW - 96, IH);
        s->hBrowseIcon = MkButton(ID_BTN_BROWSE_ICON, L"Browse...", IX + EW - 92, 132, 92, IH);

        MkButton(ID_BTN_OK,     L"OK",     IX + EW - 172, 178, 80, 30);
        MkButton(ID_BTN_CANCEL, L"Cancel", IX + EW -  86, 178, 80, 30);

        UpdateBrowseState(s);
        return 0;
    }

    case WM_COMMAND: {
        if (!s) return 0;
        int id    = LOWORD(wParam);
        int notif = HIWORD(wParam);

        if (id == ID_COMBO_TYPE && notif == CBN_SELCHANGE)
            UpdateBrowseState(s);

        if (id == ID_BTN_BROWSE) {
            int sel = (int)SendMessageW(s->hTypeCombo, CB_GETCURSEL, 0, 0);
            ShortcutType t = TypeFromIndex(sel < 0 ? 0 : sel);
            wchar_t buf[MAX_PATH] = {};
            GetDlgItemTextW(hwnd, ID_EDIT_TARGET, buf, MAX_PATH);

            if (t == ShortcutType::Folder) {
                BROWSEINFOW bi = {};
                bi.hwndOwner  = hwnd;
                bi.lpszTitle  = L"Select folder";
                bi.ulFlags    = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
                LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
                if (pidl) {
                    wchar_t path[MAX_PATH];
                    if (SHGetPathFromIDListW(pidl, path))
                        SetDlgItemTextW(hwnd, ID_EDIT_TARGET, path);
                    CoTaskMemFree(pidl);
                }
            } else {
                OPENFILENAMEW ofn = {};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner   = hwnd;
                ofn.lpstrFile   = buf;
                ofn.nMaxFile    = MAX_PATH;
                ofn.lpstrFilter = L"Executables\0*.exe\0All Files\0*.*\0";
                ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                if (GetOpenFileNameW(&ofn))
                    SetDlgItemTextW(hwnd, ID_EDIT_TARGET, buf);
            }
        }

        if (id == ID_BTN_BROWSE_ICON) {
            wchar_t buf[MAX_PATH] = {};
            GetDlgItemTextW(hwnd, ID_EDIT_ICON, buf, MAX_PATH);
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner   = hwnd;
            ofn.lpstrFile   = buf;
            ofn.nMaxFile    = MAX_PATH;
            ofn.lpstrFilter = L"Image Files\0*.ico;*.png;*.jpg;*.jpeg;*.bmp\0All Files\0*.*\0";
            ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (GetOpenFileNameW(&ofn))
                SetDlgItemTextW(hwnd, ID_EDIT_ICON, buf);
        }

        if (id == ID_BTN_OK) {
            wchar_t buf[2048] = {};
            GetDlgItemTextW(hwnd, ID_EDIT_NAME,   buf, 2048); s->result.name     = buf;
            GetDlgItemTextW(hwnd, ID_EDIT_TARGET, buf, 2048); s->result.target   = buf;
            GetDlgItemTextW(hwnd, ID_EDIT_ICON,   buf, 2048); s->result.iconPath = buf;
            int sel = (int)SendMessageW(s->hTypeCombo, CB_GETCURSEL, 0, 0);
            s->result.type    = TypeFromIndex(sel < 0 ? 0 : sel);
            s->result.ok      = true;
            s->running        = false;
        }
        if (id == ID_BTN_CANCEL)
            s->running = false;

        return 0;
    }

    case WM_CLOSE:
        if (s) s->running = false;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

EditResult ShowEditDialog(HWND parent, const Shortcut& current) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = DlgProc;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"JDeckEdit";
        RegisterClassExW(&wc);
        registered = true;
    }

    DlgState state = {};
    state.src     = &current;
    state.running = true;

    // Client area: 390 × 220 (added icon row)
    RECT rc = { 0, 0, 390, 220 };
    AdjustWindowRectEx(&rc, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE,
                       WS_EX_DLGMODALFRAME | WS_EX_TOPMOST);
    int dw = rc.right  - rc.left;
    int dh = rc.bottom - rc.top;

    RECT pr; GetWindowRect(parent, &pr);
    int x = pr.left + ((pr.right - pr.left) - dw) / 2;
    int y = pr.top  + ((pr.bottom - pr.top) - dh) / 2;

    HWND dlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"JDeckEdit", L"Edit Shortcut",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, dw, dh,
        parent, nullptr, GetModuleHandleW(nullptr), &state);

    ShowWindow(dlg, SW_SHOW);
    UpdateWindow(dlg);

    EnableWindow(parent, FALSE);

    MSG msg;
    while (state.running) {
        if (!GetMessageW(&msg, nullptr, 0, 0)) break;
        if (msg.message == WM_KEYDOWN && msg.wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) {
            SendMessageW(GetFocus(), EM_SETSEL, 0, -1);
            continue;
        }
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    DestroyWindow(dlg);

    return state.result;
}
