#include "App.h"

#include <QApplication>

#include <objidl.h>
#include <propidl.h>
#include <gdiplus.h>
#include <objbase.h>

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    SetProcessDPIAware();

    int argc = 1;
    char appName[] = "musuka";
    char* argv[] = {appName, nullptr};
    QApplication qtApplication(argc, argv);

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

    musuka::App app;
    app.Initialize(hInstance);
    app.ShowSettings(0);
    const int result = app.Run();

    Gdiplus::GdiplusShutdown(gdiplusToken);
    CoUninitialize();
    return result;
}
