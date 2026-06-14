#include "App.h"
#include "DesktopWindow.h"
#include "ImageUtil.h"
#include "QtSettingsWindow.h"
#include "SettingsLocalization.h"
#include "Util.h"
#include "WinUtil.h"

#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QRadioButton>

#include <commctrl.h>
#include <gdiplus.h>
#include <objbase.h>
#include <windows.h>

#include <cstdio>
#include <cwchar>
#include <memory>
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
    app.Config().settingsLanguage = musuka::SettingsLanguage::ChineseSimplified;
    app.Config().desktopMode = musuka::DesktopMode::StaticWallpaperVirtualDesktop;
    app.Config().backgroundSource = musuka::BackgroundSource::SolidColor;
    app.Config().solidColor = RGB(12, 34, 56);

    musuka::QtSettingsWindow settings(&app);
    app.AttachSettings(&settings);
    app.ShowSettings(2);
    QApplication::processEvents();

    auto* engineMode = settings.findChild<QRadioButton*>(QStringLiteral("wallpaperEngineModeRadio"));
    auto* desktopStaticCompatibilityMode = settings.findChild<QRadioButton*>(
        QStringLiteral("desktopStaticWallpaperCompatibilityModeRadio"));
    auto* staticVirtualDesktopMode = settings.findChild<QRadioButton*>(
        QStringLiteral("staticWallpaperVirtualDesktopModeRadio"));
    auto* compatibilityOptions = settings.findChild<QWidget*>(
        QStringLiteral("compatibilityModeOptions"));
    auto* staticOptions = settings.findChild<QWidget*>(QStringLiteral("staticWallpaperOptions"));
    auto* preview = settings.findChild<QLabel*>(QStringLiteral("colorPreviewLabel"));
    auto* languageCombo = settings.findChild<QComboBox*>(QStringLiteral("settingsLanguageCombo"));
    if (!engineMode || !desktopStaticCompatibilityMode || !staticVirtualDesktopMode ||
        !compatibilityOptions || !staticOptions || !preview || !languageCombo) {
        return Fail("Qt desktop mode controls were not created");
    }
    if (engineMode->text() != QStringLiteral("Wallpaper Engine 动态壁纸兼容模式") ||
        desktopStaticCompatibilityMode->text() != QStringLiteral("桌面静态壁纸兼容模式") ||
        staticVirtualDesktopMode->text() != QStringLiteral("静态壁纸拟桌面模式")) {
        return Fail("Qt settings mode names are incorrect");
    }
    if (!compatibilityOptions->isAncestorOf(engineMode) ||
        !compatibilityOptions->isAncestorOf(desktopStaticCompatibilityMode) ||
        compatibilityOptions->isAncestorOf(staticVirtualDesktopMode)) {
        return Fail("Qt compatibility modes are not grouped correctly");
    }

    if (preview->width() != preview->height()) {
        return Fail("Qt solid color preview is not square");
    }
    if (!preview->styleSheet().contains(QStringLiteral("rgb(12, 34, 56)"))) {
        return Fail("Qt solid color preview does not show the configured color");
    }

    desktopStaticCompatibilityMode->click();
    QApplication::processEvents();
    if (staticOptions->isVisible() ||
        app.Config().desktopMode != musuka::DesktopMode::DesktopStaticWallpaperCompatibility) {
        return Fail("Qt desktop static wallpaper compatibility mode was not applied");
    }

    engineMode->click();
    QApplication::processEvents();
    if (staticOptions->isVisible() ||
        app.Config().desktopMode != musuka::DesktopMode::WallpaperEngineCompatibility) {
        return Fail("Qt Wallpaper Engine compatibility mode was not applied");
    }

    staticVirtualDesktopMode->click();
    QApplication::processEvents();
    if (!staticOptions->isVisible() ||
        app.Config().desktopMode != musuka::DesktopMode::StaticWallpaperVirtualDesktop) {
        return Fail("Qt static wallpaper virtual desktop mode was not applied");
    }

    languageCombo->setCurrentIndex(languageCombo->findData(
        static_cast<int>(musuka::SettingsLanguage::English)));
    QApplication::processEvents();
    languageCombo = settings.findChild<QComboBox*>(QStringLiteral("settingsLanguageCombo"));
    engineMode = settings.findChild<QRadioButton*>(QStringLiteral("wallpaperEngineModeRadio"));
    auto* page3Title = settings.findChild<QLabel*>(QStringLiteral("page3TitleLabel"));
    if (!languageCombo || !engineMode || !page3Title ||
        app.Config().settingsLanguage != musuka::SettingsLanguage::English ||
        engineMode->text() != QStringLiteral("Wallpaper Engine dynamic wallpaper compatibility mode") ||
        page3Title->text() != QStringLiteral("Step 3: Select desktop mode")) {
        return Fail("Qt Settings did not switch to English");
    }

    languageCombo->setCurrentIndex(languageCombo->findData(
        static_cast<int>(musuka::SettingsLanguage::Japanese)));
    QApplication::processEvents();
    languageCombo = settings.findChild<QComboBox*>(QStringLiteral("settingsLanguageCombo"));
    engineMode = settings.findChild<QRadioButton*>(QStringLiteral("wallpaperEngineModeRadio"));
    page3Title = settings.findChild<QLabel*>(QStringLiteral("page3TitleLabel"));
    if (!languageCombo || !engineMode || !page3Title ||
        app.Config().settingsLanguage != musuka::SettingsLanguage::Japanese ||
        engineMode->text() != QString::fromUtf8("Wallpaper Engine 動く壁紙互換モード") ||
        page3Title->text() != QString::fromUtf8("ステップ3：デスクトップモードを選択")) {
        return Fail("Qt Settings did not switch to Japanese");
    }

    languageCombo->setCurrentIndex(languageCombo->findData(
        static_cast<int>(musuka::SettingsLanguage::ChineseSimplified)));
    QApplication::processEvents();
    if (app.Config().settingsLanguage != musuka::SettingsLanguage::ChineseSimplified) {
        return Fail("Qt Settings did not switch back to Chinese");
    }
    musuka::AppConfig persistedConfig;
    std::wstring warning;
    if (!app.Store().Load(persistedConfig, warning) ||
        persistedConfig.settingsLanguage != musuka::SettingsLanguage::ChineseSimplified) {
        return Fail("Qt Settings language selection was not persisted");
    }
    settings.hide();
    app.AttachSettings(nullptr);
    return 0;
}

