#include "App.h"

#include "DesktopWindow.h"
#include "QtSettingsWindow.h"
#include "WinUtil.h"

#include <QApplication>

#include <commctrl.h>

namespace musuka {

App* gApp = nullptr;

App::App() = default;

App::~App() = default;

bool App::Initialize(HINSTANCE instance) {
    instance_ = instance;
    gApp = this;

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&controls);

    std::wstring warning;
    store_.Load(config_, warning);
    if (!warning.empty()) {
        ShowInfo(nullptr, warning);
    }
    return true;
}

int App::Run() {
    return QApplication::exec();
}

void App::ShowSettings(int page) {
    if (desktop_) {
        desktop_->Hide();
    }
    if (!qtSettings_) {
        qtSettings_ = std::make_unique<QtSettingsWindow>(this);
    }
    qtSettings_->showPage(page);
}

void App::ShowDesktop() {
    if (qtSettings_) {
        qtSettings_->hide();
    }
    desktop_.reset();
    desktop_ = std::make_unique<DesktopWindow>(this);
    desktop_->Create();
}

void App::ReturnToSettings() {
    if (desktop_) {
        desktop_->Hide();
    }
    ShowSettings(1);
}

void App::Exit() {
    std::wstring error;
    store_.Save(config_, error);
    if (desktop_) {
        desktop_->Hide();
    }
    QApplication::quit();
}

} // namespace musuka
