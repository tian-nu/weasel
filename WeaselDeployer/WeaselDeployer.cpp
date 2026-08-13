// WeaselDeployer.cpp : Defines the entry point for the application.
//
#include "stdafx.h"
#include <WeaselUtility.h>
#include <fstream>
#include "WeaselDeployer.h"
#include "Configurator.h"
#include "SettingsDialog.h"

CAppModule _Module;

static int Run(LPTSTR lpCmdLine);

namespace {

// commands that open the settings window (as opposed to one-shot jobs like
// /deploy or /sync that must run even while the settings window is open)
bool OpensSettingsWindow(LPTSTR cmd) {
  return cmd[0] == 0 || !wcscmp(L"/dict", cmd) || !wcscmp(L"/install", cmd);
}

// a second instance was launched while the settings window is already open:
// instead of silently exiting, activate the running window and hand off the
// requested page (tray menu: 输入法设定 -> general, 用户词典管理 -> dict)
int ActivateExistingWindow(LPTSTR lpCmdLine) {
  HWND hwnd = ::FindWindowW(NULL, L"【小狼毫】设置");
  if (!hwnd)
    return 0;  // no visible settings window to hand off to
  if (::IsIconic(hwnd))
    ::ShowWindow(hwnd, SW_RESTORE);
  ::SetForegroundWindow(hwnd);
  int page = 0;
  if (!wcscmp(L"/dict", lpCmdLine))
    page = 3;
  ::SendMessage(hwnd, kWM_ShowPage, page, 0);
  return 0;
}

}  // namespace

int APIENTRY _tWinMain(HINSTANCE hInstance,
                       HINSTANCE hPrevInstance,
                       LPTSTR lpCmdLine,
                       int nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);

  LANGID langId = get_language_id();
  SetThreadUILanguage(langId);
  SetThreadLocale(langId);

  HRESULT hRes = ::CoInitialize(NULL);
  // If you are running on NT 4.0 or higher you can use the following call
  // instead to make the EXE free threaded. This means that calls come in on a
  // random RPC thread.
  // HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
  ATLASSERT(SUCCEEDED(hRes));

  // this resolves ATL window thunking problem when Microsoft Layer for Unicode
  // (MSLU) is used
  ::DefWindowProc(NULL, 0, 0, 0L);

  AtlInitCommonControls(
      ICC_BAR_CLASSES);  // add flags to support other controls

  hRes = _Module.Init(NULL, hInstance);
  ATLASSERT(SUCCEEDED(hRes));

  CreateDirectory(WeaselUserDataPath().c_str(), NULL);

  int ret = 0;
  if (!OpensSettingsWindow(lpCmdLine)) {
    // one-shot jobs (/deploy /sync /?): run regardless of whether the
    // settings window is already open
    ret = Run(lpCmdLine);
  } else {
    HANDLE hMutex = CreateMutex(NULL, TRUE, L"WeaselDeployerExclusiveMutex");
    if (!hMutex) {
      ret = 1;
    } else if (GetLastError() == ERROR_ALREADY_EXISTS) {
      ret = ActivateExistingWindow(lpCmdLine);
    } else {
      ret = Run(lpCmdLine);
    }
    if (hMutex) {
      CloseHandle(hMutex);
    }
  }
  _Module.Term();
  ::CoUninitialize();

  return ret;
}

static int Run(LPTSTR lpCmdLine) {
  Configurator configurator;
  configurator.Initialize();

  if (!wcscmp(L"/?", lpCmdLine) || !wcscmp(L"/help", lpCmdLine)) {
    WCHAR msg[1024] = {0};
    if (LoadString(GetModuleHandle(NULL), IDS_STR_HELP, msg,
                   sizeof(msg) / sizeof(TCHAR))) {
      MessageBox(NULL, msg, L"Weasel Deployer", MB_ICONINFORMATION | MB_OK);
    } else {
      MessageBox(NULL,
                 L"Usage: WeaselDeployer.exe [options]\n"
                 L"/? or /help		- Show this help message\n"
                 L"/deploy		- Update Workspace\n"
                 L"/dict		- Manage dictionary\n"
                 L"/sync		- Sync user data\n"
                 L"/install		- Install Weasel (Initial deployment)",
                 L"Weasel Deployer", MB_ICONINFORMATION | MB_OK);
    }
    return 0;
  }

  bool deployment_scheduled = !wcscmp(L"/deploy", lpCmdLine);
  if (deployment_scheduled) {
    return configurator.UpdateWorkspace();
  }

  bool dict_management = !wcscmp(L"/dict", lpCmdLine);
  if (dict_management) {
    // open the unified settings window on the dictionary page
    return configurator.Run(false, 3);
  }

  bool sync_user_dict = !wcscmp(L"/sync", lpCmdLine);
  if (sync_user_dict) {
    return configurator.SyncUserData();
  }

  bool installing = !wcscmp(L"/install", lpCmdLine);
  return configurator.Run(installing);
}
