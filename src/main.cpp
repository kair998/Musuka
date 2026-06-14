#include "App.h"
#include "QtSettingsWindow.h"

#include <QApplication>

#include <objidl.h>
#include <propidl.h>
#include <gdiplus.h>
#include <objbase.h>

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    QApplication qtApplication(__argc, __argv);

    HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(comResult)) {
        return 1;
    }

    Gdiplus::GdiplusStartupInput gdiplusInput;
    ULONG_PTR gdiplusToken = 0;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusInput, nullptr) != Gdiplus::Ok) {
        CoUninitialize();
        return 1;
    }

    int result = 0;
    {
        musuka::App app;
        app.Initialize(hInstance);
        musuka::QtSettingsWindow settings(&app);
        app.AttachSettings(&settings);
        app.ShowSettings(0);
        result = app.Run();
        app.AttachSettings(nullptr);
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    CoUninitialize();
    return result;
}
