#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include "ShortcutManager.h"

#pragma comment(lib, "advapi32.lib")

// ── Minimal JSON helpers ──────────────────────────────────────────────────────
namespace {

std::string TrimStr(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n\"");
    size_t b = s.find_last_not_of(" \t\r\n\"");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

std::string ExtractValue(const std::string& obj, const std::string& key) {
    std::string sk = "\"" + key + "\"";
    size_t kp = obj.find(sk);
    if (kp == std::string::npos) return "";
    size_t cp = obj.find(':', kp + sk.size());
    if (cp == std::string::npos) return "";
    size_t vs = obj.find_first_not_of(" \t\r\n", cp + 1);
    if (vs == std::string::npos) return "";
    if (obj[vs] == '"') {
        size_t end = vs + 1;
        while (end < obj.size() && obj[end] != '"') {
            if (obj[end] == '\\') end++;
            end++;
        }
        return obj.substr(vs + 1, end - vs - 1);
    }
    size_t ve = obj.find_first_of(",}\n", vs);
    return TrimStr(obj.substr(vs, ve - vs));
}

// Returns all top-level objects {…} found inside the first array [ ] in `json`.
std::vector<std::string> ExtractObjects(const std::string& json) {
    std::vector<std::string> out;
    size_t arrStart = json.find('[');
    if (arrStart == std::string::npos) return out;
    size_t pos = arrStart + 1;
    while ((pos = json.find('{', pos)) != std::string::npos) {
        int depth = 0;
        size_t start = pos;
        while (pos < json.size()) {
            if (json[pos] == '{')      depth++;
            else if (json[pos] == '}') {
                if (--depth == 0) { out.push_back(json.substr(start, pos - start + 1)); pos++; break; }
            }
            pos++;
        }
    }
    return out;
}

// Extract integer value (e.g. "currentProfile": 2)
int ExtractInt(const std::string& json, const std::string& key, int def = 0) {
    std::string v = ExtractValue(json, key);
    if (v.empty()) return def;
    try { return std::stoi(v); } catch (...) { return def; }
}

// Find the substring starting at the first '[' after the given key
std::string ExtractArrayText(const std::string& json, const std::string& key) {
    std::string sk = "\"" + key + "\"";
    size_t kp = json.find(sk);
    if (kp == std::string::npos) return "";
    size_t arrStart = json.find('[', kp + sk.size());
    if (arrStart == std::string::npos) return "";
    // Find matching ']'
    int depth = 0;
    size_t pos = arrStart;
    while (pos < json.size()) {
        if (json[pos] == '[')      depth++;
        else if (json[pos] == ']') { if (--depth == 0) return json.substr(arrStart, pos - arrStart + 1); }
        pos++;
    }
    return "";
}

} // namespace

// ── ShortcutManager ──────────────────────────────────────────────────────────

ShortcutManager::ShortcutManager() {}

ShortcutManager::~ShortcutManager() {
    for (auto& p : m_profiles) FreeProfileIcons(p);
}

void ShortcutManager::FreeProfileIcons(Profile& p) {
    for (auto& sc : p.shortcuts) { if (sc.icon) { DestroyIcon(sc.icon); sc.icon = nullptr; } }
}

void ShortcutManager::PadProfile(Profile& p) {
    while (p.shortcuts.size() < 8) {
        Shortcut e; e.type = ShortcutType::Empty;
        p.shortcuts.push_back(e);
    }
}

std::wstring ShortcutManager::ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring r(n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &r[0], n);
    return r;
}

