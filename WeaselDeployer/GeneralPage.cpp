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

// base speller rules shared by the luna_pinyin family (from librime's
// pinyin.yaml: abbreviation + spelling_correction + key_correction). A
// full-list override is required because librime's __append does not work
// for speller/algebra (see rime-ice#163, oh-my-rime fuzzy guide).
std::vector<std::wstring> BaseAlgebraRules() {
  return {
      L"abbrev/^([a-z]).+$/$1/",
      L"abbrev/^([zcs]h).+$/$1/",
      L"derive/^([nl])ve$/$1ue/correction",
      L"derive/^([jqxy])u/$1v/correction",
      L"derive/un$/uen/correction",
      L"derive/ui$/uei/correction",
      L"derive/iu$/iou/correction",
      L"derive/([aeiou])ng$/$1gn/correction",
      L"derive/([dtngkhrzcs])o(u|ng)$/$1o/correction",
      L"derive/ong$/on/correction",
      L"derive/ao$/oa/correction",
      L"derive/([iu])a(o|ng?)$/a$1$2/correction",
  };
}

// read the derive rules currently patched into the custom yamls
FuzzyFlags ReadFuzzy() {
  FuzzyFlags f;
  std::wstring simp_path =
      (WeaselUserDataPath() / L"luna_pinyin_simp.custom.yaml").wstring();
  for (const auto& path : {FuzzyFilePath(), simp_path}) {
    std::wifstream in(path.c_str());
    if (!in)
      continue;
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
  }
  return f;
}

// line-level update of the speller/algebra block in a custom yaml: the
// complete rule list (base + selected fuzzy rules) replaces the existing
// block, everything else in the file is preserved.
void WriteFuzzyTo(const std::wstring& path, const FuzzyFlags& f) {
  std::vector<std::wstring> lines;
  {
    std::wifstream in(path.c_str());
    std::wstring line;
    while (std::getline(in, line))
      lines.push_back(line);
  }

  std::vector<std::wstring> rules = BaseAlgebraRules();
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

  // locate an existing "  speller/algebra:" block and the rule lines under it
  int start = -1;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (lines[i].find(L"speller/algebra:") != std::wstring::npos &&
        lines[i].find(L"__") == std::wstring::npos) {
      start = (int)i;
      break;
    }
  }
  int end = -1;
  if (start >= 0) {
    end = start + 1;
    while ((size_t)end < lines.size() &&
           (lines[end].find(L"    - ") != std::wstring::npos ||
            lines[end].find(L"      ") != std::wstring::npos))
      ++end;
  }

  std::vector<std::wstring> out;
  if (start >= 0) {
    out.insert(out.end(), lines.begin(), lines.begin() + start);
  } else {
    out = lines;
    bool has_patch = false;
    for (const auto& line : lines)
      if (line.find(L"patch:") != std::wstring::npos)
        has_patch = true;
    if (!has_patch)
      out.push_back(L"patch:");
  }
  out.push_back(L"  speller/algebra:");
  for (const auto& rule : rules)
    out.push_back(L"    - " + rule);
  if (start >= 0)
    out.insert(out.end(), lines.begin() + end, lines.end());

  std::wofstream out_file(path.c_str());
  for (const auto& line : out)
    out_file << line << L"\n";
}

// fuzzy rules apply to both pinyin schemas (simplified default + traditional)
void WriteFuzzy(const FuzzyFlags& f) {
  WriteFuzzyTo(FuzzyFilePath(), f);
  std::wstring simp = WeaselUserDataPath() / L"luna_pinyin_simp.custom.yaml";
  WriteFuzzyTo(simp, f);
}

// ---- initial state / switch key / paging (moved from the Keys page) ----

// custom yamls that receive the keys patch (simplified default + traditional)
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

// update a scalar patch line like `  "switches/@0/reset": 0`, inserting a
// `patch:` header first if the file has none.
void UpsertPatchLine(std::vector<std::wstring>& lines,
                     const std::wstring& line) {
  for (size_t i = 0; i < lines.size(); ++i) {
    if (lines[i].find(line.substr(0, line.find(L':') + 1)) !=
        std::wstring::npos) {
      lines[i] = line;
      return;
    }
  }
  bool has_patch = false;
  for (const auto& l : lines)
    if (l.find(L"patch:") != std::wstring::npos)
      has_patch = true;
  if (!has_patch)
    lines.insert(lines.begin(), L"patch:");
  // insert right after "patch:" (or after its first child)
  for (size_t i = 0; i < lines.size(); ++i) {
    if (lines[i].find(L"patch:") != std::wstring::npos) {
      size_t j = i + 1;
      while (j < lines.size() &&
             (lines[j].empty() ||
              lines[j].find_first_not_of(L" \t") == std::wstring::npos ||
              lines[j].find(L"  ") == 0))
        ++j;
      lines.insert(lines.begin() + j, line);
      return;
    }
  }
  lines.push_back(line);
}

