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

}  // namespace

SettingsDialog::SettingsDialog(RimeSwitcherSettings* switcher_settings,
                               UIStyleSettings* ui_style_settings)
    : switcher_settings_(switcher_settings),
      ui_style_settings_(ui_style_settings),
      current_page_(0),
      modified_(false) {
  page_windows_[0] = page_windows_[1] = page_windows_[2] = page_windows_[3] =
      page_windows_[4] = NULL;
}

SettingsDialog::~SettingsDialog() {}

LRESULT SettingsDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
  HWND nav = GetDlgItem(IDC_NAV_LIST);
  // page titles; order must match ShowPage()
  const wchar_t* titles[] = {L"常规", L"界面", L"输入方案", L"词典",
                             L"AI 功能"};
  for (int i = 0; i < 5; ++i) {
    SendMessage(nav, LB_ADDSTRING, 0, (LPARAM)titles[i]);
  }
  SendMessage(nav, LB_SETCURSEL, 0, 0);

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

  ShowPage(0);
  CenterWindow();
  BringWindowToTop();
  return TRUE;
}

LRESULT SettingsDialog::OnClose(UINT, WPARAM, LPARAM, BOOL&) {
  EndDialog(IDCANCEL);
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