int ValidateDesktopModeSerialization() {
    using musuka::DesktopMode;

    if (std::wcscmp(musuka::ToString(DesktopMode::StaticWallpaperVirtualDesktop),
                    L"static_wallpaper_virtual_desktop") != 0 ||
        std::wcscmp(musuka::ToString(DesktopMode::DesktopStaticWallpaperCompatibility),
                    L"desktop_static_wallpaper_compatibility") != 0 ||
        std::wcscmp(musuka::ToString(DesktopMode::WallpaperEngineCompatibility),
                    L"wallpaper_engine_compatibility") != 0) {
        return Fail("Desktop modes do not serialize to the expected values");
    }
    if (musuka::DesktopModeFromString(L"static_wallpaper_virtual_desktop") !=
            DesktopMode::StaticWallpaperVirtualDesktop ||
        musuka::DesktopModeFromString(L"desktop_static_wallpaper_compatibility") !=
            DesktopMode::DesktopStaticWallpaperCompatibility ||
        musuka::DesktopModeFromString(L"wallpaper_engine_compatibility") !=
            DesktopMode::WallpaperEngineCompatibility ||
        musuka::DesktopModeFromString(L"wallpaper") !=
            DesktopMode::StaticWallpaperVirtualDesktop ||
        musuka::DesktopModeFromString(L"wallpaper_engine") !=
            DesktopMode::WallpaperEngineCompatibility) {
        return Fail("Desktop modes do not deserialize or migrate correctly");
    }
    return 0;
}

int ValidateSettingsLanguageSerialization() {
    using musuka::SettingsLanguage;

    if (std::wcscmp(musuka::ToString(SettingsLanguage::ChineseSimplified), L"zh_CN") != 0 ||
        std::wcscmp(musuka::ToString(SettingsLanguage::English), L"en") != 0 ||
        std::wcscmp(musuka::ToString(SettingsLanguage::Japanese), L"ja") != 0 ||
        musuka::SettingsLanguageFromString(L"zh_CN") != SettingsLanguage::ChineseSimplified ||
        musuka::SettingsLanguageFromString(L"en") != SettingsLanguage::English ||
        musuka::SettingsLanguageFromString(L"ja") != SettingsLanguage::Japanese ||
        musuka::SettingsLanguageFromString(L"unknown") != SettingsLanguage::ChineseSimplified) {
        return Fail("Settings languages do not serialize or deserialize correctly");
    }
    if (musuka::SettingsString(SettingsLanguage::English, musuka::SettingsStringId::Next) !=
            QStringLiteral("Next") ||
        musuka::SettingsString(SettingsLanguage::Japanese, musuka::SettingsStringId::Next) !=
            QString::fromUtf8("次へ") ||
        musuka::LocalizeSettingsMessage(SettingsLanguage::English, L"图片复制失败。") !=
            QStringLiteral("Could not copy the image.") ||
        musuka::LocalizeSettingsMessage(SettingsLanguage::Japanese, L"图片复制失败。") !=
            QString::fromUtf8("画像をコピーできませんでした。")) {
        return Fail("Settings language strings or localized messages are incorrect");
    }
    return 0;
}

