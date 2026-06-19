#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <string>
#include "LauncherWindow.h"
#include "EditDialog.h"
#include "SettingsDialog.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

static const wchar_t* CLASS_NAME = L"JosephsDeckLauncher";

LauncherWindow::LauncherWindow()  {}
LauncherWindow::~LauncherWindow() { if (m_hwnd) DestroyWindow(m_hwnd); }

bool LauncherWindow::Create(HINSTANCE hInst) {
    m_hInst = hInst;
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = nullptr;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    ATOM atom = RegisterClassExW(&wc);
    if (!atom && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        CLASS_NAME, L"Joseph's Deck",
        WS_POPUP,
        0, 0, WinWidth(), WinHeight(),
        nullptr, nullptr, hInst, this);
    if (!m_hwnd) return false;

    DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    BOOL dark = TRUE;
    DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

    LoadShortcuts();
    CenterWindow();
    return true;
}

void LauncherWindow::LoadShortcuts() {
    wchar_t exe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring dir(exe);
    size_t sl = dir.find_last_of(L"\\/");
    if (sl != std::wstring::npos) dir = dir.substr(0, sl + 1);
    std::wstring jsonPath = dir + L"shortcuts.json";
    if (!m_manager.Load(jsonPath)) m_manager.CreateDefaults(jsonPath);
}

void LauncherWindow::CenterWindow() {
    HMONITOR hM = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = {}; mi.cbSize = sizeof(mi);
    GetMonitorInfoW(hM, &mi);
    int sw = mi.rcWork.right - mi.rcWork.left;
    int sh = mi.rcWork.bottom - mi.rcWork.top;
    SetWindowPos(m_hwnd, HWND_TOPMOST,
        mi.rcWork.left + (sw - WinWidth())  / 2,
        mi.rcWork.top  + (sh - WinHeight()) / 2,
        WinWidth(), WinHeight(), 0);
}

void LauncherWindow::Show() {
    m_readyClose = false;
    m_dragging = false; m_dragSrc = -1; m_dragTarget = -1;
    CenterWindow();
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    SetForegroundWindow(m_hwnd);
    SetTimer(m_hwnd, TIMER_READY, 300, nullptr);
}

void LauncherWindow::Hide() {
    m_hoverIndex = -1; m_pressIndex = -1;
    m_settingsHover = false; m_dotHover = -1;
    ShowWindow(m_hwnd, SW_HIDE);
}

// ── Layout helpers ────────────────────────────────────────────────────────────

RECT LauncherWindow::SettingsBtnRect() const {
    int x = WinWidth() - PADDING - SETT_SZ;
    int y = (TITLE_H  - SETT_SZ) / 2;
    return { x, y, x + SETT_SZ, y + SETT_SZ };
}

std::vector<RECT> LauncherWindow::DotRects() const {
    int count = m_manager.GetProfileCount();
    int total = count * DOT_SZ + (count - 1) * DOT_GAP;
    int cy    = DotsBarY() + DOTS_H / 2;
    int startX = (WinWidth() - total) / 2;
    std::vector<RECT> out;
    for (int i = 0; i < count; i++) {
        int x = startX + i * (DOT_SZ + DOT_GAP);
        out.push_back({ x, cy - DOT_SZ / 2, x + DOT_SZ, cy + (DOT_SZ + 1) / 2 });
    }
    return out;
}

RECT LauncherWindow::ButtonRect(int index) const {
    int col = index % COLS, row = index / COLS;
    int x = PADDING + col * (BTN_W + PADDING);
    int y = ButtonsTop() + PADDING + row * (BTN_H + PADDING);
    return { x, y, x + BTN_W, y + BTN_H };
}

int LauncherWindow::HitTestButton(int x, int y) const {
    for (int i = 0; i < ROWS * COLS; i++) {
        RECT r = ButtonRect(i);
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return i;
    }
    return -1;
}

int LauncherWindow::HitTestDot(int x, int y) const {
    auto dots = DotRects();
    for (int i = 0; i < (int)dots.size(); i++) {
        RECT r = dots[i]; r.left -= 4; r.top -= 4; r.right += 4; r.bottom += 4; // larger hit area
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return i;
    }
    return -1;
}

