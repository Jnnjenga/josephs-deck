#include <windows.h>
#include <string>
#include "SettingsDialog.h"

enum {
    ID_LIST     = 201,
    ID_ADD      = 202,
    ID_DELETE   = 203,
    ID_NAMEDIT  = 204,
    ID_RENAME   = 205,
    ID_CLOSE    = 206,
};

struct SettState {
    ShortcutManager* mgr;
    bool             running;
    HWND             hList;
    HWND             hNameEdit;
    HWND             hAddBtn;
    HWND             hDelBtn;
    HWND             hRenBtn;
};

static void RefreshList(SettState* s, int selectIdx = -1) {
    SendMessageW(s->hList, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < s->mgr->GetProfileCount(); i++)
        SendMessageW(s->hList, LB_ADDSTRING, 0, (LPARAM)s->mgr->GetProfileName(i).c_str());
    if (selectIdx < 0) selectIdx = s->mgr->GetCurrentIndex();
    SendMessageW(s->hList, LB_SETCURSEL, selectIdx, 0);
    // Sync name edit from selection
    int sel = (int)SendMessageW(s->hList, LB_GETCURSEL, 0, 0);
    if (sel >= 0 && sel < s->mgr->GetProfileCount())
        SetWindowTextW(s->hNameEdit, s->mgr->GetProfileName(sel).c_str());
    // Enable/disable delete (can't delete the last profile)
    EnableWindow(s->hDelBtn, s->mgr->GetProfileCount() > 1 ? TRUE : FALSE);
}

static LRESULT CALLBACK SettProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettState* s = (SettState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
        s = (SettState*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)s);

        HFONT font = CreateFontW(15, 0, 0, 0, FW_NORMAL, 0, 0, 0,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

        auto Mk = [&](DWORD ex, const wchar_t* cls, const wchar_t* txt, DWORD sty,
                      int x, int y, int w, int h, int id) -> HWND {
            HWND hw = CreateWindowExW(ex, cls, txt, WS_CHILD|WS_VISIBLE|sty,
                x, y, w, h, hwnd, (HMENU)(UINT_PTR)id, GetModuleHandleW(nullptr), nullptr);
            SendMessageW(hw, WM_SETFONT, (WPARAM)font, TRUE);
            return hw;
        };

        // "Profiles" heading
        Mk(0, L"STATIC", L"Profiles:", WS_GROUP, 14, 14, 100, 20, 0);

        // Profile listbox
        s->hList = Mk(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr,
                      LBS_NOTIFY | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
                      14, 38, 252, 130, ID_LIST);

        // Add / Delete buttons
        s->hAddBtn = Mk(0, L"BUTTON", L"+",  BS_PUSHBUTTON|WS_TABSTOP, 274, 38, 34, 30, ID_ADD);
        s->hDelBtn = Mk(0, L"BUTTON", L"−",  BS_PUSHBUTTON|WS_TABSTOP, 274, 76, 34, 30, ID_DELETE);

        // Name label + edit
        Mk(0, L"STATIC", L"Name:", 0, 14, 182, 50, 20, 0);
        s->hNameEdit = Mk(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
                          ES_AUTOHSCROLL|WS_TABSTOP, 68, 180, 240, 26, ID_NAMEDIT);

        // Rename / Done
        s->hRenBtn = Mk(0, L"BUTTON", L"Rename", BS_PUSHBUTTON|WS_TABSTOP, 14, 220, 90, 30, ID_RENAME);
        Mk(0, L"BUTTON", L"Done", BS_PUSHBUTTON|WS_TABSTOP|BS_DEFPUSHBUTTON, 218, 220, 90, 30, ID_CLOSE);

        RefreshList(s);
        return 0;
    }

    case WM_COMMAND: {
        if (!s) return 0;
        int id    = LOWORD(wParam);
        int notif = HIWORD(wParam);

        if (id == ID_LIST && notif == LBN_SELCHANGE) {
            int sel = (int)SendMessageW(s->hList, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < s->mgr->GetProfileCount())
                SetWindowTextW(s->hNameEdit, s->mgr->GetProfileName(sel).c_str());
            EnableWindow(s->hDelBtn, s->mgr->GetProfileCount() > 1 ? TRUE : FALSE);
        }

        if (id == ID_ADD) {
            // Build a unique name
            std::wstring base = L"New Profile";
            std::wstring name = base;
            int n = 2;
            bool clash = true;
            while (clash) {
                clash = false;
                for (int i = 0; i < s->mgr->GetProfileCount(); i++) {
                    if (s->mgr->GetProfileName(i) == name) { clash = true; break; }
                }
                if (clash) name = base + L" " + std::to_wstring(n++);
            }
            s->mgr->AddProfile(name);
            s->mgr->Save();
            RefreshList(s, s->mgr->GetProfileCount() - 1);
        }

        if (id == ID_DELETE) {
            int sel = (int)SendMessageW(s->hList, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && s->mgr->GetProfileCount() > 1) {
                s->mgr->DeleteProfile(sel);
                s->mgr->Save();
                RefreshList(s);
            }
        }

        if (id == ID_RENAME) {
            int sel = (int)SendMessageW(s->hList, LB_GETCURSEL, 0, 0);
            if (sel >= 0) {
                wchar_t buf[256] = {};
                GetWindowTextW(s->hNameEdit, buf, 256);
                std::wstring name(buf);
                if (!name.empty()) {
                    s->mgr->RenameProfile(sel, name);
                    s->mgr->Save();
                    RefreshList(s, sel);
                }
            }
        }

        if (id == ID_CLOSE) s->running = false;
        return 0;
    }

    case WM_CLOSE:
        if (s) s->running = false;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void ShowSettingsDialog(HWND parent, ShortcutManager& manager) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = SettProc;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"JDeckSettings";
        RegisterClassExW(&wc);
        registered = true;
    }

    SettState state = {};
    state.mgr     = &manager;
    state.running = true;

    RECT rc = { 0, 0, 326, 268 };
    AdjustWindowRectEx(&rc, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE,
                       WS_EX_DLGMODALFRAME | WS_EX_TOPMOST);
    int dw = rc.right - rc.left, dh = rc.bottom - rc.top;

    RECT pr; GetWindowRect(parent, &pr);
    int x = pr.left + ((pr.right - pr.left) - dw) / 2;
    int y = pr.top  + ((pr.bottom - pr.top) - dh) / 2;

    HWND dlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"JDeckSettings", L"Profiles",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, dw, dh,
        parent, nullptr, GetModuleHandleW(nullptr), &state);

    ShowWindow(dlg, SW_SHOW);
    UpdateWindow(dlg);
    EnableWindow(parent, FALSE);

    MSG msg;
    while (state.running) {
        if (!GetMessageW(&msg, nullptr, 0, 0)) break;
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    DestroyWindow(dlg);
}
