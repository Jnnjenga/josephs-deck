#pragma once
#include <windows.h>
#include <string>
#include "Shortcut.h"

struct EditResult {
    bool         ok       = false;
    std::wstring name;
    std::wstring target;
    std::wstring iconPath;
    ShortcutType type     = ShortcutType::Application;
};

// Shows a modal edit dialog centred over `parent`.
// Returns the edited values if user clicked OK, ok=false if cancelled.
EditResult ShowEditDialog(HWND parent, const Shortcut& current);
