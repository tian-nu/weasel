#pragma once

#include "resource.h"

// Sync page: user-data sync directory (user.yaml sync_dir) and a manual
// "sync now" action backed by Configurator::SyncUserData.
class SyncPage : public CDialogImpl<SyncPage> {
 public:
  enum { IDD = IDD_SYNC_PAGE };

  SyncPage();
  ~SyncPage();

  void SetConfigurator(class Configurator* configurator) {
    configurator_ = configurator;
  }

  BEGIN_MSG_MAP(SyncPage)
  MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
  MESSAGE_HANDLER(WM_CLOSE, OnClose)
  COMMAND_ID_HANDLER(IDC_SYNC_NOW, OnSyncNow)
  COMMAND_ID_HANDLER(IDC_SYNC_BROWSE, OnBrowse)
  COMMAND_ID_HANDLER(IDC_HELP_SYNC, OnHelp)
  END_MSG_MAP()

  void Load();
  bool Apply();

 protected:
  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnSyncNow(WORD, WORD, HWND, BOOL&);
  LRESULT OnBrowse(WORD, WORD, HWND, BOOL&);
  LRESULT OnHelp(WORD, WORD wID, HWND, BOOL&);

  class Configurator* configurator_;
  bool modified_;
};
