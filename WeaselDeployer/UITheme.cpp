#include "stdafx.h"
#include "UITheme.h"
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

namespace {

// documented API only, resolved by name (never by undocumented ordinals:
// on recent Windows 11 builds uxtheme is a forwarded stub and ordinal calls
// can deadlock inside win32k, hanging the settings window at creation)
typedef HRESULT(WINAPI* SetWindowThemeProc)(HWND, LPCWSTR, LPCWSTR);
SetWindowThemeProc pSetWindowTheme = nullptr;

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
  wchar_t cls[64] = {0};
  ::GetClassNameW(hwnd, cls, 64);
  if (!pSetWindowTheme)
    return TRUE;
  if (_wcsicmp(cls, L"Button") == 0) {
    // classic rendering lets WM_CTLCOLOR* set the text color in dark mode
    pSetWindowTheme(hwnd, L"", L"");
  } else if (_wcsicmp(cls, L"Edit") == 0 || _wcsicmp(cls, L"ListBox") == 0 ||
             _wcsicmp(cls, L"ComboBox") == 0 ||
             _wcsicmp(cls, L"SysListView32") == 0 ||
             _wcsicmp(cls, L"SysTreeView32") == 0) {
    // best-effort dark scrollbars; item colors come from WM_CTLCOLOR*
    pSetWindowTheme(hwnd, dark ? L"DarkMode_Explorer" : L"Explorer", NULL);
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
  if (ux)
    pSetWindowTheme =
        (SetWindowThemeProc)::GetProcAddress(ux, "SetWindowTheme");
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
  // dark title bar / non-client frame (documented DWM attribute)
  BOOL b = g_dark;
  ::DwmSetWindowAttribute(root, DWMWA_USE_IMMERSIVE_DARK_MODE, &b, sizeof(b));
  ::EnumChildWindows(root, ApplyToChild, g_dark ? 1 : 0);
  ::InvalidateRect(root, NULL, TRUE);
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
