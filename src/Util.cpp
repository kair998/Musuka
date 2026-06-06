#include "Util.h"

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace musuka {

namespace {

std::wstring TrimTrailingSlash(std::wstring value) {
    while (value.size() > 3 && (value.back() == L'\\' || value.back() == L'/')) {
        value.pop_back();
    }
    return value;
}

std::wstring LowerCopy(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(towlower(ch));
    });
    return value;
}

} // namespace

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                        result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                                         nullptr, 0);
    if (size <= 0) {
        return {};
    }
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                        result.data(), size);
    return result;
}

std::wstring GetModuleDirectory() {
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD size = 0;
    for (;;) {
        size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (size == 0) {
            return L".";
        }
        if (size < buffer.size() - 1) {
            buffer.resize(size);
            break;
        }
        buffer.resize(buffer.size() * 2);
    }

    std::error_code ec;
    fs::path path(buffer);
    return path.parent_path().wstring();
}

std::wstring GetDataDirectory() {
    return CombinePath(GetModuleDirectory(), L"data");
}

std::wstring GetIconsDirectory() {
    return CombinePath(GetDataDirectory(), L"icons");
}

std::wstring GetConfigPath() {
    return CombinePath(GetDataDirectory(), L"config.json");
}

std::wstring GetDefaultImageDirectory() {
    return CombinePath(GetModuleDirectory(), L"default_image");
}

std::wstring CombinePath(const std::wstring& left, const std::wstring& right) {
    if (left.empty()) {
        return right;
    }
    if (right.empty()) {
        return left;
    }
    fs::path result(left);
    result /= fs::path(right);
    return result.wstring();
}

std::wstring ToAbsoluteAppPath(const std::wstring& path) {
    if (path.empty()) {
        return {};
    }
    fs::path value(path);
    if (value.is_absolute()) {
        return value.wstring();
    }
    fs::path result(GetModuleDirectory());
    result /= value;
    return result.wstring();
}

std::wstring ToAppRelativePath(const std::wstring& absolutePath) {
    if (absolutePath.empty()) {
        return {};
    }
    std::error_code ec;
    fs::path absolute = fs::absolute(fs::path(absolutePath), ec);
    if (ec) {
        return absolutePath;
    }
    fs::path root = fs::absolute(fs::path(GetModuleDirectory()), ec);
    if (ec) {
        return absolutePath;
    }
    fs::path relative = fs::relative(absolute, root, ec);
    if (ec || relative.empty() || relative.native().find(L"..") == 0) {
        return absolute.wstring();
    }
    return relative.wstring();
}

std::wstring NormalizePathForCompare(const std::wstring& path) {
    if (path.empty()) {
        return {};
    }
    std::error_code ec;
    fs::path value = fs::weakly_canonical(fs::path(path), ec);
    if (ec) {
        value = fs::absolute(fs::path(path), ec);
    }
    if (ec) {
        return LowerCopy(TrimTrailingSlash(path));
    }
    return LowerCopy(TrimTrailingSlash(value.wstring()));
}

bool EnsureDirectory(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }
    std::error_code ec;
    if (fs::exists(path, ec)) {
        return fs::is_directory(path, ec);
    }
    return fs::create_directories(path, ec) || fs::exists(path, ec);
}

bool FileExists(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }
    std::error_code ec;
    return fs::is_regular_file(fs::path(path), ec);
}

bool DirectoryExists(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }
    std::error_code ec;
    return fs::is_directory(fs::path(path), ec);
}

std::wstring ExtensionLower(const std::wstring& path) {
    std::wstring ext = fs::path(path).extension().wstring();
    return LowerCopy(ext);
}

bool IsSupportedImageFile(const std::wstring& path) {
    const std::wstring ext = ExtensionLower(path);
    return ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".bmp";
}

std::wstring FileNameFromPath(const std::wstring& path) {
    return fs::path(path).filename().wstring();
}

