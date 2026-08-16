#include "stdafx.h"
#include "AIPage.h"
#include "UITheme.h"
#include <WeaselUtility.h>
#include <fstream>
#include <string>
#include <filesystem>
#include <thread>
#pragma warning(disable : 4005)
#include "WeaselDeployer.h"

// theme-aware control backgrounds; no-op in light mode
LRESULT AIPage::OnCtlColor(UINT msg,
                           WPARAM wParam,
                           LPARAM lParam,
                           BOOL& handled) {
  LRESULT res = UITheme::HandleCtlColor(msg, (HDC)wParam, (HWND)lParam);
  if (res) {
    handled = TRUE;
    return res;
  }
  handled = FALSE;
  return 0;
}

// owner-drawn push buttons / check boxes / radio buttons / group boxes
LRESULT AIPage::OnDrawItem(UINT, WPARAM, LPARAM lParam, BOOL&) {
  return UITheme::DrawControl(reinterpret_cast<LPDRAWITEMSTRUCT>(lParam));
}

// dialog background follows the theme
LRESULT AIPage::OnEraseBkgnd(UINT, WPARAM wParam, LPARAM, BOOL& handled) {
  handled = TRUE;
  return UITheme::EraseBackground(m_hWnd, (HDC)wParam);
}

namespace {

// path of ai_pinyin.custom.yaml under the user data dir
std::wstring CustomFilePath() {
  return WeaselUserDataPath() / L"ai_pinyin.custom.yaml";
}

// directory holding the language model files
std::wstring ModelDir() {
  return (WeaselUserDataPath() / L"ai").wstring();
}

// the model file the ai_ranker actually loads (fixed in the schema)
std::wstring ActiveModelPath() {
  return ModelDir() + L"\\model.lmbin";
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

LRESULT AIPage::OnOpenModelDir(WORD, WORD, HWND, BOOL&) {
  std::error_code ec;
  std::filesystem::path dir = ModelDir();
  if (!std::filesystem::exists(dir, ec))
    std::filesystem::create_directories(dir, ec);
  ::ShellExecuteW(NULL, L"open", dir.c_str(), NULL, NULL, SW_SHOW);
  return 0;
}

LRESULT AIPage::OnHelp(WORD, WORD wID, HWND, BOOL&) {
  const wchar_t* text = nullptr;
  switch (wID) {
    case IDC_HELP_MODE:
      text =
          L"纯传统：不使用 AI，按词典默认顺序出候选。\nAI 混合（默认）：前几个"
          L"候选由语言模型按上下文概率排列，其余保持传统顺序。\n纯 "
          L"AI：全部候选按上下文概率排列。\n输入中可按 Ctrl+` 快速切换。";
      break;
    case IDC_HELP_MODEL:
      text =
          L"语言模型文件（.lmbin）放在数据目录的 ai 文件夹中。\n双击列表中的"
          L"模型文件即可切换为当前模型（复制为 model.lmbin），切换后重新部署"
          L"生效。";
      break;
  }
  if (text)
    ::MessageBox(m_hWnd, text, L"说明", MB_OK | MB_ICONINFORMATION);
  return 0;
}

// fill the model list with *.lmbin files from the ai directory; model.lmbin
// (the file the schema loads) is marked as active
void AIPage::PopulateModels() {
  ::SendDlgItemMessageW(m_hWnd, IDC_AI_MODEL_LIST, LB_RESETCONTENT, 0, 0);
  model_files_.clear();
  std::error_code ec;
  std::filesystem::path dir = ModelDir();
  if (!std::filesystem::exists(dir, ec))
    return;
  int active = -1;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (ec)
      break;
    if (!entry.is_regular_file(ec) || ec)
      continue;
    if (entry.path().extension() != L".lmbin")
      continue;
    std::wstring name = entry.path().filename().wstring();
    // size in MB with one decimal
    uintmax_t bytes = entry.file_size(ec);
    if (ec) {
      ec.clear();
      bytes = 0;
    }
    uintmax_t mb10 = (bytes * 10 + 512 * 1024) / (1024 * 1024);
    WCHAR line[512] = {0};
    int n = _snwprintf_s(line, _TRUNCATE, L"%s （%llu.%llu MB）", name.c_str(),
                         (unsigned long long)(mb10 / 10),
                         (unsigned long long)(mb10 % 10));
    bool is_active = _wcsicmp(name.c_str(), L"model.lmbin") == 0;
    if (is_active && n > 0)
      wcscat_s(line, L"　← 使用中");
    ::SendDlgItemMessageW(m_hWnd, IDC_AI_MODEL_LIST, LB_ADDSTRING, 0,
                          (LPARAM)line);
    if (is_active)
      active = (int)model_files_.size();
    model_files_.push_back(name);
  }
  ::SendDlgItemMessageW(m_hWnd, IDC_AI_MODEL_LIST, LB_SETCURSEL, (WPARAM)active,
                        0);
}

// refresh the 已就绪/未安装 status line from model.lmbin presence
void AIPage::UpdateModelStatus() {
  std::error_code ec;
  bool has_model = std::filesystem::exists(ActiveModelPath(), ec);
  // status prefix + path on one line; SS_ENDELLIPSIS truncates the tail so the
  // status word (已就绪/未安装) stays visible even when the path is long.
  std::wstring status =
      (has_model ? L"已就绪  " : L"未安装  ") + ActiveModelPath();
  SetDlgItemTextW(IDC_AI_MODEL_PATH, status.c_str());
}

// double-click: copy the selected model over model.lmbin so the schema's
// fixed model path picks it up. The copy runs on a worker thread (model
// files can be hundreds of MB); the UI thread stays responsive and picks
// up the result via a posted message.
LRESULT AIPage::OnModelActivate(WORD, WORD, HWND, BOOL&) {
  if (copying_)
    return 0;  // a switch is already in flight
  LRESULT sel =
      ::SendDlgItemMessageW(m_hWnd, IDC_AI_MODEL_LIST, LB_GETCURSEL, 0, 0);
  if (sel < 0 || sel >= (LRESULT)model_files_.size())
    return 0;
  const std::wstring name = model_files_[(size_t)sel];
  if (_wcsicmp(name.c_str(), L"model.lmbin") == 0)
    return 0;  // already active
  if (::MessageBox(
          m_hWnd,
          (L"将「" + name +
           L"」设为当前模型？\n会覆盖 model.lmbin，切换后重新部署生效。")
              .c_str(),
          L"【小狼毫】", MB_YESNO | MB_ICONQUESTION) != IDYES)
    return 0;
  copying_ = true;
  ::EnableWindow(GetDlgItem(IDC_AI_MODEL_LIST), FALSE);
  SetDlgItemTextW(IDC_AI_MODEL_PATH, L"正在切换模型，请稍候…");
  const std::wstring src = (std::filesystem::path(ModelDir()) / name).wstring();
  const std::wstring dst = ActiveModelPath();
  HWND notify = m_hWnd;  // captured by value; the thread never touches `this`
  std::thread([src, dst, notify]() {
    std::error_code ec;
    std::filesystem::copy_file(
        src, dst, std::filesystem::copy_options::overwrite_existing, ec);
    ::PostMessage(notify, WM_APP + 1, ec ? 0 : 1, 0);
  }).detach();
  return 0;
}

// worker-thread completion: re-enable the list and reflect the result
LRESULT AIPage::OnModelCopied(UINT, WPARAM wParam, LPARAM, BOOL&) {
  copying_ = false;
  ::EnableWindow(GetDlgItem(IDC_AI_MODEL_LIST), TRUE);
  if (!wParam) {
    UpdateModelStatus();
    ::MessageBox(m_hWnd, L"切换模型失败，请检查文件是否被占用。", L"【小狼毫】",
                 MB_OK | MB_ICONERROR);
    return 0;
  }
  UpdateModelStatus();
  PopulateModels();
  ::MessageBox(m_hWnd, L"已切换模型，重新部署（或重启输入法）后生效。",
               L"【小狼毫】", MB_OK | MB_ICONINFORMATION);
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

  UpdateModelStatus();
  PopulateModels();
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
