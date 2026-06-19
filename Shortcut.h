#pragma once
#include <string>

enum class ShortcutType {
    Application,
    Website,
    Folder,
    File,
    Command,
    Empty
};

struct Shortcut {
    std::wstring name;
    std::wstring target;
    std::wstring iconPath;   // optional custom .ico override
    ShortcutType type = ShortcutType::Empty;
    HICON icon = nullptr;
};
