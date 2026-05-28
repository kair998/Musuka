#include "SettingsWindow.h"

#include "App.h"
#include "DesktopScanner.h"
#include "ImageUtil.h"
#include "Util.h"
#include "WinUtil.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <windowsx.h>

namespace fs = std::filesystem;

namespace musuka {

namespace {

constexpr int ID_PATH_EDIT = 1001;
constexpr int ID_BROWSE_PATH = 1002;
constexpr int ID_PREV = 1003;
constexpr int ID_NEXT = 1004;
constexpr int ID_RUN = 1005;
constexpr int ID_SEARCH = 1101;
constexpr int ID_OBJECT_LIST = 1102;
constexpr int ID_IMPORT_SINGLE = 1103;
constexpr int ID_IMPORT_FOLDER = 1104;
constexpr int ID_CANDIDATE_LIST = 1105;
constexpr int ID_TOGGLE_INCLUDE = 1106;
constexpr int ID_REPLACE = 1107;
constexpr int ID_ICON_SIZE_SLIDER = 1108;
constexpr int ID_TOGGLE_INCLUDE_ALL = 1109;
constexpr int ID_MODE_ENGINE = 1201;
constexpr int ID_MODE_WALLPAPER = 1202;
constexpr int ID_BG_SYSTEM = 1203;
constexpr int ID_BG_SOLID = 1204;
constexpr int ID_CHOOSE_COLOR = 1205;
constexpr int ID_COLOR_PREVIEW = 1206;

std::wstring LowerText(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(towlower(ch));
    });
    return value;
}

HBITMAP CreateBlankThumbnail(int width, int height) {
    HDC screen = GetDC(nullptr);
    HDC memory = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, width, height);
    HGDIOBJ old = SelectObject(memory, bitmap);
    HBRUSH brush = CreateSolidBrush(RGB(235, 235, 235));
    RECT rect{0, 0, width, height};
    FillRect(memory, &rect, brush);
    DeleteObject(brush);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
    HGDIOBJ oldPen = SelectObject(memory, pen);
    Rectangle(memory, 0, 0, width, height);
    SelectObject(memory, oldPen);
    DeleteObject(pen);
    SelectObject(memory, old);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    return bitmap;
}

bool CandidateHasOriginalPath(const DesktopObject& object, const std::wstring& originalPath) {
    const std::wstring normalized = NormalizePathForCompare(originalPath);
    return std::any_of(object.candidates.begin(), object.candidates.end(), [&](const ImageCandidate& candidate) {
        return NormalizePathForCompare(candidate.originalPath) == normalized;
    });
}

HMENU ControlId(int id) {
    return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
}

std::wstring ObjectListText(const DesktopObject& object) {
    std::wstring name = object.name;
    if (!object.includeInDesktop) {
        name += L"  [忽略]";
    }
    return name;
}

} // namespace

SettingsWindow::SettingsWindow(App* app) : app_(app) {
    LOGFONTW logFont{};
    HFONT guiFont = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    GetObjectW(guiFont, sizeof(logFont), &logFont);
    HDC dc = GetDC(nullptr);
    const int dpiY = dc ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
    if (dc) {
        ReleaseDC(nullptr, dc);
    }
    logFont.lfHeight = -MulDiv(11, dpiY, 72);
    wcscpy_s(logFont.lfFaceName, L"Segoe UI");
    logFont.lfStrikeOut = TRUE;
    strikeFont_ = CreateFontIndirectW(&logFont);
}

SettingsWindow::~SettingsWindow() {
    if (objectImages_) {
        ImageList_Destroy(objectImages_);
        objectImages_ = nullptr;
    }
    if (candidateImages_) {
        ImageList_Destroy(candidateImages_);
        candidateImages_ = nullptr;
    }
    if (strikeFont_) {
        DeleteObject(strikeFont_);
        strikeFont_ = nullptr;
    }
    if (hwnd_) {
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

bool SettingsWindow::Create(int initialPage) {
    RegisterClasses();
    page_ = std::clamp(initialPage, 0, 2);

    hwnd_ = CreateWindowExW(0,
                            L"MusukaSettingsWindow",
                            L"musuka settings",
                            WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT,
                            CW_USEDEFAULT,
                            1240,
                            780,
                            nullptr,
                            nullptr,
                            app_->Instance(),
                            this);
    if (!hwnd_) {
        return false;
    }
    CenterWindowOnScreen(hwnd_);
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    return true;
}

void SettingsWindow::ShowPage(int page) {
    page_ = std::clamp(page, 0, 2);
    if (hwnd_) {
        BuildPage();
        ShowWindow(hwnd_, SW_SHOW);
        SetForegroundWindow(hwnd_);
    } else {
        Create(page_);
    }
}

void SettingsWindow::Hide() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
    }
}

void SettingsWindow::RegisterClasses() {
    static bool registered = false;
    if (registered) {
        return;
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = SettingsWindow::WindowProc;
    wc.hInstance = app_->Instance();
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MusukaSettingsWindow";
    ::RegisterClassW(&wc);

    WNDCLASSW preview{};
    preview.lpfnWndProc = SettingsWindow::PreviewProc;
    preview.hInstance = app_->Instance();
    preview.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    preview.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    preview.lpszClassName = L"MusukaPreviewPane";
    ::RegisterClassW(&preview);

    registered = true;
}

LRESULT CALLBACK SettingsWindow::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    SettingsWindow* self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<SettingsWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    }
    if (!self) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return self->HandleMessage(message, wParam, lParam);
}

