#include "WinUtil.h"

#include "Util.h"

#include <commdlg.h>
#include <shlobj.h>

namespace musuka {

namespace {

struct DesktopWindowHandles {
    HWND host = nullptr;
    HWND iconList = nullptr;
};

BOOL CALLBACK FindDesktopWindowsCallback(HWND topLevel, LPARAM parameter) {
    auto* handles = reinterpret_cast<DesktopWindowHandles*>(parameter);
    HWND shellView = FindWindowExW(topLevel, nullptr, L"SHELLDLL_DefView", nullptr);
    if (!shellView) {
        return TRUE;
    }

    handles->host = topLevel;
    handles->iconList = FindWindowExW(shellView, nullptr, L"SysListView32", L"FolderView");
    if (!handles->iconList) {
        handles->iconList = FindWindowExW(shellView, nullptr, L"SysListView32", nullptr);
    }
    return FALSE;
}

DesktopWindowHandles FindDesktopWindows() {
    DesktopWindowHandles handles;
    EnumWindows(FindDesktopWindowsCallback, reinterpret_cast<LPARAM>(&handles));
    if (!handles.host) {
        handles.host = FindWindowW(L"Progman", nullptr);
    }
    return handles;
}

HFONT LargerUiFont() {
    static HFONT font = []() -> HFONT {
        LOGFONTW logFont{};
        HFONT guiFont = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        GetObjectW(guiFont, sizeof(logFont), &logFont);
        HDC dc = GetDC(nullptr);
        const int dpiY = dc ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
        if (dc) {
            ReleaseDC(nullptr, dc);
        }
        logFont.lfHeight = -MulDiv(11, dpiY, 72);
        logFont.lfWeight = FW_NORMAL;
        wcscpy_s(logFont.lfFaceName, L"Segoe UI");
        return CreateFontIndirectW(&logFont);
    }();
    return font ? font : reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
}

} // namespace

void ShowError(HWND owner, const std::wstring& message) {
    MessageBoxW(owner, message.c_str(), L"musuka", MB_OK | MB_ICONERROR);
}

void ShowInfo(HWND owner, const std::wstring& message) {
    MessageBoxW(owner, message.c_str(), L"musuka", MB_OK | MB_ICONINFORMATION);
}

void ApplyDefaultFont(HWND hwnd) {
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(LargerUiFont()), TRUE);
}

void CenterWindowOnScreen(HWND hwnd) {
    RECT rect{};
    GetWindowRect(hwnd, &rect);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(hwnd, nullptr,
                 (screenWidth - width) / 2,
                 (screenHeight - height) / 2,
                 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

std::wstring BrowseForFolder(HWND owner, const std::wstring& title, const std::wstring& initialPath) {
    std::wstring result;
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dialog)))) {
        return result;
    }

    DWORD options = 0;
    if (SUCCEEDED(dialog->GetOptions(&options))) {
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    }
    dialog->SetTitle(title.c_str());

    if (!initialPath.empty() && DirectoryExists(initialPath)) {
        IShellItem* folder = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(initialPath.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
            dialog->SetFolder(folder);
            folder->Release();
        }
    }

    if (SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                result = path;
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dialog->Release();
    return result;
}

std::wstring OpenImageFileDialog(HWND owner) {
    wchar_t fileName[MAX_PATH]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Image Files (*.png;*.jpg;*.jpeg;*.bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"导入单张图片";
    if (GetOpenFileNameW(&ofn)) {
        return fileName;
    }
    return {};
}

std::wstring GetKnownDesktopPath() {
    PWSTR path = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, KF_FLAG_DEFAULT, nullptr, &path))) {
        result = path;
        CoTaskMemFree(path);
    }
    return result;
}

bool TryGetSystemWallpaperPath(std::wstring& outPath) {
    wchar_t buffer[MAX_PATH]{};
    if (!SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, buffer, 0)) {
        outPath.clear();
        return false;
    }
    outPath = buffer;
    return !outPath.empty() && FileExists(outPath);
}

HWND FindDesktopHostWindow() {
    return FindDesktopWindows().host;
}

HWND FindDesktopIconListView() {
    return FindDesktopWindows().iconList;
}

bool ChooseSolidColor(HWND owner, COLORREF initial, COLORREF& outColor) {
    static COLORREF customColors[16] = {
        RGB(36, 38, 42), RGB(245, 245, 245), RGB(28, 86, 120), RGB(128, 48, 72),
        RGB(38, 105, 72), RGB(92, 80, 48), RGB(60, 60, 84), RGB(110, 110, 110)
    };
    CHOOSECOLORW choose{};
    choose.lStructSize = sizeof(choose);
    choose.hwndOwner = owner;
    choose.rgbResult = initial;
    choose.lpCustColors = customColors;
    choose.Flags = CC_RGBINIT | CC_FULLOPEN;
    if (ChooseColorW(&choose)) {
        outColor = choose.rgbResult;
        return true;
    }
    return false;
}

std::wstring GetWindowTextString(HWND hwnd) {
    const int length = GetWindowTextLengthW(hwnd);
    std::wstring text(static_cast<size_t>(length + 1), L'\0');
    GetWindowTextW(hwnd, text.data(), length + 1);
    text.resize(static_cast<size_t>(length));
    return text;
}

void SetWindowTextString(HWND hwnd, const std::wstring& text) {
    SetWindowTextW(hwnd, text.c_str());
}

std::wstring FormatWindowsError(DWORD errorCode) {
    if (errorCode == 0) {
        errorCode = GetLastError();
    }
    wchar_t* buffer = nullptr;
    const DWORD size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                      FORMAT_MESSAGE_FROM_SYSTEM |
                                      FORMAT_MESSAGE_IGNORE_INSERTS,
                                      nullptr,
                                      errorCode,
                                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                      reinterpret_cast<LPWSTR>(&buffer),
                                      0,
                                      nullptr);
    std::wstring message = size > 0 && buffer ? buffer : L"未知错误";
    if (buffer) {
        LocalFree(buffer);
    }
    return message;
}

} // namespace musuka
