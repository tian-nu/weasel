#include "stdafx.h"
#include "DictManagementDialog.h"
#include "Configurator.h"
#include "UITheme.h"
#include <WeaselUtility.h>
#include <rime_api.h>
#include <fstream>
#include <shlobj.h>
#include "WeaselDeployer.h"

// theme-aware control backgrounds; no-op in light mode
LRESULT DictManagementDialog::OnCtlColor(UINT msg,
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
LRESULT DictManagementDialog::OnDrawItem(UINT, WPARAM, LPARAM lParam, BOOL&) {
  return UITheme::DrawControl(reinterpret_cast<LPDRAWITEMSTRUCT>(lParam));
}

// dialog background follows the theme
LRESULT DictManagementDialog::OnEraseBkgnd(UINT,
                                           WPARAM wParam,
                                           LPARAM,
                                           BOOL& handled) {
  handled = TRUE;
  return UITheme::EraseBackground(m_hWnd, (HDC)wParam);
}

namespace {

// ---- quick delete (moved from the Keys page) ----

// custom yamls that receive the editor patch (simplified + traditional)
std::vector<std::wstring> KeysFilePaths() {
  std::wstring base = WeaselUserDataPath().wstring();
  return {base + L"\\luna_pinyin.custom.yaml",
          base + L"\\luna_pinyin_simp.custom.yaml"};
}

// replace the block that starts at the line containing `key` and ends at the
// next line with equal-or-less indentation that is not blank, or EOF.
bool ReplaceBlock(std::vector<std::wstring>& lines,
                  const std::wstring& key,
                  const std::vector<std::wstring>& block) {
  int start = -1;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (lines[i].find(key) != std::wstring::npos) {
      start = (int)i;
      break;
    }
  }
  size_t indent = 0;
  if (start >= 0) {
    const std::wstring& s = lines[start];
    indent = s.find_first_not_of(L" \t");
    if (indent == std::wstring::npos)
      indent = 0;
    int end = start + 1;
    while ((size_t)end < lines.size()) {
      const std::wstring& n = lines[end];
      if (!n.empty()) {
        size_t ni = n.find_first_not_of(L" \t");
        if (ni != std::wstring::npos && ni <= indent)
          break;
      }
      ++end;
    }
    lines.erase(lines.begin() + start, lines.begin() + end);
    lines.insert(lines.begin() + start, block.begin(), block.end());
    return true;
  }
  return false;
}

bool ReadDelCtrl() {
  for (const auto& path : KeysFilePaths()) {
    std::wifstream in(path.c_str());
    if (!in)
      continue;
    std::wstring line;
    while (std::getline(in, line)) {
      if (line.find(L"Control+Delete") != std::wstring::npos)
        return true;
    }
  }
  return true;  // default: quick delete enabled
}

// editor: full binding map; Control+Delete removes the current candidate
void WriteDelCtrl(bool del_ctrl) {
  std::vector<std::wstring> editor_block = {
      L"  editor:",
      L"    bindings:",
      L"      space: confirm",
      L"      Return: commit_raw_input",
      L"      Control+Return: commit_script_text",
      L"      Control+Shift+Return: commit_comment",
      L"      BackSpace: revert",
      L"      Control+BackSpace: back_syllable",
      L"      Escape: cancel",
  };
  if (del_ctrl)
    editor_block.push_back(L"      Control+Delete: delete_candidate");

  for (const auto& path : KeysFilePaths()) {
    std::vector<std::wstring> lines;
    {
      std::wifstream in(path.c_str());
      std::wstring line;
      while (std::getline(in, line))
        lines.push_back(line);
    }
    ReplaceBlock(lines, L"editor:", editor_block);
    std::wofstream out(path.c_str());
    for (const auto& line : lines)
      out << line << L"\n";
  }
}

// ---- user data sync (moved from the Sync page) ----

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

// last-sync marker: written after a successful sync, shown on the page
std::wstring LastSyncFilePath() {
  return WeaselUserDataPath().wstring() + L"\\.last_sync";
}

}  // namespace