LRESULT CALLBACK SettingsWindow::PreviewProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    auto* self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_PAINT && self) {
        self->DrawPreview(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT SettingsWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        BuildPage();
        return 0;
    case WM_SIZE:
        BuildPage();
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = 1040;
        info->ptMinTrackSize.y = 680;
        return 0;
    }
    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        const int notify = HIWORD(wParam);
        if (id == ID_BROWSE_PATH) {
            OnBrowseDesktopPath();
        } else if (id == ID_NEXT) {
            if (page_ == 0) {
                OnPage1Next();
            } else if (page_ == 1) {
                OnPage2Next();
            }
        } else if (id == ID_PREV) {
            if (page_ > 0) {
                --page_;
                BuildPage();
            }
        } else if (id == ID_RUN) {
            OnRunDesktop();
        } else if (id == ID_SEARCH && notify == EN_CHANGE) {
            OnSearchChanged();
        } else if (id == ID_IMPORT_SINGLE) {
            ImportSingleImage();
        } else if (id == ID_IMPORT_FOLDER) {
            ImportImageFolder();
        } else if (id == ID_TOGGLE_INCLUDE) {
            ToggleIncludeSelected();
        } else if (id == ID_TOGGLE_INCLUDE_ALL) {
            ToggleIncludeAll();
        } else if (id == ID_REPLACE) {
            ReplaceSelectedImage();
        } else if (id == ID_MODE_ENGINE) {
            ShowInfo(hwnd_, L"Wallpaper Engine 模式暂未实现，当前版本请使用 Wallpaper 模式。");
            app_->Config().desktopMode = DesktopMode::Wallpaper;
            BuildPage();
        } else if (id == ID_MODE_WALLPAPER) {
            app_->Config().desktopMode = DesktopMode::Wallpaper;
            SaveConfigQuietly();
        } else if (id == ID_BG_SYSTEM) {
            app_->Config().backgroundSource = BackgroundSource::SystemWallpaper;
            SaveConfigQuietly();
        } else if (id == ID_BG_SOLID) {
            app_->Config().backgroundSource = BackgroundSource::SolidColor;
            SaveConfigQuietly();
        } else if (id == ID_CHOOSE_COLOR) {
            ChooseSolidColor();
        }
        return 0;
    }
    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lParam) == iconSizeSlider_) {
            OnIconSizeSliderChanged();
            return 0;
        }
        break;
    case WM_NOTIFY:
        return HandleNotify(lParam);
    case WM_DRAWITEM: {
        auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (item && item->CtlID == ID_COLOR_PREVIEW) {
            DrawColorPreview(item->hDC, item->rcItem);
            return TRUE;
        }
        break;
    }
    case WM_CLOSE:
        SaveConfigQuietly();
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        PostQuitMessage(0);
        return 0;
    case WM_NCDESTROY:
    {
        HWND oldHwnd = hwnd_;
        hwnd_ = nullptr;
        return DefWindowProcW(oldHwnd, message, wParam, lParam);
    }
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

LRESULT SettingsWindow::HandleNotify(LPARAM lParam) {
    auto* header = reinterpret_cast<NMHDR*>(lParam);
    if (!header || suppressNotifications_) {
        return 0;
    }
    if (header->idFrom == ID_OBJECT_LIST && header->code == LVN_ITEMCHANGED) {
        auto* item = reinterpret_cast<NMLISTVIEW*>(lParam);
        if ((item->uNewState & LVIS_SELECTED) != 0 && item->iItem >= 0) {
            LVITEMW lv{};
            lv.mask = LVIF_PARAM;
            lv.iItem = item->iItem;
            ListView_GetItem(objectList_, &lv);
            OnObjectSelected(static_cast<int>(lv.lParam));
        }
        return 0;
    }
    if (header->idFrom == ID_OBJECT_LIST && header->code == NM_CUSTOMDRAW) {
        return HandleCustomDraw(reinterpret_cast<NMLVCUSTOMDRAW*>(lParam));
    }
    if (header->idFrom == ID_CANDIDATE_LIST && header->code == LVN_ITEMCHANGED) {
        auto* item = reinterpret_cast<NMLISTVIEW*>(lParam);
        if ((item->uNewState & LVIS_SELECTED) != 0 && item->iItem >= 0) {
            LVITEMW lv{};
            lv.mask = LVIF_PARAM;
            lv.iItem = item->iItem;
            ListView_GetItem(candidateList_, &lv);
            selectedCandidateIndex_ = static_cast<int>(lv.lParam);
            if (previewPane_) {
                InvalidateRect(previewPane_, nullptr, TRUE);
            }
        }
        return 0;
    }
    return 0;
}

