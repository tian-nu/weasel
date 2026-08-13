#include "stdafx.h"
#include "SettingsDialog.h"
#include <WeaselUtility.h>
#pragma warning(disable : 4005)
#include "WeaselDeployer.h"

namespace {

// embed an already-created dialog window as a child filling `host`
HWND EmbedChild(HWND hwnd, HWND host) {
  if (!hwnd)
    return hwnd;
  LONG style = GetWindowLong(hwnd, GWL_STYLE);
  SetWindowLong(hwnd, GWL_STYLE,
                (style & ~(WS_POPUP | WS_CAPTION | WS_SYSMENU)) | WS_CHILD);
  SetParent(hwnd, host);
  RECT rc = {0};
  GetClientRect(host, &rc);
  MoveWindow(hwnd, 0, 0, rc.right, rc.bottom, TRUE);
  return hwnd;
}

// larger, semibold font for the owner-drawn navigation list
HFONT CreateNavFont(HWND nav) {
  LOGFONT lf = {0};
  HFONT base = (HFONT)::SendMessage(nav, WM_GETFONT, 0, 0);
  if (base)
    ::GetObject(base, sizeof(lf), &lf);
  HDC dc = ::GetDC(NULL);
  int ppi = dc ? ::GetDeviceCaps(dc, LOGPIXELSY) : 96;
  if (dc)
    ::ReleaseDC(NULL, dc);
  lf.lfHeight = -MulDiv(13, ppi, 72);  // 13pt, slightly larger than the dialog
  lf.lfWeight = FW_SEMIBOLD;
  return ::CreateFontIndirect(&lf);
}

}  // namespace

SettingsDialog::SettingsDialog(RimeSwitcherSettings* switcher_settings,
                               UIStyleSettings* ui_style_settings,
                               int initial_page)
    : switcher_settings_(switcher_settings),
      ui_style_settings_(ui_style_settings),
      current_page_(0),
      initial_page_(initial_page),
      modified_(false),
      nav_font_(nullptr) {
  page_windows_[0] = page_windows_[1] = page_windows_[2] = page_windows_[3] =
      page_windows_[4] = NULL;
}

SettingsDialog::~SettingsDialog() {
  if (nav_font_)
    ::DeleteObject(nav_font_);
}

LRESULT SettingsDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
  HWND nav = GetDlgItem(IDC_NAV_LIST);
  // owner-draw styling: bigger semibold font + comfortable row height
  nav_font_ = CreateNavFont(nav);
  if (nav_font_) {
    ::SendMessage(nav, WM_SETFONT, (WPARAM)nav_font_, TRUE);
    HDC dc = ::GetDC(nav);
    if (dc) {
      HFONT old = (HFONT)::SelectObject(dc, nav_font_);
      TEXTMETRIC tm = {0};
      if (::GetTextMetrics(dc, &tm))
        ::SendMessage(nav, LB_SETITEMHEIGHT, 0, tm.tmHeight + 12);
      ::SelectObject(dc, old);
      ::ReleaseDC(nav, dc);
    }
  }
  // page titles; order must match ShowPage()
  const wchar_t* titles[] = {L"常规", L"界面", L"输入方案", L"词典",
                             L"AI 功能"};
  for (int i = 0; i < 5; ++i) {
    SendMessage(nav, LB_ADDSTRING, 0, (LPARAM)titles[i]);
  }
  if (initial_page_ < 0 || initial_page_ >= 5)
    initial_page_ = 0;
  SendMessage(nav, LB_SETCURSEL, initial_page_, 0);

  HWND host = GetDlgItem(IDC_PAGE_HOST);

  RimeLeversApi* api =
      (RimeLeversApi*)rime_get_api()->find_module("levers")->get_api();
  if (api) {
    api->load_settings((RimeCustomSettings*)switcher_settings_);
    if (ui_style_settings_)
      api->load_settings(ui_style_settings_->settings());
  }

  schemes_.Init(switcher_settings_);
  style_.Init(ui_style_settings_);
  page_windows_[0] = EmbedChild(general_.Create(host), host);
  page_windows_[1] = EmbedChild(style_.CreateEmbedded(host), host);
  page_windows_[2] = EmbedChild(schemes_.CreateEmbedded(host), host);
  page_windows_[3] = EmbedChild(dict_.CreateEmbedded(host), host);
  page_windows_[4] = EmbedChild(ai_.Create(host), host);

  ShowPage(initial_page_);
  CenterWindow();
  BringWindowToTop();
  return TRUE;
}

