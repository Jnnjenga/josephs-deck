#include <windows.h>
#include <shellapi.h>
#include "ActionExecutor.h"

void ActionExecutor::Execute(const Shortcut& shortcut) {
    if (shortcut.type == ShortcutType::Empty) return;
    if (shortcut.target.empty()) return;

    const wchar_t* verb = nullptr;
    const wchar_t* target = shortcut.target.c_str();

    switch (shortcut.type) {
        case ShortcutType::Application:
            ShellExecuteW(nullptr, L"open", target, nullptr, nullptr, SW_SHOWNORMAL);
            break;
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
