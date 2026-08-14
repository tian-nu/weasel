#pragma once

#include <windows.h>
#include <commctrl.h>

// attach a hover tooltip to a child control (e.g. a "?" help button).
// The tooltip window lives until the process exits; acceptable for the
// settings dialog which owns the process lifetime.
inline void AttachTooltip(HWND parent, UINT ctl_id, const wchar_t* text) {
  HWND tt = ::CreateWindowExW(0, TOOLTIPS_CLASSW, nullptr,
                              WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, 0, 0,
                              0, 0, parent, nullptr, ::GetModuleHandleW(nullptr),
                              nullptr);
  if (!tt)
    return;
  TOOLINFOW ti = {0};
  ti.cbSize = sizeof(ti);
  ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
  ti.hwnd = parent;
  ti.uId = (UINT_PTR)::GetDlgItem(parent, ctl_id);
  ti.lpszText = const_cast<wchar_t*>(text);
  ::SendMessageW(tt, TTM_ADDTOOLW, 0, (LPARAM)&ti);
  ::SendMessageW(tt, TTM_SETMAXTIPWIDTH, 0, 440);  // allow multi-line tips
}
