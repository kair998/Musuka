#pragma once

#include <windows.h>

#include <string>

namespace musuka {

void ShowError(HWND owner, const std::wstring& message);
void ShowInfo(HWND owner, const std::wstring& message);
void ApplyDefaultFont(HWND hwnd);
void CenterWindowOnScreen(HWND hwnd);

std::wstring BrowseForFolder(HWND owner, const std::wstring& title, const std::wstring& initialPath = {});
std::wstring OpenImageFileDialog(HWND owner);
std::wstring GetKnownDesktopPath();
bool TryGetSystemWallpaperPath(std::wstring& outPath);
HWND FindDesktopHostWindow();
HWND FindDesktopIconListView();
bool ChooseSolidColor(HWND owner, COLORREF initial, COLORREF& outColor);
std::wstring GetWindowTextString(HWND hwnd);
void SetWindowTextString(HWND hwnd, const std::wstring& text);
std::wstring FormatWindowsError(DWORD errorCode);

} // namespace musuka