LRESULT SettingsWindow::HandleCustomDraw(NMLVCUSTOMDRAW* customDraw) {
    if (!customDraw) {
        return CDRF_DODEFAULT;
    }
    if (customDraw->nmcd.dwDrawStage == CDDS_PREPAINT) {
        return CDRF_NOTIFYITEMDRAW;
    }
    if (customDraw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
        LVITEMW item{};
        item.mask = LVIF_PARAM;
        item.iItem = static_cast<int>(customDraw->nmcd.dwItemSpec);
        ListView_GetItem(objectList_, &item);
        const int objectIndex = static_cast<int>(item.lParam);
        if (objectIndex >= 0 && objectIndex < static_cast<int>(app_->Config().objects.size())) {
            const auto& object = app_->Config().objects[static_cast<size_t>(objectIndex)];
            if (!object.includeInDesktop) {
                customDraw->clrText = RGB(140, 140, 140);
                SelectObject(customDraw->nmcd.hdc, strikeFont_);
                return CDRF_NEWFONT;
            }
        }
    }
    return CDRF_DODEFAULT;
}

void SettingsWindow::DestroyChildControls() {
    suppressNotifications_ = true;
    HWND child = GetWindow(hwnd_, GW_CHILD);
    while (child) {
        HWND next = GetWindow(child, GW_HWNDNEXT);
        DestroyWindow(child);
        child = next;
    }
    suppressNotifications_ = false;

    pathEdit_ = nullptr;
    searchEdit_ = nullptr;
    objectList_ = nullptr;
    candidateList_ = nullptr;
    previewPane_ = nullptr;
    colorPreview_ = nullptr;
    includeButton_ = nullptr;
    includeAllButton_ = nullptr;
    iconSizeSlider_ = nullptr;
    iconSizeValue_ = nullptr;

    if (objectImages_) {
        ImageList_Destroy(objectImages_);
        objectImages_ = nullptr;
    }
    if (candidateImages_) {
        ImageList_Destroy(candidateImages_);
        candidateImages_ = nullptr;
    }
}

void SettingsWindow::BuildPage() {
    if (!hwnd_) {
        return;
    }
    DestroyChildControls();
    if (page_ == 0) {
        BuildPage1();
    } else if (page_ == 1) {
        BuildPage2();
    } else {
        BuildPage3();
    }
}

HWND SettingsWindow::CreateStatic(const std::wstring& text, int x, int y, int w, int h, DWORD style) {
    HWND control = CreateWindowExW(0, L"STATIC", text.c_str(),
                                   WS_CHILD | WS_VISIBLE | style,
                                   x, y, w, h, hwnd_, nullptr, app_->Instance(), nullptr);
    ApplyDefaultFont(control);
    return control;
}

HWND SettingsWindow::CreateButton(const std::wstring& text, int id, int x, int y, int w, int h, DWORD style) {
    HWND control = CreateWindowExW(0, L"BUTTON", text.c_str(),
                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
                                   x, y, w, h, hwnd_, ControlId(id), app_->Instance(), nullptr);
    ApplyDefaultFont(control);
    return control;
}

HWND SettingsWindow::CreateEdit(const std::wstring& text, int id, int x, int y, int w, int h) {
    HWND control = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text.c_str(),
                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                   x, y, w, h, hwnd_, ControlId(id), app_->Instance(), nullptr);
    ApplyDefaultFont(control);
    return control;
}

void SettingsWindow::BuildNavigation(bool previousVisible, bool nextVisible, bool runVisible) {
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    const int y = rc.bottom - 48;
    if (previousVisible) {
        CreateButton(L"上一步", ID_PREV, rc.right - 320, y, 96, 30);
    }
    if (nextVisible) {
        CreateButton(L"下一步", ID_NEXT, rc.right - 208, y, 96, 30);
    }
    if (runVisible) {
        CreateButton(L"运行", ID_RUN, rc.right - 208, y, 96, 30);
    }
}

void SettingsWindow::BuildPage1() {
    RECT rc{};
    GetClientRect(hwnd_, &rc);

    CreateStatic(L"musuka settings", 28, 22, 280, 28, SS_LEFT);
    CreateStatic(L"第一步：选择桌面路径", 28, 62, 360, 24, SS_LEFT);
    CreateStatic(L"请选择主要桌面路径。musuka 会同时扫描该路径、当前用户桌面、公共桌面，并加入“此电脑”和“回收站”。", 28, 94, rc.right - 56, 28, SS_LEFT);

    std::wstring desktopPath = app_->Config().desktopPath;
    if (desktopPath.empty()) {
        desktopPath = GetKnownDesktopPath();
    }
    pathEdit_ = CreateEdit(desktopPath, ID_PATH_EDIT, 28, 132, rc.right - 180, 28);
    CreateButton(L"浏览...", ID_BROWSE_PATH, rc.right - 132, 132, 104, 28);
    BuildNavigation(false, true, false);
}

