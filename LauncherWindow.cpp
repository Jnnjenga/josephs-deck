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

// SetWindowCompositionAttribute — undocumented acrylic API, available Win10 1803+
namespace {
    struct AccentPolicy { DWORD state, flags, color, animId; };
    struct WcaData     { DWORD attr; void* data; SIZE_T sz; };
    using  PfnSwca = BOOL (WINAPI*)(HWND, WcaData*);
}
static constexpr DWORD WCA_ACCENT_POLICY          = 19;
static constexpr DWORD ACCENT_ACRYLICBLURBEHIND   =  4;
static constexpr COLORREF KEY_COLOR = RGB(1, 1, 1); // transparent hole colour

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
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        CLASS_NAME, L"Joseph's Deck",
        WS_POPUP,
        0, 0, WinWidth(), WinHeight(),
        nullptr, nullptr, hInst, this);
    if (!m_hwnd) return false;

    DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    BOOL dark = TRUE;
    DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

    ComputePalette();

    // KEY_COLOR pixels become transparent; non-key pixels drawn at 82% opacity
    SetLayeredWindowAttributes(m_hwnd, KEY_COLOR, 210, LWA_COLORKEY | LWA_ALPHA);

    // Acrylic frosted-glass effect with accent tint (Win10 1803+ / Win11)
    if (auto pSwca = (PfnSwca)GetProcAddress(GetModuleHandleW(L"user32.dll"),
                                              "SetWindowCompositionAttribute")) {
        // Tint: accent colour at 38% opacity (0x60); rest is frosted blur
        DWORD tint = (0x60u << 24) | (CLR_ACCENT & 0x00FFFFFFu);
        AccentPolicy ap  = { ACCENT_ACRYLICBLURBEHIND, 0x20, tint, 0 };
        WcaData      wd  = { WCA_ACCENT_POLICY, &ap, sizeof(ap) };
        pSwca(m_hwnd, &wd);
    }

    m_toggleMsg = RegisterWindowMessageW(L"JosephsDeckToggle");
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

