#include "ConfigStore.h"

#include "Json.h"
#include "Util.h"

#include <fstream>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <utility>

namespace musuka {

namespace {

JsonValue WideStringValue(const std::wstring& value) {
    return JsonValue::String(WideToUtf8(value));
}

std::wstring WideFromValue(const JsonValue& value) {
    return Utf8ToWide(value.AsStringOr({}));
}

JsonValue CandidateToJson(const ImageCandidate& candidate) {
    JsonValue::Object object;
    object["display_name"] = WideStringValue(candidate.displayName);
    object["original_path"] = WideStringValue(candidate.originalPath);
    object["internal_path"] = WideStringValue(candidate.internalPath);
    object["original_icon"] = JsonValue::Bool(candidate.originalIcon);
    object["layer_priority"] = JsonValue::Number(candidate.layerPriority);
    return JsonValue::ObjectValue(std::move(object));
}

ImageCandidate CandidateFromJson(const JsonValue& value) {
    ImageCandidate candidate;
    candidate.displayName = WideFromValue(value.At("display_name"));
    candidate.originalPath = WideFromValue(value.At("original_path"));
    candidate.internalPath = WideFromValue(value.At("internal_path"));
    candidate.originalIcon = value.At("original_icon").AsBool(false);
    candidate.layerPriority = static_cast<int>(
        value.At("layer_priority").AsNumber(kDefaultImageLayerPriority));
    return candidate;
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

bool IsSharedDefaultCandidate(const ImageCandidate& candidate) {
    return !candidate.originalIcon &&
           PathIsInsideDirectory(ToAbsoluteAppPath(candidate.internalPath), GetDefaultImageDirectory());
}

bool SameInternalPath(const std::wstring& left, const std::wstring& right) {
    if (left.empty() || right.empty()) {
        return false;
    }
    return NormalizePathForCompare(ToAbsoluteAppPath(left)) ==
           NormalizePathForCompare(ToAbsoluteAppPath(right));
}

JsonValue ObjectToJson(const DesktopObject& object) {
    JsonValue::Object result;
    result["id"] = WideStringValue(object.id);
    result["name"] = WideStringValue(object.name);
    result["type"] = WideStringValue(ToString(object.type));
    result["path"] = WideStringValue(object.path);
    result["shell_id"] = WideStringValue(object.shellId);
    result["include_in_desktop"] = JsonValue::Bool(object.includeInDesktop);
    result["x"] = JsonValue::Number(object.x);
    result["y"] = JsonValue::Number(object.y);
    result["icon_size"] = JsonValue::Number(object.iconSize);

    std::wstring selectedPath = object.selectedImageInternalPath;
    if (selectedPath.empty() &&
        object.selectedCandidate >= 0 &&
        object.selectedCandidate < static_cast<int>(object.candidates.size())) {
        selectedPath = object.candidates[static_cast<size_t>(object.selectedCandidate)].internalPath;
    }

    JsonValue::Array candidates;
    int selectedCandidate = -1;
    for (const auto& candidate : object.candidates) {
        if (IsSharedDefaultCandidate(candidate)) {
            continue;
        }
        if (SameInternalPath(candidate.internalPath, selectedPath)) {
            selectedCandidate = static_cast<int>(candidates.size());
        }
        candidates.push_back(CandidateToJson(candidate));
    }
    result["selected_candidate"] = JsonValue::Number(selectedCandidate);
    result["selected_image_internal_path"] = WideStringValue(selectedPath);
    result["candidates"] = JsonValue::ArrayValue(std::move(candidates));
    return JsonValue::ObjectValue(std::move(result));
}

DesktopObject ObjectFromJson(const JsonValue& value) {
    DesktopObject object;
    object.id = WideFromValue(value.At("id"));
    object.name = WideFromValue(value.At("name"));
    object.type = DesktopObjectTypeFromString(WideFromValue(value.At("type")));
    object.path = WideFromValue(value.At("path"));
    object.shellId = WideFromValue(value.At("shell_id"));
    object.includeInDesktop = value.At("include_in_desktop").AsBool(true);
    object.x = static_cast<int>(value.At("x").AsNumber(-1));
    object.y = static_cast<int>(value.At("y").AsNumber(-1));
    object.iconSize = std::clamp(
        static_cast<int>(value.At("icon_size").AsNumber(kDesktopIconDefaultSize)),
        kDesktopIconMinSize,
        kDesktopIconMaxSize);
    const int storedSelectedCandidate = static_cast<int>(value.At("selected_candidate").AsNumber(0));

    for (const auto& item : value.At("candidates").AsArray()) {
        object.candidates.push_back(CandidateFromJson(item));
    }

    const std::wstring selectedPath = WideFromValue(value.At("selected_image_internal_path"));
    object.selectedImageInternalPath = selectedPath;
    if (!selectedPath.empty()) {
        object.selectedCandidate = -1;
        for (size_t i = 0; i < object.candidates.size(); ++i) {
            if (SameInternalPath(object.candidates[i].internalPath, selectedPath)) {
                object.selectedCandidate = static_cast<int>(i);
                break;
            }
        }
    } else {
        object.selectedCandidate = storedSelectedCandidate;
    }
    if (object.selectedCandidate < 0 ||
        object.selectedCandidate >= static_cast<int>(object.candidates.size())) {
        object.selectedCandidate = -1;
    }
    if (object.selectedImageInternalPath.empty() &&
        object.selectedCandidate >= 0 &&
        object.selectedCandidate < static_cast<int>(object.candidates.size())) {
        object.selectedImageInternalPath =
            object.candidates[static_cast<size_t>(object.selectedCandidate)].internalPath;
    }
    return object;
}

JsonValue ConfigToJson(const AppConfig& config) {
    JsonValue::Object root;
    root["version"] = JsonValue::Number(3);
    root["desktop_path"] = WideStringValue(config.desktopPath);
    root["desktop_mode"] = WideStringValue(ToString(config.desktopMode));
    root["background_source"] = WideStringValue(ToString(config.backgroundSource));
    root["solid_color"] = WideStringValue(ColorToHex(config.solidColor));
    root["system_wallpaper_path"] = WideStringValue(config.systemWallpaperPath);

    JsonValue::Array objects;
    for (const auto& object : config.objects) {
        objects.push_back(ObjectToJson(object));
    }
    root["objects"] = JsonValue::ArrayValue(std::move(objects));
    return JsonValue::ObjectValue(std::move(root));
}

void ConfigFromJson(const JsonValue& root, AppConfig& config) {
    const int version = static_cast<int>(root.At("version").AsNumber(1));
    config.desktopPath = WideFromValue(root.At("desktop_path"));
    config.desktopMode = DesktopModeFromString(WideFromValue(root.At("desktop_mode")));
    config.backgroundSource = BackgroundSourceFromString(WideFromValue(root.At("background_source")));
    config.solidColor = ColorFromHex(WideFromValue(root.At("solid_color")), RGB(36, 38, 42));
    config.systemWallpaperPath = WideFromValue(root.At("system_wallpaper_path"));
    config.objects.clear();
    for (const auto& item : root.At("objects").AsArray()) {
        DesktopObject object = ObjectFromJson(item);
        if (version < 2) {
            ApplyPreferredIconSizeForSelectedCandidate(object);
        }
        config.objects.push_back(std::move(object));
    }
}

bool ReadFileUtf8(const std::wstring& path, std::string& out) {
    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    if (!file) {
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    out = buffer.str();
    if (out.size() >= 3 &&
        static_cast<unsigned char>(out[0]) == 0xEF &&
        static_cast<unsigned char>(out[1]) == 0xBB &&
        static_cast<unsigned char>(out[2]) == 0xBF) {
        out.erase(0, 3);
    }
    return true;
}

} // namespace

bool ConfigStore::Load(AppConfig& config, std::wstring& warning) {
    warning.clear();
    EnsureDirectory(GetDataDirectory());
    EnsureDirectory(GetIconsDirectory());

    const std::wstring configPath = GetConfigPath();
    if (!FileExists(configPath)) {
        return true;
    }

    std::string text;
    if (!ReadFileUtf8(configPath, text)) {
        warning = L"配置文件读取失败，将使用空配置。";
        return false;
    }

    JsonValue root;
    std::string error;
    if (!ParseJson(text, root, error) || !root.IsObject()) {
        const std::wstring backupPath = CombinePath(GetDataDirectory(),
            L"config.corrupt_" + CurrentTimestampForFileName() + L".json");
        MoveFileExW(configPath.c_str(), backupPath.c_str(), MOVEFILE_REPLACE_EXISTING);
        warning = L"配置文件损坏，已备份为 " + FileNameFromPath(backupPath) + L"，程序将重新配置。";
        config = AppConfig{};
        return false;
    }

    ConfigFromJson(root, config);
    return true;
}

bool ConfigStore::Save(const AppConfig& config, std::wstring& error) const {
    error.clear();
    if (!EnsureDirectory(GetDataDirectory())) {
        error = L"无法创建 data 目录。";
        return false;
    }
    if (!EnsureDirectory(GetIconsDirectory())) {
        error = L"无法创建 data\\icons 目录。";
        return false;
    }

    const std::string text = StringifyJson(ConfigToJson(config), 0);
    std::ofstream file(std::filesystem::path(GetConfigPath()), std::ios::binary | std::ios::trunc);
    if (!file) {
        error = L"无法写入配置文件。";
        return false;
    }
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(file);
}

} // namespace musuka
