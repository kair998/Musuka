#pragma once

#include "ConfigStore.h"
#include "Models.h"

#include <memory>

#ifdef MUSUKA_USE_QT
class QApplication;
#endif

namespace musuka {

class SettingsWindow;
class DesktopWindow;

#ifdef MUSUKA_USE_QT
class QtSettingsWindow;
#endif

class App {
public:
    App();
    ~App();

    bool Initialize(HINSTANCE instance);
    int Run();

    HINSTANCE Instance() const { return instance_; }
    AppConfig& Config() { return config_; }
    ConfigStore& Store() { return store_; }

    void ShowSettings(int page = 0);
    void ShowDesktop();
    void ReturnToSettings();
    void Exit();

private:
    HINSTANCE instance_ = nullptr;
    AppConfig config_;
    ConfigStore store_;
    std::unique_ptr<SettingsWindow> settings_;
    std::unique_ptr<DesktopWindow> desktop_;

#ifdef MUSUKA_USE_QT
    std::unique_ptr<QApplication> qtApp_;
    std::unique_ptr<QtSettingsWindow> qtSettings_;
#endif
};

extern App* gApp;

} // namespace musuka
