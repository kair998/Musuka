#pragma once

#include "Models.h"

#include <QString>

#include <string>

namespace musuka {

enum class SettingsStringId {
    Language,
    Previous,
    Next,
    Run,
    Step1Title,
    Step1Description,
    DesktopScanLocation,
    DesktopScanHint,
    Browse,
    Yes,
    No,
    Step2Title,
    Step2Description,
    SelectedReplacementPreview,
    DesktopObjects,
    Search,
    SelectFileHint,
    ImportImages,
    ImportFolder,
    DesktopDisplaySize,
    ObjectCandidates,
    Ignore,
    IgnoreAll,
    Replace,
    Step3Title,
    Step3Description,
    VirtualDesktopMode,
    StaticWallpaperVirtualDesktopMode,
    CompatibilityMode,
    DesktopStaticWallpaperCompatibilityMode,
    WallpaperEngineCompatibilityMode,
    StaticWallpaperSource,
    UseCurrentSystemStaticWallpaper,
    UseMusukaSolidColor,
    ChooseColor,
    CurrentSystemStaticWallpaperPrefix,
    CurrentSystemStaticWallpaperReadFailed,
    SelectDesktopFolder,
    DesktopPathMissing,
    SystemStaticWallpaperReadFailed,
    SelectImageFiles,
    ImageFileFilter,
    ImportImageFolder,
    NoSupportedImagesInFolder,
    BulkImportConfirmation,
    FolderImagesAlreadyImported,
    ImageImportFailed,
    ImportSummary,
    SelectCandidateFirst,
    NoDesktopObjects,
    NoMatchingDesktopObjects,
    SelectDesktopObjectFirst,
    CurrentPrefix,
    NoObjectCandidates,
    NoDefaultImages,
    NoFileSelected,
    OriginalIcon,
    ImageReadFailed,
    Include,
    IncludeAll,
    ThisPC,
    RecycleBin,
    IgnoredSuffix,
    UnsupportedImageFormat,
    ImageTooLarge,
    InvalidImage,
    UnsafeObjectDirectory,
    ReparsePointObjectDirectory,
    Count
};

QString SettingsString(SettingsLanguage language, SettingsStringId id);
QString SettingsLanguageName(SettingsLanguage language);
QString LocalizeSettingsMessage(SettingsLanguage language, const std::wstring& message);

} // namespace musuka
