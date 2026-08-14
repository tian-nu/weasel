#include "stdafx.h"
#include "KeysPage.h"
#include <WeaselUtility.h>
#include <fstream>
#include <vector>
#include "TooltipHelper.h"
#pragma warning(disable : 4005)
#include "WeaselDeployer.h"

namespace {

// custom yamls that receive the keys patch (simplified default + traditional)
std::vector<std::wstring> KeysFilePaths() {
  std::wstring base = WeaselUserDataPath().wstring();
  return {base + L"\\luna_pinyin.custom.yaml",
          base + L"\\luna_pinyin_simp.custom.yaml"};
}

// replace the block that starts at the line containing `key` and ends at the
// next line with equal-or-less indentation that is not blank, or EOF.
// `block` is the replacement text (without trailing newline handling).
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

}  // namespace

KeysPage::KeysPage() : modified_(false) {}

KeysPage::~KeysPage() {}

LRESULT KeysPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
  Load();
  AttachTooltip(m_hWnd, IDC_HELP_SWITCH,
                L"切换中英文输入状态的快捷键。\n按 Shift "
                L"切换（多数输入法的习惯），或按 Ctrl 切换。");
  AttachTooltip(m_hWnd, IDC_HELP_PAGEKEYS,
                L"在候选词之间翻页的快捷键。\n数字键 1-9 直接选择候选，Tab "
                L"选择下一候选。");
  AttachTooltip(m_hWnd, IDC_HELP_DEL,
                L"删除当前候选。\n被删除的词会从用户词库移除（词典词条不受影响"
                L"），之后不再优先出现。");
  return TRUE;
}

LRESULT KeysPage::OnClose(UINT, WPARAM, LPARAM, BOOL&) {
  DestroyWindow();
  return 0;
}

LRESULT KeysPage::OnChanged(WORD, WORD, HWND, BOOL&) {
  modified_ = true;
  return 0;
}

void KeysPage::Load() {
  bool init_cn = true;       // ascii_mode reset 0
  bool init_half = true;     // full_shape reset 0
  bool switch_shift = true;  // Shift toggles ascii
  bool switch_ctrl = false;
  bool page_comma = true;
  bool page_minus = true;
  bool page_bracket = false;
  bool del_ctrl = true;

  for (const auto& path : KeysFilePaths()) {
    std::wifstream in(path.c_str());
    if (!in)
      continue;
    std::wstring line;
    while (std::getline(in, line)) {
      if (line.find(L"switches/@0/reset") != std::wstring::npos) {
        if (line.find(L": 1") != std::wstring::npos ||
            line.find(L":1") != std::wstring::npos)
          init_cn = false;
      } else if (line.find(L"switches/@1/reset") != std::wstring::npos) {
        if (line.find(L": 1") != std::wstring::npos ||
            line.find(L":1") != std::wstring::npos)
          init_half = false;
      } else if (line.find(L"Shift_L:") != std::wstring::npos) {
        switch_shift = line.find(L"inline_ascii") != std::wstring::npos;
      } else if (line.find(L"Control_L:") != std::wstring::npos) {
        switch_ctrl = line.find(L"inline_ascii") != std::wstring::npos;
      } else if (line.find(L"accept: comma") != std::wstring::npos) {
        page_comma = true;
      } else if (line.find(L"accept: minus") != std::wstring::npos) {
        page_minus = true;
      } else if (line.find(L"accept: bracketleft") != std::wstring::npos) {
        page_bracket = true;
      } else if (line.find(L"Control+Delete") != std::wstring::npos) {
        del_ctrl = true;
      }
    }
  }

  CheckRadioButton(IDC_KEY_INIT_CN, IDC_KEY_INIT_EN,
                   init_cn ? IDC_KEY_INIT_CN : IDC_KEY_INIT_EN);
  CheckRadioButton(IDC_KEY_INIT_HALF, IDC_KEY_INIT_FULL,
                   init_half ? IDC_KEY_INIT_HALF : IDC_KEY_INIT_FULL);
  int sw = switch_ctrl ? IDC_KEY_CTRL
                       : (switch_shift ? IDC_KEY_SHIFT : IDC_KEY_NONE);
  CheckRadioButton(IDC_KEY_SHIFT, IDC_KEY_NONE, sw);
  CheckDlgButton(IDC_PAGE_COMMA, page_comma ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(IDC_PAGE_MINUS, page_minus ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(IDC_PAGE_BRACKET, page_bracket ? BST_CHECKED : BST_UNCHECKED);
  CheckRadioButton(IDC_DEL_CTRL, IDC_DEL_NONE,
                   del_ctrl ? IDC_DEL_CTRL : IDC_DEL_NONE);
  modified_ = false;
}

bool KeysPage::Apply() {
  if (!modified_)
    return false;

  bool init_cn = IsDlgButtonChecked(IDC_KEY_INIT_CN) == BST_CHECKED;
  bool init_half = IsDlgButtonChecked(IDC_KEY_INIT_HALF) == BST_CHECKED;
  bool switch_shift = IsDlgButtonChecked(IDC_KEY_SHIFT) == BST_CHECKED;
  bool switch_ctrl = IsDlgButtonChecked(IDC_KEY_CTRL) == BST_CHECKED;
  bool page_comma = IsDlgButtonChecked(IDC_PAGE_COMMA) == BST_CHECKED;
  bool page_minus = IsDlgButtonChecked(IDC_PAGE_MINUS) == BST_CHECKED;
  bool page_bracket = IsDlgButtonChecked(IDC_PAGE_BRACKET) == BST_CHECKED;
  bool del_ctrl = IsDlgButtonChecked(IDC_DEL_CTRL) == BST_CHECKED;

  // switch_key: Shift_L/Shift_R toggle ascii, or Control_L/Control_R, or none
  std::wstring shift_action = switch_shift ? L"inline_ascii" : L"noop";
  std::wstring ctrl_action = switch_ctrl ? L"inline_ascii" : L"noop";
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
  if (page_comma) {
    bindings.push_back(L"      - {accept: comma, send: Page_Up, when: paging}");
    bindings.push_back(
        L"      - {accept: period, send: Page_Down, when: has_menu}");
  }
  if (page_minus) {
    bindings.push_back(
        L"      - {accept: minus, send: Page_Up, when: has_menu}");
    bindings.push_back(
        L"      - {accept: equal, send: Page_Down, when: has_menu}");
  }
  if (page_bracket) {
    bindings.push_back(
        L"      - {accept: bracketleft, send: Page_Up, when: has_menu}");
    bindings.push_back(
        L"      - {accept: bracketright, send: Page_Down, when: has_menu}");
  }

  // editor: full binding map; Control+Delete removes the current candidate
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

    std::vector<std::wstring> patch_lines = {
        L"  \"switches/@0/reset\": " + std::wstring(init_cn ? L"0" : L"1"),
        L"  \"switches/@1/reset\": " + std::wstring(init_half ? L"0" : L"1"),
    };
    for (const auto& pl : patch_lines)
      UpsertPatchLine(lines, pl);

    ReplaceBlock(lines, L"ascii_composer:", switch_key_block);
    ReplaceBlock(lines, L"key_binder:", bindings);
    ReplaceBlock(lines, L"editor:", editor_block);

    std::wofstream out(path.c_str());
    for (const auto& line : lines)
      out << line << L"\n";
  }

  modified_ = false;
  return true;
}
