#pragma once

#include <windows.h>

// Dark/light theming for the settings window itself (not the candidate
// window). Controls opt into immersive dark mode via the uxtheme ordinals;
// dialog backgrounds and text are painted through the shared WM_CTLCOLOR*
// helper. The preference lives under HKCU\Software\Rime\Weasel
// (DarkSettingsUI); when absent, the system app theme decides.
namespace UITheme {

// process-wide init; call once before any settings window is created
void InitForProcess();

// effective dark state: registry override, else system app theme
bool IsDark();
// stores the explicit choice (caller re-applies via Apply)
void SetDark(bool dark);

// (re)applies the current theme to a window tree (dialog + all children)
void Apply(HWND root);

// shared WM_CTLCOLOR* handler; returns the brush to return from the dialog
// proc, or 0 when the theme is light (default handling applies)
LRESULT HandleCtlColor(UINT msg, HDC dc, HWND ctrl);

}  // namespace UITheme
