#include "stdafx.h"
#include "GeneralPage.h"
#include <WeaselUtility.h>
#include <rime_api.h>
#include <rime_levers_api.h>
#include <fstream>
#include <vector>
#pragma warning(disable : 4005)
#include "WeaselDeployer.h"

namespace {

// path of default.custom.yaml under the user data dir
std::wstring DefaultCustomFilePath() {
  return WeaselUserDataPath() / L"default.custom.yaml";
}

// path of luna_pinyin.custom.yaml under the user data dir
std::wstring FuzzyFilePath() {
  return WeaselUserDataPath() / L"luna_pinyin.custom.yaml";
}

struct FuzzyFlags {
  bool nl = false;     // n/l
  bool flat = false;   // z/zh c/ch s/sh
  bool nasal = false;  // an/ang en/eng in/ing
  bool lr = false;     // l/r
};

// read the derive rules currently patched into luna_pinyin.custom.yaml
FuzzyFlags ReadFuzzy() {
  FuzzyFlags f;
  std::wifstream in(FuzzyFilePath().c_str());
  if (!in)
    return f;
  std::wstring line;
  while (std::getline(in, line)) {
    if (line.find(L"derive/^l/n/") != std::wstring::npos)
      f.nl = true;
    else if (line.find(L"derive/^([zcs])h/$1/") != std::wstring::npos)
      f.flat = true;
    else if (line.find(L"derive/([^aeiou])in$/$1ing/") != std::wstring::npos)
      f.nasal = true;
    else if (line.find(L"derive/^l/r/") != std::wstring::npos)
      f.lr = true;
  }
  return f;
}

// line-level update of the speller/algebra/__append block: existing derive
// rules are replaced, everything else in the file (e.g. the AI page's patch)
// is preserved.
void WriteFuzzy(const FuzzyFlags& f) {
  std::wstring path = FuzzyFilePath();
  std::vector<std::wstring> lines;
  {
    std::wifstream in(path.c_str());
    std::wstring line;
    while (std::getline(in, line))
      lines.push_back(line);
  }

  std::vector<std::wstring> rules;
  if (f.nl) {
    rules.push_back(L"derive/^l/n/");
    rules.push_back(L"derive/^n/l/");
  }
  if (f.flat) {
    rules.push_back(L"derive/^([zcs])h/$1/");
    rules.push_back(L"derive/^([zcs])([^h])/$1h$2/");
  }
  if (f.nasal) {
    rules.push_back(L"derive/([^aeiou])an$/$1ang/");
    rules.push_back(L"derive/([^aeiou])ang$/$1an/");
    rules.push_back(L"derive/([^aeiou])en$/$1eng/");
    rules.push_back(L"derive/([^aeiou])eng$/$1en/");
    rules.push_back(L"derive/([^aeiou])in$/$1ing/");
    rules.push_back(L"derive/([^aeiou])ing$/$1in/");
  }
  if (f.lr) {
    rules.push_back(L"derive/^l/r/");
    rules.push_back(L"derive/^r/l/");
  }

  // locate an existing append block: the line holding the key, plus every
  // following line that lists a derive rule
  int start = -1;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (lines[i].find(L"speller/algebra/__append") != std::wstring::npos) {
      start = (int)i;
      break;
    }
  }
  int end = -1;
  if (start >= 0) {
    end = start + 1;
    while ((size_t)end < lines.size() &&
           lines[end].find(L"- derive/") != std::wstring::npos)
      ++end;
  }

  std::vector<std::wstring> out;
  if (start >= 0) {
    out.insert(out.end(), lines.begin(), lines.begin() + start);
    out.push_back(L"  speller/algebra/__append:");
    for (const auto& rule : rules)
      out.push_back(L"    - " + rule);
    out.insert(out.end(), lines.begin() + end, lines.end());
  } else {
    out = lines;
    bool has_patch = false;
    for (const auto& line : lines)
      if (line.find(L"patch:") != std::wstring::npos)
        has_patch = true;
    if (!has_patch)
      out.push_back(L"patch:");
    out.push_back(L"  speller/algebra/__append:");
    for (const auto& rule : rules)
      out.push_back(L"    - " + rule);
  }

  std::wofstream out_file(path.c_str());
  for (const auto& line : out)
    out_file << line << L"\n";
}

}  // namespace

GeneralPage::GeneralPage()
    : api_(nullptr),
      settings_(nullptr),
      default_settings_(nullptr),
      modified_(false) {
  RimeApi* rime = rime_get_api();
  RimeModule* levers = rime->find_module("levers");
  if (levers) {
    api_ = (RimeLeversApi*)levers->get_api();
    settings_ = api_->custom_settings_init("weasel", "Weasel::GeneralPage");
    // "default" custom settings own the global menu/page_size patch; using the
    // API (load -> customize -> save) preserves other patches such as the
    // schema_list written by the schemes page.
    default_settings_ =
        api_->custom_settings_init("default", "Weasel::GeneralPage");
  }
}

GeneralPage::~GeneralPage() {
  if (api_ && settings_) {
    api_->custom_settings_destroy(settings_);
  }
  if (api_ && default_settings_) {
    api_->custom_settings_destroy(default_settings_);
  }
}

LRESULT GeneralPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
  Load();
  return TRUE;
}

LRESULT GeneralPage::OnClose(UINT, WPARAM, LPARAM, BOOL&) {
  DestroyWindow();
  return 0;
}

