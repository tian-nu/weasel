#pragma once

#include <Windows.h>
#include "resource.h"
#include <rime_levers_api.h>

// custom message posted by the background wait thread after rime-install
// finishes
const UINT kWM_SchemataRefreshed = WM_APP + 100;

class SwitcherSettingsDialog : public CDialogImpl<SwitcherSettingsDialog> {
 public:
  enum { IDD = IDD_SWITCHER_SETTING };

  SwitcherSettingsDialog();
  explicit SwitcherSettingsDialog(RimeSwitcherSettings* settings);
  ~SwitcherSettingsDialog();

  void Init(RimeSwitcherSettings* settings) { settings_ = settings; }

  // embeds the dialog as a child of `host` (used by the settings window)
  HWND CreateEmbedded(HWND host);
  // persists schema selection; returns true if anything changed
  bool Apply();
  bool modified() const { return modified_; }

 protected:
  BEGIN_MSG_MAP(SwitcherSettingsDialog)
  MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
  MESSAGE_HANDLER(WM_CLOSE, OnClose)
  MESSAGE_HANDLER(kWM_SchemataRefreshed, OnSchemataRefreshed)
  COMMAND_HANDLER(IDC_GET_SCHEMATA, BN_CLICKED, OnGetSchemata)
  COMMAND_ID_HANDLER(IDOK, OnOK)
  COMMAND_ID_HANDLER(IDC_HOTKEYS, OnHotkeysChanged)
  NOTIFY_HANDLER(IDC_SCHEMA_LIST, LVN_ITEMCHANGED, OnSchemaListItemChanged)
  END_MSG_MAP()

  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnGetSchemata(WORD, WORD, HWND, BOOL&);
  LRESULT OnSchemataRefreshed(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnOK(WORD, WORD, HWND, BOOL&);
  LRESULT OnHotkeysChanged(WORD, WORD, HWND, BOOL&);
  LRESULT OnSchemaListItemChanged(int, LPNMHDR, BOOL&);

  void Populate();
  void ShowDetails(RimeSchemaInfo* info);
  bool DoSave();

  RimeLeversApi* api_;
  RimeSwitcherSettings* settings_;
  bool loaded_;
  bool modified_;
  bool embedded_;
  bool fetching_;

  CCheckListViewCtrl schema_list_;
  CStatic description_;
  CEdit hotkeys_;
  CButton get_schemata_;
};