LRESULT SettingsDialog::OnClose(UINT, WPARAM, LPARAM, BOOL&) {
  EndDialog(IDCANCEL);
  return 0;
}

// owner-draw painting for the navigation list: full-row selection highlight,
// vertically centered text, inactive-selection treated like a classic
// unfocused listbox.
LRESULT SettingsDialog::OnDrawItem(UINT, WPARAM, LPARAM lParam, BOOL&) {
  LPDRAWITEMSTRUCT dis = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
  if (!dis || dis->CtlID != IDC_NAV_LIST || dis->itemID == (UINT)-1)
    return 0;
  HDC dc = dis->hDC;
  RECT rc = dis->rcItem;
  bool selected = (dis->itemState & ODS_SELECTED) != 0;
  bool focused = ::GetFocus() == dis->hwndItem;
  COLORREF bg = selected ? GetSysColor(focused ? COLOR_HIGHLIGHT : COLOR_3DFACE)
                         : GetSysColor(COLOR_WINDOW);
  COLORREF fg = selected
                    ? GetSysColor(focused ? COLOR_HIGHLIGHTTEXT : COLOR_BTNTEXT)
                    : GetSysColor(COLOR_WINDOWTEXT);
  HBRUSH brush = ::CreateSolidBrush(bg);
  ::FillRect(dc, &rc, brush);
  ::DeleteObject(brush);

  wchar_t text[128] = {0};
  ::SendMessage(dis->hwndItem, LB_GETTEXT, dis->itemID, (LPARAM)text);
  RECT rcText = rc;
  rcText.left += 12;
  rcText.right -= 8;
  HFONT old = (HFONT)::SelectObject(
      dc, nav_font_ ? nav_font_ : (HFONT)::GetStockObject(DEFAULT_GUI_FONT));
  ::SetBkMode(dc, TRANSPARENT);
  ::SetTextColor(dc, fg);
  ::DrawText(dc, text, -1, &rcText,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
  ::SelectObject(dc, old);
  if (selected && focused)
    ::DrawFocusRect(dc, &dis->rcItem);
  return TRUE;
}

LRESULT SettingsDialog::OnShowPage(UINT, WPARAM wParam, LPARAM, BOOL&) {
  // handoff from a second WeaselDeployer instance (tray menu): jump to the
  // requested page; the sending process already brought us to the foreground.
  int page = (int)wParam;
  if (page < 0 || page >= 5)
    page = 0;
  ShowPage(page);
  return 0;
}

LRESULT SettingsDialog::OnNavSelChange(WORD, WORD, HWND, BOOL&) {
  HWND nav = GetDlgItem(IDC_NAV_LIST);
  int sel = (int)SendMessage(nav, LB_GETCURSEL, 0, 0);
  if (sel >= 0 && sel < 5)
    ShowPage(sel);
  return 0;
}

void SettingsDialog::ShowPage(int index) {
  if (index < 0 || index >= 5)
    return;
  // keep the navigation highlight in sync when the page is switched from
  // outside (e.g. the /dict handoff message)
  HWND nav = GetDlgItem(IDC_NAV_LIST);
  ::SendMessage(nav, LB_SETCURSEL, index, 0);
  for (int i = 0; i < 5; ++i) {
    if (page_windows_[i]) {
      ::ShowWindow(page_windows_[i], i == index ? SW_SHOW : SW_HIDE);
    }
  }
  current_page_ = index;
}

bool SettingsDialog::ApplyAll() {
  bool changed = false;
  if (general_.Apply())
    changed = true;
  if (ai_.Apply())
    changed = true;
  if (style_.Apply())
    changed = true;
  if (schemes_.Apply())
    changed = true;
  dict_.Apply();
  return changed;
}

LRESULT SettingsDialog::OnOK(WORD, WORD, HWND, BOOL&) {
  if (ApplyAll()) {
    modified_ = true;
  }
  EndDialog(IDOK);
  return 0;
}

LRESULT SettingsDialog::OnCancel(WORD, WORD, HWND, BOOL&) {
  EndDialog(IDCANCEL);
  return 0;
}
