#include <windows.h>
#include <shellapi.h>
#include "ActionExecutor.h"

// Split "C:/path/app.exe --args" → {exe, args}. Returns {target, ""} if no args.
static std::pair<std::wstring, std::wstring> SplitTarget(const std::wstring& t) {
    for (size_t i = 0; i + 5 <= t.size(); i++) {
        if (t[i] == L'.' &&
            (t[i+1] == L'e' || t[i+1] == L'E') &&
            (t[i+2] == L'x' || t[i+2] == L'X') &&
            (t[i+3] == L'e' || t[i+3] == L'E') &&
            t[i+4] == L' ') {
            return { t.substr(0, i + 4), t.substr(i + 5) };
        }
    }
    return { t, {} };
}

void ActionExecutor::Execute(const Shortcut& shortcut) {
    if (shortcut.type == ShortcutType::Empty) return;
    if (shortcut.target.empty()) return;

    const wchar_t* verb = nullptr;
    const wchar_t* target = shortcut.target.c_str();

    switch (shortcut.type) {
        case ShortcutType::Application: {
            auto [exe, args] = SplitTarget(shortcut.target);
            ShellExecuteW(nullptr, L"open", exe.c_str(),
                args.empty() ? nullptr : args.c_str(), nullptr, SW_SHOWNORMAL);
            break;
        }
        case ShortcutType::Website: {
            // Ensure URL has a scheme so ShellExecute hands it to the browser
            std::wstring url = shortcut.target;
            if (url.substr(0, 7) != L"http://" &&
                url.substr(0, 8) != L"https://") {
                url = L"https://" + url;
            }
            ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            break;
        }
        case ShortcutType::Folder:
            ShellExecuteW(nullptr, L"explore", target, nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case ShortcutType::File:
            ShellExecuteW(nullptr, L"open", target, nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case ShortcutType::Command:
            ShellExecuteW(nullptr, L"open", L"cmd.exe",
                (std::wstring(L"/c ") + target).c_str(), nullptr, SW_HIDE);
            break;
        default:
            break;
    }
}
