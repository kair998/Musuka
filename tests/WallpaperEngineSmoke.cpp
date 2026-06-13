#include "App.h"
#include "DesktopWindow.h"
#include "QtSettingsWindow.h"
#include "Util.h"
#include "WinUtil.h"

#include <QApplication>
#include <QLabel>
#include <QRadioButton>

#include <commctrl.h>
#include <gdiplus.h>
#include <objbase.h>
#include <windows.h>

#include <cstdio>
#include <utility>

namespace {

constexpr wchar_t kHostClass[] = L"MusukaWallpaperSmokeHost";
constexpr wchar_t kShellViewClass[] = L"SHELLDLL_DefView";
constexpr COLORREF kTransparencyKey = RGB(1, 2, 3);

LRESULT CALLBACK TestWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool RegisterTestClass(HINSTANCE instance, const wchar_t* className) {
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = TestWindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    if (RegisterClassW(&windowClass)) {
        return true;
    }
    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

bool RenderedIconPixelExists(HWND desktopWindow) {
    HDC dc = GetDC(desktopWindow);
    if (!dc) {
        return false;
    }
    bool found = false;
    for (int y = 24; y < 120 && !found; ++y) {
        for (int x = 24; x < 120; ++x) {
            const COLORREF pixel = GetPixel(dc, x, y);
            if (pixel != CLR_INVALID && pixel != kTransparencyKey) {
                found = true;
                break;
            }
        }
    }
    ReleaseDC(desktopWindow, dc);
    return found;
}

int Fail(const char* message) {
    std::fprintf(stderr, "%s\n", message);
    return 1;
}

int FailWithLastError(const char* message) {
    std::fprintf(stderr, "%s (Win32 error %lu)\n", message, GetLastError());
    return 1;
}

int ValidateSettingsModePage(musuka::App& app) {
    app.Config().desktopMode = musuka::DesktopMode::Wallpaper;
    app.Config().backgroundSource = musuka::BackgroundSource::SolidColor;
    app.Config().solidColor = RGB(12, 34, 56);

    musuka::QtSettingsWindow settings(&app);
    settings.showPage(2);
    QApplication::processEvents();

    auto* engineMode = settings.findChild<QRadioButton*>(QStringLiteral("wallpaperEngineModeRadio"));
    auto* staticMode = settings.findChild<QRadioButton*>(QStringLiteral("staticWallpaperModeRadio"));
    auto* staticOptions = settings.findChild<QWidget*>(QStringLiteral("staticWallpaperOptions"));
    auto* preview = settings.findChild<QLabel*>(QStringLiteral("colorPreviewLabel"));
    if (!engineMode || !staticMode || !staticOptions || !preview) {
        return Fail("Qt static wallpaper mode controls were not created");
    }
    if (engineMode->text() != QStringLiteral("Wallpaper Engine 动态壁纸兼容模式") ||
        staticMode->text() != QStringLiteral("静态壁纸模式")) {
        return Fail("Qt settings mode names are incorrect");
    }

    if (preview->width() != preview->height()) {
        return Fail("Qt solid color preview is not square");
    }
    if (!preview->styleSheet().contains(QStringLiteral("rgb(12, 34, 56)"))) {
        return Fail("Qt solid color preview does not show the configured color");
    }

    engineMode->click();
    QApplication::processEvents();
    if (staticOptions->isVisible()) {
        return Fail("Qt static wallpaper options remain visible in dynamic wallpaper mode");
    }

    staticMode->click();
    QApplication::processEvents();
    if (!staticOptions->isVisible()) {
        return Fail("Qt static wallpaper options were not restored");
    }
    settings.hide();
    return 0;
}

} // namespace

int main() {
    int argc = 1;
    char appName[] = "wallpaper_engine_smoke";
    char* argv[] = {appName, nullptr};
    QApplication qtApplication(argc, argv);

    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
        return Fail("COM initialization failed");
    }

