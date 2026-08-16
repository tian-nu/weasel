#include "stdafx.h"
#include "UITheme.h"
#include <cwchar>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

namespace {

// ---------------------------------------------------------------------------
// palette
// ---------------------------------------------------------------------------
struct Palette {
  COLORREF bg;              // dialog / static background
  COLORREF control_bg;      // edit / listbox / combo background
  COLORREF text;            // primary text
  COLORREF text_secondary;  // labels / hover text
  COLORREF group_frame;     // group box frame
  COLORREF button_bg;       // push button face
  COLORREF button_text;
  COLORREF button_border;
  COLORREF button_pressed;
  COLORREF button_hover;
  COLORREF accent;  // selection / primary accent
  COLORREF accent_text;
  COLORREF nav_bg;      // navigation rail background
  COLORREF nav_active;  // active nav row
  COLORREF nav_active_text;
  COLORREF control_face;  // checkbox/radio box face in dark
  COLORREF glyph;         // checkmark / radio dot / focus ring
  COLORREF disabled_text;
};

Palette LightPalette() {
  Palette p;
  p.bg = RGB(0xF3, 0xF3, 0xF3);
  p.control_bg = RGB(0xFF, 0xFF, 0xFF);
  p.text = RGB(0x1F, 0x1F, 0x1F);
  p.text_secondary = RGB(0x6E, 0x6E, 0x6E);
  p.group_frame = RGB(0xC8, 0xC8, 0xC8);
  p.button_bg = RGB(0xFC, 0xFC, 0xFC);
  p.button_text = RGB(0x1F, 0x1F, 0x1F);
  p.button_border = RGB(0xB9, 0xB9, 0xB9);
  p.button_pressed = RGB(0xE6, 0xE6, 0xE6);
  p.button_hover = RGB(0xF0, 0xF0, 0xF0);
  p.accent = RGB(0xD9, 0x77, 0x57);
  p.accent_text = RGB(0xFF, 0xFF, 0xFF);
  p.nav_bg = RGB(0xEC, 0xEC, 0xEC);
  p.nav_active = RGB(0xFF, 0xFF, 0xFF);
  p.nav_active_text = RGB(0x2A, 0x2A, 0x2A);
  p.control_face = RGB(0xFF, 0xFF, 0xFF);
  p.glyph = RGB(0x2A, 0x2A, 0x2A);
  p.disabled_text = RGB(0x9A, 0x9A, 0x9A);
  return p;
}

Palette DarkPalette() {
  Palette p;
  p.bg = RGB(0x1E, 0x1E, 0x1E);
  p.control_bg = RGB(0x28, 0x28, 0x28);
  p.text = RGB(0xE8, 0xE8, 0xE8);
  p.text_secondary = RGB(0x9A, 0x9A, 0x9A);
  p.group_frame = RGB(0x4A, 0x4A, 0x4A);
  p.button_bg = RGB(0x33, 0x33, 0x33);
  p.button_text = RGB(0xE8, 0xE8, 0xE8);
  p.button_border = RGB(0x55, 0x55, 0x55);
  p.button_pressed = RGB(0x45, 0x45, 0x45);
  p.button_hover = RGB(0x3C, 0x3C, 0x3C);
  p.accent = RGB(0xE0, 0x8A, 0x66);
  p.accent_text = RGB(0x1E, 0x1E, 0x1E);
  p.nav_bg = RGB(0x25, 0x25, 0x26);
  p.nav_active = RGB(0x33, 0x33, 0x33);
  p.nav_active_text = RGB(0xF0, 0xE0, 0xD8);
  p.control_face = RGB(0x1A, 0x1A, 0x1A);
  p.glyph = RGB(0xE0, 0x8A, 0x66);
  p.disabled_text = RGB(0x6E, 0x6E, 0x6E);
  return p;
}

void GetPalette(bool dark, Palette* out) {
  *out = dark ? DarkPalette() : LightPalette();
}

HBRUSH g_bg_brush = NULL;
HBRUSH g_control_brush = NULL;
HFONT g_nav_font = NULL;

HBRUSH CreateBrush(COLORREF c) {
  return ::CreateSolidBrush(c);
}

// paint an SSEndHorizontal focus/disabled state not needed; we re-create the
// brushes whenever the palette changes so they always match the current theme.
void RecreateBrushes(const Palette& p) {
  if (g_bg_brush)
    ::DeleteObject(g_bg_brush);
  if (g_control_brush)
    ::DeleteObject(g_control_brush);
  g_bg_brush = CreateBrush(p.bg);
  g_control_brush = CreateBrush(p.control_bg);
}

// registry override: 0 = follow system, 1 = force dark, 2 = force light
int g_override = 0;
bool g_dark = false;
bool g_init = false;

const wchar_t* kRegKey = L"Software\\Rime\\Weasel";
const wchar_t* kRegValue = L"DarkSettingsUI";

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

void FillRectColor(HDC dc, const RECT& rc, COLORREF c) {
  HBRUSH b = CreateBrush(c);
  ::FillRect(dc, &rc, b);
  ::DeleteObject(b);
}

void FillRoundRect(HDC dc, const RECT& rc, COLORREF fill, COLORREF border) {
  int radius = 4;
  HRGN rgn = ::CreateRoundRectRgn(rc.left, rc.top, rc.right + 1, rc.bottom + 1,
                                  radius, radius);
  HBRUSH fb = CreateBrush(fill);
  ::FillRgn(dc, rgn, fb);
  ::DeleteObject(fb);
  HPEN pen = ::CreatePen(PS_SOLID, 1, border);
  HPEN old_pen = (HPEN)::SelectObject(dc, pen);
  HBRUSH old_brush =
      (HBRUSH)::SelectObject(dc, (HBRUSH)GetStockObject(NULL_BRUSH));
  ::RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
  ::SelectObject(dc, old_pen);
  ::SelectObject(dc, old_brush);
  ::DeleteObject(pen);
  ::DeleteObject(rgn);
}

// horizontal/vertical centering helpers via DrawText flags
void DrawCenteredText(HDC dc,
                      const RECT& rc,
                      const wchar_t* text,
                      COLORREF color,
                      HFONT font,
                      UINT extra_flags) {
  if (!text)
    return;
  COLORREF old_color = ::SetTextColor(dc, color);
  int old_mode = ::SetBkMode(dc, TRANSPARENT);
  HFONT old_font = (HFONT)::SelectObject(
      dc, font ? font : (HFONT)::GetStockObject(DEFAULT_GUI_FONT));
  ::DrawTextW(dc, text, -1, (LPRECT)&rc,
              DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | extra_flags);
  ::SelectObject(dc, old_font);
  ::SetBkMode(dc, old_mode);
  ::SetTextColor(dc, old_color);
}

// check mark in a small box
void DrawCheckGlyph(HDC dc, int left, int top, int size, COLORREF c) {
  HPEN pen = ::CreatePen(PS_SOLID, 2, c);
  HPEN old = (HPEN)::SelectObject(dc, pen);
  int x = left, y = top;
  ::MoveToEx(dc, x + size / 10, y + size / 2, NULL);
  ::LineTo(dc, x + (int)(size * 0.4), y + (int)(size * 0.85));
  ::LineTo(dc, x + (int)(size * 0.92), y + (int)(size * 0.12));
  ::SelectObject(dc, old);
  ::DeleteObject(pen);
}

// radio dot
void DrawRadioGlyph(HDC dc, int cx, int cy, int radius, COLORREF c) {
  HBRUSH b = CreateBrush(c);
  HBRUSH old = (HBRUSH)::SelectObject(dc, b);
  HPEN pen = ::CreatePen(PS_SOLID, 1, c);
  HPEN old_pen = (HPEN)::SelectObject(dc, pen);
  // Ellipse uses inclusive-exclusive rect
  ::Ellipse(dc, cx - radius, cy - radius, cx + radius + 1, cy + radius + 1);
  ::SelectObject(dc, old_pen);
  ::SelectObject(dc, old);
  ::DeleteObject(pen);
  ::DeleteObject(b);
}

}  // namespace

