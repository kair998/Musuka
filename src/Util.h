#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace musuka {

std::string WideToUtf8(const std::wstring& value);
std::wstring Utf8ToWide(const std::string& value);

std::wstring GetModuleDirectory();
std::wstring GetDataDirectory();
std::wstring GetIconsDirectory();
std::wstring GetConfigPath();
std::wstring GetDefaultImageDirectory();

std::wstring CombinePath(const std::wstring& left, const std::wstring& right);
std::wstring ToAbsoluteAppPath(const std::wstring& path);
std::wstring ToAppRelativePath(const std::wstring& absolutePath);
std::wstring NormalizePathForCompare(const std::wstring& path);

bool EnsureDirectory(const std::wstring& path);
bool FileExists(const std::wstring& path);
bool DirectoryExists(const std::wstring& path);
bool IsSupportedImageFile(const std::wstring& path);

std::wstring FileNameFromPath(const std::wstring& path);
std::wstring FileStemFromPath(const std::wstring& path);
std::wstring ExtensionLower(const std::wstring& path);
std::wstring SanitizeFileName(const std::wstring& value);
std::wstring HashHex(const std::wstring& value);
std::wstring MakeObjectId(const std::wstring& displayName, const std::wstring& stableKey);
std::wstring MakeUniqueFilePath(const std::wstring& directory, const std::wstring& desiredFileName);
bool CopyFileToInternal(const std::wstring& sourcePath,
                        const std::wstring& destinationDirectory,
                        const std::wstring& prefix,
                        std::wstring& outRelativePath,
                        std::wstring& outError);

std::vector<std::wstring> EnumerateImageFiles(const std::wstring& directory, bool recursive);
std::wstring CurrentTimestampForFileName();
std::wstring ColorToHex(COLORREF color);
COLORREF ColorFromHex(const std::wstring& value, COLORREF fallback);
std::wstring ParentDirectory(const std::wstring& path);
bool IsSafeRelativeId(const std::wstring& id);

// Verify that candidatePath (after canonical resolution) is strictly inside
// the data\icons root directory. Returns false if path escapes or cannot be resolved.
bool IsPathInsideIconsRoot(const std::wstring& candidatePath);

// Check whether a filesystem path is a reparse point (junction / symlink).
// Returns true if the attribute check indicates a reparse point.
bool IsReparsePoint(const std::wstring& path);

} // namespace musuka