std::wstring FileStemFromPath(const std::wstring& path) {
    return fs::path(path).stem().wstring();
}

std::wstring SanitizeFileName(const std::wstring& value) {
    std::wstring result;
    result.reserve(value.size());
    for (wchar_t ch : value) {
        const bool invalid = ch < 32 || ch == L'<' || ch == L'>' || ch == L':' ||
                             ch == L'"' || ch == L'/' || ch == L'\\' ||
                             ch == L'|' || ch == L'?' || ch == L'*';
        result.push_back(invalid ? L'_' : ch);
    }
    while (!result.empty() && (result.back() == L'.' || result.back() == L' ')) {
        result.pop_back();
    }
    if (result.empty()) {
        result = L"item";
    }
    if (result.size() > 80) {
        result.resize(80);
    }
    return result;
}

std::wstring HashHex(const std::wstring& value) {
    uint64_t hash = 1469598103934665603ull;
    for (wchar_t ch : value) {
        const uint32_t code = static_cast<uint32_t>(ch);
        for (int i = 0; i < 4; ++i) {
            hash ^= static_cast<uint8_t>((code >> (i * 8)) & 0xffu);
            hash *= 1099511628211ull;
        }
    }
    std::wstringstream stream;
    stream << std::hex << std::nouppercase << std::setw(16) << std::setfill(L'0') << hash;
    return stream.str();
}

std::wstring MakeObjectId(const std::wstring& displayName, const std::wstring& stableKey) {
    return SanitizeFileName(displayName) + L"_" + HashHex(NormalizePathForCompare(stableKey));
}

std::wstring MakeUniqueFilePath(const std::wstring& directory, const std::wstring& desiredFileName) {
    fs::path dir(directory);
    fs::path desired(SanitizeFileName(desiredFileName));
    std::wstring stem = desired.stem().wstring();
    std::wstring ext = desired.extension().wstring();
    if (stem.empty()) {
        stem = L"image";
    }
    fs::path candidate = dir / (stem + ext);
    int suffix = 1;
    while (FileExists(candidate.wstring())) {
        candidate = dir / (stem + L"_" + std::to_wstring(suffix++) + ext);
    }
    return candidate.wstring();
}

bool CopyFileToInternal(const std::wstring& sourcePath,
                        const std::wstring& destinationDirectory,
                        const std::wstring& prefix,
                        std::wstring& outRelativePath,
                        std::wstring& outError) {
    outRelativePath.clear();
    outError.clear();

    if (!FileExists(sourcePath)) {
        outError = L"源图片不存在。";
        return false;
    }
    if (!IsSupportedImageFile(sourcePath)) {
        outError = L"不支持的图片格式。";
        return false;
    }
    if (!EnsureDirectory(destinationDirectory)) {
        outError = L"无法创建 musuka 内部图片目录。";
        return false;
    }

    const std::wstring fileName = SanitizeFileName(prefix + L"_" + FileNameFromPath(sourcePath));
    const std::wstring destination = MakeUniqueFilePath(destinationDirectory, fileName);
    if (!CopyFileW(sourcePath.c_str(), destination.c_str(), TRUE)) {
        outError = L"图片复制失败。";
        return false;
    }
    outRelativePath = ToAppRelativePath(destination);
    return true;
}

