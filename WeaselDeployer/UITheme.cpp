#include "stdafx.h"
#include "UITheme.h"
#include <uxtheme.h>
#include <dwmapi.h>
#include <string>

#pragma comment(lib, "dwmapi.lib")

namespace {

// undocumented uxtheme ordinals; stable since Windows 10 1809
typedef BOOL(WINAPI* AllowDarkModeForWindowProc)(HWND, BOOL);
typedef ULONG(WINAPI* SetPreferredAppModeProc)(ULONG);
typedef void(WINAPI* RefreshImmersiveColorPolicyStateProc)();

AllowDarkModeForWindowProc pAllowDarkModeForWindow = nullptr;
SetPreferredAppModeProc pSetPreferredAppMode = nullptr;
RefreshImmersiveColorPolicyStateProc pRefreshPolicy = nullptr;

// registry override: 0 = follow system, 1 = force dark, 2 = force light
int g_override = 0;
bool g_dark = false;
bool g_init = false;

HBRUSH g_bg_brush = NULL;       // dialog / static background
HBRUSH g_control_brush = NULL;  // edit / listbox background

const wchar_t* kRegKey = L"Software\\Rime\\Weasel";
const wchar_t* kRegValue = L"DarkSettingsUI";

const COLORREF kDarkBg = RGB(32, 32, 32);
const COLORREF kDarkControlBg = RGB(48, 48, 48);
const COLORREF kDarkText = RGB(232, 232, 232);

bool SystemAppsUseLightTheme() {
  DWORD light = 1;
  DWORD size = sizeof(light);
  LSTATUS st = ::RegGetValueW(
      HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
      L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &light, &size);
  return st == ERROR_SUCCESS ? light != 0 : true;
}

int ReadOverride() {
  DWORD v = 0;
  DWORD size = sizeof(v);
  if (::RegGetValueW(HKEY_CURRENT_USER, kRegKey, kRegValue, RRF_RT_REG_DWORD,
                     NULL, &v, &size) == ERROR_SUCCESS)
    return (int)v;
  return 0;
}

void WriteOverride(int v) {
  HKEY key = NULL;
  if (::RegCreateKeyExW(HKEY_CURRENT_USER, kRegKey, 0, NULL, 0, KEY_SET_VALUE,
                        NULL, &key, NULL) == ERROR_SUCCESS) {
    ::RegSetValueExW(key, kRegValue, 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    ::RegCloseKey(key);
  }
}

BOOL CALLBACK ApplyToChild(HWND hwnd, LPARAM lp) {
  bool dark = lp != 0;
  if (pAllowDarkModeForWindow)
    pAllowDarkModeForWindow(hwnd, dark);
  wchar_t cls[64] = {0};
  ::GetClassNameW(hwnd, cls, 64);
  if (_wcsicmp(cls, L"Button") == 0) {
    // classic rendering lets WM_CTLCOLOR* set the text color in dark mode
    SetWindowTheme(hwnd, L"", L"");
  } else if (_wcsicmp(cls, L"Edit") == 0 || _wcsicmp(cls, L"ListBox") == 0 ||
             _wcsicmp(cls, L"ComboBox") == 0 ||
             _wcsicmp(cls, L"SysListView32") == 0 ||
             _wcsicmp(cls, L"SysTreeView32") == 0) {
    // darkens scrollbars and selection; item colors come from
    // WM_CTLCOLOR* / NM_CUSTOMDRAW
    SetWindowTheme(hwnd, dark ? L"DarkMode_Explorer" : L"Explorer", NULL);
  }
  return TRUE;
}

}  // namespace

namespace UITheme {

void InitForProcess() {
  if (g_init)
    return;
  g_init = true;
  HMODULE ux = ::LoadLibraryW(L"uxtheme.dll");
  if (ux) {
    pAllowDarkModeForWindow =
        (AllowDarkModeForWindowProc)::GetProcAddress(ux, (LPCSTR)133);
    pSetPreferredAppMode =
        (SetPreferredAppModeProc)::GetProcAddress(ux, (LPCSTR)135);
    pRefreshPolicy =
        (RefreshImmersiveColorPolicyStateProc)::GetProcAddress(ux, (LPCSTR)134);
    if (pSetPreferredAppMode)
      pSetPreferredAppMode(1);  // allow dark
    if (pRefreshPolicy)
      pRefreshPolicy();
  }
  g_override = ReadOverride();
  if (g_override == 1)
    g_dark = true;
  else if (g_override == 2)
    g_dark = false;
  else
    g_dark = !SystemAppsUseLightTheme();
  g_bg_brush = ::CreateSolidBrush(kDarkBg);
  g_control_brush = ::CreateSolidBrush(kDarkControlBg);
}

bool IsDark() {
  return g_dark;
}

void SetDark(bool dark) {
  g_dark = dark;
  WriteOverride(dark ? 1 : 2);
}

void Apply(HWND root) {
  InitForProcess();
  if (!root)
    return;
  // dark title bar / non-client frame on the top-level window
  BOOL b = g_dark;
  ::DwmSetWindowAttribute(root, DWMWA_USE_IMMERSIVE_DARK_MODE, &b, sizeof(b));
  if (pAllowDarkModeForWindow)
    pAllowDarkModeForWindow(root, g_dark);
  ::EnumChildWindows(root, ApplyToChild, g_dark ? 1 : 0);
  ::InvalidateRect(root, NULL, TRUE);
  // redraw pages on the next idle pass so erased areas settle
  ::UpdateWindow(root);
}

LRESULT HandleCtlColor(UINT msg, HDC dc, HWND ctrl) {
  if (!g_dark || !dc || !ctrl)
    return 0;
  wchar_t cls[64] = {0};
  ::GetClassNameW(ctrl, cls, 64);
  if (msg == WM_CTLCOLORBTN) {
    ::SetTextColor(dc, kDarkText);
    ::SetBkColor(dc, kDarkBg);
    return (LRESULT)g_bg_brush;
  }
  if (_wcsicmp(cls, L"Edit") == 0 || _wcsicmp(cls, L"ListBox") == 0 ||
      _wcsicmp(cls, L"ComboBox") == 0) {
    ::SetTextColor(dc, kDarkText);
    ::SetBkColor(dc, kDarkControlBg);
    return (LRESULT)g_control_brush;
  }
  // WM_CTLCOLORSTATIC and WM_CTLCOLORDLG share the window background
  ::SetTextColor(dc, kDarkText);
  ::SetBkColor(dc, kDarkBg);
  return (LRESULT)g_bg_brush;
}

}  // namespace UITheme