std::string ShortcutManager::ToNarrow(const std::wstring& str) const {
    if (str.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string r(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, &r[0], n, nullptr, nullptr);
    return r;
}

ShortcutType ShortcutManager::ParseType(const std::string& s) {
    if (s == "application") return ShortcutType::Application;
    if (s == "website")     return ShortcutType::Website;
    if (s == "folder")      return ShortcutType::Folder;
    if (s == "file")        return ShortcutType::File;
    if (s == "command")     return ShortcutType::Command;
    return ShortcutType::Empty;
}

HICON ShortcutManager::LoadIconForShortcut(const Shortcut& sc) {
    if (sc.type == ShortcutType::Application || sc.type == ShortcutType::File) {
        HICON hL = nullptr;
        if (ExtractIconExW(sc.target.c_str(), 0, &hL, nullptr, 1) > 0 && hL) return hL;
        SHFILEINFOW sfi = {};
        SHGetFileInfoW(sc.target.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON);
        return sfi.hIcon;
    }
    if (sc.type == ShortcutType::Folder) {
        SHFILEINFOW sfi = {};
        SHGetFileInfoW(sc.target.c_str(), FILE_ATTRIBUTE_DIRECTORY,
            &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES);
        return sfi.hIcon;
    }
    if (sc.type == ShortcutType::Website) {
        const wchar_t* chrome[] = {
            L"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
            L"C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe",
        };
        for (auto p : chrome) { HICON h = nullptr; if (ExtractIconExW(p, 0, &h, nullptr, 1) > 0 && h) return h; }
        HICON hG = nullptr;
        ExtractIconExW(L"C:\\Windows\\System32\\shell32.dll", 13, &hG, nullptr, 1);
        return hG;
    }
    if (sc.type == ShortcutType::Command) {
        HICON h = nullptr;
        ExtractIconExW(L"C:\\Windows\\System32\\cmd.exe", 0, &h, nullptr, 1);
        return h;
    }
    return nullptr;
}

// Parses a profile JSON object string (already extracted by ExtractObjects).
ShortcutManager::Profile ShortcutManager::ParseProfile(const std::string& obj) {
    Profile p;
    std::string nameStr = ExtractValue(obj, "name");
    p.name = nameStr.empty() ? L"Profile" : ToWide(nameStr);

    // The shortcuts array lives inside this profile object
    std::string scArr = ExtractArrayText(obj, "shortcuts");
    if (!scArr.empty()) {
        for (const auto& scObj : ExtractObjects(scArr)) {
            std::string nm  = ExtractValue(scObj, "name");
            std::string tgt = ExtractValue(scObj, "target");
            std::string tp  = ExtractValue(scObj, "type");
            ShortcutType t  = ParseType(tp);
            if (t == ShortcutType::Empty) {
                Shortcut e; e.type = ShortcutType::Empty;
                p.shortcuts.push_back(e);
                continue;
            }
            Shortcut sc;
            sc.name   = ToWide(nm);
            sc.target = ToWide(tgt);
            sc.type   = t;
            sc.icon   = LoadIconForShortcut(sc);
            p.shortcuts.push_back(sc);
        }
    }
    PadProfile(p);
    return p;
}

bool ShortcutManager::Load(const std::wstring& path) {
    m_jsonPath = path;
    for (auto& p : m_profiles) FreeProfileIcons(p);
    m_profiles.clear();
    m_current = 0;

    std::ifstream file(ToNarrow(path));
    if (!file.is_open()) return false;
    std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    if (json.find("\"profiles\"") != std::string::npos) {
        // ── New multi-profile format ──
        m_current = ExtractInt(json, "currentProfile", 0);
        std::string profArr = ExtractArrayText(json, "profiles");
        for (const auto& pObj : ExtractObjects(profArr))
            m_profiles.push_back(ParseProfile(pObj));
    } else if (json.find("\"shortcuts\"") != std::string::npos) {
        // ── Old single-profile format — migrate ──
        Profile main;
        main.name = L"Main";
        for (const auto& scObj : ExtractObjects(json)) {
            std::string nm  = ExtractValue(scObj, "name");
            std::string tgt = ExtractValue(scObj, "target");
            std::string tp  = ExtractValue(scObj, "type");
            ShortcutType t  = ParseType(tp);
            if (t == ShortcutType::Empty || (nm.empty() && tgt.empty())) {
                Shortcut e; e.type = ShortcutType::Empty; main.shortcuts.push_back(e); continue;
            }
            Shortcut sc;
            sc.name   = ToWide(nm);
            sc.target = ToWide(tgt);
            sc.type   = t;
            sc.icon   = LoadIconForShortcut(sc);
            main.shortcuts.push_back(sc);
        }
        PadProfile(main);
        m_profiles.push_back(std::move(main));
    }

    if (m_profiles.empty()) return false;
    if (m_current >= (int)m_profiles.size()) m_current = 0;
    return true;
}

void ShortcutManager::CreateDefaults(const std::wstring& path) {
    m_jsonPath = path;
    // Write a minimal new-format JSON then load it
    std::string json = R"({
  "currentProfile": 0,
  "profiles": [
    {
      "name": "Main",
      "shortcuts": [
        {"name":"VS Code","type":"application","target":"C:/Program Files/Microsoft VS Code/Code.exe"},
        {"name":"Chrome","type":"application","target":"C:/Program Files/Google/Chrome/Application/chrome.exe"},
        {"name":"GitHub","type":"website","target":"https://github.com"},
        {"name":"Downloads","type":"folder","target":"C:/Users/Public/Downloads"},
        {"name":"Calculator","type":"application","target":"C:/Windows/System32/calc.exe"},
        {"name":"Notepad","type":"application","target":"C:/Windows/System32/notepad.exe"},
        {"name":"Task Manager","type":"application","target":"C:/Windows/System32/Taskmgr.exe"},
        {"name":"Explorer","type":"application","target":"C:/Windows/explorer.exe"}
      ]
    }
  ]
}
)";
    { std::ofstream f(ToNarrow(path)); if (f.is_open()) f << json; }
    Load(path);
}

