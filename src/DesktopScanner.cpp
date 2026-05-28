#include "DesktopScanner.h"

#include "ImageUtil.h"
#include "Util.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <map>
#include <set>
#include <shlobj.h>

namespace fs = std::filesystem;

namespace musuka {

namespace {

DesktopObject MakeVirtualObject(DesktopObjectType type) {
    DesktopObject object;
    object.type = type;
    if (type == DesktopObjectType::ThisPC) {
        object.name = L"此电脑";
        object.shellId = L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}";
    } else {
        object.name = L"回收站";
        object.shellId = L"::{645FF040-5081-101B-9F08-00AA002F954E}";
    }
    object.id = MakeObjectId(object.name, ObjectStableKey(object));
    object.includeInDesktop = true;
    return object;
}

std::wstring ExistingKey(const DesktopObject& object) {
    return NormalizePathForCompare(ObjectStableKey(object));
}

bool CandidateHasInternalPath(const DesktopObject& object, const std::wstring& internalPath) {
    const std::wstring normalized = NormalizePathForCompare(ToAbsoluteAppPath(internalPath));
    return std::any_of(object.candidates.begin(), object.candidates.end(), [&](const ImageCandidate& candidate) {
        return NormalizePathForCompare(ToAbsoluteAppPath(candidate.internalPath)) == normalized;
    });
}

bool HasOriginalIconCandidate(const DesktopObject& object) {
    return std::any_of(object.candidates.begin(), object.candidates.end(), [](const ImageCandidate& candidate) {
        return candidate.originalIcon;
    });
}

bool PathIsInsideDirectory(const std::wstring& path, const std::wstring& directory) {
    const std::wstring normalizedPath = NormalizePathForCompare(path);
    std::wstring normalizedDirectory = NormalizePathForCompare(directory);
    if (normalizedPath.empty() || normalizedDirectory.empty()) {
        return false;
    }
    if (normalizedDirectory.back() != L'\\' && normalizedDirectory.back() != L'/') {
        normalizedDirectory.push_back(L'\\');
    }
    return normalizedPath.size() > normalizedDirectory.size() &&
           normalizedPath.compare(0, normalizedDirectory.size(), normalizedDirectory) == 0;
}

bool IsLegacyCopiedDefaultCandidate(const ImageCandidate& candidate, const std::wstring& objectDir) {
    if (candidate.originalIcon) {
        return false;
    }
    const std::wstring fileName = FileNameFromPath(candidate.internalPath);
    if (fileName.rfind(L"default_", 0) != 0) {
        return false;
    }
    return PathIsInsideDirectory(ToAbsoluteAppPath(candidate.internalPath), objectDir);
}

bool IsSharedDefaultCandidate(const ImageCandidate& candidate, const std::wstring& defaultDir) {
    if (candidate.originalIcon) {
        return false;
    }
    return PathIsInsideDirectory(ToAbsoluteAppPath(candidate.internalPath), defaultDir);
}

int FindCandidateByInternalPath(const DesktopObject& object, const std::wstring& internalPath) {
    if (internalPath.empty()) {
        return -1;
    }
    const std::wstring normalized = NormalizePathForCompare(ToAbsoluteAppPath(internalPath));
    for (int i = 0; i < static_cast<int>(object.candidates.size()); ++i) {
        if (NormalizePathForCompare(ToAbsoluteAppPath(object.candidates[static_cast<size_t>(i)].internalPath)) ==
            normalized) {
            return i;
        }
    }
    return -1;
}

int FindCandidateByDisplayName(const DesktopObject& object, const std::wstring& displayName) {
    if (displayName.empty()) {
        return -1;
    }
    for (int i = 0; i < static_cast<int>(object.candidates.size()); ++i) {
        if (object.candidates[static_cast<size_t>(i)].displayName == displayName) {
            return i;
        }
    }
    return -1;
}

void PruneStaleDefaultCandidates(DesktopObject& object,
                                 const std::wstring& objectDir,
                                 const std::wstring& defaultDir) {
    std::vector<ImageCandidate> kept;
    kept.reserve(object.candidates.size());
    for (const auto& candidate : object.candidates) {
        if (IsLegacyCopiedDefaultCandidate(candidate, objectDir)) {
            const std::wstring absolutePath = ToAbsoluteAppPath(candidate.internalPath);
            if (PathIsInsideDirectory(absolutePath, objectDir)) {
                DeleteFileW(absolutePath.c_str());
            }
            continue;
        }
        if (IsSharedDefaultCandidate(candidate, defaultDir) &&
            !FileExists(ToAbsoluteAppPath(candidate.internalPath))) {
            continue;
        }
        kept.push_back(candidate);
    }
    object.candidates = std::move(kept);
}

std::wstring KnownFolderPath(REFKNOWNFOLDERID folderId) {
    PWSTR path = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(folderId, KF_FLAG_DEFAULT, nullptr, &path))) {
        result = path;
        CoTaskMemFree(path);
    }
    return result;
}