void LauncherWindow::ComputePalette() {
    DWORD dwColor = 0; BOOL opaque = FALSE;
    int ar = 0x55, ag = 0x88, ab = 0xDD; // fallback blue
    if (SUCCEEDED(DwmGetColorizationColor(&dwColor, &opaque))) {
        ar = (int)((dwColor >> 16) & 0xFF);
        ag = (int)((dwColor >>  8) & 0xFF);
        ab = (int)( dwColor        & 0xFF);
    }
    CLR_ACCENT = RGB(ar, ag, ab);

    // Compute dark accent-tinted base (used for hover/press/settings tiles)
    int r = std::max(2, ar * 15 / 100 + 0x18 * 85 / 100);
    int g = std::max(2, ag * 15 / 100 + 0x18 * 85 / 100);
    int b = std::max(2, ab * 15 / 100 + 0x18 * 85 / 100);
    m_clrBase = RGB(r, g, b);

    // Every background is the key colour → fully transparent → uniform frosted glass
    CLR_BG = CLR_TITLE = CLR_BTN = CLR_EMPTY = KEY_COLOR;

    // Interaction colours (non-key, visible on glass)
    CLR_HOVER = RGB(std::min(255, r + 0x18), std::min(255, g + 0x18), std::min(255, b + 0x18));
    CLR_PRESS = RGB(std::max(2,   r - 0x12), std::max(2,   g - 0x12), std::max(2,   b - 0x12));
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
    CommitRename();
    if (m_hRenEdit) ShowWindow(m_hRenEdit, SW_HIDE);
    m_view = View::Main;
    m_editOpen = false;
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
    for (int i = 0; i < Rows() * COLS; i++) {
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

std::vector<RECT> LauncherWindow::GridBtnRects() const {
    const int bw = 72, bh = 22, gap = 12;
    int total = 3 * bw + 2 * gap;
    int sx = (WinWidth() - total) / 2;
    int cy = DotsBarY() + DOTS_H / 2;
    std::vector<RECT> out;
    for (int i = 0; i < 3; i++) {
        int x = sx + i * (bw + gap);
        out.push_back({ x, cy - bh / 2, x + bw, cy + bh / 2 });
    }
    return out;
}

int LauncherWindow::HitTestGridBtn(int x, int y) const {
    if (y < DotsBarY() || y >= DotsBarY() + DOTS_H) return -1;
    auto rects = GridBtnRects();
    for (int i = 0; i < (int)rects.size(); i++) {
        RECT r = rects[i]; r.left -= 4; r.right += 4;
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return i;
    }
    return -1;
}

bool LauncherWindow::HitTestSettings(int x, int y) const {
    RECT r = SettingsBtnRect();
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

// ── Drawing ───────────────────────────────────────────────────────────────────

static WNDPROC s_origRenProc = nullptr;
static LRESULT CALLBACK RenEditProc(HWND hw, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_KEYDOWN) {
        if (w == VK_RETURN) { SendMessageW(GetParent(hw), WM_APP+1, 0, 0); return 0; }
        if (w == VK_ESCAPE) { SendMessageW(GetParent(hw), WM_APP+2, 0, 0); return 0; }
        if (w == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) { SendMessageW(hw, EM_SETSEL, 0, -1); return 0; }
    }
    return CallWindowProcW(s_origRenProc, hw, m, w, l);
}

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

    if (m_view == View::Settings) {
        // ── Settings mode: "Profiles" centred, "←" on the right ──
        HFONT f = MakeFont(15, true);
        HFONT of = (HFONT)SelectObject(hdc, f);
        SetTextColor(hdc, CLR_TEXT);
        RECT r = { 0, 0, WinWidth(), TITLE_H };
        DrawTextW(hdc, L"Profiles", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, of);
        DeleteObject(f);

        RECT sb = SettingsBtnRect();
        COLORREF fill = m_settingsHover ? CLR_HOVER : CLR_BTN;
        RRect(hdc, sb, 6, fill, CLR_BORDER);
        HFONT fb = MakeFont(16);
        HFONT ofb = (HFONT)SelectObject(hdc, fb);
        SetTextColor(hdc, CLR_TEXT);
        DrawTextW(hdc, L"←", -1, &sb, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, ofb);
        DeleteObject(fb);
    } else {
        // ── Normal mode ──
        HFONT f = MakeFont(12);
        HFONT of = (HFONT)SelectObject(hdc, f);
        SetTextColor(hdc, CLR_SUBTEXT);
        RECT r = { PADDING, 0, WinWidth() / 3, TITLE_H };
        DrawTextW(hdc, m_manager.GetProfileName(m_manager.GetCurrentIndex()).c_str(),
                  -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(hdc, of);
        DeleteObject(f);

        HFONT fc = MakeFont(15, true);
        HFONT ofc = (HFONT)SelectObject(hdc, fc);
        SetTextColor(hdc, CLR_TEXT);
        RECT rc = { 0, 0, WinWidth(), TITLE_H };
        DrawTextW(hdc, L"Joseph's Stream deck", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, ofc);
        DeleteObject(fc);

        RECT sb = SettingsBtnRect();
        COLORREF fill = m_settingsHover ? CLR_HOVER : CLR_BTN;
        RRect(hdc, sb, 6, fill, CLR_BORDER);
        HFONT fs = MakeFont(16);
        HFONT ofs = (HFONT)SelectObject(hdc, fs);
        SetTextColor(hdc, CLR_TEXT);
        DrawTextW(hdc, L"⚙", -1, &sb, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, ofs);
        DeleteObject(fs);
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

    COLORREF fill = CLR_BTN, border = CLR_BTN;
    if (sc.type == ShortcutType::Empty) {
        bool hov = (index == m_hoverIndex && !m_dragging);
        RRect(hdc, r, CORNER_R, hov ? CLR_HOVER : m_clrBase, hov ? CLR_HOVER : m_clrBase);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, hov ? CLR_TEXT : CLR_SUBTEXT);
        HFONT  pf = MakeFont(28);
        HFONT opf = (HFONT)SelectObject(hdc, pf);
        DrawTextW(hdc, L"+", 1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, opf);
        DeleteObject(pf);
        return;
    }
    if (isDrop) {
        fill = RGB(GetRValue(CLR_ACCENT) * 25 / 100,
                   GetGValue(CLR_ACCENT) * 25 / 100,
                   GetBValue(CLR_ACCENT) * 25 / 100);
        border = CLR_ACCENT;
    }
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
    if (m_view == View::Settings) {
        static const wchar_t* labels[]  = { L"2×4", L"3×4", L"4×4" };
        static const int      rowVals[] = { 2, 3, 4 };
        int curRows = m_manager.GetRows();
        auto rects  = GridBtnRects();
        for (int i = 0; i < 3; i++) {
            bool active = (rowVals[i] == curRows);
            bool hov    = (m_dotHover == i);
            COLORREF bg = active ? CLR_ACCENT : (hov ? CLR_HOVER : CLR_EMPTY);
            COLORREF fg = active ? CLR_BG     : (hov ? CLR_TEXT  : CLR_SUBTEXT);
            RRect(hdc, rects[i], 5, bg, active ? CLR_ACCENT : CLR_BORDER);
            HFONT f = MakeFont(11); HFONT of = (HFONT)SelectObject(hdc, f);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, fg);
            DrawTextW(hdc, labels[i], -1, &rects[i], DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, of); DeleteObject(f);
        }
        return;
    }
    auto dots = DotRects();
    int cur = m_manager.GetCurrentIndex();
    for (int i = 0; i < (int)dots.size(); i++) {
        RECT r = dots[i];
        bool active = (i == cur);
        bool hover  = (i == m_dotHover && !active);
        COLORREF fill   = active ? CLR_TEXT  : (hover ? CLR_BORDER : CLR_EMPTY);
        COLORREF border = active ? CLR_TEXT  : CLR_BORDER;
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

    if (m_view == View::Settings) {
        DrawSettings(hdc);
    } else {
        const auto& shortcuts = m_manager.GetShortcuts();
        for (int i = 0; i < Rows() * COLS; i++) {
            if (i < (int)shortcuts.size()) DrawButton(hdc, i, shortcuts[i]);
            else { Shortcut e; e.type = ShortcutType::Empty; DrawButton(hdc, i, e); }
        }
        if (m_dragging && m_dragSrc >= 0 && m_dragSrc < (int)shortcuts.size() &&
            shortcuts[m_dragSrc].type != ShortcutType::Empty)
            DrawGhostButton(hdc, shortcuts[m_dragSrc]);
    }

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
    m_dotHover      = (m_view == View::Settings) ? HitTestGridBtn(x, y) : HitTestDot(x, y);
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

    // Settings / back button
    if (HitTestSettings(x, y)) {
        if (m_view == View::Settings) ExitSettings();
        else OnSettingsClick();
        return;
    }

    // Profile dot (main view only — settings mode uses this area for grid size)
    if (m_view != View::Settings) {
        int dot = HitTestDot(x, y);
        if (dot >= 0) { OnDotClick(dot); return; }
    }

    // Settings view tile + grid size clicks
    if (m_view == View::Settings) { OnSettViewLClick(x, y); return; }

    // Button click
    int clicked = HitTestButton(x, y);
    int pressed = m_pressIndex;
    m_pressIndex = -1;
    InvalidateRect(m_hwnd, nullptr, FALSE);
    if (clicked >= 0 && clicked == pressed) {
        const auto& sc = m_manager.GetShortcuts();
        bool isEmpty = (clicked >= (int)sc.size() || sc[clicked].type == ShortcutType::Empty);
        if (!isEmpty) {
            ActionExecutor::Execute(sc[clicked]);
            Hide();
        } else {
            Shortcut empty{};
            m_editOpen = true;
            EditResult res = ShowEditDialog(m_hwnd, empty);
            m_editOpen = false;
            if (res.ok) {
                Shortcut upd; upd.name = res.name; upd.target = res.target; upd.iconPath = res.iconPath; upd.type = res.type;
                m_manager.SetShortcut(clicked, upd);
                m_manager.Save();
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
            SetForegroundWindow(m_hwnd);
        }
    }
}

void LauncherWindow::OnRButtonUp(int x, int y) {
    if (m_dragging) return;
    if (m_view == View::Settings) { OnSettViewRClick(x, y); return; }
    int idx = HitTestButton(x, y);
    if (idx < 0) return;

    const auto& sc = m_manager.GetShortcuts();
    Shortcut cur = (idx < (int)sc.size()) ? sc[idx] : Shortcut{};

    m_editOpen = true;
    EditResult res = ShowEditDialog(m_hwnd, cur);
    m_editOpen = false;

    if (res.ok) {
        Shortcut upd; upd.name = res.name; upd.target = res.target; upd.iconPath = res.iconPath; upd.type = res.type;
        m_manager.SetShortcut(idx, upd);
        m_manager.Save();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
    SetForegroundWindow(m_hwnd);
}

void LauncherWindow::OnSettingsClick() { EnterSettings(); }

void LauncherWindow::OnDotClick(int idx) {
    if (idx != m_manager.GetCurrentIndex()) {
        m_manager.SetCurrentProfile(idx);
        m_manager.Save();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

// ── Settings view ─────────────────────────────────────────────────────────────

void LauncherWindow::EnterSettings() {
    CommitRename();
    m_view    = View::Settings;
    m_settSel = m_manager.GetCurrentIndex();
    m_editOpen = true;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void LauncherWindow::ExitSettings() {
    CommitRename();
    m_view     = View::Main;
    m_editOpen = false;
    InvalidateRect(m_hwnd, nullptr, FALSE);
    SetForegroundWindow(m_hwnd);
}

void LauncherWindow::CommitRename() {
    if (!m_hRenEdit || !IsWindowVisible(m_hRenEdit)) return;
    wchar_t buf[256] = {};
    GetWindowTextW(m_hRenEdit, buf, 256);
    std::wstring name(buf);
    if (!name.empty() && m_settSel >= 0 && m_settSel < m_manager.GetProfileCount()) {
        m_manager.RenameProfile(m_settSel, name);
        m_manager.Save();
    }
    ShowWindow(m_hRenEdit, SW_HIDE);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void LauncherWindow::BeginRename(int idx) {
    CommitRename();
    m_settSel = idx;
    RECT r = ButtonRect(idx);
    if (!m_hRenEdit) {
        m_hRenEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
            WS_CHILD | ES_AUTOHSCROLL, 0, 0, 10, 10,
            m_hwnd, (HMENU)(UINT_PTR)1001, m_hInst, nullptr);
        HFONT f = MakeFont(14);
        SendMessageW(m_hRenEdit, WM_SETFONT, (WPARAM)f, FALSE);
        SendMessageW(m_hRenEdit, EM_LIMITTEXT, 64, 0);
        s_origRenProc = (WNDPROC)SetWindowLongPtrW(m_hRenEdit, GWLP_WNDPROC, (LONG_PTR)RenEditProc);
    }
    int ey = r.top + (r.bottom - r.top) / 2 - 14;
    SetWindowPos(m_hRenEdit, nullptr, r.left + 8, ey, r.right - r.left - 16, 28, SWP_NOZORDER);
    SetWindowTextW(m_hRenEdit, m_manager.GetProfileName(idx).c_str());
    ShowWindow(m_hRenEdit, SW_SHOW);
    SetFocus(m_hRenEdit);
    SendMessageW(m_hRenEdit, EM_SETSEL, 0, -1);
}

void LauncherWindow::DrawSettings(HDC hdc) {
    int count = m_manager.GetProfileCount();
    int cur   = m_manager.GetCurrentIndex();

    for (int i = 0; i <= std::min(count, Rows() * COLS - 1); i++) {
        RECT r = ButtonRect(i);

        if (i == count) {
            // ── Add-profile tile ──
            bool hov = (i == m_hoverIndex);
            COLORREF hint = hov ? CLR_TEXT : CLR_SUBTEXT;
            if (hov) RRect(hdc, r, CORNER_R, CLR_HOVER, CLR_HOVER);
            HPEN dp = CreatePen(PS_DOT, 1, hint);
            HPEN op = (HPEN)SelectObject(hdc, dp);
            HBRUSH nb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, r.left, r.top, r.right, r.bottom, CORNER_R, CORNER_R);
            SelectObject(hdc, op); SelectObject(hdc, nb); DeleteObject(dp);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, hint);
            HFONT pf = MakeFont(28); HFONT opf = (HFONT)SelectObject(hdc, pf);
            DrawTextW(hdc, L"+", 1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, opf); DeleteObject(pf);
        } else {
            // ── Profile tile ──
            bool isSel = (i == m_settSel);
            bool hov   = (i == m_hoverIndex);
            bool act   = (i == cur);

            COLORREF fill   = isSel ? CLR_HOVER : (hov ? CLR_HOVER : m_clrBase);
            COLORREF border = act ? CLR_ACCENT : (isSel ? CLR_BORDER : m_clrBase);
            RRect(hdc, r, CORNER_R, fill, border);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, CLR_TEXT);
            RECT tr = { r.left + 8, r.top + 8, r.right - 8, r.bottom - 28 };
            HFONT tf = MakeFont(14, isSel); HFONT otf = (HFONT)SelectObject(hdc, tf);
            DrawTextW(hdc, m_manager.GetProfileName(i).c_str(), -1, &tr,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(hdc, otf); DeleteObject(tf);

            // "Active" label at bottom — hidden when selected (hint already says "Active · click to rename")
            if (act && !isSel) {
                SetTextColor(hdc, CLR_ACCENT);
                RECT ar2 = { r.left + 4, r.bottom - 26, r.right - 4, r.bottom - 6 };
                HFONT af = MakeFont(11); HFONT oaf = (HFONT)SelectObject(hdc, af);
                DrawTextW(hdc, L"Active", -1, &ar2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, oaf); DeleteObject(af);
            }

            // "rename hint" for selected tile
            if (isSel && !IsWindowVisible(m_hRenEdit)) {
                SetTextColor(hdc, CLR_SUBTEXT);
                RECT hr2 = { r.left + 4, r.bottom - 26, r.right - 4, r.bottom - 6 };
                HFONT hf = MakeFont(10); HFONT ohf = (HFONT)SelectObject(hdc, hf);
                DrawTextW(hdc, act ? L"Active · click to rename" : L"click to rename",
                          -1, &hr2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, ohf); DeleteObject(hf);
            }
        }
    }
}

void LauncherWindow::OnSettViewLClick(int x, int y) {
    // Grid size buttons in the dots bar
    if (y >= DotsBarY()) {
        static const int rowVals[] = { 2, 3, 4 };
        int btn = HitTestGridBtn(x, y);
        if (btn >= 0 && rowVals[btn] != m_manager.GetRows()) {
            CommitRename();
            m_manager.SetRows(rowVals[btn]);
            m_manager.Save();
            CenterWindow();
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return;
    }

    int idx   = HitTestButton(x, y);
    int count = m_manager.GetProfileCount();
    if (idx < 0) { CommitRename(); return; }

    if (idx == count && count < Rows() * COLS) {
        // Add new profile
        CommitRename();
        std::wstring base = L"New Profile", name = base;
        for (int n = 2; ; n++) {
            bool clash = false;
            for (int i = 0; i < m_manager.GetProfileCount(); i++)
                if (m_manager.GetProfileName(i) == name) { clash = true; break; }
            if (!clash) break;
            name = base + L" " + std::to_wstring(n);
        }
        m_manager.AddProfile(name);
        m_manager.Save();
        m_settSel = m_manager.GetProfileCount() - 1;
        BeginRename(m_settSel);
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    if (idx >= count) return;

    if (idx == m_settSel) {
        // Second click on already-selected tile → rename
        BeginRename(idx);
    } else {
        CommitRename();
        m_settSel = idx;
        m_manager.SetCurrentProfile(idx);
        m_manager.Save();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void LauncherWindow::OnSettViewRClick(int x, int y) {
    int idx = HitTestButton(x, y);
    if (idx < 0 || idx >= m_manager.GetProfileCount()) return;
    if (m_manager.GetProfileCount() <= 1) return;
    CommitRename();
    m_manager.DeleteProfile(idx);
    m_manager.Save();
    m_settSel = std::min(m_settSel, m_manager.GetProfileCount() - 1);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

// ── WndProc ───────────────────────────────────────────────────────────────────

LRESULT LauncherWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    // Toggle show/hide from second instance pressing the key
    if (m_toggleMsg && msg == m_toggleMsg) {
        if (IsWindowVisible(m_hwnd)) {
            Hide();
            ShowWindow(m_hwnd, SW_HIDE);
        } else {
            Show();
        }
        return 0;
    }
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

    case WM_MBUTTONUP: {
        if (m_view == View::Main) {
            int idx = HitTestButton(LOWORD(lParam), HIWORD(lParam));
            if (idx >= 0) {
                const auto& sc = m_manager.GetShortcuts();
                if (idx < (int)sc.size() && sc[idx].type != ShortcutType::Empty)
                    ActionExecutor::Execute(sc[idx]);
            }
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            if (m_view == View::Settings) ExitSettings();
            else if (GetKeyState(VK_SHIFT) & 0x8000) DestroyWindow(m_hwnd); // Shift+Esc = truly exit
            else { Hide(); ShowWindow(m_hwnd, SW_HIDE); }
        }
        // 1–9 → profiles 0–8, 0 → profile 9 (main view only)
        if (m_view == View::Main) {
            int pidx = -1;
            if      (wParam >= '1' && wParam <= '9')                pidx = (int)(wParam - '1');
            else if (wParam == '0')                                  pidx = 9;
            else if (wParam >= VK_NUMPAD1 && wParam <= VK_NUMPAD9)  pidx = (int)(wParam - VK_NUMPAD1);
            else if (wParam == VK_NUMPAD0)                           pidx = 9;
            if (pidx >= 0 && pidx < m_manager.GetProfileCount()) {
                m_manager.SetCurrentProfile(pidx);
                m_manager.Save();
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
        }
        return 0;

    case WM_APP+1: CommitRename(); return 0;  // Enter in rename edit
    case WM_APP+2:                             // Escape in rename edit
        if (m_hRenEdit) ShowWindow(m_hRenEdit, SW_HIDE);
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return 0;

    case WM_COMMAND:
        if (HIWORD(wParam) == EN_KILLFOCUS && (HWND)lParam == m_hRenEdit)
            CommitRename();
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_READY) { KillTimer(m_hwnd, TIMER_READY); m_readyClose = true; }
        return 0;

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE && m_readyClose && !m_editOpen) {
            HWND activating = (HWND)lParam;
            if (activating && GetParent(activating) == m_hwnd) return 0;
            Hide();
            ShowWindow(m_hwnd, SW_HIDE);
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
