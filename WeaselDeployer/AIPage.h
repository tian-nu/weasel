#pragma once

#include "resource.h"

// AI page: default AI mode (traditional / hybrid / pure) and head count.
// Persists to ai_pinyin.custom.yaml via patch.
class AIPage : public CDialogImpl<AIPage> {
 public:
  enum { IDD = IDD_AI_PAGE };

  AIPage();
  ~AIPage();

  BEGIN_MSG_MAP(AIPage)
  MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
  MESSAGE_HANDLER(WM_CLOSE, OnClose)
  COMMAND_ID_HANDLER(IDC_AI_MODE_OFF, OnModeChanged)
  COMMAND_ID_HANDLER(IDC_AI_MODE_HYBRID, OnModeChanged)
  COMMAND_ID_HANDLER(IDC_AI_MODE_PURE, OnModeChanged)
  END_MSG_MAP()

  void Load();
  bool Apply();

 protected:
  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnModeChanged(WORD, WORD, HWND, BOOL&);

  bool modified_;
};
