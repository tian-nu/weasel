#pragma once

#include "resource.h"
#include "GeneralPage.h"
#include "AIPage.h"
#include "SwitcherSettingsDialog.h"
#include "UIStyleSettingsDialog.h"
#include "DictManagementDialog.h"
#include "UIStyleSettings.h"
#include <rime_levers_api.h>

// cross-instance handoff: a second WeaselDeployer process sends this to the
// running settings window to activate it and jump to a page (wParam = index).
static const UINT kWM_ShowPage = WM_APP + 101;

// page count for the nav list and page_windows_
static const int kPageCount = 5;

// The single settings window: left navigation list + stacked pages
// (General / UI style / Schemes / Dictionary / AI).
class SettingsDialog : public CDialogImpl<SettingsDialog> {
 public:
  enum { IDD = IDD_SETTINGS_MAIN };

  SettingsDialog(class Configurator* configurator,
                 RimeSwitcherSettings* switcher_settings,
                 UIStyleSettings* ui_style_settings,
                 int initial_page = 0);
  ~SettingsDialog();

  bool Modified() const { return modified_; }

  BEGIN_MSG_MAP(SettingsDialog)
  MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
  MESSAGE_HANDLER(WM_CLOSE, OnClose)
  MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
  MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
  MESSAGE_HANDLER(kWM_ShowPage, OnShowPage)
  MESSAGE_RANGE_HANDLER(WM_CTLCOLORMSGBOX, WM_CTLCOLORSTATIC, OnCtlColor)
  COMMAND_ID_HANDLER(IDOK, OnOK)
  COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
  COMMAND_ID_HANDLER(IDC_APPLY, OnApply)
  COMMAND_ID_HANDLER(IDC_TOGGLE_THEME, OnToggleTheme)
  COMMAND_HANDLER(IDC_NAV_LIST, LBN_SELCHANGE, OnNavSelChange)
  END_MSG_MAP()

  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnDrawItem(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnShowPage(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnCtlColor(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnOK(WORD, WORD, HWND, BOOL&);
  LRESULT OnCancel(WORD, WORD, HWND, BOOL&);
  LRESULT OnApply(WORD, WORD, HWND, BOOL&);
  LRESULT OnToggleTheme(WORD, WORD, HWND, BOOL&);
  LRESULT OnNavSelChange(WORD, WORD, HWND, BOOL&);

  void ShowPage(int index);
  bool ApplyAll();

  class Configurator* configurator_;
  RimeSwitcherSettings* switcher_settings_;
  UIStyleSettings* ui_style_settings_;

  GeneralPage general_;
  UIStyleSettingsDialog style_;
  SwitcherSettingsDialog schemes_;
  DictManagementDialog dict_;
  AIPage ai_;

  HWND page_windows_[kPageCount];
  HFONT nav_font_;
  int current_page_;
  int initial_page_;
  bool modified_;
};