struct KeysState {
  bool init_cn = true;       // ascii_mode reset 0
  bool init_half = true;     // full_shape reset 0
  bool switch_shift = true;  // Shift toggles ascii
  bool switch_ctrl = false;
  bool page_comma = true;
  bool page_minus = true;
  bool page_bracket = false;
};

KeysState ReadKeysState() {
  KeysState st;
  for (const auto& path : KeysFilePaths()) {
    std::wifstream in(path.c_str());
    if (!in)
      continue;
    std::wstring line;
    while (std::getline(in, line)) {
      if (line.find(L"switches/@0/reset") != std::wstring::npos) {
        if (line.find(L": 1") != std::wstring::npos ||
            line.find(L":1") != std::wstring::npos)
          st.init_cn = false;
      } else if (line.find(L"switches/@1/reset") != std::wstring::npos) {
        if (line.find(L": 1") != std::wstring::npos ||
            line.find(L":1") != std::wstring::npos)
          st.init_half = false;
      } else if (line.find(L"Shift_L:") != std::wstring::npos) {
        st.switch_shift = line.find(L"inline_ascii") != std::wstring::npos;
      } else if (line.find(L"Control_L:") != std::wstring::npos) {
        st.switch_ctrl = line.find(L"inline_ascii") != std::wstring::npos;
      } else if (line.find(L"accept: comma") != std::wstring::npos) {
        st.page_comma = true;
      } else if (line.find(L"accept: minus") != std::wstring::npos) {
        st.page_minus = true;
      } else if (line.find(L"accept: bracketleft") != std::wstring::npos) {
        st.page_bracket = true;
      }
    }
  }
  return st;
}

