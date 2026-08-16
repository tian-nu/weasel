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
  MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
  MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
  MESSAGE_HANDLER(kWM_SchemataRefreshed, OnSchemataRefreshed)
  MESSAGE_RANGE_HANDLER(WM_CTLCOLORMSGBOX, WM_CTLCOLORSTATIC, OnCtlColor)
  COMMAND_HANDLER(IDC_GET_SCHEMATA, BN_CLICKED, OnGetSchemata)
  COMMAND_ID_HANDLER(IDOK, OnOK)
  COMMAND_ID_HANDLER(IDC_HOTKEYS, OnHotkeysChanged)
  NOTIFY_HANDLER(IDC_SCHEMA_LIST, LVN_ITEMCHANGED, OnSchemaListItemChanged)
  NOTIFY_HANDLER(IDC_SCHEMA_LIST, NM_CUSTOMDRAW, OnSchemaListCustomDraw)
  END_MSG_MAP()

  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnDrawItem(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnEraseBkgnd(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnGetSchemata(WORD, WORD, HWND, BOOL&);
  LRESULT OnSchemataRefreshed(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnOK(WORD, WORD, HWND, BOOL&);
  LRESULT OnHotkeysChanged(WORD, WORD, HWND, BOOL&);
  LRESULT OnSchemaListItemChanged(int, LPNMHDR, BOOL&);
  LRESULT OnSchemaListCustomDraw(int, LPNMHDR, BOOL&);
  LRESULT OnCtlColor(UINT, WPARAM, LPARAM, BOOL&);

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
