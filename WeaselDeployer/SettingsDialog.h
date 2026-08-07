#pragma once

#include "resource.h"
#include "GeneralPage.h"
#include "AIPage.h"
#include "SwitcherSettingsDialog.h"
#include "UIStyleSettingsDialog.h"
#include "DictManagementDialog.h"
#include "UIStyleSettings.h"
#include <rime_levers_api.h>

// The single settings window: left navigation list + stacked pages
// (General / UI style / Schemes / Dictionary / AI).
class SettingsDialog : public CDialogImpl<SettingsDialog> {
 public:
  enum { IDD = IDD_SETTINGS_MAIN };

  SettingsDialog(RimeSwitcherSettings* switcher_settings,
                 UIStyleSettings* ui_style_settings);
  ~SettingsDialog();

  bool Modified() const { return modified_; }

  BEGIN_MSG_MAP(SettingsDialog)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    MESSAGE_HANDLER(WM_CLOSE, OnClose)
    COMMAND_ID_HANDLER(IDOK, OnOK)
    COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
    COMMAND_HANDLER(IDC_NAV_LIST, LBN_SELCHANGE, OnNavSelChange)
  END_MSG_MAP()

  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnOK(WORD, WORD, HWND, BOOL&);
  LRESULT OnCancel(WORD, WORD, HWND, BOOL&);
  LRESULT OnNavSelChange(WORD, WORD, HWND, BOOL&);

  void ShowPage(int index);
  bool ApplyAll();

  RimeSwitcherSettings* switcher_settings_;
  UIStyleSettings* ui_style_settings_;

  GeneralPage general_;
  AIPage ai_;
  UIStyleSettingsDialog style_;
  SwitcherSettingsDialog schemes_;
  DictManagementDialog dict_;

  HWND page_windows_[5];
  int current_page_;
  bool modified_;
};