void WriteKeysState(const KeysState& st) {
  // switch_key: Shift_L/Shift_R toggle ascii, or Control_L/Control_R, or none
  std::wstring shift_action = st.switch_shift ? L"inline_ascii" : L"noop";
  std::wstring ctrl_action = st.switch_ctrl ? L"inline_ascii" : L"noop";
  std::vector<std::wstring> switch_key_block = {
      L"  ascii_composer:",
      L"    switch_key:",
      L"      Shift_L: " + shift_action,
      L"      Shift_R: commit_code",
      L"      Control_L: " + ctrl_action,
      L"      Control_R: " + ctrl_action,
  };

  // key_binder: control bindings + paging keys per selection
  std::vector<std::wstring> bindings = {
      L"  key_binder:",
      L"    bindings:",
      L"      - {accept: \"Control+p\", send: Up, when: composing}",
      L"      - {accept: \"Control+n\", send: Down, when: composing}",
      L"      - {accept: \"Control+b\", send: Left, when: composing}",
      L"      - {accept: \"Control+f\", send: Right, when: composing}",
      L"      - {accept: \"Control+a\", send: Home, when: composing}",
      L"      - {accept: \"Control+e\", send: End, when: composing}",
      L"      - {accept: \"Control+d\", send: Delete, when: composing}",
      L"      - {accept: \"Control+k\", send: \"Shift+Delete\", when: "
      L"composing}",
      L"      - {accept: \"Control+h\", send: BackSpace, when: composing}",
      L"      - {accept: \"Control+g\", send: Escape, when: composing}",
      L"      - {accept: \"Control+bracketleft\", send: Escape, when: "
      L"composing}",
      L"      - {accept: \"Control+y\", send: Page_Up, when: composing}",
      L"      - {accept: \"Alt+v\", send: Page_Up, when: composing}",
      L"      - {accept: \"Control+v\", send: Page_Down, when: composing}",
  };
  if (st.page_comma) {
    bindings.push_back(L"      - {accept: comma, send: Page_Up, when: paging}");
    bindings.push_back(
        L"      - {accept: period, send: Page_Down, when: has_menu}");
  }
  if (st.page_minus) {
    bindings.push_back(
        L"      - {accept: minus, send: Page_Up, when: has_menu}");
    bindings.push_back(
        L"      - {accept: equal, send: Page_Down, when: has_menu}");
  }
  if (st.page_bracket) {
    bindings.push_back(
        L"      - {accept: bracketleft, send: Page_Up, when: has_menu}");
    bindings.push_back(
        L"      - {accept: bracketright, send: Page_Down, when: has_menu}");
  }

  std::vector<std::wstring> patch_lines = {
      L"  \"switches/@0/reset\": " + std::wstring(st.init_cn ? L"0" : L"1"),
      L"  \"switches/@1/reset\": " + std::wstring(st.init_half ? L"0" : L"1"),
  };

  for (const auto& path : KeysFilePaths()) {
    std::vector<std::wstring> lines;
    {
      std::wifstream in(path.c_str());
      std::wstring line;
      while (std::getline(in, line))
        lines.push_back(line);
    }
    for (const auto& pl : patch_lines)
      UpsertPatchLine(lines, pl);
    ReplaceBlock(lines, L"ascii_composer:", switch_key_block);
    ReplaceBlock(lines, L"key_binder:", bindings);
    std::wofstream out(path.c_str());
    for (const auto& line : lines)
      out << line << L"\n";
  }
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

LRESULT GeneralPage::OnHelp(WORD, WORD wID, HWND, BOOL&) {
  const wchar_t* text = nullptr;
  switch (wID) {
    case IDC_HELP_PAGESIZE:
      text =
          L"每页显示的候选词数量（1-20）。\nAI 混合模式下，AI "
          L"候选数量不应超过此值，否则多余的 AI 候选不会显示。";
      break;
    case IDC_HELP_TRAY:
      text =
          L"在系统托盘显示小狼毫图标。\n关闭后，可改用任务栏语言栏切换输入法。";
      break;
    case IDC_HELP_FUZZY:
      text =
          L"开启后输入拼音时对指定声母/韵母不做区分。\n例如「n/l "
          L"不分」时，输入 nian 也能打出「连、联」。\n仅作用于 "
          L"luna_pinyin 与 luna_pinyin_simp 方案。";
      break;
    case IDC_HELP_SWITCH:
      text =
          L"切换中英文输入状态的快捷键。\n按 Shift "
          L"切换（多数输入法的习惯），或按 Ctrl 切换。";
      break;
    case IDC_HELP_PAGEKEYS:
      text =
          L"在候选词之间翻页的快捷键。\n数字键 1-9 直接选择候选，Tab "
          L"选择下一候选。";
      break;
  }
  if (text)
    ::MessageBox(m_hWnd, text, L"说明", MB_OK | MB_ICONINFORMATION);
  return 0;
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

LRESULT GeneralPage::OnChanged(WORD, WORD, HWND, BOOL&) {
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

  KeysState st = ReadKeysState();
  CheckRadioButton(IDC_KEY_INIT_CN, IDC_KEY_INIT_EN,
                   st.init_cn ? IDC_KEY_INIT_CN : IDC_KEY_INIT_EN);
  CheckRadioButton(IDC_KEY_INIT_HALF, IDC_KEY_INIT_FULL,
                   st.init_half ? IDC_KEY_INIT_HALF : IDC_KEY_INIT_FULL);
  int sw = st.switch_ctrl ? IDC_KEY_CTRL
                          : (st.switch_shift ? IDC_KEY_SHIFT : IDC_KEY_NONE);
  CheckRadioButton(IDC_KEY_SHIFT, IDC_KEY_NONE, sw);
  CheckDlgButton(IDC_PAGE_COMMA, st.page_comma ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(IDC_PAGE_MINUS, st.page_minus ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(IDC_PAGE_BRACKET,
                 st.page_bracket ? BST_CHECKED : BST_UNCHECKED);

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

  KeysState st;
  st.init_cn = IsDlgButtonChecked(IDC_KEY_INIT_CN) == BST_CHECKED;
  st.init_half = IsDlgButtonChecked(IDC_KEY_INIT_HALF) == BST_CHECKED;
  st.switch_shift = IsDlgButtonChecked(IDC_KEY_SHIFT) == BST_CHECKED;
  st.switch_ctrl = IsDlgButtonChecked(IDC_KEY_CTRL) == BST_CHECKED;
  st.page_comma = IsDlgButtonChecked(IDC_PAGE_COMMA) == BST_CHECKED;
  st.page_minus = IsDlgButtonChecked(IDC_PAGE_MINUS) == BST_CHECKED;
  st.page_bracket = IsDlgButtonChecked(IDC_PAGE_BRACKET) == BST_CHECKED;
  WriteKeysState(st);

  modified_ = false;
  return api_->save_settings(settings_);
}