int ValidateCompatibilityDesktopMode(musuka::App& app,
                                     HWND host,
                                     HWND iconList,
                                     musuka::DesktopMode mode) {
    app.Config().desktopMode = mode;
    ShowWindow(iconList, SW_SHOW);

    int result = 0;
    musuka::DesktopWindow desktop(&app);
    if (!desktop.Create()) {
        return FailWithLastError("Compatibility desktop window creation failed");
    }

    HWND musukaWindow = FindWindowExW(host, nullptr, L"MusukaDesktopWindow", nullptr);
    COLORREF colorKey = 0;
    BYTE alpha = 0;
    DWORD layeredFlags = 0;
    if (!musukaWindow || GetParent(musukaWindow) != host) {
        result = Fail("Musuka desktop was not attached to the desktop icon host");
    } else if ((GetWindowLongPtrW(musukaWindow, GWL_EXSTYLE) & WS_EX_LAYERED) == 0) {
        result = Fail("Compatibility desktop window is not layered");
    } else if (!GetLayeredWindowAttributes(musukaWindow, &colorKey, &alpha, &layeredFlags) ||
               colorKey != kTransparencyKey ||
               (layeredFlags & LWA_COLORKEY) == 0) {
        result = Fail("Compatibility desktop transparency key was not applied");
    } else if (IsWindowVisible(iconList)) {
        result = Fail("Explorer desktop icons were not hidden");
    } else if (!RenderedIconPixelExists(musukaWindow)) {
        result = Fail("Replacement icon was not rendered");
    } else if (desktop.RenderItemCountForTesting() != app.Config().objects.size()) {
        result = Fail("Desktop did not create one render item per configured object");
    } else if (desktop.UniqueBitmapCountForTesting() != 1) {
        result = Fail("Desktop did not reuse the shared replacement bitmap");
    }

    if (result == 0) {
        for (const auto& configuredObject : app.Config().objects) {
            if (configuredObject.iconSize != musuka::kDesktopIconMaxSize) {
                result = Fail("Auto-arrange changed a configured icon size");
                break;
            }
        }
    }

    desktop.Hide();
    if (result == 0 && !IsWindowVisible(iconList)) {
        result = Fail("Explorer desktop icons were not restored");
    }
    return result;
}

int ValidateTinyTransparentCanvasTrimming() {
    auto bitmap = std::make_unique<Gdiplus::Bitmap>(256, 256, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics graphics(bitmap.get());
        graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
        graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
        Gdiplus::SolidBrush visible(Gdiplus::Color(255, 40, 120, 220));
        graphics.FillRectangle(&visible, 112, 112, 32, 32);
        graphics.Flush();
    }

    bitmap = musuka::TrimTinyTransparentCanvas(std::move(bitmap));
    if (!bitmap || bitmap->GetWidth() >= 96 || bitmap->GetHeight() >= 96) {
        return Fail("Tiny native icon content was not trimmed from its transparent canvas");
    }
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
    app.Config().desktopMode = musuka::DesktopMode::WallpaperEngineCompatibility;
    app.Config().objects.clear();

    for (int i = 0; i < 8; ++i) {
        musuka::DesktopObject object;
        object.name = L"Wallpaper smoke icon";
        object.includeInDesktop = true;
        object.x = -1;
        object.y = -1;
        object.iconSize = musuka::kDesktopIconMaxSize;
        musuka::ImageCandidate candidate;
        candidate.displayName = L"Musuka";
        candidate.internalPath = musuka::CombinePath(musuka::GetDefaultImageDirectory(), L"Musuka.png");
        object.candidates.push_back(candidate);
        musuka::SelectObjectCandidate(object, 0);
        app.Config().objects.push_back(std::move(object));
    }

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

    int result = ValidateTinyTransparentCanvasTrimming();
    if (result == 0) {
        result = ValidateDesktopModeSerialization();
    }
    if (result == 0) {
        result = ValidateSettingsLanguageSerialization();
    }
    if (result == 0 &&
        (musuka::FindDesktopHostWindow() != host ||
         musuka::FindDesktopIconListView() != iconList)) {
        result = Fail("Fake Explorer desktop hierarchy was not discovered");
    }
    if (result == 0) {
        result = ValidateCompatibilityDesktopMode(
            app,
            host,
            iconList,
            musuka::DesktopMode::DesktopStaticWallpaperCompatibility);
    }
    if (result == 0) {
        result = ValidateCompatibilityDesktopMode(
            app,
            host,
            iconList,
            musuka::DesktopMode::WallpaperEngineCompatibility);
    }

    if (result == 0) {
        result = ValidateSettingsModePage(app);
    }

    DestroyWindow(host);
    Gdiplus::GdiplusShutdown(gdiplusToken);
    CoUninitialize();
    return result;
}
