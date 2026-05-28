#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace musuka {

enum class DesktopObjectType {
    Shortcut,
    File,
    Folder,
    ThisPC,
    RecycleBin
};

enum class DesktopMode {
    WallpaperEngine,
    Wallpaper
};

enum class BackgroundSource {
    SystemWallpaper,
    SolidColor
};

constexpr int kDesktopIconMinSize = 48;
constexpr int kDesktopIconDefaultSize = 128;
constexpr int kDesktopIconMaxSize = 512;
constexpr int kDefaultImageLayerPriority = 0;
constexpr int kImportedImageLayerPriority = 10;

struct ImageCandidate {
    std::wstring displayName;
    std::wstring originalPath;
    std::wstring internalPath;
    bool originalIcon = false;
    int layerPriority = kDefaultImageLayerPriority;
};

struct DesktopObject {
    std::wstring id;
    std::wstring name;
    std::wstring path;
    std::wstring shellId;
    DesktopObjectType type = DesktopObjectType::Shortcut;
    bool includeInDesktop = true;
    int x = -1;
    int y = -1;
    int iconSize = kDesktopIconDefaultSize;
    int selectedCandidate = 0;
    std::vector<ImageCandidate> candidates;
};

struct AppConfig {
    std::wstring desktopPath;
    std::vector<DesktopObject> objects;
    DesktopMode desktopMode = DesktopMode::Wallpaper;
    BackgroundSource backgroundSource = BackgroundSource::SystemWallpaper;
    COLORREF solidColor = RGB(36, 38, 42);
    std::wstring systemWallpaperPath;
};

inline const wchar_t* ToString(DesktopObjectType type) {
    switch (type) {
    case DesktopObjectType::Shortcut:
        return L"shortcut";
    case DesktopObjectType::File:
        return L"file";
    case DesktopObjectType::Folder:
        return L"folder";
    case DesktopObjectType::ThisPC:
        return L"this_pc";
    case DesktopObjectType::RecycleBin:
        return L"recycle_bin";
    }
    return L"shortcut";
}

inline DesktopObjectType DesktopObjectTypeFromString(const std::wstring& value) {
    if (value == L"file") {
        return DesktopObjectType::File;
    }
    if (value == L"folder") {
        return DesktopObjectType::Folder;
    }
    if (value == L"this_pc") {
        return DesktopObjectType::ThisPC;
    }
    if (value == L"recycle_bin") {
        return DesktopObjectType::RecycleBin;
    }
    return DesktopObjectType::Shortcut;
}

inline const wchar_t* ToString(DesktopMode mode) {
    return mode == DesktopMode::WallpaperEngine ? L"wallpaper_engine" : L"wallpaper";
}

inline DesktopMode DesktopModeFromString(const std::wstring& value) {
    return value == L"wallpaper_engine" ? DesktopMode::WallpaperEngine : DesktopMode::Wallpaper;
}

inline const wchar_t* ToString(BackgroundSource source) {
    return source == BackgroundSource::SolidColor ? L"solid_color" : L"system_wallpaper";
}

inline BackgroundSource BackgroundSourceFromString(const std::wstring& value) {
    return value == L"solid_color" ? BackgroundSource::SolidColor : BackgroundSource::SystemWallpaper;
}

inline std::wstring ObjectStableKey(const DesktopObject& object) {
    if (object.type == DesktopObjectType::ThisPC || object.type == DesktopObjectType::RecycleBin) {
        return std::wstring(ToString(object.type)) + L"|" + object.shellId;
    }
    return std::wstring(ToString(object.type)) + L"|" + object.path;
}

inline std::wstring OpenShellIdForObject(const DesktopObject& object) {
    if (object.type == DesktopObjectType::ThisPC) {
        return L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}";
    }
    if (object.type == DesktopObjectType::RecycleBin) {
        return L"::{645FF040-5081-101B-9F08-00AA002F954E}";
    }
    return object.shellId;
}

} // namespace musuka
