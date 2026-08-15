#include "stdafx.h"
#include "SwitcherSettingsDialog.h"
#include "Configurator.h"
#include "UITheme.h"
#include <algorithm>
#include <set>
#include <rime_levers_api.h>
#include <WeaselUtility.h>
#include <thread>
#include "WeaselDeployer.h"

// theme-aware control backgrounds; no-op in light mode
LRESULT SwitcherSettingsDialog::OnCtlColor(UINT msg,
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

// dark item colors for the schema list; default drawing in light mode
LRESULT SwitcherSettingsDialog::OnSchemaListCustomDraw(int,
                                                       LPNMHDR hdr,
                                                       BOOL& handled) {
  if (!UITheme::IsDark()) {
    handled = FALSE;
    return CDRF_DODEFAULT;
  }
  LPNMCUSTOMDRAW cd = reinterpret_cast<LPNMCUSTOMDRAW>(hdr);
  if (cd->dwDrawStage == CDDS_PREPAINT) {
    handled = TRUE;
    return CDRF_NOTIFYITEMDRAW;
  }
  if (cd->dwDrawStage == CDDS_ITEMPREPAINT) {
    cd->clrText = RGB(232, 232, 232);
    cd->clrTextBk = RGB(48, 48, 48);
    handled = TRUE;
    return CDRF_DODEFAULT;
  }
  handled = FALSE;
  return CDRF_DODEFAULT;
}

SwitcherSettingsDialog::SwitcherSettingsDialog()
    : settings_(nullptr),
      loaded_(false),
      modified_(false),
      embedded_(false),
      fetching_(false) {
  api_ = (RimeLeversApi*)rime_get_api()->find_module("levers")->get_api();
}

SwitcherSettingsDialog::SwitcherSettingsDialog(RimeSwitcherSettings* settings)
    : settings_(settings),
      loaded_(false),
      modified_(false),
      embedded_(false),
      fetching_(false) {
  api_ = (RimeLeversApi*)rime_get_api()->find_module("levers")->get_api();
}

SwitcherSettingsDialog::~SwitcherSettingsDialog() {}

HWND SwitcherSettingsDialog::CreateEmbedded(HWND host) {
  // set before Create so OnInitDialog can branch on it
  embedded_ = true;
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

void SwitcherSettingsDialog::Populate() {
  if (!settings_)
    return;
  RimeSchemaList available = {0};
  api_->get_available_schema_list(settings_, &available);
  RimeSchemaList selected = {0};
  api_->get_selected_schema_list(settings_, &selected);
  schema_list_.DeleteAllItems();
  size_t k = 0;
  std::set<RimeSchemaInfo*> recruited;
  for (size_t i = 0; i < selected.size; ++i) {
    const char* schema_id = selected.list[i].schema_id;
    for (size_t j = 0; j < available.size; ++j) {
      RimeSchemaListItem& item(available.list[j]);
      RimeSchemaInfo* info = (RimeSchemaInfo*)item.reserved;
      if (!strcmp(item.schema_id, schema_id) &&
          recruited.find(info) == recruited.end()) {
        recruited.insert(info);
        std::wstring itemwstr = u8tow(item.name);
        schema_list_.AddItem(k, 0, itemwstr.c_str());
        schema_list_.SetItemData(k, (DWORD_PTR)info);
        schema_list_.SetCheckState(k, TRUE);
        ++k;
        break;
      }
    }
  }
  for (size_t i = 0; i < available.size; ++i) {
    RimeSchemaListItem& item(available.list[i]);
    RimeSchemaInfo* info = (RimeSchemaInfo*)item.reserved;
    if (recruited.find(info) == recruited.end()) {
      recruited.insert(info);
      std::wstring itemwstr = u8tow(item.name);
      schema_list_.AddItem(k, 0, itemwstr.c_str());
      schema_list_.SetItemData(k, (DWORD_PTR)info);
      ++k;
    }
  }
  auto hotkeys_str = api_->get_hotkeys(settings_);
  if (hotkeys_str) {
    std::wstring txt = u8tow(hotkeys_str);
    hotkeys_.SetWindowTextW(txt.c_str());
  }
  loaded_ = true;
  modified_ = false;
}

void SwitcherSettingsDialog::ShowDetails(RimeSchemaInfo* info) {
  if (!info)
    return;
  std::string details;
  if (const char* name = api_->get_schema_name(info)) {
    details += name;
  }
  if (const char* author = api_->get_schema_author(info)) {
    (details += "\n\n") += author;
  }
  if (const char* description = api_->get_schema_description(info)) {
    (details += "\n\n") += description;
  }
  std::wstring txt = u8tow(details.c_str());
  description_.SetWindowTextW(txt.c_str());
}

LRESULT SwitcherSettingsDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
  schema_list_.SubclassWindow(GetDlgItem(IDC_SCHEMA_LIST));
  schema_list_.SetExtendedListViewStyle(
      LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES,
      LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES);

  CString schema_name;
  schema_name.LoadStringW(IDS_STR_SCHEMA_NAME);
  schema_list_.AddColumn(schema_name, 0);
  CRect rc;
  schema_list_.GetClientRect(&rc);
  schema_list_.SetColumnWidth(0, rc.Width() - 20);

  description_.Attach(GetDlgItem(IDC_SCHEMA_DESCRIPTION));

  hotkeys_.Attach(GetDlgItem(IDC_HOTKEYS));
  // schema hotkeys (Ctrl+` menu) are editable: format like
  // "Control+grave, Control+Shift+grave, F4"
  hotkeys_.EnableWindow(TRUE);

  get_schemata_.Attach(GetDlgItem(IDC_GET_SCHEMATA));
  get_schemata_.EnableWindow(fetching_ ? FALSE : TRUE);

  // hide the leftover modal OK button when embedded in the settings window;
  // disabling it also stops Enter from silently re-running DoSave
  if (embedded_) {
    ::ShowWindow(GetDlgItem(IDOK), SW_HIDE);
    ::EnableWindow(GetDlgItem(IDOK), FALSE);
  }

  Populate();

  // select the first schema by default so the description pane is not empty
  if (schema_list_.GetItemCount() > 0) {
    schema_list_.SetItemState(0, LVIS_SELECTED, LVIS_SELECTED);
    ShowDetails((RimeSchemaInfo*)schema_list_.GetItemData(0));
  }

  CenterWindow();
  BringWindowToTop();
  return TRUE;
}

LRESULT SwitcherSettingsDialog::OnClose(UINT, WPARAM, LPARAM, BOOL&) {
  EndDialog(IDCANCEL);
  return 0;
}

LRESULT SwitcherSettingsDialog::OnGetSchemata(WORD, WORD, HWND hWndCtl, BOOL&) {
  if (fetching_)
    return 0;  // a fetch is already running
  HKEY hKey = NULL;
  std::wstring hPath;
  if (is_wow64())
    hPath = _T("Software\\WOW6432Node\\Rime\\Weasel");
  else
    hPath = _T("Software\\Rime\\Weasel");
  std::wstring weasel_root;
  LSTATUS ret = RegOpenKey(HKEY_LOCAL_MACHINE, hPath.c_str(), &hKey);
  if (ret == ERROR_SUCCESS) {
    WCHAR value[MAX_PATH] = {0};
    DWORD len = sizeof(value);
    DWORD type = 0;
    if (RegQueryValueExW(hKey, L"WeaselRoot", NULL, &type, (LPBYTE)value,
                         &len) == ERROR_SUCCESS &&
        type == REG_SZ) {
      weasel_root = value;
    }
    RegCloseKey(hKey);
  }
  if (weasel_root.empty())
    return 0;
  // /c: console auto-closes when the installer finishes. The old code used /k,
  // which kept the console open and froze the settings window until the user
  // closed it by hand.
  std::wstring parameters = L"/c \"" + weasel_root + L"\\rime-install.bat\"";
  SHELLEXECUTEINFOW cmd = {sizeof(SHELLEXECUTEINFO)};
  cmd.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
  cmd.hwnd = hWndCtl;
  cmd.lpVerb = L"open";
  cmd.lpFile = L"cmd";
  cmd.lpParameters = parameters.c_str();
  cmd.nShow = SW_SHOW;
  if (!ShellExecuteExW(&cmd) || !cmd.hProcess)
    return 0;
  fetching_ = true;
  get_schemata_.EnableWindow(FALSE);
  // Wait on a background thread so the settings window stays responsive. The
  // lambda captures only the window + process handles, never `this`, so a
  // dialog destroyed before the thread finishes only yields a benign
  // PostMessage to a dead window.
  std::thread([hwnd = m_hWnd, hProcess = cmd.hProcess]() {
    ::WaitForSingleObject(hProcess, INFINITE);
    ::CloseHandle(hProcess);
    ::PostMessage(hwnd, kWM_SchemataRefreshed, 0, 0);
  }).detach();
  return 0;
}

LRESULT SwitcherSettingsDialog::OnSchemataRefreshed(UINT,
                                                    WPARAM,
                                                    LPARAM,
                                                    BOOL&) {
  if (settings_)
    api_->load_settings(reinterpret_cast<RimeCustomSettings*>(settings_));
  Populate();
  fetching_ = false;
  get_schemata_.EnableWindow(TRUE);
  return 0;
}

bool SwitcherSettingsDialog::DoSave() {
  if (!modified_ || !settings_ || schema_list_.GetItemCount() == 0)
    return false;
  const char** selection = new const char*[schema_list_.GetItemCount()];
  int count = 0;
  for (int i = 0; i < schema_list_.GetItemCount(); ++i) {
    if (!schema_list_.GetCheckState(i))
      continue;
    RimeSchemaInfo* info = (RimeSchemaInfo*)(schema_list_.GetItemData(i));
    if (info) {
      selection[count++] = api_->get_schema_id(info);
    }
  }
  if (count == 0) {
    MSG_BY_IDS(IDS_STR_ERR_AT_LEAST_ONE_SEL, IDS_STR_NOT_REGULAR,
               MB_OK | MB_ICONEXCLAMATION);
    delete[] selection;
    return false;
  }
  api_->select_schemas(settings_, selection, count);
  delete[] selection;
  // persist the schema-menu hotkeys if the user edited them
  CString hotkeys_text;
  hotkeys_.GetWindowText(hotkeys_text);
  if (!hotkeys_text.IsEmpty()) {
    std::string hotkeys_utf8 = wtou8(hotkeys_text.GetString());
    api_->set_hotkeys(settings_, hotkeys_utf8.c_str());
  }
  // select_schemas only mutates the in-memory config; without save_settings the
  // selection is never written to default.custom.yaml.
  bool saved =
      api_->save_settings(reinterpret_cast<RimeCustomSettings*>(settings_));
  modified_ = false;
  return saved;
}

LRESULT SwitcherSettingsDialog::OnOK(WORD, WORD code, HWND, BOOL&) {
  DoSave();
  if (!embedded_)
    EndDialog(code);
  return 0;
}

LRESULT SwitcherSettingsDialog::OnHotkeysChanged(WORD, WORD, HWND, BOOL&) {
  modified_ = true;
  return 0;
}

bool SwitcherSettingsDialog::Apply() {
  return DoSave();
}

LRESULT SwitcherSettingsDialog::OnSchemaListItemChanged(int, LPNMHDR p, BOOL&) {
  LPNMLISTVIEW lv = reinterpret_cast<LPNMLISTVIEW>(p);
  if (!loaded_ || !lv || lv->iItem < 0 ||
      lv->iItem >= schema_list_.GetItemCount())
    return 0;
  if ((lv->uNewState & LVIS_STATEIMAGEMASK) !=
      (lv->uOldState & LVIS_STATEIMAGEMASK)) {
    modified_ = true;
  } else if ((lv->uNewState & LVIS_SELECTED) &&
             !(lv->uOldState & LVIS_SELECTED)) {
    ShowDetails((RimeSchemaInfo*)(schema_list_.GetItemData(lv->iItem)));
  }
  return 0;
}