bool LauncherWindow::HitTestSettings(int x, int y) const {
    RECT r = SettingsBtnRect();
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

// ── Drawing ───────────────────────────────────────────────────────────────────

static HBRUSH MkBrush(COLORREF c) { return CreateSolidBrush(c); }

static void RRect(HDC hdc, RECT r, int rx, COLORREF fill, COLORREF border) {
    HBRUSH hF = MkBrush(fill);
    HPEN   hP = CreatePen(PS_SOLID, 1, border);
    HBRUSH ob = (HBRUSH)SelectObject(hdc, hF);
    HPEN   op = (HPEN)SelectObject(hdc, hP);
    RoundRect(hdc, r.left, r.top, r.right, r.bottom, rx, rx);
    SelectObject(hdc, ob); SelectObject(hdc, op);
    DeleteObject(hF); DeleteObject(hP);
}

static HFONT MakeFont(int size, bool bold = false) {
    return CreateFontW(size, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL,
        0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

void LauncherWindow::DrawTitleBar(HDC hdc) {
    // Background
    RECT tb = { 0, 0, WinWidth(), TITLE_H };
    HBRUSH bg = MkBrush(CLR_TITLE);
    FillRect(hdc, &tb, bg);
    DeleteObject(bg);

    // Separator line
    HPEN sep = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN op  = (HPEN)SelectObject(hdc, sep);
    MoveToEx(hdc, 0, TITLE_H - 1, nullptr);
    LineTo(hdc, WinWidth(), TITLE_H - 1);
    SelectObject(hdc, op);
    DeleteObject(sep);

    SetBkMode(hdc, TRANSPARENT);

    // ── Profile name (left) ──
    {
        HFONT f = MakeFont(12);
        HFONT of = (HFONT)SelectObject(hdc, f);
        SetTextColor(hdc, CLR_SUBTEXT);
        RECT r = { PADDING, 0, WinWidth() / 3, TITLE_H };
        DrawTextW(hdc, m_manager.GetProfileName(m_manager.GetCurrentIndex()).c_str(),
                  -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(hdc, of);
        DeleteObject(f);
    }

    // ── Centred title ──
    {
        HFONT f = MakeFont(15, true);
        HFONT of = (HFONT)SelectObject(hdc, f);
        SetTextColor(hdc, CLR_TEXT);
        RECT r = { 0, 0, WinWidth(), TITLE_H };
        DrawTextW(hdc, L"Joseph's Stream deck", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, of);
        DeleteObject(f);
    }

    // ── Settings button (right) ──
    {
        RECT sb = SettingsBtnRect();
        COLORREF fill = m_settingsHover ? CLR_HOVER : 0x00303030;
        RRect(hdc, sb, 6, fill, CLR_BORDER);
        HFONT f = MakeFont(16);
        HFONT of = (HFONT)SelectObject(hdc, f);
        SetTextColor(hdc, CLR_TEXT);
        DrawTextW(hdc, L"⚙", -1, &sb, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, of);
        DeleteObject(f);
    }
}

static void DrawButtonContent(HDC hdc, RECT r, const Shortcut& sc, COLORREF tc, int iconSz) {
    if (sc.icon) {
        DrawIconEx(hdc,
            r.left + (r.right - r.left - iconSz) / 2,
            r.top + 16,
            sc.icon, iconSz, iconSz, 0, nullptr, DI_NORMAL);
    }
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, tc);
    HFONT f = MakeFont(14);
    HFONT of = (HFONT)SelectObject(hdc, f);
    RECT tr = { r.left + 4, r.bottom - 28, r.right - 4, r.bottom - 6 };
    DrawTextW(hdc, sc.name.c_str(), -1, &tr,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(hdc, of);
    DeleteObject(f);
}

void LauncherWindow::DrawButton(HDC hdc, int index, const Shortcut& sc) {
    RECT r = ButtonRect(index);
    bool isSrc  = (m_dragging && index == m_dragSrc);
    bool isDrop = (m_dragging && index == m_dragTarget && index != m_dragSrc);

    if (isSrc) {
        RRect(hdc, r, CORNER_R, 0x00282828, 0x00404040);
        return;
    }

    COLORREF fill = CLR_BTN, border = CLR_BORDER;
    if (sc.type == ShortcutType::Empty) {
        fill = border = CLR_EMPTY;
        RRect(hdc, r, CORNER_R, fill, border);
        return;
    }
    if (isDrop)                          { fill = 0x00203040; border = CLR_ACCENT; }
    else if (index == m_pressIndex)      { fill = CLR_PRESS; }
    else if (index == m_hoverIndex && !m_dragging) { fill = CLR_HOVER; }

    RRect(hdc, r, CORNER_R, fill, border);
    DrawButtonContent(hdc, r, sc, CLR_TEXT, ICON_SIZE);
}

void LauncherWindow::DrawGhostButton(HDC hdc, const Shortcut& sc) {
    RECT r = { m_dragCurX - BTN_W/2, m_dragCurY - BTN_H/2,
               m_dragCurX + BTN_W/2, m_dragCurY + BTN_H/2 };
    RRect(hdc, r, CORNER_R, 0x00484848, CLR_ACCENT);
    DrawButtonContent(hdc, r, sc, 0x00C8C8C8, ICON_SIZE);
}

void LauncherWindow::DrawDots(HDC hdc) {
    auto dots = DotRects();
    int cur = m_manager.GetCurrentIndex();
    for (int i = 0; i < (int)dots.size(); i++) {
        RECT r = dots[i];
        bool active = (i == cur);
        bool hover  = (i == m_dotHover && !active);
        COLORREF fill   = active ? CLR_TEXT     : (hover ? CLR_BORDER : CLR_EMPTY);
        COLORREF border = active ? CLR_TEXT     : CLR_BORDER;
        HBRUSH hB = MkBrush(fill);
        HPEN   hP = CreatePen(PS_SOLID, 1, border);
        HBRUSH ob = (HBRUSH)SelectObject(hdc, hB);
        HPEN   op = (HPEN)SelectObject(hdc, hP);
        Ellipse(hdc, r.left, r.top, r.right, r.bottom);
        SelectObject(hdc, ob); SelectObject(hdc, op);
        DeleteObject(hB); DeleteObject(hP);
    }
}

void LauncherWindow::OnPaint(HDC hdc) {
    RECT client; GetClientRect(m_hwnd, &client);

    // Main background
    HBRUSH bg = MkBrush(CLR_BG);
    FillRect(hdc, &client, bg);
    DeleteObject(bg);

    // Dots bar background (slightly different shade)
    RECT dotsBar = { 0, DotsBarY(), WinWidth(), WinHeight() };
    HBRUSH dbb = MkBrush(CLR_TITLE);
    FillRect(hdc, &dotsBar, dbb);
    DeleteObject(dbb);

    // Top separator of dots bar
    HPEN sep = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN op  = (HPEN)SelectObject(hdc, sep);
    MoveToEx(hdc, 0, DotsBarY(), nullptr);
    LineTo(hdc, WinWidth(), DotsBarY());
    SelectObject(hdc, op); DeleteObject(sep);

    // Window outline
    HPEN bord = CreatePen(PS_SOLID, 1, CLR_BORDER);
    op = (HPEN)SelectObject(hdc, bord);
    HBRUSH nb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, 0, 0, WinWidth() - 1, WinHeight() - 1, CORNER_R * 2, CORNER_R * 2);
    SelectObject(hdc, op); SelectObject(hdc, nb);
    DeleteObject(bord);

    DrawTitleBar(hdc);

    const auto& shortcuts = m_manager.GetShortcuts();
    for (int i = 0; i < ROWS * COLS; i++) {
        if (i < (int)shortcuts.size()) DrawButton(hdc, i, shortcuts[i]);
        else { Shortcut e; e.type = ShortcutType::Empty; DrawButton(hdc, i, e); }
    }

    if (m_dragging && m_dragSrc >= 0 && m_dragSrc < (int)shortcuts.size() &&
        shortcuts[m_dragSrc].type != ShortcutType::Empty)
        DrawGhostButton(hdc, shortcuts[m_dragSrc]);

    DrawDots(hdc);
}

// ── Input ─────────────────────────────────────────────────────────────────────

void LauncherWindow::OnMouseMove(int x, int y) {
    // Drag threshold check
    if (m_pressIndex >= 0 && !m_dragging) {
        int dx = x - m_dragStartX, dy = y - m_dragStartY;
        if (dx*dx + dy*dy > 25) {
            const auto& sc = m_manager.GetShortcuts();
            if (m_pressIndex < (int)sc.size() && sc[m_pressIndex].type != ShortcutType::Empty) {
                m_dragging = true; m_dragSrc = m_pressIndex;
            }
        }
    }
    if (m_dragging) {
        m_dragCurX = x; m_dragCurY = y;
        m_dragTarget = HitTestButton(x, y);
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    bool prevSett = m_settingsHover;
    int  prevDot  = m_dotHover;
    int  prevHov  = m_hoverIndex;

    m_settingsHover = HitTestSettings(x, y);
    m_dotHover      = HitTestDot(x, y);
    m_hoverIndex    = HitTestButton(x, y);

    if (prevSett != m_settingsHover || prevDot != m_dotHover || prevHov != m_hoverIndex)
        InvalidateRect(m_hwnd, nullptr, FALSE);

    TRACKMOUSEEVENT tme = {}; tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE; tme.hwndTrack = m_hwnd;
    TrackMouseEvent(&tme);
}

void LauncherWindow::OnMouseLeave() {
    if (!m_dragging) {
        m_hoverIndex = -1; m_pressIndex = -1;
        m_settingsHover = false; m_dotHover = -1;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void LauncherWindow::OnLButtonDown(int x, int y) {
    m_pressIndex = HitTestButton(x, y);
    if (m_pressIndex >= 0) {
        m_dragStartX = x; m_dragStartY = y;
        SetCapture(m_hwnd);
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void LauncherWindow::OnLButtonUp(int x, int y) {
    ReleaseCapture();

    if (m_dragging) {
        int drop = HitTestButton(x, y);
        if (drop >= 0 && drop != m_dragSrc) { m_manager.SwapShortcuts(m_dragSrc, drop); m_manager.Save(); }
        m_dragging = false; m_dragSrc = -1; m_dragTarget = -1;
        m_pressIndex = -1; m_hoverIndex = HitTestButton(x, y);
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    // Settings button
    if (HitTestSettings(x, y)) { OnSettingsClick(); return; }

    // Profile dot
    int dot = HitTestDot(x, y);
    if (dot >= 0) { OnDotClick(dot); return; }

    // Button click
    int clicked = HitTestButton(x, y);
    int pressed = m_pressIndex;
    m_pressIndex = -1;
    InvalidateRect(m_hwnd, nullptr, FALSE);
    if (clicked >= 0 && clicked == pressed) {
        const auto& sc = m_manager.GetShortcuts();
        if (clicked < (int)sc.size() && sc[clicked].type != ShortcutType::Empty) {
            ActionExecutor::Execute(sc[clicked]);
            Hide();
        }
    }
}

void LauncherWindow::OnRButtonUp(int x, int y) {
    if (m_dragging) return;
    int idx = HitTestButton(x, y);
    if (idx < 0) return;

    const auto& sc = m_manager.GetShortcuts();
    Shortcut cur = (idx < (int)sc.size()) ? sc[idx] : Shortcut{};

    m_editOpen = true;
    EditResult res = ShowEditDialog(m_hwnd, cur);
    m_editOpen = false;

    if (res.ok) {
        Shortcut upd; upd.name = res.name; upd.target = res.target; upd.type = res.type;
        m_manager.SetShortcut(idx, upd);
        m_manager.Save();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
    SetForegroundWindow(m_hwnd);
}

void LauncherWindow::OnSettingsClick() {
    m_editOpen = true;
    ShowSettingsDialog(m_hwnd, m_manager);
    m_editOpen = false;
    // Current profile may have changed — ensure index is valid
    InvalidateRect(m_hwnd, nullptr, FALSE);
    SetForegroundWindow(m_hwnd);
}

void LauncherWindow::OnDotClick(int idx) {
    if (idx != m_manager.GetCurrentIndex()) {
        m_manager.SetCurrentProfile(idx);
        m_manager.Save();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

// ── WndProc ───────────────────────────────────────────────────────────────────

LRESULT LauncherWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(m_hwnd, &ps);
        RECT rc; GetClientRect(m_hwnd, &rc);
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HBITMAP ob  = (HBITMAP)SelectObject(mem, bmp);
        OnPaint(mem);
        BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, ob); DeleteObject(bmp); DeleteDC(mem);
        EndPaint(m_hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;

    case WM_SETCURSOR:
        if (m_dragging) { SetCursor(LoadCursor(nullptr, IDC_SIZEALL)); return TRUE; }
        break;

    case WM_MOUSEMOVE:   OnMouseMove(LOWORD(lParam), HIWORD(lParam)); return 0;
    case WM_MOUSELEAVE:  OnMouseLeave(); return 0;
    case WM_LBUTTONDOWN: OnLButtonDown(LOWORD(lParam), HIWORD(lParam)); return 0;
    case WM_LBUTTONUP:   OnLButtonUp(LOWORD(lParam), HIWORD(lParam)); return 0;
    case WM_RBUTTONUP:   OnRButtonUp(LOWORD(lParam), HIWORD(lParam)); return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) { Hide(); PostQuitMessage(0); }
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_READY) { KillTimer(m_hwnd, TIMER_READY); m_readyClose = true; }
        return 0;

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE && m_readyClose && !m_editOpen) {
            HWND activating = (HWND)lParam;
            if (activating && GetParent(activating) == m_hwnd) return 0;
            Hide();
            PostQuitMessage(0);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK LauncherWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    LauncherWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        self = (LauncherWindow*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        self->m_hwnd = hwnd;
    } else {
        self = (LauncherWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }
    if (self) return self->HandleMessage(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
