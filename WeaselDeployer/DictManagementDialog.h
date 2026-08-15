#pragma once

#include "resource.h"

#include <rime_levers_api.h>

class DictManagementDialog : public CDialogImpl<DictManagementDialog> {
 public:
  enum { IDD = IDD_DICT_MANAGEMENT };

  DictManagementDialog();
  ~DictManagementDialog();

  // embeds the dialog as a child of `host` (used by the settings window)
  HWND CreateEmbedded(HWND host);
  // called by the settings window; needed for "sync now"
  void SetConfigurator(class Configurator* configurator) {
    configurator_ = configurator;
  }
  // loads quick-delete and sync-dir values; call once after creation
  void Load();
  // persists quick-delete choice and the sync directory
  bool Apply();

 protected:
  BEGIN_MSG_MAP(DictManagementDialog)
  MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
  MESSAGE_HANDLER(WM_CLOSE, OnClose)
  COMMAND_ID_HANDLER(IDC_BACKUP, OnBackup)
  COMMAND_ID_HANDLER(IDC_RESTORE, OnRestore)
  COMMAND_ID_HANDLER(IDC_EXPORT, OnExport)
  COMMAND_ID_HANDLER(IDC_IMPORT, OnImport)
  COMMAND_ID_HANDLER(IDC_CLEAR_USERDB, OnClearUserDb)
  COMMAND_ID_HANDLER(IDC_DEL_CTRL, OnChanged)
  COMMAND_ID_HANDLER(IDC_DEL_NONE, OnChanged)
  COMMAND_ID_HANDLER(IDC_SYNC_NOW, OnSyncNow)
  COMMAND_ID_HANDLER(IDC_SYNC_BROWSE, OnBrowse)
  COMMAND_ID_HANDLER(IDC_OPEN_SYNC_DIR, OnOpenSyncDir)
  COMMAND_RANGE_HANDLER(IDC_HELP_DEL, IDC_HELP_SYNC, OnHelp)
  COMMAND_HANDLER(IDC_USER_DICT_LIST, LBN_SELCHANGE, OnUserDictListSelChange)
  END_MSG_MAP()

  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnBackup(WORD, WORD code, HWND, BOOL&);
  LRESULT OnRestore(WORD, WORD code, HWND, BOOL&);
  LRESULT OnExport(WORD, WORD code, HWND, BOOL&);
  LRESULT OnImport(WORD, WORD code, HWND, BOOL&);
  LRESULT OnClearUserDb(WORD, WORD, HWND, BOOL&);
  LRESULT OnChanged(WORD, WORD, HWND, BOOL&);
  LRESULT OnSyncNow(WORD, WORD, HWND, BOOL&);
  LRESULT OnBrowse(WORD, WORD, HWND, BOOL&);
  LRESULT OnOpenSyncDir(WORD, WORD, HWND, BOOL&);
  LRESULT OnHelp(WORD, WORD wID, HWND, BOOL&);
  LRESULT OnUserDictListSelChange(WORD, WORD, HWND, BOOL&);

  void Populate();
  void UpdateLastSyncText();

  CListBox user_dict_list_;
  CButton backup_;
  CButton restore_;
  CButton export_;
  CButton import_;

  RimeLeversApi* api_;
  class Configurator* configurator_;
  bool modified_;
};
