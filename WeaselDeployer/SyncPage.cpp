#include "stdafx.h"
#include "SyncPage.h"
#include "Configurator.h"
#include <WeaselUtility.h>
#include <fstream>
#include <shlobj.h>
#pragma warning(disable : 4005)
#include "WeaselDeployer.h"

namespace {

// path of user.yaml under the user data dir
std::wstring UserYamlPath() {
  return WeaselUserDataPath().wstring() + L"\\user.yaml";
}

// read sync_dir from user.yaml (empty if unset)
std::wstring ReadSyncDir() {
  std::wifstream in(UserYamlPath().c_str());
  if (!in)
    return L"";
  std::wstring line;
  while (std::getline(in, line)) {
    size_t p = line.find(L"sync_dir");
    if (p == std::wstring::npos)
      continue;
    size_t colon = line.find(L':', p);
    if (colon == std::wstring::npos)
      continue;
    std::wstring v = line.substr(colon + 1);
    // strip whitespace and quotes
    size_t b = v.find_first_not_of(L" \t\"'");
    size_t e = v.find_last_not_of(L" \t\"'\r\n");
    if (b == std::wstring::npos || e == std::wstring::npos)
      return L"";
    return v.substr(b, e - b + 1);
  }
  return L"";
}

// line-level update of sync_dir in user.yaml; preserves other keys
void WriteSyncDir(const std::wstring& dir) {
  std::vector<std::wstring> lines;
  {
    std::wifstream in(UserYamlPath().c_str());
    std::wstring line;
    while (std::getline(in, line))
      lines.push_back(line);
  }
  bool found = false;
  for (auto& line : lines) {
    if (line.find(L"sync_dir") != std::wstring::npos) {
      line = L"sync_dir: \"" + dir + L"\"";
      found = true;
      break;
    }
  }
  if (!found)
    lines.insert(lines.begin(), L"sync_dir: \"" + dir + L"\"");
  std::wofstream out(UserYamlPath().c_str());
  for (const auto& line : lines)
    out << line << L"\n";
}

}  // namespace

SyncPage::SyncPage() : configurator_(nullptr), modified_(false) {}

SyncPage::~SyncPage() {}

LRESULT SyncPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
  Load();
  return TRUE;
}

LRESULT SyncPage::OnHelp(WORD, WORD, HWND, BOOL&) {
  ::MessageBox(m_hWnd,
               L"词库与配置将同步到该目录，用于多台设备间迁移。\n同步只合并、不"
               L"删除本地内容。",
               L"说明", MB_OK | MB_ICONINFORMATION);
  return 0;
}

LRESULT SyncPage::OnClose(UINT, WPARAM, LPARAM, BOOL&) {
  DestroyWindow();
  return 0;
}

LRESULT SyncPage::OnSyncNow(WORD, WORD, HWND, BOOL&) {
  int rc = configurator_ ? configurator_->SyncUserData() : 1;
  ::MessageBox(m_hWnd, rc == 0 ? L"同步完成。" : L"同步失败，请检查同步目录。",
               L"【小狼毫】",
               MB_OK | (rc == 0 ? MB_ICONINFORMATION : MB_ICONERROR));
  return 0;
}

LRESULT SyncPage::OnBrowse(WORD, WORD, HWND, BOOL&) {
  // SHBrowseForFolder: pick a sync directory and drop it in the edit box.
  BROWSEINFOW bi = {0};
  bi.hwndOwner = m_hWnd;
  bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI | BIF_NEWDIALOGSTYLE;
  bi.lpszTitle = L"选择同步目录";
  LPITEMIDLIST pidl = ::SHBrowseForFolderW(&bi);
  if (!pidl)
    return 0;
  WCHAR path[MAX_PATH] = {0};
  bool ok = ::SHGetPathFromIDListW(pidl, path) != 0;
  LPMALLOC shellMalloc = NULL;
  if (SUCCEEDED(::SHGetMalloc(&shellMalloc))) {
    shellMalloc->Free(pidl);
    shellMalloc->Release();
  }
  if (ok && path[0]) {
    SetDlgItemTextW(IDC_SYNC_DIR, path);
    modified_ = true;
  }
  return 0;
}

void SyncPage::Load() {
  SetDlgItemTextW(IDC_SYNC_DIR, ReadSyncDir().c_str());
  modified_ = false;
}

bool SyncPage::Apply() {
  if (!modified_)
    return false;
  WCHAR buf[512] = {0};
  GetDlgItemTextW(IDC_SYNC_DIR, buf, _countof(buf));
  WriteSyncDir(buf);
  modified_ = false;
  return true;
}
