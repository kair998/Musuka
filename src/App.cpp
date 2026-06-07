#include "App.h"

#include "DesktopWindow.h"
#include "SettingsWindow.h"
#include "WinUtil.h"

#ifdef MUSUKA_USE_QT
#include <QApplication>
#include "QtSettingsWindow.h"
#endif

#include <commctrl.h>
#include <objbase.h>

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
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
#ifdef MUSUKA_USE_QT
        if (qtApp_) {
            qtApp_->processEvents();
        }
#endif
    }
    return static_cast<int>(message.wParam);
}

void App::ShowSettings(int page) {
    if (desktop_) {
        desktop_->Hide();
    }
#ifdef MUSUKA_USE_QT
    if (!qtApp_) {
        static int argc = 0;
        qtApp_ = std::make_unique<QApplication>(argc, nullptr);
    }
    if (!qtSettings_) {
        qtSettings_ = std::make_unique<QtSettingsWindow>(this);
    }
    qtSettings_->showPage(page);
#else
    if (!settings_) {
        settings_ = std::make_unique<SettingsWindow>(this);
        settings_->Create(page);
    } else {
        settings_->ShowPage(page);
    }
#endif
}

void App::ShowDesktop() {
    if (settings_) {
        settings_->Hide();
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
#ifdef MUSUKA_USE_QT
    qtSettings_.reset();
    qtApp_.reset();
#endif
    if (settings_) {
        settings_->Hide();
    }
    if (desktop_) {
        desktop_->Hide();
    }
    PostQuitMessage(0);
}

} // namespace musuka
