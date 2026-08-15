#pragma once

#include "resource.h"
#include "UIStyleSettings.h"

class UIStyleSettingsDialog : public CDialogImpl<UIStyleSettingsDialog> {
 public:
  enum { IDD = IDD_STYLE_SETTING };

  UIStyleSettingsDialog();
  explicit UIStyleSettingsDialog(UIStyleSettings* settings);
  ~UIStyleSettingsDialog();

  void Init(UIStyleSettings* settings) { settings_ = settings; }

  HWND CreateEmbedded(HWND host);
  // persists color scheme selection
  bool Apply();

 protected:
  BEGIN_MSG_MAP(UIStyleSettingsDialog)
  MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
  MESSAGE_HANDLER(WM_CLOSE, OnClose)
  MESSAGE_RANGE_HANDLER(WM_CTLCOLORMSGBOX, WM_CTLCOLORSTATIC, OnCtlColor)
  COMMAND_ID_HANDLER(IDOK, OnOK)
  COMMAND_ID_HANDLER(IDC_SELECT_FONT, OnSelectFont)
  COMMAND_ID_HANDLER(IDC_FONT_POINT, OnFontPointChanged)
  COMMAND_ID_HANDLER(IDC_CHECK_INLINE, OnInlineChanged)
  COMMAND_HANDLER(IDC_COLOR_SCHEME, LBN_SELCHANGE, OnColorSchemeSelChange)
  COMMAND_HANDLER(IDC_COLOR_SCHEME_DARK, LBN_SELCHANGE, OnDarkSchemeSelChange)
  COMMAND_RANGE_HANDLER(IDC_HELP_INLINE, IDC_HELP_DARK, OnHelp)
  END_MSG_MAP()

  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnCtlColor(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnOK(WORD, WORD code, HWND, BOOL&);
  LRESULT OnSelectFont(WORD, WORD, HWND, BOOL&);
  LRESULT OnFontPointChanged(WORD, WORD, HWND, BOOL&);
  LRESULT OnInlineChanged(WORD, WORD, HWND, BOOL&);
  LRESULT OnHelp(WORD, WORD wID, HWND, BOOL&);
  LRESULT OnColorSchemeSelChange(WORD, WORD, HWND, BOOL&);
  LRESULT OnDarkSchemeSelChange(WORD, WORD, HWND, BOOL&);

  void Populate();
  void Preview(int index);

  UIStyleSettings* settings_;
  bool loaded_;
  bool embedded_;
  bool modified_;
  std::vector<ColorSchemeInfo> preset_;

  CListBox color_schemes_;
  CListBox color_schemes_dark_;
  CStatic preview_;
  CImage image_;
  CBitmap preview_bmp_;  // WIC-decoded preview bitmap (GDI+ avoided)
  CButton select_font_;
};
