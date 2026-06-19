#pragma once
#include <windows.h>
#include "ShortcutManager.h"

// Opens a modal "Profiles" settings dialog centred over `parent`.
// Directly modifies `manager` (add/rename/delete profiles) and saves on each change.
void ShowSettingsDialog(HWND parent, ShortcutManager& manager);