namespace UITheme {

void InitForProcess() {
  if (g_init)
    return;
  g_init = true;
  g_override = ReadOverride();
  if (g_override == 1)
    g_dark = true;
  else if (g_override == 2)
    g_dark = false;
  else
    g_dark = !SystemAppsUseLightTheme();
  Palette p;
  GetPalette(g_dark, &p);
  RecreateBrushes(p);
}

bool IsDark() {
  return g_dark;
}

HFONT GetNavFont() {
  if (g_nav_font)
    return g_nav_font;
  LOGFONT lf = {0};
  HFONT base = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
  ::GetObject(base, sizeof(lf), &lf);
  HDC dc = ::GetDC(NULL);
  int ppi = dc ? ::GetDeviceCaps(dc, LOGPIXELSY) : 96;
  if (dc)
    ::ReleaseDC(NULL, dc);
  lf.lfHeight = -MulDiv(13, ppi, 72);  // 13pt
  lf.lfWeight = FW_SEMIBOLD;
  lf.lfCharSet = DEFAULT_CHARSET;
  g_nav_font = ::CreateFontIndirect(&lf);
  return g_nav_font;
}

void SetDark(bool dark) {
  if (g_dark == dark)
    return;
  g_dark = dark;
  WriteOverride(dark ? 1 : 2);
  Palette p;
  GetPalette(dark, &p);
  RecreateBrushes(p);
}

void Apply(HWND root) {
  InitForProcess();
  if (!root)
    return;
  BOOL b = g_dark;
  ::DwmSetWindowAttribute(root, DWMWA_USE_IMMERSIVE_DARK_MODE, &b, sizeof(b));
  // RedrawWindow(Erase) repaints the whole tree exactly once; no
  // EnumChildWindows + SetWindowTheme, so a second toggle can never deadlock.
  ::RedrawWindow(root, NULL, NULL,
                 RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

LRESULT DrawControl(LPDRAWITEMSTRUCT dis) {
  if (!dis || !dis->hwndItem)
    return TRUE;
  wchar_t label[256] = {0};
  ::GetWindowTextW(dis->hwndItem, label, _countof(label));
  LRESULT style = ::GetWindowLongPtr(dis->hwndItem, GWL_STYLE);
  wchar_t cls[64] = {0};
  ::GetClassNameW(dis->hwndItem, cls, 64);
  if (_wcsicmp(cls, L"Button") == 0) {
    // low nibble (BS_TYPEMASK) holds the button type; auto* variants differ
    // from the plain type only by the low nibble value.
    LONG type = (LONG)style & BS_TYPEMASK;
    if (type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON)
      return DrawItem(dis, Kind::RadioButton, label);
    if (type == BS_CHECKBOX || type == BS_AUTOCHECKBOX || type == BS_3STATE ||
        type == BS_AUTO3STATE)
      return DrawItem(dis, Kind::CheckBox, label);
    return DrawItem(dis, Kind::PushButton, label);
  }
  if (_wcsicmp(cls, L"Static") == 0 && (style & SS_OWNERDRAW))
    return DrawItem(dis, Kind::GroupBox, label);
  return TRUE;
}

LRESULT DrawItem(LPDRAWITEMSTRUCT dis, Kind kind, const wchar_t* label) {
  if (!dis)
    return TRUE;
  HDC dc = dis->hDC;
  RECT rc = dis->rcItem;
  Palette p;
  GetPalette(g_dark, &p);
  RECT inner = rc;
  // inherit the control's font (the dialog font) so owner-drawn text matches
  // the labels rendered by the standard controls on the same page
  HFONT ctrl_font = dis->hwndItem
                        ? (HFONT)::SendMessageW(dis->hwndItem, WM_GETFONT, 0, 0)
                        : NULL;

  bool enabled = (dis->itemState & ODS_DISABLED) == 0;
  bool selected = (dis->itemState & ODS_SELECTED) != 0;
  bool focused = (dis->itemState & ODS_FOCUS) != 0;

  switch (kind) {
    case Kind::PushButton: {
      bool def = (dis->itemState & ODS_DEFAULT) != 0;
      COLORREF face = p.button_bg;
      if (!enabled)
        face = p.control_bg;
      else if (selected)
        face = p.button_pressed;
      else if ((dis->itemState & ODS_HOTLIGHT))
        face = p.button_hover;
      COLORREF border = enabled ? p.button_border : p.control_bg;
      if (def)
        border = p.accent;
      ::SetRect(&inner, rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1);
      FillRoundRect(dc, inner, face, border);
      COLORREF text = enabled ? p.button_text : p.disabled_text;
      DrawCenteredText(dc, inner, label, text, ctrl_font, DT_CENTER);
      if (focused) {
        RECT fr = rc;
        ::InflateRect(&fr, -4, -4);
        HPEN pen = ::CreatePen(PS_DOT, 1, p.accent);
        HPEN old = (HPEN)::SelectObject(dc, pen);
        ::SelectObject(dc, (HBRUSH)GetStockObject(NULL_BRUSH));
        ::Rectangle(dc, fr.left, fr.top, fr.right, fr.bottom);
        ::SelectObject(dc, old);
        ::DeleteObject(pen);
      }
      break;
    }
    case Kind::CheckBox:
    case Kind::RadioButton: {
      // layout: box on the left, text to its right
      int box = 16;
      int gap = 6;
      RECT boxRc = {rc.left + 1, rc.top, rc.left + box, rc.bottom};
      COLORREF box_face = enabled ? p.control_face : p.control_bg;
      COLORREF box_border = enabled ? p.button_border : p.control_bg;
      RECT textRc = {rc.left + box + gap, rc.top, rc.right, rc.bottom};
      COLORREF text = enabled ? p.text : p.disabled_text;

      if (kind == Kind::CheckBox) {
        FillRoundRect(dc, boxRc, box_face, box_border);
        bool checked = (dis->itemState & ODS_CHECKED) != 0;
        if (checked)
          DrawCheckGlyph(dc, boxRc.left + 2, boxRc.top + 2, box - 4, p.glyph);
        if (focused && enabled) {
          RECT fr = textRc;
          ::InflateRect(&fr, 0, -1);
          HPEN pen = ::CreatePen(PS_DOT, 1, p.accent);
          HPEN old = (HPEN)::SelectObject(dc, pen);
          ::SelectObject(dc, (HBRUSH)GetStockObject(NULL_BRUSH));
          ::Rectangle(dc, fr.left, fr.top, fr.right, fr.bottom);
          ::SelectObject(dc, old);
          ::DeleteObject(pen);
        }
      } else {
        int cy = (boxRc.top + boxRc.bottom) / 2;
        FillRoundRect(dc, boxRc, box_face, box_border);
        // punch a round hole by drawing a filled circle border manually
        HPEN pen = ::CreatePen(PS_SOLID, 1, box_border);
        HPEN old_pen = (HPEN)::SelectObject(dc, pen);
        HBRUSH old_brush = (HBRUSH)::SelectObject(dc, CreateBrush(box_face));
        ::Ellipse(dc, boxRc.left + 1, boxRc.top + 1, boxRc.right, boxRc.bottom);
        ::SelectObject(dc, old_brush);
        ::SelectObject(dc, old_pen);
        ::DeleteObject(pen);
        bool checked = (dis->itemState & ODS_CHECKED) != 0;
        if (checked)
          DrawRadioGlyph(dc, boxRc.left + box / 2, cy, 3, p.glyph);
        if (focused && enabled) {
          RECT fr = textRc;
          ::InflateRect(&fr, 0, -1);
          HPEN pen2 = ::CreatePen(PS_DOT, 1, p.accent);
          HPEN old2 = (HPEN)::SelectObject(dc, pen2);
          ::SelectObject(dc, (HBRUSH)GetStockObject(NULL_BRUSH));
          ::Rectangle(dc, fr.left, fr.top, fr.right, fr.bottom);
          ::SelectObject(dc, old2);
          ::DeleteObject(pen2);
        }
      }
      DrawCenteredText(dc, textRc, label, text, ctrl_font, DT_LEFT);
      break;
    }
    case Kind::GroupBox: {
      // fill background then paint a frame + title; punch out the line behind
      // the title text so both light & dark render cleanly.
      FillRectColor(dc, rc, p.bg);
      HFONT font = ctrl_font;
      SIZE sz = {0};
      HFONT oldf = (HFONT)::SelectObject(dc, font);
      ::GetTextExtentPoint32W(dc, label, label ? (int)wcslen(label) : 0, &sz);
      ::SelectObject(dc, oldf);
      int title_w = sz.cx;

      HPEN pen = ::CreatePen(PS_SOLID, 1, p.group_frame);
      HPEN old_pen = (HPEN)::SelectObject(dc, pen);
      HBRUSH old_brush =
          (HBRUSH)::SelectObject(dc, (HBRUSH)GetStockObject(NULL_BRUSH));
      int mid = rc.top + 5;  // title baseline row
      // top arc: left segment, title gap, right segment
      ::MoveToEx(dc, rc.left + 8, mid, NULL);
      ::LineTo(dc, rc.left + 12 + title_w + 4, mid);
      ::MoveToEx(dc, rc.left + 12 + title_w + 8, mid, NULL);
      ::LineTo(dc, rc.right - 4, mid);
      // bottom + sides
      ::MoveToEx(dc, rc.left + 8, rc.bottom - 2, NULL);
      ::LineTo(dc, rc.right - 4, rc.bottom - 2);
      ::MoveToEx(dc, rc.left + 2, mid + 5, NULL);
      ::LineTo(dc, rc.left + 2, rc.bottom - 4);
      ::MoveToEx(dc, rc.right - 2, mid + 5, NULL);
      ::LineTo(dc, rc.right - 2, rc.bottom - 4);
      ::SelectObject(dc, old_brush);
      ::SelectObject(dc, old_pen);
      ::DeleteObject(pen);
      // title text at the top-left
      RECT tRc = {rc.left + 12, rc.top - 2, rc.left + 12 + title_w + 8,
                  rc.top + 12};
      FillRectColor(dc, tRc, p.bg);  // erase the frame under the title
      DrawCenteredText(dc, tRc, label, p.text_secondary, font, DT_LEFT);
      break;
    }
    case Kind::NavItem: {
      FillRectColor(dc, rc, selected ? p.nav_active : p.nav_bg);
      if (selected) {
        RECT bar = {rc.left, rc.top, rc.left + 3, rc.bottom};
        FillRectColor(dc, bar, p.accent);
      }
      COLORREF fg =
          enabled ? (selected ? p.nav_active_text : p.text) : p.disabled_text;
      RECT textRc = rc;
      textRc.left += 14;
      textRc.right -= 6;
      DrawCenteredText(dc, textRc, label, fg, GetNavFont(), DT_LEFT);
      break;
    }
  }
  return TRUE;
}

void DrawNavBackground(HDC dc, const RECT& rc, bool dark) {
  Palette p;
  GetPalette(dark, &p);
  FillRectColor(dc, rc, p.nav_bg);
}

LRESULT HandleCtlColor(UINT msg, HDC dc, HWND ctrl) {
  if (!g_dark || !dc || !ctrl)
    return 0;
  wchar_t cls[64] = {0};
  ::GetClassNameW(ctrl, cls, 64);
  if (msg == WM_CTLCOLORBTN)
    return 0;  // buttons are owner-drawn; background handled there
  if (_wcsicmp(cls, L"Edit") == 0 || _wcsicmp(cls, L"ListBox") == 0 ||
      _wcsicmp(cls, L"ComboBox") == 0) {
    ::SetTextColor(dc, RGB(232, 232, 232));
    ::SetBkColor(dc, RGB(40, 40, 40));
    return (LRESULT)g_control_brush;
  }
  // WM_CTLCOLORSTATIC / WM_CTLCOLORDLG share the window background
  ::SetTextColor(dc, RGB(232, 232, 232));
  ::SetBkColor(dc, RGB(30, 30, 30));
  return (LRESULT)g_bg_brush;
}

LRESULT EraseBackground(HWND hwnd, HDC dc) {
  Palette p;
  GetPalette(g_dark, &p);
  RECT rc;
  ::GetClientRect(hwnd, &rc);
  FillRectColor(dc, rc, p.bg);
  return 1;
}

}  // namespace UITheme