void static OpenFolderAndSelectItem(std::wstring filepath) {
  filepath = std::filesystem::path(filepath).make_preferred().wstring();
  std::wstring directory = std::filesystem::path(filepath).parent_path();

  // The deployer's UI thread is already STA-initialized (WinMain CoInitialize);
  // CoInitializeEx(MULTITHREADED) would return RPC_E_CHANGED_MODE without
  // adding a reference, so a matching CoUninitialize would drop the thread's
  // STA ref and break COM for the rest of the settings window. Match the
  // apartment.
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  bool com_inited = SUCCEEDED(hr);

  auto folder = ILCreateFromPath(directory.c_str());
  std::vector<LPITEMIDLIST> v;
  v.push_back(ILCreateFromPath(filepath.c_str()));

  SHOpenFolderAndSelectItems(folder, v.size(), (LPCITEMIDLIST*)v.data(), 0);

  for (auto idl : v) {
    ILFree(idl);
  }
  ILFree(folder);
  if (com_inited)
    CoUninitialize();
}

template <typename T, typename U>
inline static std::wstring DoFileDialog(HWND hwndOwner,
                                        LPCWSTR title,
                                        UINT filterSize,
                                        COMDLG_FILTERSPEC filter[],
                                        LPCWSTR filename,
                                        LPCWSTR defExt) {
  std::wstring path;
  HRESULT com_hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  bool com_inited = SUCCEEDED(com_hr);
  CComPtr<T> spFileDialog;
  if (SUCCEEDED(spFileDialog.CoCreateInstance(__uuidof(U)))) {
    spFileDialog->SetFileTypes(filterSize, filter);
    spFileDialog->SetTitle(title);
    if (filename)
      spFileDialog->SetFileName(filename);

    spFileDialog->SetDefaultExtension(defExt);
    if (SUCCEEDED(spFileDialog->Show(hwndOwner))) {
      CComPtr<IShellItem> spResult;
      if (SUCCEEDED(spFileDialog->GetResult(&spResult))) {
        wchar_t* name;
        if (SUCCEEDED(spResult->GetDisplayName(SIGDN_FILESYSPATH, &name))) {
          path = name;
          CoTaskMemFree(name);
        }
      }
    }
  }
  if (com_inited)
    CoUninitialize();
  return path;
}

DictManagementDialog::DictManagementDialog()
    : api_((RimeLeversApi*)rime_get_api()->find_module("levers")->get_api()),
      configurator_(nullptr),
      modified_(false) {}

DictManagementDialog::~DictManagementDialog() {}

HWND DictManagementDialog::CreateEmbedded(HWND host) {
  HWND hwnd = Create(host);
  if (hwnd) {
    LONG style = ::GetWindowLong(hwnd, GWL_STYLE);
    ::SetWindowLong(hwnd, GWL_STYLE,
                    (style & ~(WS_POPUP | WS_CAPTION | WS_SYSMENU)) | WS_CHILD);
    ::SetParent(hwnd, host);
    RECT rc = {0};
    ::GetClientRect(host, &rc);
    ::MoveWindow(hwnd, 0, 0, rc.right, rc.bottom, TRUE);
  }
  return hwnd;
}

void DictManagementDialog::Populate() {
  RimeUserDictIterator iter = {0};
  api_->user_dict_iterator_init(&iter);
  while (const char* dict = api_->next_user_dict(&iter)) {
    std::wstring txt = u8tow(dict);
    user_dict_list_.AddString(txt.c_str());
  }
  api_->user_dict_iterator_destroy(&iter);
  user_dict_list_.SetCurSel(-1);
}