LRESULT GeneralPage::OnLayoutChanged(WORD, WORD, HWND, BOOL&) {
  modified_ = true;
  return 0;
}

LRESULT GeneralPage::OnTrayIconChanged(WORD, WORD, HWND, BOOL&) {
  modified_ = true;
  return 0;
}

LRESULT GeneralPage::OnPageSizeChanged(WORD, WORD, HWND, BOOL&) {
  modified_ = true;
  return 0;
}

LRESULT GeneralPage::OnFuzzyChanged(WORD, WORD, HWND, BOOL&) {
  modified_ = true;
  return 0;
}

LRESULT GeneralPage::OnRedeploy(WORD, WORD, HWND, BOOL&) {
  RimeApi* rime = rime_get_api();
  if (!rime)
    return 0;
  // mirror Configurator::UpdateWorkspace's core steps without the mutex/TSF
  // maintenance dance; a few seconds at most
  bool ok = rime->deploy() != 0;
  ok = rime->deploy_config_file("weasel.yaml", "config_version") != 0 && ok;
  ::MessageBox(m_hWnd, ok ? L"重新部署完成。" : L"重新部署失败，请查看日志。",
               L"【小狼毫】", MB_OK | (ok ? MB_ICONINFORMATION : MB_ICONERROR));
  return 0;
}

LRESULT GeneralPage::OnOpenDataDir(WORD, WORD, HWND, BOOL&) {
  ::ShellExecuteW(NULL, L"open", WeaselUserDataPath().c_str(), NULL, NULL,
                  SW_SHOW);
  return 0;
}

void GeneralPage::Load() {
  if (!api_ || !settings_)
    return;
  if (!api_->load_settings(settings_))
    return;

  RimeConfig config = {0};
  api_->settings_get_config(settings_, &config);
  RimeApi* rime = rime_get_api();

  bool horizontal = true;  // default: horizontal candidates
  Bool horizontal_value = horizontal ? 1 : 0;
  if (rime->config_get_bool(&config, "style/horizontal", &horizontal_value))
    horizontal = horizontal_value != 0;
  CheckRadioButton(IDC_RADIO_HORIZONTAL, IDC_RADIO_VERTICAL,
                   horizontal ? IDC_RADIO_HORIZONTAL : IDC_RADIO_VERTICAL);

  bool tray_icon = false;
  Bool tray_icon_value = 0;
  if (rime->config_get_bool(&config, "style/display_tray_icon",
                            &tray_icon_value))
    tray_icon = tray_icon_value != 0;
  CheckDlgButton(IDC_CHECK_TRAY_ICON, tray_icon ? BST_CHECKED : BST_UNCHECKED);

  // menu/page_size lives in the default.custom.yaml patch (settings_get_config
  // only exposes the base default.yaml, not the patch), so read it directly.
  int page_size = 7;  // Configurator seeds this on first run
  std::wifstream in(DefaultCustomFilePath().c_str());
  if (in) {
    std::wstring line;
    while (std::getline(in, line)) {
      size_t p = line.find(L"page_size");
      if (p == std::wstring::npos)
        continue;
      size_t colon = line.find(L':', p);
      if (colon == std::wstring::npos)
        continue;
      page_size = _wtoi(line.substr(colon + 1).c_str());
    }
  }
  if (page_size < 1 || page_size > 20)
    page_size = 7;
  WCHAR buf[16] = {0};
  _itow_s(page_size, buf, 10);
  SetDlgItemTextW(IDC_PAGE_SIZE, buf);

  FuzzyFlags f = ReadFuzzy();
  CheckDlgButton(IDC_CHECK_NL, f.nl ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(IDC_CHECK_FLAT, f.flat ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(IDC_CHECK_NASAL, f.nasal ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(IDC_CHECK_LR, f.lr ? BST_CHECKED : BST_UNCHECKED);

  modified_ = false;
}

bool GeneralPage::Apply() {
  if (!api_ || !settings_ || !modified_)
    return false;
  if (!api_->load_settings(settings_))
    return false;

  bool horizontal = IsDlgButtonChecked(IDC_RADIO_HORIZONTAL) == BST_CHECKED;
  api_->customize_bool(settings_, "style/horizontal", horizontal);

  bool tray_icon = IsDlgButtonChecked(IDC_CHECK_TRAY_ICON) == BST_CHECKED;
  api_->customize_bool(settings_, "style/display_tray_icon", tray_icon);

  int page_size = GetDlgItemInt(IDC_PAGE_SIZE, NULL, FALSE);
  if (page_size < 1)
    page_size = 1;
  if (page_size > 20)
    page_size = 20;
  if (default_settings_) {
    api_->load_settings(default_settings_);
    api_->customize_int(default_settings_, "menu/page_size", page_size);
    api_->save_settings(default_settings_);
  }

  FuzzyFlags f;
  f.nl = IsDlgButtonChecked(IDC_CHECK_NL) == BST_CHECKED;
  f.flat = IsDlgButtonChecked(IDC_CHECK_FLAT) == BST_CHECKED;
  f.nasal = IsDlgButtonChecked(IDC_CHECK_NASAL) == BST_CHECKED;
  f.lr = IsDlgButtonChecked(IDC_CHECK_LR) == BST_CHECKED;
  WriteFuzzy(f);

  modified_ = false;
  return api_->save_settings(settings_);
}