void AddScanPath(const std::wstring& path,
                 std::vector<std::wstring>& paths,
                 std::set<std::wstring>& seen) {
    if (!DirectoryExists(path)) {
        return;
    }
    const std::wstring key = NormalizePathForCompare(path);
    if (seen.insert(key).second) {
        paths.push_back(path);
    }
}

std::vector<std::wstring> DesktopScanPaths(const std::wstring& primaryPath) {
    std::vector<std::wstring> paths;
    std::set<std::wstring> seen;
    AddScanPath(primaryPath, paths, seen);
    AddScanPath(KnownFolderPath(FOLDERID_Desktop), paths, seen);
    AddScanPath(KnownFolderPath(FOLDERID_PublicDesktop), paths, seen);
    return paths;
}

bool MakeObjectFromEntry(const fs::directory_entry& entry,
                         std::error_code& ec,
                         DesktopObject& object) {
    object = DesktopObject{};
    if (entry.is_directory(ec)) {
        object.type = DesktopObjectType::Folder;
        object.name = entry.path().filename().wstring();
        object.path = entry.path().wstring();
    } else if (entry.is_regular_file(ec)) {
        std::wstring fileName = entry.path().filename().wstring();
        std::transform(fileName.begin(), fileName.end(), fileName.begin(), [](wchar_t ch) {
            return static_cast<wchar_t>(towlower(ch));
        });
        if (fileName == L"desktop.ini") {
            return false;
        }
        if (ExtensionLower(entry.path().wstring()) == L".lnk") {
            object.type = DesktopObjectType::Shortcut;
            object.name = entry.path().stem().wstring();
        } else {
            object.type = DesktopObjectType::File;
            object.name = entry.path().filename().wstring();
        }
        object.path = entry.path().wstring();
    } else {
        return false;
    }

    object.id = MakeObjectId(object.name, ObjectStableKey(object));
    object.includeInDesktop = true;
    return true;
}

} // namespace

bool DesktopScanner::ScanAndPrepare(AppConfig& config, std::wstring& error, std::wstring& warning) {
    error.clear();
    warning.clear();

    if (!DirectoryExists(config.desktopPath)) {
        error = L"桌面路径不存在。";
        return false;
    }

    std::vector<DesktopObject> scanned;
    std::set<std::wstring> scannedKeys;
    const auto scanPaths = DesktopScanPaths(config.desktopPath);
    for (const auto& scanPath : scanPaths) {
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(scanPath, fs::directory_options::skip_permission_denied, ec)) {
            if (ec) {
                warning += L"扫描桌面路径失败：" + scanPath + L"\n";
                break;
            }

            DesktopObject object;
            if (!MakeObjectFromEntry(entry, ec, object)) {
                continue;
            }
            const std::wstring key = ExistingKey(object);
            if (!scannedKeys.insert(key).second) {
                continue;
            }
            scanned.push_back(std::move(object));
        }
    }

    scanned.push_back(MakeVirtualObject(DesktopObjectType::ThisPC));
    scanned.push_back(MakeVirtualObject(DesktopObjectType::RecycleBin));

    std::map<std::wstring, DesktopObject> existingByKey;
    for (auto& object : config.objects) {
        existingByKey[ExistingKey(object)] = object;
    }

    std::vector<DesktopObject> merged;
    for (auto& object : scanned) {
        auto it = existingByKey.find(ExistingKey(object));
        if (it != existingByKey.end()) {
            DesktopObject preserved = it->second;
            preserved.name = object.name;
            preserved.path = object.path;
            preserved.shellId = object.shellId;
            preserved.type = object.type;
            preserved.iconSize = std::clamp(preserved.iconSize,
                                            kDesktopIconMinSize,
                                            kDesktopIconMaxSize);
            if (preserved.id.empty()) {
                preserved.id = object.id;
            }
            InitializeObjectImages(preserved, warning);
            merged.push_back(std::move(preserved));
        } else {
            InitializeObjectImages(object, warning);
            merged.push_back(std::move(object));
        }
    }

    config.objects = std::move(merged);
    return true;
}