LRESULT DictManagementDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
  user_dict_list_.Attach(GetDlgItem(IDC_USER_DICT_LIST));
  backup_.Attach(GetDlgItem(IDC_BACKUP));
  backup_.EnableWindow(FALSE);
  restore_.Attach(GetDlgItem(IDC_RESTORE));
  restore_.EnableWindow(TRUE);
  export_.Attach(GetDlgItem(IDC_EXPORT));
  export_.EnableWindow(FALSE);
  import_.Attach(GetDlgItem(IDC_IMPORT));
  import_.EnableWindow(FALSE);

  Populate();
  Load();

  CenterWindow();
  BringWindowToTop();
  return TRUE;
}

void DictManagementDialog::Load() {
  CheckRadioButton(IDC_DEL_CTRL, IDC_DEL_NONE,
                   ReadDelCtrl() ? IDC_DEL_CTRL : IDC_DEL_NONE);
  SetDlgItemTextW(IDC_SYNC_DIR, ReadSyncDir().c_str());
  UpdateLastSyncText();
  modified_ = false;
}

bool DictManagementDialog::Apply() {
  if (!modified_)
    return false;
  bool del_ctrl = IsDlgButtonChecked(IDC_DEL_CTRL) == BST_CHECKED;
  WriteDelCtrl(del_ctrl);
  WCHAR buf[512] = {0};
  GetDlgItemTextW(IDC_SYNC_DIR, buf, _countof(buf));
  WriteSyncDir(buf);
  modified_ = false;
  return true;
}

void DictManagementDialog::UpdateLastSyncText() {
  std::wstring when;
  std::wifstream in(LastSyncFilePath().c_str());
  if (in) {
    std::getline(in, when);
    // trim trailing whitespace/CR
    while (!when.empty() && (when.back() == L'\r' || when.back() == L'\n' ||
                             when.back() == L' '))
      when.pop_back();
  }
  SetDlgItemTextW(
      IDC_SYNC_STATUS,
      (L"上次同步：" + (when.empty() ? L"尚未同步" : when)).c_str());
}

LRESULT DictManagementDialog::OnChanged(WORD, WORD, HWND, BOOL&) {
  modified_ = true;
  return 0;
}

LRESULT DictManagementDialog::OnSyncNow(WORD, WORD, HWND, BOOL&) {
  // flush a pending sync-dir edit first so the sync uses what the user sees
  if (modified_) {
    WCHAR buf[512] = {0};
    GetDlgItemTextW(IDC_SYNC_DIR, buf, _countof(buf));
    WriteSyncDir(buf);
  }
  int rc = configurator_ ? configurator_->SyncUserData() : 1;
  if (rc == 0) {
    SYSTEMTIME st = {0};
    ::GetLocalTime(&st);
    WCHAR when[32] = {0};
    _snwprintf_s(when, _TRUNCATE, L"%04d-%02d-%02d %02d:%02d", st.wYear,
                 st.wMonth, st.wDay, st.wHour, st.wMinute);
    std::wofstream out(LastSyncFilePath().c_str());
    out << when << L"\n";
    UpdateLastSyncText();
  }
  ::MessageBox(m_hWnd, rc == 0 ? L"同步完成。" : L"同步失败，请检查同步目录。",
               L"【小狼毫】",
               MB_OK | (rc == 0 ? MB_ICONINFORMATION : MB_ICONERROR));
  return 0;
}

