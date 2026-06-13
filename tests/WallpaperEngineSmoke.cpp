#include "App.h"
#include "DesktopWindow.h"
#include "SettingsWindow.h"
#include "Util.h"
#include "WinUtil.h"

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
constexpr int kModeEngineId = 1201;
constexpr int kModeStaticWallpaperId = 1202;
constexpr int kBackgroundSystemId = 1203;
constexpr int kBackgroundSolidId = 1204;
constexpr int kChooseColorId = 1205;
constexpr int kColorPreviewId = 1206;

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

std::wstring WindowText(HWND hwnd) {
    const int length = GetWindowTextLengthW(hwnd);
    std::wstring text(static_cast<size_t>(length + 1), L'\0');
    GetWindowTextW(hwnd, text.data(), length + 1);
    text.resize(static_cast<size_t>(length));
    return text;
}

int ValidateSettingsModePage(musuka::App& app) {
    app.Config().desktopMode = musuka::DesktopMode::Wallpaper;
    app.Config().backgroundSource = musuka::BackgroundSource::SolidColor;
    app.Config().solidColor = RGB(12, 34, 56);

    musuka::SettingsWindow settings(&app);
    if (!settings.Create(2)) {
        return Fail("Settings mode page creation failed");
    }

    HWND settingsWindow = FindWindowW(L"MusukaSettingsWindow", L"musuka settings");
    HWND engineMode = GetDlgItem(settingsWindow, kModeEngineId);
    HWND staticMode = GetDlgItem(settingsWindow, kModeStaticWallpaperId);
    HWND preview = GetDlgItem(settingsWindow, kColorPreviewId);
    if (!settingsWindow || !engineMode || !staticMode || !preview) {
        return Fail("Static wallpaper mode controls were not created");
    }
    if (WindowText(engineMode) != L"Wallpaper Engine 动态壁纸兼容模式" ||
        WindowText(staticMode) != L"静态壁纸模式") {
        return Fail("Settings mode names are incorrect");
    }

    RECT previewRect{};
    GetWindowRect(preview, &previewRect);
    if (previewRect.right - previewRect.left != previewRect.bottom - previewRect.top) {
        return Fail("Solid color preview is not square");
    }
    HDC screenDc = GetDC(nullptr);
    HDC previewDc = CreateCompatibleDC(screenDc);
    HBITMAP previewBitmap = CreateCompatibleBitmap(screenDc, 30, 30);
    HGDIOBJ oldBitmap = SelectObject(previewDc, previewBitmap);
    SendMessageW(preview, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(previewDc), PRF_CLIENT);
    const COLORREF previewPixel = GetPixel(previewDc, 15, 15);
    SelectObject(previewDc, oldBitmap);
    DeleteObject(previewBitmap);
    DeleteDC(previewDc);
    ReleaseDC(nullptr, screenDc);
    if (previewPixel != app.Config().solidColor) {
        return Fail("Solid color preview does not show the configured color");
    }

    SendMessageW(settingsWindow,
                 WM_COMMAND,
                 MAKEWPARAM(kModeEngineId, BN_CLICKED),
                 reinterpret_cast<LPARAM>(engineMode));
    if (GetDlgItem(settingsWindow, kBackgroundSystemId) ||
        GetDlgItem(settingsWindow, kBackgroundSolidId) ||
        GetDlgItem(settingsWindow, kChooseColorId) ||
        GetDlgItem(settingsWindow, kColorPreviewId)) {
        return Fail("Static wallpaper options remain visible in dynamic wallpaper mode");
    }

    staticMode = GetDlgItem(settingsWindow, kModeStaticWallpaperId);
    SendMessageW(settingsWindow,
                 WM_COMMAND,
                 MAKEWPARAM(kModeStaticWallpaperId, BN_CLICKED),
                 reinterpret_cast<LPARAM>(staticMode));
    if (!GetDlgItem(settingsWindow, kBackgroundSystemId) ||
        !GetDlgItem(settingsWindow, kBackgroundSolidId) ||
        !GetDlgItem(settingsWindow, kChooseColorId) ||
        !GetDlgItem(settingsWindow, kColorPreviewId)) {
        return Fail("Static wallpaper options were not restored");
    }
    return 0;
}

} // namespace

int main() {
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
