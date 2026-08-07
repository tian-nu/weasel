#pragma once

#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atldlgs.h>

// AI page: default AI mode (traditional / hybrid / pure) and head count.
// Persists to ai_pinyin.custom.yaml via patch.
class AIPage : public CDialogImpl<AIPage> {
 public:
  enum { IDD = IDD_AI_PAGE };

  AIPage();
  ~AIPage() override;

  BEGIN_MSG_MAP(AIPage)
  MSG_WM_INITDIALOG(OnInitDialog)
  MSG_WM_CLOSE(OnClose)
  COMMAND_ID_HANDLER_EX(IDC_AI_MODE_OFF, OnModeChanged)
  COMMAND_ID_HANDLER_EX(IDC_AI_MODE_HYBRID, OnModeChanged)
  COMMAND_ID_HANDLER_EX(IDC_AI_MODE_PURE, OnModeChanged)
  END_MSG_MAP()

  void Load();
  bool Apply();

 private:
  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnClose();
  void OnModeChanged(UINT, int, HWND, BOOL&);

  bool modified_;
};