LRESULT DictManagementDialog::OnBrowse(WORD, WORD, HWND, BOOL&) {
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

LRESULT DictManagementDialog::OnOpenSyncDir(WORD, WORD, HWND, BOOL&) {
  // resolve the effective sync dir: the edit box if set, else librime's
  std::wstring dir = ReadSyncDir();
  if (dir.empty()) {
    char buf[MAX_PATH] = {0};
    rime_get_api()->get_user_data_sync_dir(buf, _countof(buf));
    WCHAR wbuf[MAX_PATH] = {0};
    MultiByteToWideChar(CP_ACP, 0, buf, -1, wbuf, _countof(wbuf));
    dir = wbuf;
  }
  if (dir.empty() ||
      ::GetFileAttributesW(dir.c_str()) == INVALID_FILE_ATTRIBUTES)
    ::MessageBox(m_hWnd, L"同步目录不存在，请先设置并同步。", L"【小狼毫】",
                 MB_OK | MB_ICONINFORMATION);
  else
    ::ShellExecuteW(NULL, L"open", dir.c_str(), NULL, NULL, SW_SHOW);
  return 0;
}

LRESULT DictManagementDialog::OnHelp(WORD, WORD wID, HWND, BOOL&) {
  const wchar_t* text = nullptr;
  switch (wID) {
    case IDC_HELP_DEL:
      text =
          L"删除当前候选。\n被删除的词会从用户词库移除（词典词条不受影响），之"
          L"后不再优先出现。";
      break;
    case IDC_HELP_SYNC:
      text =
          L"词库与配置将同步到该目录，用于多台设备间迁移。\n同步只合并、不"
          L"删除本地内容。";
      break;
  }
  if (text)
    ::MessageBox(m_hWnd, text, L"说明", MB_OK | MB_ICONINFORMATION);
  return 0;
}

LRESULT DictManagementDialog::OnClose(UINT, WPARAM, LPARAM, BOOL&) {
  EndDialog(IDCANCEL);
  return 0;
}

LRESULT DictManagementDialog::OnBackup(WORD, WORD code, HWND, BOOL&) {
  int sel = user_dict_list_.GetCurSel();
  if (sel < 0 || sel >= user_dict_list_.GetCount()) {
    MSG_BY_IDS(IDS_STR_SEL_EXPORT_DICT_NAME, IDS_STR_SAD,
               MB_OK | MB_ICONINFORMATION);
    return 0;
  }
  std::wstring path;
  {
    char dir[MAX_PATH] = {0};
    rime_get_api()->get_user_data_sync_dir(dir, _countof(dir));
    WCHAR wdir[MAX_PATH] = {0};
    MultiByteToWideChar(CP_ACP, 0, dir, -1, wdir, _countof(wdir));
    path = wdir;
  }
  if (_waccess_s(path.c_str(), 0) != 0 &&
      !CreateDirectoryW(path.c_str(), NULL) &&
      GetLastError() == ERROR_PATH_NOT_FOUND) {
    MSG_BY_IDS(IDS_STR_ERREXPORT_SYNC_UV, IDS_STR_SAD, MB_OK | MB_ICONERROR);
    return 0;
  }
  WCHAR dict_name[100] = {0};
  user_dict_list_.GetText(sel, dict_name);
  path += std::wstring(L"\\") + dict_name + L".userdb.txt";
  std::string dict_name_str = wtou8(dict_name);
  if (!api_->backup_user_dict(dict_name_str.c_str())) {
    MSG_BY_IDS(IDS_STR_ERR_EXPORT_UNKNOWN, IDS_STR_SAD, MB_OK | MB_ICONERROR);
    return 0;
  } else if (_waccess(path.c_str(), 0) != 0) {
    MSG_BY_IDS(IDS_STR_ERR_EXPORT_SNAP_LOST, IDS_STR_SAD, MB_OK | MB_ICONERROR);
    return 0;
  }
  OpenFolderAndSelectItem(path);
  return 0;
}

LRESULT DictManagementDialog::OnRestore(WORD, WORD code, HWND, BOOL&) {
  CString open_str, dict_snapshot_str, kcss_dict_snapshot_str, all_files_str;
  open_str.LoadStringW(IDS_STR_OPEN);
  dict_snapshot_str.LoadStringW(IDS_STR_DICT_SNAPSHOT);
  kcss_dict_snapshot_str.LoadStringW(IDS_STR_KCSS_DICT_SNAPSHOT);
  all_files_str.LoadStringW(IDS_STR_ALL_FILES);

  const std::wstring dict_snapshot_name =
      dict_snapshot_str + L" (*.userdb.txt)";
  const std::wstring kcss_dict_snapshot_name =
      kcss_dict_snapshot_str + L" (*.userdb.kct.snapshot)";
  const std::wstring all_files_name = all_files_str;

  COMDLG_FILTERSPEC filter[3] = {
      {dict_snapshot_name.c_str(), L"*.userdb.txt"},
      {kcss_dict_snapshot_name.c_str(), L"*.userdb.kct.snapshot"},
      {all_files_name.c_str(), L"*.*"}};

  std::wstring selected_path = DoFileDialog<IFileOpenDialog, FileOpenDialog>(
      m_hWnd, open_str, ARRAYSIZE(filter), filter, NULL, L"snapshot");
  if (!selected_path.empty()) {
    char path[MAX_PATH] = {0};
    WideCharToMultiByte(CP_UTF8, 0, selected_path.c_str(), -1, path,
                        _countof(path), NULL, NULL);
    if (!api_->restore_user_dict(path)) {
      MSG_BY_IDS(IDS_STR_ERR_UNKNOWN, IDS_STR_SAD, MB_OK | MB_ICONERROR);
    } else {
      MSG_BY_IDS(IDS_STR_ERR_SUCCESS, IDS_STR_HAPPY,
                 MB_OK | MB_ICONINFORMATION);
    }
  }
  return 0;
}

LRESULT DictManagementDialog::OnExport(WORD, WORD code, HWND, BOOL&) {
  CString save_as_str, exported_str, record_count_str, all_files_str,
      txt_files_str;
  save_as_str.LoadStringW(IDS_STR_SAVE_AS);
  exported_str.LoadStringW(IDS_STR_EXPORTED);
  record_count_str.LoadStringW(IDS_STR_RECORD_COUNT);
  txt_files_str.LoadStringW(IDS_STR_TXT_FILES);
  all_files_str.LoadStringW(IDS_STR_ALL_FILES);
  const std::wstring txt_files_name = txt_files_str + L" (*.txt)";
  const std::wstring all_files_name = all_files_str;

  int sel = user_dict_list_.GetCurSel();
  if (sel < 0 || sel >= user_dict_list_.GetCount()) {
    MSG_BY_IDS(IDS_STR_SEL_EXPORT_DICT_NAME, IDS_STR_SAD,
               MB_OK | MB_ICONINFORMATION);
    return 0;
  }
  WCHAR dict_name[MAX_PATH] = {0};
  user_dict_list_.GetText(sel, dict_name);
  std::wstring file_name(dict_name);
  file_name += L"_export.txt";

  COMDLG_FILTERSPEC filter[2] = {{txt_files_name.c_str(), L"*.txt"},
                                 {all_files_name.c_str(), L"*.*"}};

  OutputDebugString(filter[0].pszName);
  std::wstring selected_path = DoFileDialog<IFileSaveDialog, FileSaveDialog>(
      m_hWnd, save_as_str, ARRAYSIZE(filter), filter, file_name.c_str(),
      L"txt");
  if (!selected_path.empty()) {
    char path[MAX_PATH] = {0};
    WideCharToMultiByte(CP_UTF8, 0, selected_path.c_str(), -1, path,
                        _countof(path), NULL, NULL);
    std::string dict_name_str = wtou8(dict_name);
    int result = api_->export_user_dict(dict_name_str.c_str(), path);
    if (result < 0) {
      MSG_BY_IDS(IDS_STR_ERR_UNKNOWN, IDS_STR_SAD, MB_OK | MB_ICONERROR);
    } else if (_waccess(selected_path.c_str(), 0) != 0) {
      MSG_BY_IDS(IDS_STR_ERR_EXPORT_FILE_LOST, IDS_STR_SAD,
                 MB_OK | MB_ICONERROR);
    } else {
      std::wstring report(std::wstring(exported_str) + L" " +
                          std::to_wstring(result) + L" " +
                          std::wstring(record_count_str));
      MSG_ID_CAP(report.c_str(), IDS_STR_HAPPY, MB_OK | MB_ICONINFORMATION);
      OpenFolderAndSelectItem(selected_path);
    }
  }
  return 0;
}

LRESULT DictManagementDialog::OnImport(WORD, WORD code, HWND, BOOL&) {
  CString open_str, imported_str, record_count_str, all_files_str,
      txt_files_str;
  open_str.LoadStringW(IDS_STR_OPEN);
  imported_str.LoadStringW(IDS_STR_IMPORTED);
  record_count_str.LoadStringW(IDS_STR_RECORD_COUNT);
  txt_files_str.LoadStringW(IDS_STR_TXT_FILES);
  all_files_str.LoadStringW(IDS_STR_ALL_FILES);
  const std::wstring txt_files_name = txt_files_str + L" (*.txt)";
  const std::wstring all_files_name = all_files_str;

  int sel = user_dict_list_.GetCurSel();
  if (sel < 0 || sel >= user_dict_list_.GetCount()) {
    MSG_BY_IDS(IDS_STR_SEL_IMPORT_DICT_NAME, IDS_STR_SAD,
               MB_OK | MB_ICONINFORMATION);
    return 0;
  }
  WCHAR dict_name[MAX_PATH] = {0};
  user_dict_list_.GetText(sel, dict_name);
  std::wstring file_name(dict_name);
  file_name += L"_export.txt";

  COMDLG_FILTERSPEC filter[2] = {{txt_files_name.c_str(), L"*.txt"},
                                 {all_files_name.c_str(), L"*.*"}};

  OutputDebugString(filter[0].pszName);
  std::wstring selected_path = DoFileDialog<IFileOpenDialog, FileOpenDialog>(
      m_hWnd, open_str, ARRAYSIZE(filter), filter, file_name.c_str(), L"txt");
  if (!selected_path.empty()) {
    char path[MAX_PATH] = {0};
    WideCharToMultiByte(CP_UTF8, 0, selected_path.c_str(), -1, path,
                        _countof(path), NULL, NULL);
    int result = api_->import_user_dict(wtou8(dict_name).c_str(), path);
    if (result < 0) {
      MSG_BY_IDS(IDS_STR_ERR_UNKNOWN, IDS_STR_SAD, MB_OK | MB_ICONERROR);
    } else {
      std::wstring report(std::wstring(imported_str) + L" " +
                          std::to_wstring(result) + L" " +
                          std::wstring(record_count_str));
      MSG_ID_CAP(report.c_str(), IDS_STR_HAPPY, MB_OK | MB_ICONINFORMATION);
    }
  }
  return 0;
}

LRESULT DictManagementDialog::OnClearUserDb(WORD, WORD, HWND, BOOL&) {
  if (::MessageBox(m_hWnd,
                   L"将删除所有用户词库（自造词与选词记忆），操作不可撤销。"
                   L"\n建议先「输出词典快照」备份。确定清空吗？",
                   L"【小狼毫】", MB_YESNO | MB_ICONWARNING) != IDYES)
    return 0;
  int removed = 0;
  std::filesystem::path user_dir = WeaselUserDataPath();
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(user_dir, ec)) {
    if (ec)
      break;
    if (!entry.is_directory())
      continue;
    const std::wstring name = entry.path().filename().wstring();
    if (name.size() >= 6 && name.compare(name.size() - 6, 6, L".userdb") == 0) {
      std::filesystem::remove_all(entry.path(), ec);
      ++removed;
    }
  }
  ::MessageBox(m_hWnd,
               removed > 0 ? L"已清空用户词库。" : L"没有找到用户词库。",
               L"【小狼毫】", MB_OK | MB_ICONINFORMATION);
  return 0;
}

LRESULT DictManagementDialog::OnUserDictListSelChange(WORD, WORD, HWND, BOOL&) {
  int index = user_dict_list_.GetCurSel();
  BOOL enabled = index < 0 ? FALSE : TRUE;
  backup_.EnableWindow(enabled);
  export_.EnableWindow(enabled);
  import_.EnableWindow(enabled);
  return 0;
}
