#pragma once

#include <windows.h>

// Modern dark/light theming for the settings window. The win32 theme API
// cannot recolor a GROUPBOX frame or a themed push button without enabling
// full app dark mode, so every Button-class control and every group box is
// drawn owner-drawn through UITheme. That gives a consistent flat look in
// both light and dark, and removes the fragile SetWindowTheme/ordinal calls
// that previously hung the window on a second theme toggle.
//
// The preference lives under HKCU\Software\Rime\Weasel (DarkSettingsUI);
// when absent, the system app theme decides.
namespace UITheme {

// process-wide init; call once before any settings window is created
void InitForProcess();

// effective dark state: registry override, else system app theme
bool IsDark();
// stores the explicit choice (caller re-applies via Apply)
void SetDark(bool dark);

// cached navigation font used for the sidebar items
HFONT GetNavFont();

// (re)applies the current theme to a window tree: sets the dark title bar,
// redraws the whole tree. Safe to call from a click handler: repaints are
// coalesced with RedrawWindow and never re-enter SetWindowTheme.
void Apply(HWND root);

// shared WM_CTLCOLOR* handler; returns the brush a dialog proc should return,
// or 0 when the theme does not need to overrule the default colors. Used for
// Edit/ListBox/ComboBox backgrounds that we do NOT owner-draw.
LRESULT HandleCtlColor(UINT msg, HDC dc, HWND ctrl);

// fills the dialog background with the current theme color. Return value is a
// valid WM_ERASEBKGND result (1).
LRESULT EraseBackground(HWND hwnd, HDC dc);

// unified WM_DRAWITEM entry point. `owner` is the dialog that received the
// draw; returns TRUE when the item was painted. A dialog maps each of its
// owner-drawn control ids to a UITheme element kind and calls this via its
// WM_DRAWITEM handler.
enum class Kind {
  PushButton,   // normal push button / default push button
  CheckBox,     // check box
  RadioButton,  // radio button
  GroupBox,     // framed group box (SS_OWNERDRAW static)
  NavItem,      // navigation list row
};

// Inspects the owner-drawn control (dis->hwndItem) to decide whether it is a
// push button, check box, radio button or group box and paints it with the
// current theme. This is the single WM_DRAWITEM entry each settings page uses.
LRESULT DrawControl(LPDRAWITEMSTRUCT dis);

// paints a specific kind (used when the kind is known ahead of time, e.g. the
// navigation list)
LRESULT DrawItem(LPDRAWITEMSTRUCT dis, Kind kind, const wchar_t* label);

// paints a custom navigation row in the dialog (used by the main window's
// owner-drawn sidebar when it is not a WM_DRAWITEM listbox row)
void DrawNavBackground(HDC dc, const RECT& rc, bool dark);

}  // namespace UITheme
