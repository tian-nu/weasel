#pragma once

#include "resource.h"
#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atldlgs.h>
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

  RimeLeversApi* api_;
  RimeCustomSettings* settings_;
  bool modified_;
};