std::vector<std::wstring> EnumerateImageFiles(const std::wstring& directory, bool recursive) {
    std::vector<std::wstring> result;
    if (!DirectoryExists(directory)) {
        return result;
    }

    std::error_code ec;
    if (recursive) {
        for (const auto& entry : fs::recursive_directory_iterator(directory, fs::directory_options::skip_permission_denied, ec)) {
            if (ec) {
                break;
            }
            if (entry.is_regular_file(ec) && IsSupportedImageFile(entry.path().wstring())) {
                result.push_back(entry.path().wstring());
            }
        }
    } else {
        for (const auto& entry : fs::directory_iterator(directory, fs::directory_options::skip_permission_denied, ec)) {
            if (ec) {
                break;
            }
            if (entry.is_regular_file(ec) && IsSupportedImageFile(entry.path().wstring())) {
                result.push_back(entry.path().wstring());
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::wstring CurrentTimestampForFileName() {
    SYSTEMTIME time{};
    GetLocalTime(&time);
    wchar_t buffer[64]{};
    swprintf_s(buffer, L"%04u%02u%02u_%02u%02u%02u",
               time.wYear, time.wMonth, time.wDay,
               time.wHour, time.wMinute, time.wSecond);
    return buffer;
}

std::wstring ColorToHex(COLORREF color) {
    wchar_t buffer[16]{};
    swprintf_s(buffer, L"#%02X%02X%02X", GetRValue(color), GetGValue(color), GetBValue(color));
    return buffer;
}

COLORREF ColorFromHex(const std::wstring& value, COLORREF fallback) {
    if (value.size() != 7 || value[0] != L'#') {
        return fallback;
    }
    unsigned int r = 0;
    unsigned int g = 0;
    unsigned int b = 0;
    if (swscanf_s(value.c_str() + 1, L"%02x%02x%02x", &r, &g, &b) != 3) {
        return fallback;
    }
    return RGB(r, g, b);
}

std::wstring ParentDirectory(const std::wstring& path) {
    return fs::path(path).parent_path().wstring();
}

bool IsSafeRelativeId(const std::wstring& id) {
    if (id.empty() || id.size() > 128) {
        return false;
    }
    for (wchar_t ch : id) {
        const bool safe = (ch >= L'0' && ch <= L'9') ||
                          (ch >= L'A' && ch <= L'Z') ||
                          (ch >= L'a' && ch <= L'z') ||
                          ch == L'_' || ch == L'-';
        if (!safe) {
            return false;
        }
    }
    // Reject "." and ".."
    if (id == L"." || id == L"..") {
        return false;
    }
    // Reject trailing dots or leading dots (Windows treats them as relative)
    if (id.front() == L'.' || id.back() == L'.') {
        return false;
    }
    // Reject Windows reserved device names (case-insensitive)
    std::wstring upper = id;
    for (auto& c : upper) { if (c >= L'a' && c <= L'z') c -= 32; }
    static const std::wstring reserved[] = {
        L"CON", L"PRN", L"AUX", L"NUL",
        L"COM1", L"COM2", L"COM3", L"COM4", L"COM5",
        L"COM6", L"COM7", L"COM8", L"COM9",
        L"LPT1", L"LPT2", L"LPT3", L"LPT4", L"LPT5",
        L"LPT6", L"LPT7", L"LPT8", L"LPT9"
    };
    for (const auto& r : reserved) {
        if (upper == r) {
            return false;
        }
    }
    return true;
}

bool IsPathInsideIconsRoot(const std::wstring& candidatePath) {
    // Verify the icons root directory itself is safe (exists and canonical).
    std::error_code ec;
    fs::path canonicalRoot = fs::canonical(fs::path(GetIconsDirectory()), ec);
    if (ec) {
        return false; // root doesn't exist or can't resolve
    }
    // For candidate: use weakly_canonical so it works even if path doesn't exist yet.
    // This fixes a regression where new object directories couldn't be created.
    fs::path resolvedCandidate = fs::weakly_canonical(fs::path(candidatePath));
    std::wstring objStr = NormalizePathForCompare(resolvedCandidate.wstring());
    std::wstring rootStr = NormalizePathForCompare(canonicalRoot.wstring());
    // Ensure root ends with a separator for strict prefix matching.
    if (!rootStr.empty() && rootStr.back() != L'\\' && rootStr.back() != L'/') {
        rootStr += L'\\';
    }
    return objStr.size() >= rootStr.size() &&
           objStr.compare(0, rootStr.size(), rootStr) == 0;
}

bool IsReparsePoint(const std::wstring& path) {
    const DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;
    return (attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

} // namespace musuka
