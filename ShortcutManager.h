#pragma once
#include <vector>
#include <string>
#include "Shortcut.h"

class ShortcutManager {
public:
    ShortcutManager();
    ~ShortcutManager();

    bool Load(const std::wstring& path);
    void CreateDefaults(const std::wstring& path);
    bool Save() const;

    // ── Profile management ──────────────────────────────────────────────────
    int  GetProfileCount()         const { return (int)m_profiles.size(); }
    int  GetCurrentIndex()         const { return m_current; }
    const std::wstring& GetProfileName(int idx) const { return m_profiles[idx].name; }

    void SetCurrentProfile(int idx);
    void AddProfile(const std::wstring& name);
    void DeleteProfile(int idx);      // no-op if only 1 profile remains
    void RenameProfile(int idx, const std::wstring& name);

    // ── Shortcuts on the active profile ────────────────────────────────────
    const std::vector<Shortcut>& GetShortcuts() const {
        return m_profiles[m_current].shortcuts;
    }
    void SetShortcut(int index, const Shortcut& sc);
    void SwapShortcuts(int a, int b);

    HICON LoadIconForShortcut(const Shortcut& sc);

private:
    struct Profile {
        std::wstring          name;
        std::vector<Shortcut> shortcuts;   // always 8 slots
    };

    std::vector<Profile> m_profiles;
    int                  m_current = 0;
    std::wstring         m_jsonPath;

    void         PadProfile(Profile& p);
    void         FreeProfileIcons(Profile& p);
    Profile      ParseProfile(const std::string& obj);
    ShortcutType ParseType(const std::string& typeStr);
    std::wstring ToWide(const std::string& str);
    std::string  ToNarrow(const std::wstring& str) const;
};