void DesktopScanner::InitializeObjectImages(DesktopObject& object, std::wstring& warning) {
    if (object.id.empty()) {
        object.id = MakeObjectId(object.name, ObjectStableKey(object));
    }

    const std::wstring objectDir = CombinePath(GetIconsDirectory(), object.id);
    EnsureDirectory(objectDir);
    const std::wstring defaultDir = GetDefaultImageDirectory();

    std::wstring selectedInternalPath;
    std::wstring selectedDisplayName;
    if (object.selectedCandidate >= 0 &&
        object.selectedCandidate < static_cast<int>(object.candidates.size())) {
        const auto& selected = object.candidates[static_cast<size_t>(object.selectedCandidate)];
        selectedInternalPath = selected.internalPath;
        selectedDisplayName = selected.displayName;
    }

    PruneStaleDefaultCandidates(object, objectDir, defaultDir);

    const std::wstring originalIconPath = CombinePath(objectDir, L"original_icon.png");
    if (!FileExists(originalIconPath)) {
        HICON icon = LoadShellIconForObject(object, true);
        if (icon) {
            SaveHIconAsPng(icon, originalIconPath);
            DestroyIcon(icon);
        }
    }

    if (!HasOriginalIconCandidate(object) && FileExists(originalIconPath)) {
        ImageCandidate candidate;
        candidate.displayName = L"原始图标";
        candidate.originalPath = object.path.empty() ? object.shellId : object.path;
        candidate.internalPath = ToAppRelativePath(originalIconPath);
        candidate.originalIcon = true;
        object.candidates.insert(object.candidates.begin(), std::move(candidate));
        object.selectedCandidate = 0;
    }

    if (!DirectoryExists(defaultDir)) {
        if (warning.find(L"default_image") == std::wstring::npos) {
            warning += L"default_image 目录不存在，默认图片为空。\n";
        }
    } else {
        const auto defaultImages = EnumerateImageFiles(defaultDir, false);
        if (defaultImages.empty()) {
            if (warning.find(L"默认图片为空") == std::wstring::npos) {
                warning += L"default_image 目录中没有可用图片。\n";
            }
        }
        for (const auto& imagePath : defaultImages) {
            const std::wstring relativePath = ToAppRelativePath(imagePath);
            if (CandidateHasInternalPath(object, relativePath)) {
                continue;
            }
            ImageCandidate candidate;
            candidate.displayName = FileNameFromPath(imagePath);
            candidate.originalPath = relativePath;
            candidate.internalPath = relativePath;
            candidate.originalIcon = false;
            candidate.layerPriority = kDefaultImageLayerPriority;
            object.candidates.push_back(std::move(candidate));
        }
    }

    int selected = FindCandidateByInternalPath(object, selectedInternalPath);
    if (selected < 0) {
        selected = FindCandidateByDisplayName(object, selectedDisplayName);
    }
    if (selected < 0 &&
        object.selectedCandidate >= 0 &&
        object.selectedCandidate < static_cast<int>(object.candidates.size())) {
        selected = object.selectedCandidate;
    }
    object.selectedCandidate = selected >= 0 ? selected : (object.candidates.empty() ? -1 : 0);
}

} // namespace musuka
