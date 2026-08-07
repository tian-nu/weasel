#include "stdafx.h"
#include "GeneralPage.h"
#include <WeaselUtility.h>
#include <rime_api.h>
#include <rime_levers_api.h>
#pragma warning(disable : 4005)
#include "WeaselDeployer.h"

GeneralPage::GeneralPage()
    : api_(nullptr), settings_(nullptr), modified_(false) {
  RimeApi* rime = rime_get_api();
  RimeModule* levers = rime->find_module("levers");
  if (levers) {
    api_ = (RimeLeversApi*)levers->get_api();
    settings_ = api_->custom_settings_init("weasel", "Weasel::GeneralPage");
  }
}

GeneralPage::~GeneralPage() {
  if (api_ && settings_) {
    api_->custom_settings_destroy(settings_);
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

  modified_ = false;
}

bool GeneralPage::Apply() {
  if (!api_ || !settings_ || !modified_)
    return true;
  if (!api_->load_settings(settings_))
    return false;

  bool horizontal = IsDlgButtonChecked(IDC_RADIO_HORIZONTAL) == BST_CHECKED;
  api_->customize_bool(settings_, "style/horizontal", horizontal);

  bool tray_icon = IsDlgButtonChecked(IDC_CHECK_TRAY_ICON) == BST_CHECKED;
  api_->customize_bool(settings_, "style/display_tray_icon", tray_icon);

  modified_ = false;
  return api_->save_settings(settings_);
}
