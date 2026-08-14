#pragma once

#include "resource.h"
#include <rime_levers_api.h>

// General page: candidate layout, tray icon. Persists to weasel.custom.yaml.
class GeneralPage : public CDialogImpl<GeneralPage> {
 public:
  enum { IDD = IDD_GENERAL_PAGE };

  GeneralPage();
  ~GeneralPage();

  BEGIN_MSG_MAP(GeneralPage)
  MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
  MESSAGE_HANDLER(WM_CLOSE, OnClose)
  COMMAND_ID_HANDLER(IDC_RADIO_HORIZONTAL, OnLayoutChanged)
  COMMAND_ID_HANDLER(IDC_RADIO_VERTICAL, OnLayoutChanged)
  COMMAND_ID_HANDLER(IDC_CHECK_TRAY_ICON, OnTrayIconChanged)
  COMMAND_ID_HANDLER(IDC_PAGE_SIZE, OnPageSizeChanged)
  COMMAND_ID_HANDLER(IDC_CHECK_NL, OnFuzzyChanged)
  COMMAND_ID_HANDLER(IDC_CHECK_FLAT, OnFuzzyChanged)
  COMMAND_ID_HANDLER(IDC_CHECK_NASAL, OnFuzzyChanged)
  COMMAND_ID_HANDLER(IDC_CHECK_LR, OnFuzzyChanged)
  COMMAND_ID_HANDLER(IDC_REDEPLOY, OnRedeploy)
  COMMAND_ID_HANDLER(IDC_OPEN_DATA_DIR, OnOpenDataDir)
  COMMAND_RANGE_HANDLER(IDC_HELP_PAGESIZE, IDC_HELP_FUZZY, OnHelp)
  END_MSG_MAP()

  // loads current values; call once after creation
  void Load();
  // writes pending changes to weasel.custom.yaml
  bool Apply();

 protected:
  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnLayoutChanged(WORD, WORD, HWND, BOOL&);
  LRESULT OnTrayIconChanged(WORD, WORD, HWND, BOOL&);
  LRESULT OnPageSizeChanged(WORD, WORD, HWND, BOOL&);
  LRESULT OnFuzzyChanged(WORD, WORD, HWND, BOOL&);
  LRESULT OnRedeploy(WORD, WORD, HWND, BOOL&);
  LRESULT OnOpenDataDir(WORD, WORD, HWND, BOOL&);
  LRESULT OnHelp(WORD, WORD wID, HWND, BOOL&);

  RimeLeversApi* api_;
  RimeCustomSettings* settings_;
  RimeCustomSettings* default_settings_;
  bool modified_;
};
