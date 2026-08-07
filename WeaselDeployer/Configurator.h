#pragma once

class UIStyleSettings;

class Configurator {
 public:
  explicit Configurator();

  void Initialize();
  // initial_page selects the settings-window page to show on open
  int Run(bool installing, int initial_page = 0);
  int UpdateWorkspace(bool report_errors = false);
  int SyncUserData();
};
