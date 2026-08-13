#include "stdafx.h"
#include "UIStyleSettingsDialog.h"
#include "UIStyleSettings.h"
#include "Configurator.h"
#include <WeaselUtility.h>
#include <commdlg.h>
#include <wincodec.h>
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "windowscodecs.lib")

UIStyleSettingsDialog::UIStyleSettingsDialog()
    : settings_(nullptr), loaded_(false), embedded_(false), modified_(false) {}

UIStyleSettingsDialog::UIStyleSettingsDialog(UIStyleSettings* settings)
    : settings_(settings), loaded_(false), embedded_(false), modified_(false) {}

UIStyleSettingsDialog::~UIStyleSettingsDialog() {
  image_.Destroy();
  preview_bmp_.DeleteObject();
}

HWND UIStyleSettingsDialog::CreateEmbedded(HWND host) {
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
bool UIStyleSettingsDialog::Apply() {
  if (!settings_ || !modified_)
    return false;
  RimeLeversApi* api =
      (RimeLeversApi*)rime_get_api()->find_module("levers")->get_api();
  if (!api)
    return false;
  // Reload first: the General page may have just written weasel.custom.yaml
  // for horizontal/tray. Saving this (init-time-loaded) config without
  // reloading would clobber that change. Re-apply the chosen scheme on the
  // fresh config, then persist.
  api->load_settings(settings_->settings());
  int index = color_schemes_.GetCurSel();
  if (index >= 0 && index < (int)preset_.size())
    settings_->SelectColorScheme(preset_[index].color_scheme_id);
  // candidate font size shares style/font_point with the font picker
  int point = GetDlgItemInt(IDC_FONT_POINT, NULL, FALSE);
  if (point < 8)
    point = 8;
  if (point > 40)
    point = 40;
  api->customize_int(settings_->settings(), "style/font_point", point);
  // inline preedit: show pinyin at the caret, candidates in the popup only
  bool inline_preedit = IsDlgButtonChecked(IDC_CHECK_INLINE) == BST_CHECKED;
  api->customize_bool(settings_->settings(), "style/inline_preedit",
                      inline_preedit);
  bool saved = api->save_settings(settings_->settings());
  modified_ = false;
  return saved;
}

void UIStyleSettingsDialog::Populate() {
  if (!settings_)
    return;
  std::string active(settings_->GetActiveColorScheme());
  int active_index = -1;
  settings_->GetPresetColorSchemes(&preset_);
  for (size_t i = 0; i < preset_.size(); ++i) {
    std::wstring txt = u8tow(preset_[i].name);
    color_schemes_.AddString(txt.c_str());
    if (preset_[i].color_scheme_id == active) {
      active_index = i;
    }
  }
  color_schemes_.SetCurSel(active_index);
  Preview(active_index);
  loaded_ = true;
}

LRESULT UIStyleSettingsDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
  color_schemes_.Attach(GetDlgItem(IDC_COLOR_SCHEME));
  preview_.Attach(GetDlgItem(IDC_PREVIEW));
  select_font_.Attach(GetDlgItem(IDC_SELECT_FONT));

  if (settings_) {
    RimeLeversApi* api =
        (RimeLeversApi*)rime_get_api()->find_module("levers")->get_api();
    if (api) {
      RimeConfig config = {0};
      api->settings_get_config(settings_->settings(), &config);
      int point = 14;  // weasel.yaml default
      rime_get_api()->config_get_int(&config, "style/font_point", &point);
      WCHAR buf[16] = {0};
      _itow_s(point, buf, 10);
      SetDlgItemTextW(IDC_FONT_POINT, buf);
      Bool inline_preedit = 0;
      if (rime_get_api()->config_get_bool(&config, "style/inline_preedit",
                                          &inline_preedit)) {
        CheckDlgButton(IDC_CHECK_INLINE,
                       inline_preedit ? BST_CHECKED : BST_UNCHECKED);
      }
    }
  }

  if (embedded_) {
    ::ShowWindow(GetDlgItem(IDOK), SW_HIDE);
    ::EnableWindow(GetDlgItem(IDOK), FALSE);
  }
  Populate();

  CenterWindow();
  BringWindowToTop();
  return TRUE;
}

LRESULT UIStyleSettingsDialog::OnClose(UINT, WPARAM, LPARAM, BOOL&) {
  EndDialog(IDCANCEL);
  return 0;
}

LRESULT UIStyleSettingsDialog::OnOK(WORD, WORD code, HWND, BOOL&) {
  if (!embedded_)
    EndDialog(code);
  return 0;
}

