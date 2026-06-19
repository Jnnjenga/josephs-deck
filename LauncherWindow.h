#pragma once
#include <windows.h>
#include <vector>
#include "Shortcut.h"
#include "ShortcutManager.h"
#include "ActionExecutor.h"

class LauncherWindow {
public:
    LauncherWindow();
    ~LauncherWindow();

    bool Create(HINSTANCE hInstance);
    void Show();
    void Hide();

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HWND      m_hwnd       = nullptr;
    HINSTANCE m_hInst      = nullptr;
    int       m_hoverIndex = -1;
    int       m_pressIndex = -1;
    bool      m_readyClose = false;
    bool      m_editOpen   = false;

    // Drag-to-reorder state
    bool m_dragging   = false;
    int  m_dragSrc    = -1;
    int  m_dragTarget = -1;
    int  m_dragCurX   = 0;
    int  m_dragCurY   = 0;
    int  m_dragStartX = 0;
    int  m_dragStartY = 0;

    // UI hover state for non-button elements
    bool m_settingsHover = false;
    int  m_dotHover      = -1;

    // Settings view
    enum class View { Main, Settings };
    View     m_view     = View::Main;
    int      m_settSel  = -1;
    HWND     m_hRenEdit = nullptr;
    COLORREF m_clrBase  = 0x00181818; // dark accent-tinted tile colour

    static const UINT TIMER_READY = 1;
    UINT m_toggleMsg = 0;

    ShortcutManager m_manager;

    // ── Layout constants ────────────────────────────────────────────────────
    static const int TITLE_H   = 40;   // title bar
    static const int DOTS_H    = 30;   // dots navigation bar
    static const int COLS      = 4;
    static const int ROWS      = 2;
    static const int BTN_W     = 130;
    static const int BTN_H     = 100;
    static const int PADDING   = 12;
    static const int CORNER_R  = 12;
    static const int ICON_SIZE = 32;
    static const int SETT_SZ   = 26;   // settings button square size
    static const int DOT_SZ    = 9;    // dot diameter
    static const int DOT_GAP   = 8;    // gap between dots

    // ── Colors — BG/BTN/HOVER/PRESS/ACCENT computed from Windows accent at startup
    COLORREF CLR_BG      = 0x00303030;
    COLORREF CLR_TITLE   = 0x00303030;
    COLORREF CLR_BTN     = 0x00303030;
    COLORREF CLR_HOVER   = 0x00484848;
    COLORREF CLR_PRESS   = 0x001E1E1E;
    COLORREF CLR_EMPTY   = 0x00303030;
    COLORREF CLR_ACCENT  = 0x00DD8855;
    static const COLORREF CLR_BORDER  = 0x00505050;
    static const COLORREF CLR_TEXT    = 0x00F0F0F0;
    static const COLORREF CLR_SUBTEXT = 0x00909090;

    int WinWidth()  const { return COLS * BTN_W + (COLS + 1) * PADDING; }
    int WinHeight() const { return TITLE_H + ROWS * BTN_H + (ROWS + 1) * PADDING + DOTS_H; }
    int ButtonsTop()const { return TITLE_H; }
    int DotsBarY()  const { return TITLE_H + ROWS * BTN_H + (ROWS + 1) * PADDING; }

    RECT SettingsBtnRect() const;
    std::vector<RECT> DotRects() const;

    RECT ButtonRect(int index) const;
    int  HitTestButton(int x, int y) const;
    int  HitTestDot(int x, int y) const;
    bool HitTestSettings(int x, int y) const;

    void OnPaint(HDC hdc);
    void DrawTitleBar(HDC hdc);
    void DrawButton(HDC hdc, int index, const Shortcut& sc);
    void DrawGhostButton(HDC hdc, const Shortcut& sc);
    void DrawDots(HDC hdc);

    void OnLButtonDown(int x, int y);
    void OnLButtonUp(int x, int y);
    void OnRButtonUp(int x, int y);
    void OnMouseMove(int x, int y);
    void OnMouseLeave();
    void OnSettingsClick();
    void OnDotClick(int idx);
    void CenterWindow();
    void LoadShortcuts();
    void ComputePalette();

    // Settings view
    void EnterSettings();
    void ExitSettings();
    void DrawSettings(HDC hdc);
    void OnSettViewLClick(int x, int y);
    void OnSettViewRClick(int x, int y);
    void BeginRename(int idx);
    void CommitRename();

    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
};
