#include "stdafx.h"
#include "AIPage.h"
#include <WeaselUtility.h>
#include <fstream>
#include <string>
#pragma warning(disable : 4005)
#include "WeaselDeployer.h"

namespace {

// path of ai_pinyin.custom.yaml under the user data dir
std::wstring CustomFilePath() {
  return WeaselUserDataPath() / L"ai_pinyin.custom.yaml";
}

// read the current patch; returns the "switches/1/reset" value (-1 if absent)
int ReadCurrentMode() {
  std::wifstream in(CustomFilePath().c_str());
  if (!in)
    return -1;
  std::wstring line;
  while (std::getline(in, line)) {
    size_t p = line.find(L"switches/@2/reset");
    if (p == std::wstring::npos)
      continue;
    size_t colon = line.find(L':', p);
    if (colon == std::wstring::npos)
      continue;
    return _wtoi(line.substr(colon + 1).c_str());
  }
  return -1;
}

// read current ai_ranker/head (0 if absent)
int ReadCurrentHead() {
  std::wifstream in(CustomFilePath().c_str());
  if (!in)
    return 0;
  std::wstring line;
  while (std::getline(in, line)) {
    size_t p = line.find(L"ai_ranker/head");
    if (p == std::wstring::npos)
      continue;
    size_t colon = line.find(L':', p);
    if (colon == std::wstring::npos)
      continue;
    return _wtoi(line.substr(colon + 1).c_str());
  }
  return 0;
}

// write the whole patch file
void WritePatch(int mode, int head) {
  std::wofstream out(CustomFilePath().c_str());
  out << L"patch:\n";
  if (mode >= 0) {
    out << L"  \"switches/@2/reset\": " << mode << L"\n";
  }
  if (head > 0) {
    out << L"  \"ai_ranker/head\": " << head << L"\n";
  }
}

}  // namespace

AIPage::AIPage() : modified_(false) {}

AIPage::~AIPage() {}

LRESULT AIPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
  Load();
  return TRUE;
}

LRESULT AIPage::OnClose(UINT, WPARAM, LPARAM, BOOL&) {
  DestroyWindow();
  return 0;
}

LRESULT AIPage::OnModeChanged(WORD, WORD, HWND, BOOL&) {
  modified_ = true;
  return 0;
}

LRESULT AIPage::OnHeadChanged(WORD, WORD, HWND, BOOL&) {
  modified_ = true;
  return 0;
}

void AIPage::Load() {
  // mode: schema default is reset: 1 (AI hybrid); a custom file overrides it.
  int mode = ReadCurrentMode();
  if (mode < 0)
    mode = 1;  // default: AI hybrid
  CheckRadioButton(IDC_AI_MODE_OFF, IDC_AI_MODE_PURE,
                   mode == 0
                       ? IDC_AI_MODE_OFF
                       : (mode == 2 ? IDC_AI_MODE_PURE : IDC_AI_MODE_HYBRID));
  int head = ReadCurrentHead();
  if (head <= 0)
    head = 3;  // schema default
  WCHAR buf[16] = {0};
  _itow_s(head, buf, 10);
  SetDlgItemTextW(IDC_AI_HEAD, buf);
  std::wstring model_path = WeaselUserDataPath() / L"ai" / L"model.lmbin";
  SetDlgItemTextW(IDC_AI_MODEL_PATH, model_path.c_str());
  modified_ = false;
}

bool AIPage::Apply() {
  if (!modified_)
    return false;
  int mode = 1;
  if (IsDlgButtonChecked(IDC_AI_MODE_OFF) == BST_CHECKED)
    mode = 0;
  else if (IsDlgButtonChecked(IDC_AI_MODE_PURE) == BST_CHECKED)
    mode = 2;
  int head = GetDlgItemInt(IDC_AI_HEAD, NULL, FALSE);
  if (head < 1)
    head = 3;
  WritePatch(mode, head);
  modified_ = false;
  return true;
}