// pick a font for the candidate window; persists style/font_face and
// style/font_point to weasel.custom.yaml
LRESULT UIStyleSettingsDialog::OnSelectFont(WORD, WORD, HWND, BOOL&) {
  if (!settings_)
    return 0;
  RimeLeversApi* api =
      (RimeLeversApi*)rime_get_api()->find_module("levers")->get_api();
  if (!api)
    return 0;

  RimeConfig config = {0};
  api->settings_get_config(settings_->settings(), &config);
  RimeApi* rime = rime_get_api();

  int point = 14;  // weasel.yaml default
  rime->config_get_int(&config, "style/font_point", &point);
  const char* face = rime->config_get_cstring(&config, "style/font_face");

  HDC dc = ::GetDC(NULL);
  int ppi = dc ? ::GetDeviceCaps(dc, LOGPIXELSY) : 96;
  if (dc)
    ::ReleaseDC(NULL, dc);

  LOGFONT lf = {0};
  lf.lfHeight = -MulDiv(point, ppi, 72);
  lf.lfWeight = FW_NORMAL;
  wcscpy_s(lf.lfFaceName, L"Microsoft YaHei");
  if (face) {
    std::wstring face_name = u8tow(face);
    if (!face_name.empty())
      wcscpy_s(lf.lfFaceName, face_name.c_str());
  }

  CHOOSEFONT cf = {sizeof(cf)};
  cf.hwndOwner = m_hWnd;
  cf.lpLogFont = &lf;
  cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_NOSCRIPTSEL |
             CF_FORCEFONTEXIST;
  if (!::ChooseFont(&cf))
    return 0;

  std::string face_name = wtou8(lf.lfFaceName);
  if (face_name.empty())
    return 0;
  api->customize_string(settings_->settings(), "style/font_face",
                        face_name.c_str());
  int new_point = -MulDiv(lf.lfHeight, 72, ppi);
  if (new_point < 1)
    new_point = point;
  api->customize_int(settings_->settings(), "style/font_point", new_point);
  modified_ = true;
  return 0;
}

LRESULT UIStyleSettingsDialog::OnFontPointChanged(WORD, WORD, HWND, BOOL&) {
  modified_ = true;
  return 0;
}

LRESULT UIStyleSettingsDialog::OnInlineChanged(WORD, WORD, HWND, BOOL&) {
  modified_ = true;
  return 0;
}

namespace {

// Decode a PNG via WIC into a top-down 32bpp DIB section. Avoids GDI+
// (CImage::Load), which can hang the UI thread on some systems.
HBITMAP LoadPreviewBitmap(const wchar_t* path) {
  CComPtr<IWICImagingFactory> factory;
  if (FAILED(::CoCreateInstance(CLSID_WICImagingFactory, NULL,
                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
    return NULL;
  CComPtr<IWICBitmapDecoder> decoder;
  if (FAILED(factory->CreateDecoderFromFilename(
          path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder)))
    return NULL;
  CComPtr<IWICBitmapFrameDecode> frame;
  if (FAILED(decoder->GetFrame(0, &frame)))
    return NULL;
  CComPtr<IWICFormatConverter> converter;
  if (FAILED(factory->CreateFormatConverter(&converter)))
    return NULL;
  if (FAILED(converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA,
                                   WICBitmapDitherTypeNone, NULL, 0.0,
                                   WICBitmapPaletteTypeCustom)))
    return NULL;
  UINT width = 0, height = 0;
  converter->GetSize(&width, &height);
  if (!width || !height || width > 4096 || height > 4096)
    return NULL;
  BITMAPINFO bmi = {0};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = (LONG)width;
  bmi.bmiHeader.biHeight = -(LONG)height;  // top-down
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = NULL;
  HDC dc = ::GetDC(NULL);
  HBITMAP hbmp = ::CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
  ::ReleaseDC(NULL, dc);
  if (!hbmp || !bits) {
    if (hbmp)
      ::DeleteObject(hbmp);
    return NULL;
  }
  if (FAILED(converter->CopyPixels(NULL, width * 4, width * height * 4,
                                   (BYTE*)bits))) {
    ::DeleteObject(hbmp);
    return NULL;
  }
  return hbmp;
}

}  // namespace

LRESULT UIStyleSettingsDialog::OnColorSchemeSelChange(WORD, WORD, HWND, BOOL&) {
  int index = color_schemes_.GetCurSel();
  if (index >= 0 && index < (int)preset_.size()) {
    settings_->SelectColorScheme(preset_[index].color_scheme_id);
    Preview(index);
    modified_ = true;
  }
  return 0;
}

void UIStyleSettingsDialog::Preview(int index) {
  if (index < 0 || index >= (int)preset_.size())
    return;
  const std::string file_path(
      settings_->GetColorSchemePreview(preset_[index].color_scheme_id));
  if (file_path.empty())
    return;
  preview_bmp_.DeleteObject();
  image_.Destroy();
  HBITMAP hbmp = LoadPreviewBitmap(acptow(file_path).c_str());
  if (hbmp) {
    preview_bmp_.Attach(hbmp);
    preview_.SetBitmap(hbmp);
  } else {
    // last-resort fallback; GDI+ is normally avoided
    image_.Load(acptow(file_path).c_str());
    if (!image_.IsNull()) {
      preview_.SetBitmap(image_);
    }
  }
}