bool ShortcutManager::Save() const {
    if (m_jsonPath.empty()) return false;
    std::ofstream file(ToNarrow(m_jsonPath));
    if (!file.is_open()) return false;

    auto typeStr = [](ShortcutType t) -> std::string {
        switch (t) {
            case ShortcutType::Application: return "application";
            case ShortcutType::Website:     return "website";
            case ShortcutType::Folder:      return "folder";
            case ShortcutType::File:        return "file";
            case ShortcutType::Command:     return "command";
            default:                        return "empty";
        }
    };

    file << "{\n  \"currentProfile\": " << m_current << ",\n  \"profiles\": [\n";
    for (int pi = 0; pi < (int)m_profiles.size(); pi++) {
        if (pi) file << ",\n";
        const auto& p = m_profiles[pi];
        file << "    {\n      \"name\": \"" << ToNarrow(p.name) << "\",\n      \"shortcuts\": [\n";
        for (int si = 0; si < (int)p.shortcuts.size(); si++) {
            if (si) file << ",\n";
            const auto& sc = p.shortcuts[si];
            file << "        {\"name\":\"" << ToNarrow(sc.name)
                 << "\",\"type\":\""       << typeStr(sc.type)
                 << "\",\"target\":\""     << ToNarrow(sc.target) << "\"}";
        }
        file << "\n      ]\n    }";
    }
    file << "\n  ]\n}\n";
    return true;
}

// ── Profile operations ────────────────────────────────────────────────────────

void ShortcutManager::SetCurrentProfile(int idx) {
    if (idx >= 0 && idx < (int)m_profiles.size()) m_current = idx;
}

void ShortcutManager::AddProfile(const std::wstring& name) {
    Profile p;
    p.name = name;
    PadProfile(p);
    m_profiles.push_back(std::move(p));
}

void ShortcutManager::DeleteProfile(int idx) {
    if ((int)m_profiles.size() <= 1 || idx < 0 || idx >= (int)m_profiles.size()) return;
    FreeProfileIcons(m_profiles[idx]);
    m_profiles.erase(m_profiles.begin() + idx);
    if (m_current >= (int)m_profiles.size()) m_current = (int)m_profiles.size() - 1;
}

void ShortcutManager::RenameProfile(int idx, const std::wstring& name) {
    if (idx >= 0 && idx < (int)m_profiles.size()) m_profiles[idx].name = name;
}

// ── Shortcut operations on active profile ─────────────────────────────────────

void ShortcutManager::SetShortcut(int index, const Shortcut& sc) {
    auto& slots = m_profiles[m_current].shortcuts;
    if (index < 0 || index >= (int)slots.size()) return;
    if (slots[index].icon) { DestroyIcon(slots[index].icon); slots[index].icon = nullptr; }
    slots[index]      = sc;
    slots[index].icon = LoadIconForShortcut(slots[index]);
}

void ShortcutManager::SwapShortcuts(int a, int b) {
    auto& slots = m_profiles[m_current].shortcuts;
    if (a < 0 || b < 0 || a >= (int)slots.size() || b >= (int)slots.size()) return;
    std::swap(slots[a], slots[b]);
}