void SettingsWindow::BuildPage2() {
    if ((selectedObjectIndex_ < 0 ||
         selectedObjectIndex_ >= static_cast<int>(app_->Config().objects.size())) &&
        !app_->Config().objects.empty()) {
        selectedObjectIndex_ = 0;
        selectedCandidateIndex_ = app_->Config().objects.front().selectedCandidate;
    }

    RECT rc{};
    GetClientRect(hwnd_, &rc);
    const int margin = 18;
    const int top = 52;
    const int bottom = rc.bottom - 60;
    const int contentHeight = bottom - top;
    const int leftW = 360;
    const int midW = 380;
    const int rightW = rc.right - margin * 4 - leftW - midW;
    const int leftX = margin;
    const int midX = leftX + leftW + margin;
    const int rightX = midX + midW + margin;

    CreateStatic(L"第二步：文件选择和替代图片配置", margin, 18, 520, 24, SS_LEFT);

    CreateStatic(L"", leftX, top, leftW, contentHeight, SS_ETCHEDFRAME);
    CreateStatic(L"已选文件替代图片预览", leftX + 12, top + 14, leftW - 24, 26, SS_LEFT);
    previewPane_ = CreateWindowExW(WS_EX_CLIENTEDGE,
                                   L"MusukaPreviewPane",
                                   L"",
                                   WS_CHILD | WS_VISIBLE,
                                   leftX + 16,
                                   top + 50,
                                   leftW - 32,
                                   contentHeight - 68,
                                   hwnd_,
                                   nullptr,
                                   app_->Instance(),
                                   this);

    CreateStatic(L"", midX, top, midW, contentHeight, SS_ETCHEDFRAME);
    searchEdit_ = CreateEdit(searchText_, ID_SEARCH, midX + 12, top + 16, midW - 24, 26);
    objectList_ = CreateWindowExW(WS_EX_CLIENTEDGE,
                                  WC_LISTVIEWW,
                                  L"",
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                                  midX + 12,
                                  top + 52,
                                  midW - 24,
                                  contentHeight - 66,
                                  hwnd_,
                                  ControlId(ID_OBJECT_LIST),
                                  app_->Instance(),
                                  nullptr);
    ApplyDefaultFont(objectList_);
    ListView_SetExtendedListViewStyle(objectList_, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.pszText = const_cast<LPWSTR>(L"Specified Desktop File");
    column.cx = midW - 56;
    ListView_InsertColumn(objectList_, 0, &column);
    PopulateObjectList();

    CreateStatic(L"", rightX, top, rightW, contentHeight, SS_ETCHEDFRAME);
    DesktopObject* selected = SelectedObject();
    if (!selected) {
        CreateStatic(L"选定文件后，在此导入图片并配置替代图。",
                     rightX + 20, top + 24, rightW - 40, 80, SS_LEFT);
    } else {
        CreateStatic(L"替代图片", rightX + 14, top + 14, rightW - 28, 24, SS_LEFT);
        CreateButton(L"导入单张图片", ID_IMPORT_SINGLE, rightX + 14, top + 48, 130, 30);
        CreateButton(L"导入整个图片文件夹", ID_IMPORT_FOLDER, rightX + 154, top + 48, 170, 30);

        const int sizeY = top + 88;
        CreateStatic(L"桌面显示尺寸", rightX + 14, sizeY + 3, 102, 22, SS_LEFT);
        iconSizeSlider_ = CreateWindowExW(0,
                                          TRACKBAR_CLASSW,
                                          L"",
                                          WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                                          rightX + 118,
                                          sizeY,
                                          rightW - 210,
                                          30,
                                          hwnd_,
                                          ControlId(ID_ICON_SIZE_SLIDER),
                                          app_->Instance(),
                                          nullptr);
        SendMessageW(iconSizeSlider_, TBM_SETRANGE, TRUE, MAKELPARAM(kDesktopIconMinSize, kDesktopIconMaxSize));
        SendMessageW(iconSizeSlider_, TBM_SETPAGESIZE, 0, 24);
        SendMessageW(iconSizeSlider_, TBM_SETTICFREQ, 32, 0);
        SendMessageW(iconSizeSlider_, TBM_SETPOS, TRUE, std::clamp(selected->iconSize, kDesktopIconMinSize, kDesktopIconMaxSize));
        iconSizeValue_ = CreateStatic(std::to_wstring(selected->iconSize) + L" px",
                                      rightX + rightW - 82,
                                      sizeY + 3,
                                      62,
                                      22,
                                      SS_RIGHT);

        const int listX = rightX + 14;
        const int listY = top + 132;
        const int buttonW = 118;
        candidateList_ = CreateWindowExW(WS_EX_CLIENTEDGE,
                                         WC_LISTVIEWW,
                                         L"",
                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_ICON | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                                         listX,
                                         listY,
                                         rightW - 42 - buttonW,
                                         contentHeight - 150,
                                         hwnd_,
                                         ControlId(ID_CANDIDATE_LIST),
                                         app_->Instance(),
                                         nullptr);
        ApplyDefaultFont(candidateList_);
        ListView_SetExtendedListViewStyle(candidateList_, LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
        PopulateCandidateList();

        const std::wstring includeText = selected->includeInDesktop ? L"忽略" : L"带入";
        includeButton_ = CreateButton(includeText, ID_TOGGLE_INCLUDE, rightX + rightW - buttonW - 14, listY, buttonW, 32);
        includeAllButton_ = CreateButton(L"忽略全部", ID_TOGGLE_INCLUDE_ALL, rightX + rightW - buttonW - 14, listY + 42, buttonW, 32);
        CreateButton(L"替换", ID_REPLACE, rightX + rightW - buttonW - 14, listY + 84, buttonW, 32);
        UpdateSelectionDetailControls();
    }

    BuildNavigation(true, true, false);
}

void SettingsWindow::BuildPage3() {
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    const int x = 34;
    int y = 28;

    CreateStatic(L"第三步：Desktop 模式选择页面", x, y, 460, 26, SS_LEFT);
    y += 46;

    CreateButton(L"Wallpaper Engine 模式：暂未实现 / 保留选项",
                 ID_MODE_ENGINE,
                 x,
                 y,
                 420,
                 28,
                 BS_AUTORADIOBUTTON);
    y += 36;
    CreateButton(L"Wallpaper 模式",
                 ID_MODE_WALLPAPER,
                 x,
                 y,
                 260,
                 28,
                 BS_AUTORADIOBUTTON);
    CheckRadioButton(hwnd_, ID_MODE_ENGINE, ID_MODE_WALLPAPER,
                     app_->Config().desktopMode == DesktopMode::WallpaperEngine ? ID_MODE_ENGINE : ID_MODE_WALLPAPER);

    y += 52;
    CreateStatic(L"Wallpaper 模式背景来源", x, y, 320, 24, SS_LEFT);
    y += 34;
    CreateButton(L"使用当前系统静态壁纸",
                 ID_BG_SYSTEM,
                 x + 16,
                 y,
                 300,
                 28,
                 BS_AUTORADIOBUTTON);
    y += 34;
    CreateButton(L"使用 musuka 纯色背景",
                 ID_BG_SOLID,
                 x + 16,
                 y,
                 300,
                 28,
                 BS_AUTORADIOBUTTON);
    CheckRadioButton(hwnd_, ID_BG_SYSTEM, ID_BG_SOLID,
                     app_->Config().backgroundSource == BackgroundSource::SolidColor ? ID_BG_SOLID : ID_BG_SYSTEM);

    y += 44;
    CreateButton(L"选择颜色", ID_CHOOSE_COLOR, x + 16, y, 110, 30);
    colorPreview_ = CreateWindowExW(0,
                                    L"STATIC",
                                    L"",
                                    WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                                    x + 140,
                                    y,
                                    88,
                                    30,
                                    hwnd_,
                                    ControlId(ID_COLOR_PREVIEW),
                                    app_->Instance(),
                                    nullptr);

    std::wstring wallpaperPath;
    const bool hasWallpaper = TryGetSystemWallpaperPath(wallpaperPath);
    if (hasWallpaper) {
        app_->Config().systemWallpaperPath = wallpaperPath;
    }
    y += 48;
    CreateStatic(hasWallpaper ? (L"当前系统静态壁纸：" + wallpaperPath)
                              : L"当前系统静态壁纸读取失败，可改用 musuka 纯色背景。",
                 x + 16,
                 y,
                 rc.right - x * 2,
                 42,
                 SS_LEFT);

    BuildNavigation(true, false, true);
}

void SettingsWindow::OnBrowseDesktopPath() {
    const std::wstring current = pathEdit_ ? GetWindowTextString(pathEdit_) : app_->Config().desktopPath;
    const std::wstring selected = BrowseForFolder(hwnd_, L"选择桌面文件夹", current);
    if (!selected.empty() && pathEdit_) {
        SetWindowTextString(pathEdit_, selected);
    }
}

void SettingsWindow::OnPage1Next() {
    const std::wstring path = pathEdit_ ? GetWindowTextString(pathEdit_) : app_->Config().desktopPath;
    if (!DirectoryExists(path)) {
        ShowError(hwnd_, L"桌面路径不存在，请重新选择。");
        return;
    }

    app_->Config().desktopPath = path;
    DesktopScanner scanner;
    std::wstring error;
    std::wstring warning;
    if (!scanner.ScanAndPrepare(app_->Config(), error, warning)) {
        ShowError(hwnd_, error);
        return;
    }
    SaveConfigQuietly();
    if (!warning.empty()) {
        ShowInfo(hwnd_, warning);
    }
    page_ = 1;
    selectedObjectIndex_ = app_->Config().objects.empty() ? -1 : 0;
    selectedCandidateIndex_ = selectedObjectIndex_ >= 0
        ? app_->Config().objects[static_cast<size_t>(selectedObjectIndex_)].selectedCandidate
        : -1;
    BuildPage();
}

void SettingsWindow::OnPage2Next() {
    SaveConfigQuietly();
    page_ = 2;
    BuildPage();
}

void SettingsWindow::OnRunDesktop() {
    AppConfig& config = app_->Config();
    if (config.desktopMode == DesktopMode::WallpaperEngine) {
        ShowInfo(hwnd_, L"该模式暂未实现，当前版本请使用 Wallpaper 模式。");
        config.desktopMode = DesktopMode::Wallpaper;
        BuildPage();
        return;
    }

    if (config.backgroundSource == BackgroundSource::SystemWallpaper) {
        std::wstring wallpaperPath;
        if (!TryGetSystemWallpaperPath(wallpaperPath)) {
            ShowError(hwnd_, L"读取系统静态壁纸失败，请选择 musuka 纯色背景。");
            config.backgroundSource = BackgroundSource::SolidColor;
            BuildPage();
            return;
        }
        config.systemWallpaperPath = wallpaperPath;
    }

    std::wstring error;
    if (!app_->Store().Save(config, error)) {
        ShowError(hwnd_, error);
        return;
    }
    app_->ShowDesktop();
}

void SettingsWindow::OnSearchChanged() {
    searchText_ = searchEdit_ ? GetWindowTextString(searchEdit_) : L"";
    PopulateObjectList();
}

void SettingsWindow::OnObjectSelected(int objectIndex) {
    if (objectIndex < 0 || objectIndex >= static_cast<int>(app_->Config().objects.size())) {
        return;
    }
    selectedObjectIndex_ = objectIndex;
    selectedCandidateIndex_ = app_->Config().objects[static_cast<size_t>(objectIndex)].selectedCandidate;
    RefreshSelectedObjectControls();
}

void SettingsWindow::ImportSingleImage() {
    DesktopObject* object = SelectedObject();
    if (!object) {
        return;
    }
    const std::wstring path = OpenImageFileDialog(hwnd_);
    if (path.empty()) {
        return;
    }
    std::wstring error;
    if (!AddCandidateFromFile(*object, path, error)) {
        ShowError(hwnd_, error);
        return;
    }
    SaveConfigQuietly();
    RefreshSelectedObjectControls();
}

void SettingsWindow::ImportImageFolder() {
    DesktopObject* object = SelectedObject();
    if (!object) {
        return;
    }
    const std::wstring folder = BrowseForFolder(hwnd_, L"导入整个图片文件夹");
    if (folder.empty()) {
        return;
    }
    const auto images = EnumerateImageFiles(folder, false);
    if (images.empty()) {
        ShowError(hwnd_, L"该文件夹中没有支持的图片格式。当前只导入所选文件夹内的图片，不递归子目录。");
        return;
    }
    if (images.size() > 50) {
        const std::wstring message = L"该文件夹内有 " + std::to_wstring(images.size()) +
            L" 张图片。确认批量导入这些图片吗？";
        if (MessageBoxW(hwnd_, message.c_str(), L"musuka", MB_YESNO | MB_ICONQUESTION) != IDYES) {
            return;
        }
    }

    int imported = 0;
    int skipped = 0;
    std::wstring lastError;
    for (const auto& image : images) {
        if (CandidateHasOriginalPath(*object, image)) {
            ++skipped;
            continue;
        }
        std::wstring error;
        if (AddCandidateFromFile(*object, image, error)) {
            ++imported;
        } else {
            lastError = error;
        }
    }
    if (imported == 0) {
        if (skipped > 0) {
            ShowInfo(hwnd_, L"所选文件夹中的图片已经全部导入过，本次未新增候选图片。");
            return;
        }
        ShowError(hwnd_, lastError.empty() ? L"图片导入失败。" : lastError);
        return;
    }
    SaveConfigQuietly();
    RefreshSelectedObjectControls();
    if (images.size() > 1 || skipped > 0) {
        ShowInfo(hwnd_, L"已导入 " + std::to_wstring(imported) +
                        L" 张图片，跳过 " + std::to_wstring(skipped) + L" 张重复图片。");
    }
}

void SettingsWindow::ToggleIncludeSelected() {
    DesktopObject* object = SelectedObject();
    if (!object) {
        return;
    }
    object->includeInDesktop = !object->includeInDesktop;
    SaveConfigQuietly();
    UpdateObjectListRow(selectedObjectIndex_);
    UpdateSelectionDetailControls();
}

void SettingsWindow::ToggleIncludeAll() {
    auto& objects = app_->Config().objects;
    if (objects.empty()) {
        return;
    }
    const bool anyIncluded = std::any_of(objects.begin(), objects.end(), [](const DesktopObject& object) {
        return object.includeInDesktop;
    });
    for (auto& object : objects) {
        object.includeInDesktop = !anyIncluded;
    }
    SaveConfigQuietly();
    UpdateVisibleObjectRows();
    UpdateSelectionDetailControls();
}

void SettingsWindow::ReplaceSelectedImage() {
    DesktopObject* object = SelectedObject();
    if (!object || selectedCandidateIndex_ < 0 ||
        selectedCandidateIndex_ >= static_cast<int>(object->candidates.size())) {
        ShowError(hwnd_, L"请先选择一个候选图片。");
        return;
    }
    object->selectedCandidate = selectedCandidateIndex_;
    SaveConfigQuietly();
    if (previewPane_) {
        InvalidateRect(previewPane_, nullptr, TRUE);
    }
    PopulateCandidateList();
}

void SettingsWindow::OnIconSizeSliderChanged() {
    DesktopObject* object = SelectedObject();
    if (!object || !iconSizeSlider_) {
        return;
    }
    const int size = std::clamp(static_cast<int>(SendMessageW(iconSizeSlider_, TBM_GETPOS, 0, 0)),
                                kDesktopIconMinSize,
                                kDesktopIconMaxSize);
    if (object->iconSize == size) {
        return;
    }
    object->iconSize = size;
    SaveConfigQuietly();
    UpdateSelectionDetailControls();
}

void SettingsWindow::ChooseSolidColor() {
    COLORREF color = app_->Config().solidColor;
    if (musuka::ChooseSolidColor(hwnd_, color, color)) {
        app_->Config().solidColor = color;
        app_->Config().backgroundSource = BackgroundSource::SolidColor;
        SaveConfigQuietly();
        BuildPage();
    }
}

bool SettingsWindow::AddCandidateFromFile(DesktopObject& object, const std::wstring& imagePath, std::wstring& error) {
    error.clear();
    if (!IsSupportedImageFile(imagePath)) {
        error = L"图片格式不支持。当前支持 PNG/JPG/JPEG/BMP。";
        return false;
    }
    if (!ImageCanBeLoaded(imagePath)) {
        error = L"图片读取失败，可能不是有效图片文件。";
        return false;
    }
    const std::wstring objectDir = CombinePath(GetIconsDirectory(), object.id);
    std::wstring relative;
    if (!CopyFileToInternal(imagePath, objectDir, L"import", relative, error)) {
        return false;
    }

    ImageCandidate candidate;
    candidate.displayName = FileNameFromPath(imagePath);
    candidate.originalPath = imagePath;
    candidate.internalPath = relative;
    candidate.originalIcon = false;
    candidate.layerPriority = kImportedImageLayerPriority;
    object.candidates.push_back(std::move(candidate));
    selectedCandidateIndex_ = static_cast<int>(object.candidates.size()) - 1;
    return true;
}

void SettingsWindow::RefreshSelectedObjectControls() {
    if (!candidateList_ || !previewPane_) {
        BuildPage();
        return;
    }
    PopulateCandidateList();
    UpdateSelectionDetailControls();
    InvalidateRect(previewPane_, nullptr, TRUE);
}

void SettingsWindow::UpdateSelectionDetailControls() {
    DesktopObject* object = SelectedObject();
    if (includeButton_ && object) {
        SetWindowTextString(includeButton_, object->includeInDesktop ? L"忽略" : L"带入");
    }
    if (includeAllButton_) {
        const auto& objects = app_->Config().objects;
        const bool anyIncluded = std::any_of(objects.begin(), objects.end(), [](const DesktopObject& item) {
            return item.includeInDesktop;
        });
        SetWindowTextString(includeAllButton_, anyIncluded ? L"忽略全部" : L"带入全部");
    }
    if (iconSizeSlider_ && object) {
        const int size = std::clamp(object->iconSize, kDesktopIconMinSize, kDesktopIconMaxSize);
        SendMessageW(iconSizeSlider_, TBM_SETPOS, TRUE, size);
    }
    if (iconSizeValue_ && object) {
        const int size = std::clamp(object->iconSize, kDesktopIconMinSize, kDesktopIconMaxSize);
        SetWindowTextString(iconSizeValue_, std::to_wstring(size) + L" px");
    }
}

void SettingsWindow::PopulateObjectList() {
    if (!objectList_) {
        return;
    }
    suppressNotifications_ = true;
    ListView_DeleteAllItems(objectList_);
    if (objectImages_) {
        ImageList_Destroy(objectImages_);
    }
    objectImages_ = ImageList_Create(32, 32, ILC_COLOR32 | ILC_MASK, 8, 8);
    ListView_SetImageList(objectList_, objectImages_, LVSIL_SMALL);

    filteredObjects_.clear();
    const std::wstring query = LowerText(searchText_);
    auto& objects = app_->Config().objects;
    for (size_t i = 0; i < objects.size(); ++i) {
        if (!query.empty() && LowerText(objects[i].name).find(query) == std::wstring::npos) {
            continue;
        }
        filteredObjects_.push_back(static_cast<int>(i));
    }

    for (int row = 0; row < static_cast<int>(filteredObjects_.size()); ++row) {
        const int objectIndex = filteredObjects_[static_cast<size_t>(row)];
        DesktopObject& object = objects[static_cast<size_t>(objectIndex)];
        HICON icon = LoadShellIconForObject(object, false);
        int imageIndex = -1;
        if (icon) {
            imageIndex = ImageList_AddIcon(objectImages_, icon);
            DestroyIcon(icon);
        }
        std::wstring name = ObjectListText(object);
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM | LVIF_STATE;
        item.iItem = row;
        item.pszText = name.data();
        item.iImage = imageIndex;
        item.lParam = objectIndex;
        if (objectIndex == selectedObjectIndex_) {
            item.state = LVIS_SELECTED | LVIS_FOCUSED;
            item.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
        }
        ListView_InsertItem(objectList_, &item);
    }
    suppressNotifications_ = false;
}

void SettingsWindow::UpdateObjectListRow(int objectIndex) {
    if (!objectList_ ||
        objectIndex < 0 ||
        objectIndex >= static_cast<int>(app_->Config().objects.size())) {
        return;
    }

    const auto it = std::find(filteredObjects_.begin(), filteredObjects_.end(), objectIndex);
    if (it == filteredObjects_.end()) {
        return;
    }

    const int row = static_cast<int>(std::distance(filteredObjects_.begin(), it));
    std::wstring text = ObjectListText(app_->Config().objects[static_cast<size_t>(objectIndex)]);
    ListView_SetItemText(objectList_, row, 0, text.data());
    ListView_RedrawItems(objectList_, row, row);
    UpdateWindow(objectList_);
}

void SettingsWindow::UpdateVisibleObjectRows() {
    if (!objectList_) {
        return;
    }
    for (int row = 0; row < static_cast<int>(filteredObjects_.size()); ++row) {
        const int objectIndex = filteredObjects_[static_cast<size_t>(row)];
        if (objectIndex < 0 || objectIndex >= static_cast<int>(app_->Config().objects.size())) {
            continue;
        }
        std::wstring text = ObjectListText(app_->Config().objects[static_cast<size_t>(objectIndex)]);
        ListView_SetItemText(objectList_, row, 0, text.data());
        ListView_RedrawItems(objectList_, row, row);
    }
    UpdateWindow(objectList_);
}

void SettingsWindow::PopulateCandidateList() {
    if (!candidateList_) {
        return;
    }
    DesktopObject* object = SelectedObject();
    if (!object) {
        return;
    }

    suppressNotifications_ = true;
    ListView_DeleteAllItems(candidateList_);
    if (candidateImages_) {
        ImageList_Destroy(candidateImages_);
    }
    candidateImages_ = ImageList_Create(kThumbnailSize, kThumbnailSize, ILC_COLOR32 | ILC_MASK, 8, 8);
    ListView_SetImageList(candidateList_, candidateImages_, LVSIL_NORMAL);

    if (selectedCandidateIndex_ < 0 ||
        selectedCandidateIndex_ >= static_cast<int>(object->candidates.size())) {
        selectedCandidateIndex_ = object->selectedCandidate;
    }

    for (int i = 0; i < static_cast<int>(object->candidates.size()); ++i) {
        const auto& candidate = object->candidates[static_cast<size_t>(i)];
        HBITMAP thumbnail = CreateThumbnailBitmap(candidate.internalPath, kThumbnailSize, kThumbnailSize);
        if (!thumbnail) {
            thumbnail = CreateBlankThumbnail(kThumbnailSize, kThumbnailSize);
        }
        const int imageIndex = ImageList_Add(candidateImages_, thumbnail, nullptr);
        DeleteObject(thumbnail);

        std::wstring text = candidate.displayName.empty() ? FileNameFromPath(candidate.internalPath) : candidate.displayName;
        if (i == object->selectedCandidate) {
            text = L"[当前] " + text;
        }

        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM | LVIF_STATE;
        item.iItem = i;
        item.pszText = text.data();
        item.iImage = imageIndex;
        item.lParam = i;
        if (i == selectedCandidateIndex_) {
            item.state = LVIS_SELECTED | LVIS_FOCUSED;
            item.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
        }
        ListView_InsertItem(candidateList_, &item);
    }
    suppressNotifications_ = false;
}

void SettingsWindow::DrawPreview(HWND hwnd) {
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc{};
    GetClientRect(hwnd, &rc);
    HBRUSH background = CreateSolidBrush(RGB(250, 250, 250));
    FillRect(dc, &rc, background);
    DeleteObject(background);

    DesktopObject* object = SelectedObject();
    if (!object || object->selectedCandidate < 0 ||
        object->selectedCandidate >= static_cast<int>(object->candidates.size())) {
        DrawTextW(dc, L"未选中文件", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        EndPaint(hwnd, &ps);
        return;
    }

    const auto& candidate = object->candidates[static_cast<size_t>(object->selectedCandidate)];
    auto bitmap = LoadBitmapFromPath(ToAbsoluteAppPath(candidate.internalPath));
    if (!bitmap) {
        DrawTextW(dc, L"图片读取失败", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        EndPaint(hwnd, &ps);
        return;
    }

    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    DrawImageContain(graphics,
                     bitmap.get(),
                     Gdiplus::RectF(8.0f, 8.0f,
                                    static_cast<float>((rc.right - rc.left) - 16),
                                    static_cast<float>((rc.bottom - rc.top) - 16)));
    EndPaint(hwnd, &ps);
}

void SettingsWindow::DrawColorPreview(HDC dc, const RECT& rect) {
    HBRUSH brush = CreateSolidBrush(app_->Config().solidColor);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
    HGDIOBJ old = SelectObject(dc, pen);
    Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(dc, old);
    DeleteObject(pen);
}

void SettingsWindow::SaveConfigQuietly() {
    std::wstring error;
    app_->Store().Save(app_->Config(), error);
}

DesktopObject* SettingsWindow::SelectedObject() {
    if (selectedObjectIndex_ < 0 ||
        selectedObjectIndex_ >= static_cast<int>(app_->Config().objects.size())) {
        return nullptr;
    }
    return &app_->Config().objects[static_cast<size_t>(selectedObjectIndex_)];
}

const DesktopObject* SettingsWindow::SelectedObject() const {
    if (selectedObjectIndex_ < 0 ||
        selectedObjectIndex_ >= static_cast<int>(app_->Config().objects.size())) {
        return nullptr;
    }
    return &app_->Config().objects[static_cast<size_t>(selectedObjectIndex_)];
}

ImageCandidate* SettingsWindow::SelectedCandidate() {
    DesktopObject* object = SelectedObject();
    if (!object || selectedCandidateIndex_ < 0 ||
        selectedCandidateIndex_ >= static_cast<int>(object->candidates.size())) {
        return nullptr;
    }
    return &object->candidates[static_cast<size_t>(selectedCandidateIndex_)];
}

} // namespace musuka
