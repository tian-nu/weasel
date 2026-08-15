#pragma once

#include "resource.h"

#include <string>
#include <vector>

// AI page: default AI mode (traditional / hybrid / pure), head count and
// language-model files. The mode/head persist to ai_pinyin.custom.yaml via
// patch; the model list switches the active model.lmbin.
class AIPage : public CDialogImpl<AIPage> {
 public:
  enum { IDD = IDD_AI_PAGE };

  AIPage();
  ~AIPage();

  BEGIN_MSG_MAP(AIPage)
  MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
  MESSAGE_HANDLER(WM_CLOSE, OnClose)
  MESSAGE_HANDLER(WM_APP + 1, OnModelCopied)
  MESSAGE_RANGE_HANDLER(WM_CTLCOLORMSGBOX, WM_CTLCOLORSTATIC, OnCtlColor)
  COMMAND_ID_HANDLER(IDC_AI_MODE_OFF, OnModeChanged)
  COMMAND_ID_HANDLER(IDC_AI_MODE_HYBRID, OnModeChanged)
  COMMAND_ID_HANDLER(IDC_AI_MODE_PURE, OnModeChanged)
  COMMAND_ID_HANDLER(IDC_AI_HEAD, OnHeadChanged)
  COMMAND_ID_HANDLER(IDC_OPEN_MODEL_DIR, OnOpenModelDir)
  COMMAND_RANGE_HANDLER(IDC_HELP_MODE, IDC_HELP_MODEL, OnHelp)
  COMMAND_HANDLER(IDC_AI_MODEL_LIST, LBN_DBLCLK, OnModelActivate)
  END_MSG_MAP()

  void Load();
  bool Apply();

 protected:
  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
  // posted by the copy worker thread when model switching finishes
  LRESULT OnModelCopied(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnCtlColor(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnModeChanged(WORD, WORD, HWND, BOOL&);
  LRESULT OnHeadChanged(WORD, WORD, HWND, BOOL&);
  LRESULT OnOpenModelDir(WORD, WORD, HWND, BOOL&);
  LRESULT OnHelp(WORD, WORD wID, HWND, BOOL&);
  // double-click a model file: copy it over model.lmbin (active model)
  LRESULT OnModelActivate(WORD, WORD, HWND, BOOL&);

  void PopulateModels();
  void UpdateModelStatus();

  bool modified_;
  bool copying_ = false;  // a model copy is running on a worker thread
  std::vector<std::wstring> model_files_;  // list row -> file name
};
