#pragma once

#include "ConfigStore.h"
#include "Models.h"

#include <memory>

namespace musuka {

class DesktopWindow;
class QtSettingsWindow;

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
    std::unique_ptr<DesktopWindow> desktop_;
    std::unique_ptr<QtSettingsWindow> qtSettings_;
};

extern App* gApp;

} // namespace musuka
