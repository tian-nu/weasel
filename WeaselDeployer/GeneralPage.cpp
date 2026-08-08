#include "stdafx.h"
#include "GeneralPage.h"
#include <WeaselUtility.h>
#include <rime_api.h>
#include <rime_levers_api.h>
#include <fstream>
#pragma warning(disable : 4005)
#include "WeaselDeployer.h"

namespace {

// path of default.custom.yaml under the user data dir
std::wstring DefaultCustomFilePath() {
  return WeaselUserDataPath() / L"default.custom.yaml";
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

  modified_ = false;
  return api_->save_settings(settings_);
}