    Gdiplus::GdiplusStartupInput gdiplusInput;
    ULONG_PTR gdiplusToken = 0;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusInput, nullptr) != Gdiplus::Ok) {
        CoUninitialize();
        return Fail("GDI+ initialization failed");
    }

    musuka::App app;
    app.Initialize(instance);
    app.Config().desktopMode = musuka::DesktopMode::WallpaperEngine;
    app.Config().objects.clear();

    musuka::DesktopObject object;
    object.name = L"Wallpaper smoke icon";
    object.includeInDesktop = true;
    object.x = 24;
    object.y = 24;
    object.iconSize = musuka::kReplacementImageDefaultSize;
    musuka::ImageCandidate candidate;
    candidate.displayName = L"Musuka";
    candidate.internalPath = musuka::CombinePath(musuka::GetDefaultImageDirectory(), L"Musuka.png");
    object.candidates.push_back(candidate);
    musuka::SelectObjectCandidate(object, 0);
    app.Config().objects.push_back(std::move(object));

    if (!RegisterTestClass(instance, kHostClass) ||
        !RegisterTestClass(instance, kShellViewClass)) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
        CoUninitialize();
        return Fail("Test window class registration failed");
    }

    HWND host = CreateWindowExW(0,
                                kHostClass,
                                L"",
                                WS_POPUP | WS_VISIBLE,
                                0,
                                0,
                                640,
                                480,
                                nullptr,
                                nullptr,
                                instance,
                                nullptr);
    HWND shellView = CreateWindowExW(0,
                                     kShellViewClass,
                                     L"",
                                     WS_CHILD | WS_VISIBLE,
                                     0,
                                     0,
                                     640,
                                     480,
                                     host,
                                     nullptr,
                                     instance,
                                     nullptr);
    HWND iconList = CreateWindowExW(0,
                                    WC_LISTVIEWW,
                                    L"FolderView",
                                    WS_CHILD | WS_VISIBLE,
                                    0,
                                    0,
                                    640,
                                    480,
                                    shellView,
                                    nullptr,
                                    instance,
                                    nullptr);
    if (!host || !shellView || !iconList) {
        if (host) {
            DestroyWindow(host);
        }
        Gdiplus::GdiplusShutdown(gdiplusToken);
        CoUninitialize();
        return Fail("Fake Explorer desktop hierarchy creation failed");
    }

    int result = 0;
    {
        if (musuka::FindDesktopHostWindow() != host ||
            musuka::FindDesktopIconListView() != iconList) {
            result = Fail("Fake Explorer desktop hierarchy was not discovered");
        }
        musuka::DesktopWindow desktop(&app);
        if (result == 0) {
            if (!desktop.Create()) {
                const DWORD createError = GetLastError();
                WNDCLASSW desktopClass{};
                const BOOL classExists = GetClassInfoW(instance, L"MusukaDesktopWindow", &desktopClass);
                std::fprintf(stderr,
                             "Desktop class registered=%d classError=%lu\n",
                             classExists,
                             GetLastError());
                SetLastError(createError);
                result = FailWithLastError("Wallpaper Engine desktop window creation failed");
            } else {
                HWND musukaWindow = FindWindowExW(host, nullptr, L"MusukaDesktopWindow", nullptr);
                COLORREF colorKey = 0;
                BYTE alpha = 0;
                DWORD layeredFlags = 0;
                if (!musukaWindow || GetParent(musukaWindow) != host) {
                    result = Fail("Musuka desktop was not attached to the desktop icon host");
                } else if ((GetWindowLongPtrW(musukaWindow, GWL_EXSTYLE) & WS_EX_LAYERED) == 0) {
                    result = Fail("Wallpaper Engine desktop window is not layered");
                } else if (!GetLayeredWindowAttributes(musukaWindow, &colorKey, &alpha, &layeredFlags) ||
                           colorKey != kTransparencyKey ||
                           (layeredFlags & LWA_COLORKEY) == 0) {
                    result = Fail("Wallpaper Engine transparency key was not applied");
                } else if (IsWindowVisible(iconList)) {
                    result = Fail("Explorer desktop icons were not hidden");
                } else if (!RenderedIconPixelExists(musukaWindow)) {
                    result = Fail("Replacement icon was not rendered");
                }

                desktop.Hide();
                if (result == 0 && !IsWindowVisible(iconList)) {
                    result = Fail("Explorer desktop icons were not restored");
                }
            }
        }
    }

    if (result == 0) {
        result = ValidateSettingsModePage(app);
    }

    DestroyWindow(host);
    Gdiplus::GdiplusShutdown(gdiplusToken);
    CoUninitialize();
    return result;
}
