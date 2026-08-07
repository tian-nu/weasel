#pragma once

#include "resource.h"
#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <atldlgs.h>
#include <rime_levers_api.h>

// General page: candidate layout, tray icon. Persists to weasel.custom.yaml.
class GeneralPage : public CDialogImpl<GeneralPage> {
 public:
  enum { IDD = IDD_GENERAL_PAGE };

  GeneralPage();
  ~GeneralPage() override;

  BEGIN_MSG_MAP(GeneralPage)
  MSG_WM_INITDIALOG(OnInitDialog)
  MSG_WM_CLOSE(OnClose)
  COMMAND_ID_HANDLER_EX(IDC_RADIO_HORIZONTAL, OnLayoutChanged)
  COMMAND_ID_HANDLER_EX(IDC_RADIO_VERTICAL, OnLayoutChanged)
  COMMAND_ID_HANDLER_EX(IDC_CHECK_TRAY_ICON, OnTrayIconChanged)
  END_MSG_MAP()

  // loads current values; call once after creation
  void Load();
  // writes pending changes to weasel.custom.yaml
  bool Apply();

 private:
  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnClose();
  void OnLayoutChanged(UINT, int, HWND, BOOL&);
  void OnTrayIconChanged(UINT, int, HWND, BOOL&);

  RimeLeversApi* api_;
  RimeCustomSettings* settings_;
  bool modified_;
};
