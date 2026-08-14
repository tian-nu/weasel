#pragma once

#include "resource.h"

// Keys page: initial input state (Chinese/English, half/full width),
// Chinese-English switch key, candidate paging keys, quick word deletion.
// Persists to luna_pinyin.custom.yaml and luna_pinyin_simp.custom.yaml via
// line-level patch updates (librime's __append is unreliable, so full-list
// overrides are used for key_binder/editor).
class KeysPage : public CDialogImpl<KeysPage> {
 public:
  enum { IDD = IDD_KEYS_PAGE };

  KeysPage();
  ~KeysPage();

  BEGIN_MSG_MAP(KeysPage)
  MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
  MESSAGE_HANDLER(WM_CLOSE, OnClose)
  COMMAND_ID_HANDLER(IDC_KEY_INIT_CN, OnChanged)
  COMMAND_ID_HANDLER(IDC_KEY_INIT_EN, OnChanged)
  COMMAND_ID_HANDLER(IDC_KEY_INIT_HALF, OnChanged)
  COMMAND_ID_HANDLER(IDC_KEY_INIT_FULL, OnChanged)
  COMMAND_ID_HANDLER(IDC_KEY_SHIFT, OnChanged)
  COMMAND_ID_HANDLER(IDC_KEY_CTRL, OnChanged)
  COMMAND_ID_HANDLER(IDC_KEY_NONE, OnChanged)
  COMMAND_ID_HANDLER(IDC_PAGE_COMMA, OnChanged)
  COMMAND_ID_HANDLER(IDC_PAGE_MINUS, OnChanged)
  COMMAND_ID_HANDLER(IDC_PAGE_BRACKET, OnChanged)
  COMMAND_ID_HANDLER(IDC_DEL_CTRL, OnChanged)
  COMMAND_ID_HANDLER(IDC_DEL_NONE, OnChanged)
  END_MSG_MAP()

  void Load();
  bool Apply();

 protected:
  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnChanged(WORD, WORD, HWND, BOOL&);

  bool modified_;
};
