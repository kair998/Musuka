#include "DesktopWindow.h"

#include "App.h"
#include "ImageUtil.h"
#include "Resource.h"
#include "Util.h"
#include "WinUtil.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <shellapi.h>
#include <shlobj.h>
#include <windowsx.h>

namespace fs = std::filesystem;

namespace musuka {

namespace {

constexpr int ID_CONTEXT_OPEN = 2001;
constexpr int ID_CONTEXT_OPEN_LOCATION = 2002;
constexpr int ID_CONTEXT_RUN_AS_ADMIN = 2003;
constexpr int ID_CONTEXT_RETURN_SETTINGS = 2004;
constexpr int ID_CONTEXT_EXIT = 2005;
constexpr BYTE kAlphaThreshold = 20;
constexpr int kLabelHeight = 36;
constexpr int kLabelExtraWidth = 48;
constexpr int kIconScaleStep = 24;
constexpr int kDesktopMargin = 24;
constexpr int kNativeAutoArrangeGap = 4;
constexpr int kReplacementAutoArrangeGap = 16;
constexpr int kAutoArrangeScanStep = 16;

bool ShellExecuteChecked(HWND owner,
                         const wchar_t* operation,
                         const std::wstring& file,
                         const std::wstring& parameters = {}) {
    HINSTANCE result = ShellExecuteW(owner,
                                     operation,
                                     file.c_str(),
                                     parameters.empty() ? nullptr : parameters.c_str(),
                                     nullptr,
                                     SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        ShowError(owner, L"打开失败。");
        return false;
    }
    return true;
}

std::wstring ExplorerSelectParameter(const std::wstring& path) {
    return L"/select,\"" + path + L"\"";
}

RECT PreferredDesktopBounds() {
    return RECT{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
}

RECT RectFromRectF(const Gdiplus::RectF& rect) {
    RECT result{
        static_cast<LONG>(std::floor(rect.X)) - 4,
        static_cast<LONG>(std::floor(rect.Y)) - 4,
        static_cast<LONG>(std::ceil(rect.X + rect.Width)) + 4,
        static_cast<LONG>(std::ceil(rect.Y + rect.Height)) + 4
    };
    return result;
}

bool RectFIntersectsRect(const Gdiplus::RectF& rect, const RECT& other) {
    RECT native = RectFromRectF(rect);
    RECT intersection{};
    return IntersectRect(&intersection, &native, &other) != FALSE;
}

Gdiplus::RectF UnionBounds(const Gdiplus::RectF& left, const Gdiplus::RectF& right) {
    const float x1 = std::min(left.X, right.X);
    const float y1 = std::min(left.Y, right.Y);
    const float x2 = std::max(left.X + left.Width, right.X + right.Width);
    const float y2 = std::max(left.Y + left.Height, right.Y + right.Height);
    return Gdiplus::RectF(x1, y1, x2 - x1, y2 - y1);
}

int RectWidth(const RECT& rect) {
    return std::max(0L, rect.right - rect.left);
}

int RectHeight(const RECT& rect) {
    return std::max(0L, rect.bottom - rect.top);
}

RECT InflateCopy(RECT rect, int amount) {
    InflateRect(&rect, amount, amount);
    return rect;
}

RECT PlacementBounds(const DesktopObject& object, bool showLabel, int x, int y) {
    const int size = std::clamp(object.iconSize, kDesktopIconMinSize, kDesktopIconMaxSize);
    const int labelOffset = showLabel ? kLabelExtraWidth / 2 : 0;
    const int labelHeight = showLabel ? kLabelHeight + 4 : 0;
    return RECT{
        x - labelOffset,
        y,
        x + size + labelOffset,
        y + size + labelHeight
    };
}

bool RectInside(const RECT& rect, const RECT& bounds) {
    return rect.left >= bounds.left &&
           rect.top >= bounds.top &&
           rect.right <= bounds.right &&
           rect.bottom <= bounds.bottom;
}

bool RectIntersectsAny(const RECT& rect, const std::vector<RECT>& occupied) {
    for (const auto& existing : occupied) {
        RECT intersection{};
        if (IntersectRect(&intersection, &rect, &existing)) {
            return true;
        }
    }
    return false;
}

int AutoArrangeGapForItem(bool showLabel) {
    return showLabel ? kNativeAutoArrangeGap : kReplacementAutoArrangeGap;
}

bool SameInternalPath(const std::wstring& left, const std::wstring& right) {
    if (left.empty() || right.empty()) {
        return false;
    }
    return NormalizePathForCompare(ToAbsoluteAppPath(left)) ==
           NormalizePathForCompare(ToAbsoluteAppPath(right));
}

const ImageCandidate* FindCandidateByInternalPath(const DesktopObject& object, const std::wstring& internalPath) {
    for (const auto& candidate : object.candidates) {
        if (SameInternalPath(candidate.internalPath, internalPath)) {
            return &candidate;
        }
    }
    return nullptr;
}

ImageCandidate MakeExternalImageCandidate(const std::wstring& internalPath) {
    ImageCandidate candidate;
    candidate.displayName = FileNameFromPath(internalPath);
    candidate.originalPath = internalPath;
    candidate.internalPath = internalPath;
    candidate.originalIcon = false;
    candidate.layerPriority = kDefaultImageLayerPriority;
    return candidate;
}

HICON LoadMusukaIcon(HINSTANCE instance, int width, int height) {
    return reinterpret_cast<HICON>(LoadImageW(instance,
                                             MAKEINTRESOURCEW(IDI_MUSUKA),
                                             IMAGE_ICON,
                                             width,
                                             height,
                                             LR_DEFAULTCOLOR | LR_SHARED));
}

bool TryFindPlacement(const DesktopObject& object,
                      bool showLabel,
                      const RECT& client,
                      const std::vector<RECT>& occupied,
                      int gap,
                      POINT& point) {
    const int width = RectWidth(client);
    const int height = RectHeight(client);
    const int size = std::clamp(object.iconSize, kDesktopIconMinSize, kDesktopIconMaxSize);
    const int labelOffset = showLabel ? kLabelExtraWidth / 2 : 0;
    const int labelHeight = showLabel ? kLabelHeight + 4 : 0;
    const int minX = kDesktopMargin + labelOffset;
    const int minY = kDesktopMargin;
    const int maxX = width - kDesktopMargin - size - labelOffset;
    const int maxY = height - kDesktopMargin - size - labelHeight;
    if (maxX < minX || maxY < minY) {
        return false;
    }

    for (int x = minX; x <= maxX; x += kAutoArrangeScanStep) {
        for (int y = minY; y <= maxY; y += kAutoArrangeScanStep) {
            const RECT bounds = PlacementBounds(object, showLabel, x, y);
            if (!RectInside(bounds, client)) {
                continue;
            }
            if (RectIntersectsAny(InflateCopy(bounds, gap), occupied)) {
                continue;
            }
            point = POINT{x, y};
            return true;
        }
    }
    return false;
}

} // namespace

DesktopWindow::DesktopWindow(App* app) : app_(app) {}

DesktopWindow::~DesktopWindow() {
    if (hwnd_) {
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

bool DesktopWindow::Create() {
    RegisterWindowClass();

    const RECT bounds = PreferredDesktopBounds();
    hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                            L"MusukaDesktopWindow",
                            L"musuka desktop",
                            WS_POPUP,
                            bounds.left,
                            bounds.top,
                            bounds.right - bounds.left,
                            bounds.bottom - bounds.top,
                            nullptr,
                            nullptr,
                            app_->Instance(),
                            this);
    if (!hwnd_) {
        return false;
    }
    SendMessageW(hwnd_,
                 WM_SETICON,
                 ICON_BIG,
                 reinterpret_cast<LPARAM>(LoadMusukaIcon(app_->Instance(),
                                                          GetSystemMetrics(SM_CXICON),
                                                          GetSystemMetrics(SM_CYICON))));
    SendMessageW(hwnd_,
                 WM_SETICON,
                 ICON_SMALL,
                 reinterpret_cast<LPARAM>(LoadMusukaIcon(app_->Instance(),
                                                          GetSystemMetrics(SM_CXSMICON),
                                                          GetSystemMetrics(SM_CYSMICON))));

    LoadAssets();
    RecalculateRects();
    AutoArrangeMissingPositions();
    RecalculateRects();
    PositionDesktopWindow();
    UpdateWindow(hwnd_);
    return true;
}

void DesktopWindow::Hide() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
    }
}

void DesktopWindow::RegisterWindowClass() {
    static bool registered = false;
    if (registered) {
        return;
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = DesktopWindow::WindowProc;
    wc.hInstance = app_->Instance();
    wc.hIcon = LoadMusukaIcon(app_->Instance(), GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MusukaDesktopWindow";
    wc.style = CS_DBLCLKS;
    ::RegisterClassW(&wc);
    registered = true;
}

void DesktopWindow::PositionDesktopWindow() {
    if (!hwnd_) {
        return;
    }
    const RECT bounds = PreferredDesktopBounds();
    SetWindowPos(hwnd_,
                 nullptr,
                 bounds.left,
                 bounds.top,
                 bounds.right - bounds.left,
                 bounds.bottom - bounds.top,
                 SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
}

LRESULT CALLBACK DesktopWindow::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    DesktopWindow* self = reinterpret_cast<DesktopWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<DesktopWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    }
    if (!self) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return self->HandleMessage(message, wParam, lParam);
}

LRESULT DesktopWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        Paint();
        return 0;
    case WM_SIZE:
        RecalculateRects();
        AutoArrangeMissingPositions();
        RecalculateRects();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    case WM_DISPLAYCHANGE:
        PositionDesktopWindow();
        RecalculateRects();
        AutoArrangeMissingPositions();
        RecalculateRects();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN: {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        const int itemIndex = HitTest(x, y);
        if (itemIndex >= 0) {
            draggingItem_ = itemIndex;
            const int objectIndex = items_[static_cast<size_t>(itemIndex)].objectIndex;
            if (!IsObjectSelected(objectIndex)) {
                SetSingleSelection(objectIndex);
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            dragStart_.x = x;
            dragStart_.y = y;
            dragOrigins_.clear();
            for (int selectedIndex : selectedObjectIndices_) {
                const DesktopObject& selectedObject = app_->Config().objects[static_cast<size_t>(selectedIndex)];
                dragOrigins_.push_back(DragOrigin{selectedIndex, selectedObject.x, selectedObject.y});
            }
            movedDuringDrag_ = false;
            SetCapture(hwnd_);
        } else {
            ClearSelection();
            selectingBox_ = true;
            selectionStart_.x = x;
            selectionStart_.y = y;
            selectionCurrent_ = selectionStart_;
            SetCapture(hwnd_);
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        if (draggingItem_ >= 0 && (wParam & MK_LBUTTON) != 0) {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            const int dx = x - dragStart_.x;
            const int dy = y - dragStart_.y;
            std::vector<Gdiplus::RectF> oldBounds;
            bool changed = false;
            for (const auto& origin : dragOrigins_) {
                const int renderIndex = FindItemByObjectIndex(origin.objectIndex);
                if (renderIndex >= 0) {
                    oldBounds.push_back(items_[static_cast<size_t>(renderIndex)].bounds);
                }
                DesktopObject& object = app_->Config().objects[static_cast<size_t>(origin.objectIndex)];
                const int newX = origin.x + dx;
                const int newY = origin.y + dy;
                if (object.x != newX || object.y != newY) {
                    object.x = newX;
                    object.y = newY;
                    changed = true;
                }
            }
            if (changed) {
                movedDuringDrag_ = true;
                RecalculateRects();
                for (const auto& oldRect : oldBounds) {
                    InvalidateRenderRect(oldRect);
                }
                for (const auto& origin : dragOrigins_) {
                    const int renderIndex = FindItemByObjectIndex(origin.objectIndex);
                    if (renderIndex >= 0) {
                        InvalidateRenderItem(items_[static_cast<size_t>(renderIndex)]);
                    }
                }
            }
        } else if (selectingBox_ && (wParam & MK_LBUTTON) != 0) {
            const RECT oldRect = CurrentSelectionBoxRect();
            selectionCurrent_.x = GET_X_LPARAM(lParam);
            selectionCurrent_.y = GET_Y_LPARAM(lParam);
            InvalidateSelectionBox(oldRect);
            InvalidateSelectionBox(CurrentSelectionBoxRect());
        }
        return 0;
    case WM_MOUSEWHEEL:
        ScaleSelectedObjects(GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;
    case WM_LBUTTONUP:
        if (selectingBox_) {
            selectionCurrent_.x = GET_X_LPARAM(lParam);
            selectionCurrent_.y = GET_Y_LPARAM(lParam);
            SelectObjectsInBox();
            selectingBox_ = false;
            ReleaseCapture();
            InvalidateRect(hwnd_, nullptr, FALSE);
        } else if (draggingItem_ >= 0) {
            ReleaseCapture();
            if (movedDuringDrag_) {
                SaveConfigQuietly();
            }
            draggingItem_ = -1;
            movedDuringDrag_ = false;
            dragOrigins_.clear();
        }
        return 0;
    case WM_LBUTTONDBLCLK: {
        const int itemIndex = HitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        if (itemIndex >= 0) {
            SetSingleSelection(items_[static_cast<size_t>(itemIndex)].objectIndex);
            OpenObject(selectedObjectIndex_);
        }
        return 0;
    }
    case WM_RBUTTONUP: {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        const int itemIndex = HitTest(x, y);
        if (itemIndex >= 0) {
            const int objectIndex = items_[static_cast<size_t>(itemIndex)].objectIndex;
            if (!IsObjectSelected(objectIndex)) {
                SetSingleSelection(objectIndex);
            } else {
                selectedObjectIndex_ = objectIndex;
            }
        } else {
            ClearSelection();
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
        POINT point{x, y};
        ClientToScreen(hwnd_, &point);
        ShowContextMenu(point.x, point.y);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_CONTEXT_OPEN:
            if (selectedObjectIndex_ >= 0) {
                OpenObject(selectedObjectIndex_);
            }
            break;
        case ID_CONTEXT_OPEN_LOCATION:
            if (selectedObjectIndex_ >= 0) {
                OpenContainingLocation(selectedObjectIndex_);
            }
            break;
        case ID_CONTEXT_RUN_AS_ADMIN:
            if (selectedObjectIndex_ >= 0) {
                RunObjectAsAdmin(selectedObjectIndex_);
            }
            break;
        case ID_CONTEXT_RETURN_SETTINGS:
            SaveConfigQuietly();
            app_->ReturnToSettings();
            break;
        case ID_CONTEXT_EXIT:
            app_->Exit();
            break;
        }
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            SaveConfigQuietly();
            app_->ReturnToSettings();
            return 0;
        }
        break;
    case WM_CLOSE:
        SaveConfigQuietly();
        app_->Exit();
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

void DesktopWindow::LoadAssets() {
    items_.clear();
    wallpaper_.reset();

    AppConfig& config = app_->Config();
    if (config.backgroundSource == BackgroundSource::SystemWallpaper && !config.systemWallpaperPath.empty()) {
        wallpaper_ = LoadBitmapFromPath(config.systemWallpaperPath);
    }

    for (int i = 0; i < static_cast<int>(config.objects.size()); ++i) {
        DesktopObject& object = config.objects[static_cast<size_t>(i)];
        if (!object.includeInDesktop) {
            continue;
        }

        ImageCandidate externalCandidate;
        const ImageCandidate* selectedCandidate = nullptr;
        if (!object.selectedImageInternalPath.empty()) {
            selectedCandidate = FindCandidateByInternalPath(object, object.selectedImageInternalPath);
            if (!selectedCandidate && FileExists(ToAbsoluteAppPath(object.selectedImageInternalPath))) {
                externalCandidate = MakeExternalImageCandidate(object.selectedImageInternalPath);
                selectedCandidate = &externalCandidate;
            }
        }
        if (!selectedCandidate &&
            object.selectedCandidate >= 0 &&
            object.selectedCandidate < static_cast<int>(object.candidates.size())) {
            selectedCandidate = &object.candidates[static_cast<size_t>(object.selectedCandidate)];
        }
        if (!selectedCandidate && !object.candidates.empty()) {
            selectedCandidate = &object.candidates.front();
        }
        if (!selectedCandidate) {
            continue;
        }

        std::unique_ptr<Gdiplus::Bitmap> bitmap =
            LoadBitmapFromPath(ToAbsoluteAppPath(selectedCandidate->internalPath));
        if (!bitmap) {
            for (const auto& candidate : object.candidates) {
                bitmap = LoadBitmapFromPath(ToAbsoluteAppPath(candidate.internalPath));
                if (bitmap) {
                    selectedCandidate = &candidate;
                    break;
                }
            }
        }
        if (!bitmap) {
            continue;
        }

        RenderItem item;
        item.objectIndex = i;
        item.showLabel = selectedCandidate->originalIcon;
        item.label = object.name;
        item.layerPriority = selectedCandidate->layerPriority;
        item.bitmap = std::move(bitmap);
        items_.push_back(std::move(item));
    }
    std::stable_sort(items_.begin(), items_.end(), [](const RenderItem& left, const RenderItem& right) {
        if (left.layerPriority != right.layerPriority) {
            return left.layerPriority < right.layerPriority;
        }
        return left.objectIndex < right.objectIndex;
    });
}

void DesktopWindow::AutoArrangeMissingPositions() {
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    if (RectWidth(rc) <= kDesktopMargin * 2 || RectHeight(rc) <= kDesktopMargin * 2) {
        return;
    }

    RecalculateRects();

    std::vector<RECT> occupied;
    std::vector<int> toPlace;
    bool changed = false;

    for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        const auto& item = items_[static_cast<size_t>(i)];
        DesktopObject& object = app_->Config().objects[static_cast<size_t>(item.objectIndex)];
        const RECT bounds = RectFromRectF(item.bounds);
        const RECT paddedBounds = InflateCopy(bounds, AutoArrangeGapForItem(item.showLabel));
        if (object.x < 0 ||
            object.y < 0 ||
            !RectInside(bounds, rc) ||
            RectIntersectsAny(paddedBounds, occupied)) {
            toPlace.push_back(i);
        } else {
            occupied.push_back(paddedBounds);
        }
    }

    if (toPlace.size() > std::max<size_t>(1, items_.size() / 5)) {
        toPlace.clear();
        occupied.clear();
        for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
            toPlace.push_back(i);
        }
    }

    std::stable_sort(toPlace.begin(), toPlace.end(), [&](int left, int right) {
        const auto& leftItem = items_[static_cast<size_t>(left)];
        const auto& rightItem = items_[static_cast<size_t>(right)];
        if (leftItem.showLabel != rightItem.showLabel) {
            return leftItem.showLabel;
        }
        return leftItem.objectIndex < rightItem.objectIndex;
    });

    for (int itemIndex : toPlace) {
        auto& item = items_[static_cast<size_t>(itemIndex)];
        DesktopObject& object = app_->Config().objects[static_cast<size_t>(item.objectIndex)];
        const int originalX = object.x;
        const int originalY = object.y;
        const int originalSize = std::clamp(object.iconSize, kDesktopIconMinSize, kDesktopIconMaxSize);
        const int gap = AutoArrangeGapForItem(item.showLabel);

        std::vector<int> sizes;
        for (int size = originalSize; size > kDesktopIconMinSize; size -= kIconScaleStep) {
            sizes.push_back(size);
        }
        sizes.push_back(kDesktopIconMinSize);

        bool placed = false;
        POINT placement{};
        for (int size : sizes) {
            object.iconSize = size;
            if (TryFindPlacement(object, item.showLabel, rc, occupied, gap, placement)) {
                placed = true;
                break;
            }
        }

        if (!placed) {
            object.iconSize = originalSize;
            continue;
        }

        object.x = placement.x;
        object.y = placement.y;
        occupied.push_back(InflateCopy(PlacementBounds(object, item.showLabel, object.x, object.y),
                                       gap));
        if (object.x != originalX || object.y != originalY || object.iconSize != originalSize) {
            changed = true;
        }
    }

    if (changed) {
        SaveConfigQuietly();
    }
}

void DesktopWindow::RecalculateRects() {
    for (auto& item : items_) {
        RecalculateItemRect(item);
    }
}

void DesktopWindow::RecalculateItemRect(RenderItem& item) {
    DesktopObject& object = app_->Config().objects[static_cast<size_t>(item.objectIndex)];
    object.iconSize = std::clamp(object.iconSize, kDesktopIconMinSize, kDesktopIconMaxSize);
    const float iconSize = static_cast<float>(object.iconSize);
    item.rect = CalculateContainRect(item.bitmap.get(),
                                     static_cast<float>(object.x),
                                     static_cast<float>(object.y),
                                     iconSize,
                                     iconSize);
    item.bounds = Gdiplus::RectF(static_cast<float>(object.x),
                                 static_cast<float>(object.y),
                                 iconSize,
                                 iconSize);
    if (item.showLabel) {
        item.labelRect = Gdiplus::RectF(static_cast<float>(object.x - kLabelExtraWidth / 2),
                                        static_cast<float>(object.y + object.iconSize + 4),
                                        static_cast<float>(object.iconSize + kLabelExtraWidth),
                                        static_cast<float>(kLabelHeight));
        item.bounds = UnionBounds(item.bounds, item.labelRect);
    } else {
        item.labelRect = Gdiplus::RectF();
    }
}

void DesktopWindow::Paint() {
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(hwnd_, &ps);
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    RECT dirty = ps.rcPaint;
    if (dirty.right <= dirty.left || dirty.bottom <= dirty.top) {
        dirty = rc;
    }
    const int dirtyWidth = dirty.right - dirty.left;
    const int dirtyHeight = dirty.bottom - dirty.top;

    HDC memory = CreateCompatibleDC(dc);
    HBITMAP buffer = CreateCompatibleBitmap(dc, dirtyWidth, dirtyHeight);
    HGDIOBJ oldBitmap = SelectObject(memory, buffer);

    Gdiplus::Graphics graphics(memory);
    graphics.TranslateTransform(static_cast<Gdiplus::REAL>(-dirty.left),
                                static_cast<Gdiplus::REAL>(-dirty.top));
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    DrawBackground(graphics, rc);

    for (const auto& item : items_) {
        if (!RectFIntersectsRect(item.bounds, dirty)) {
            continue;
        }
        graphics.DrawImage(item.bitmap.get(), item.rect);
        if (IsObjectSelected(item.objectIndex)) {
            Gdiplus::Pen pen(Gdiplus::Color(210, 40, 120, 230), 2.0f);
            graphics.DrawRectangle(&pen, item.rect);
        }
    }
    DrawSelectionBox(graphics);
    graphics.Flush();

    for (const auto& item : items_) {
        if (item.showLabel && RectFIntersectsRect(item.labelRect, dirty)) {
            DrawItemLabel(memory, item, dirty);
        }
    }

    BitBlt(dc, dirty.left, dirty.top, dirtyWidth, dirtyHeight, memory, 0, 0, SRCCOPY);
    SelectObject(memory, oldBitmap);
    DeleteObject(buffer);
    DeleteDC(memory);
    EndPaint(hwnd_, &ps);
}

void DesktopWindow::DrawBackground(Gdiplus::Graphics& graphics, const RECT& rc) {
    const Gdiplus::RectF bounds(0.0f,
                                0.0f,
                                static_cast<float>(rc.right - rc.left),
                                static_cast<float>(rc.bottom - rc.top));
    AppConfig& config = app_->Config();
    if (config.backgroundSource == BackgroundSource::SystemWallpaper && wallpaper_) {
        DrawImageCover(graphics, wallpaper_.get(), bounds);
        return;
    }

    Gdiplus::Color color(255,
                         GetRValue(config.solidColor),
                         GetGValue(config.solidColor),
                         GetBValue(config.solidColor));
    graphics.Clear(color);
}

void DesktopWindow::DrawItemLabel(HDC dc, const RenderItem& item, const RECT& dirtyRect) {
    RECT labelRect = RectFromRectF(item.labelRect);
    labelRect.left -= dirtyRect.left;
    labelRect.right -= dirtyRect.left;
    labelRect.top -= dirtyRect.top;
    labelRect.bottom -= dirtyRect.top;

    if (IsObjectSelected(item.objectIndex)) {
        HBRUSH brush = CreateSolidBrush(RGB(45, 95, 175));
        FillRect(dc, &labelRect, brush);
        DeleteObject(brush);
    }

    SetBkMode(dc, TRANSPARENT);
    HFONT font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    HGDIOBJ oldFont = SelectObject(dc, font);
    RECT shadowRect = labelRect;
    OffsetRect(&shadowRect, 1, 1);
    SetTextColor(dc, RGB(0, 0, 0));
    DrawTextW(dc,
              item.label.c_str(),
              -1,
              &shadowRect,
              DT_CENTER | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);
    SetTextColor(dc, RGB(255, 255, 255));
    DrawTextW(dc,
              item.label.c_str(),
              -1,
              &labelRect,
              DT_CENTER | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);
    SelectObject(dc, oldFont);
}

void DesktopWindow::DrawSelectionBox(Gdiplus::Graphics& graphics) {
    if (!selectingBox_) {
        return;
    }
    const RECT rect = CurrentSelectionBoxRect();
    const Gdiplus::RectF box(static_cast<Gdiplus::REAL>(rect.left),
                             static_cast<Gdiplus::REAL>(rect.top),
                             static_cast<Gdiplus::REAL>(rect.right - rect.left),
                             static_cast<Gdiplus::REAL>(rect.bottom - rect.top));
    Gdiplus::SolidBrush brush(Gdiplus::Color(44, 78, 136, 210));
    Gdiplus::Pen pen(Gdiplus::Color(180, 70, 130, 220), 1.0f);
    graphics.FillRectangle(&brush, box);
    graphics.DrawRectangle(&pen, box);
}

int DesktopWindow::HitTest(int x, int y) const {
    for (int i = static_cast<int>(items_.size()) - 1; i >= 0; --i) {
        const RenderItem& item = items_[static_cast<size_t>(i)];
        if (AlphaHitTest(item.bitmap.get(), item.rect, x, y, kAlphaThreshold)) {
            return i;
        }
        if (item.showLabel &&
            x >= item.labelRect.X &&
            y >= item.labelRect.Y &&
            x < item.labelRect.X + item.labelRect.Width &&
            y < item.labelRect.Y + item.labelRect.Height) {
            return i;
        }
    }
    return -1;
}

int DesktopWindow::FindItemByObjectIndex(int objectIndex) const {
    for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        if (items_[static_cast<size_t>(i)].objectIndex == objectIndex) {
            return i;
        }
    }
    return -1;
}

bool DesktopWindow::IsObjectSelected(int objectIndex) const {
    return std::find(selectedObjectIndices_.begin(), selectedObjectIndices_.end(), objectIndex) !=
        selectedObjectIndices_.end();
}

void DesktopWindow::SetSingleSelection(int objectIndex) {
    selectedObjectIndex_ = objectIndex;
    selectedObjectIndices_.clear();
    if (objectIndex >= 0) {
        selectedObjectIndices_.push_back(objectIndex);
    }
}

void DesktopWindow::ClearSelection() {
    selectedObjectIndex_ = -1;
    selectedObjectIndices_.clear();
}

RECT DesktopWindow::CurrentSelectionBoxRect() const {
    RECT rect{
        std::min(selectionStart_.x, selectionCurrent_.x),
        std::min(selectionStart_.y, selectionCurrent_.y),
        std::max(selectionStart_.x, selectionCurrent_.x),
        std::max(selectionStart_.y, selectionCurrent_.y)
    };
    return rect;
}

void DesktopWindow::SelectObjectsInBox() {
    ClearSelection();
    const RECT rect = CurrentSelectionBoxRect();
    if ((rect.right - rect.left) < 4 || (rect.bottom - rect.top) < 4) {
        return;
    }
    for (const auto& item : items_) {
        RECT native = RectFromRectF(item.bounds);
        RECT intersection{};
        if (IntersectRect(&intersection, &native, &rect)) {
            selectedObjectIndices_.push_back(item.objectIndex);
        }
    }
    if (!selectedObjectIndices_.empty()) {
        selectedObjectIndex_ = selectedObjectIndices_.front();
    }
}

void DesktopWindow::InvalidateRenderItem(const RenderItem& item) {
    InvalidateRenderRect(item.bounds);
}

void DesktopWindow::InvalidateRenderRect(const Gdiplus::RectF& rect) {
    RECT native = RectFromRectF(rect);
    InvalidateRect(hwnd_, &native, FALSE);
}

void DesktopWindow::InvalidateSelectionBox(const RECT& rect) {
    RECT expanded = rect;
    InflateRect(&expanded, 3, 3);
    InvalidateRect(hwnd_, &expanded, FALSE);
}

void DesktopWindow::ScaleSelectedObjects(int delta) {
    if (selectedObjectIndices_.empty() || delta == 0) {
        return;
    }
    const int direction = delta > 0 ? 1 : -1;
    bool changed = false;
    std::vector<Gdiplus::RectF> oldBounds;
    for (int objectIndex : selectedObjectIndices_) {
        const int renderIndex = FindItemByObjectIndex(objectIndex);
        if (renderIndex >= 0) {
            oldBounds.push_back(items_[static_cast<size_t>(renderIndex)].bounds);
        }
        DesktopObject& object = app_->Config().objects[static_cast<size_t>(objectIndex)];
        const int oldSize = object.iconSize;
        object.iconSize = std::clamp(object.iconSize + direction * kIconScaleStep,
                                     kDesktopIconMinSize,
                                     kDesktopIconMaxSize);
        changed = changed || object.iconSize != oldSize;
    }
    if (!changed) {
        return;
    }
    RecalculateRects();
    for (const auto& oldRect : oldBounds) {
        InvalidateRenderRect(oldRect);
    }
    for (int objectIndex : selectedObjectIndices_) {
        const int renderIndex = FindItemByObjectIndex(objectIndex);
        if (renderIndex >= 0) {
            InvalidateRenderItem(items_[static_cast<size_t>(renderIndex)]);
        }
    }
    SaveConfigQuietly();
}

void DesktopWindow::OpenObject(int objectIndex) {
    if (objectIndex < 0 || objectIndex >= static_cast<int>(app_->Config().objects.size())) {
        return;
    }
    const DesktopObject& object = app_->Config().objects[static_cast<size_t>(objectIndex)];
    if (!object.includeInDesktop) {
        return;
    }

    if (object.type == DesktopObjectType::ThisPC || object.type == DesktopObjectType::RecycleBin) {
        ShellExecuteChecked(hwnd_, L"open", OpenShellIdForObject(object));
        return;
    }
    if (object.path.empty()) {
        return;
    }
    ShellExecuteChecked(hwnd_, L"open", object.path);
}

void DesktopWindow::OpenContainingLocation(int objectIndex) {
    if (objectIndex < 0 || objectIndex >= static_cast<int>(app_->Config().objects.size())) {
        return;
    }
    const DesktopObject& object = app_->Config().objects[static_cast<size_t>(objectIndex)];
    if (object.type == DesktopObjectType::ThisPC || object.type == DesktopObjectType::RecycleBin) {
        OpenObject(objectIndex);
        return;
    }
    ShellExecuteChecked(hwnd_, L"open", L"explorer.exe", ExplorerSelectParameter(object.path));
}

void DesktopWindow::RunObjectAsAdmin(int objectIndex) {
    if (objectIndex < 0 || objectIndex >= static_cast<int>(app_->Config().objects.size())) {
        return;
    }
    const DesktopObject& object = app_->Config().objects[static_cast<size_t>(objectIndex)];
    if (object.type == DesktopObjectType::ThisPC || object.type == DesktopObjectType::RecycleBin) {
        ShowError(hwnd_, L"该系统对象不支持以管理员身份运行。");
        return;
    }
    if (object.path.empty() || !FileExists(object.path)) {
        ShowError(hwnd_, L"目标文件不存在，无法以管理员身份运行。");
        return;
    }
    // Only allow admin elevation for known executable types to prevent
    // a tampered config from tricking the user into elevating arbitrary files.
    // .lnk is intentionally excluded: a desktop shortcut can point anywhere,
    // and resolving its real target requires COM (IShellLinkW) which adds
    // complexity without guaranteeing trustworthiness.
    const std::wstring ext = ExtensionLower(object.path);
    const bool isExecutable = ext == L".exe" || ext == L".bat" || ext == L".cmd" ||
                              ext == L".ps1" || ext == L".vbs" || ext == L".vbe" ||
                              ext == L".js"  || ext == L".wsf" || ext == L".msc";
    if (!isExecutable) {
        ShowError(hwnd_, L"该文件类型不支持以管理员身份运行。");
        return;
    }
    // Verify the target file actually resides under a REAL desktop directory
    // (not config.desktopPath, which is untrusted user input).
    // Use SHGetKnownFolderPath to get trusted system desktop paths:
    // both the user's desktop and the public desktop.
    {
        auto getKnown = [](REFKNOWNFOLDERID folderId) -> std::wstring {
            PWSTR path = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(folderId, 0, nullptr, &path))) {
                std::wstring result(path);
                CoTaskMemFree(path);
                return result;
            }
            return L"";
        };
        const std::vector<std::wstring> trustedRoots = {
            getKnown(FOLDERID_Desktop),
            getKnown(FOLDERID_PublicDesktop),
        };
        std::wstring targetDir = NormalizePathForCompare(ParentDirectory(object.path));
        bool found = false;
        for (const auto& root : trustedRoots) {
            if (root.empty()) continue;
            std::wstring desktopRoot = NormalizePathForCompare(root);
            if (!desktopRoot.empty() && desktopRoot.back() != L'\\' && desktopRoot.back() != L'/') {
                desktopRoot += L'\\';
            }
            if (targetDir.size() >= desktopRoot.size() &&
                targetDir.compare(0, desktopRoot.size(), desktopRoot) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            ShowError(hwnd_, L"目标文件不在系统桌面目录下，已拒绝以管理员身份运行。");
            return;
        }
    }
    // Show full target path and real filename so user can verify before elevating.
    {
        std::wstring realName = FileNameFromPath(object.path);
        // If config name differs from filesystem name, show both to detect spoofing.
        std::wstring msg = L"即将以管理员身份运行以下文件：\n\n";
        msg += L"路径： " + object.path + L"\n";
        if (object.name != realName) {
            msg += L"配置名称：" + object.name + L"\n";
            msg += L"实际文件名：" + realName + L"\n";
        }
        msg += L"\n请确认这是您想要运行的程序。";
        if (MessageBoxW(hwnd_, msg.c_str(), L"musuka — 管理员权限确认",
                        MB_YESNO | MB_ICONWARNING) != IDYES) {
            return;
        }
    }
    ShellExecuteChecked(hwnd_, L"runas", object.path);
}

void DesktopWindow::ShowContextMenu(int x, int y) {
    HMENU menu = CreatePopupMenu();
    const bool hasSelection = selectedObjectIndex_ >= 0;
    bool canRunAsAdmin = false;
    if (hasSelection && selectedObjectIndex_ < static_cast<int>(app_->Config().objects.size())) {
        const DesktopObject& object = app_->Config().objects[static_cast<size_t>(selectedObjectIndex_)];
        const bool isSystemObject = object.type == DesktopObjectType::ThisPC ||
                                   object.type == DesktopObjectType::RecycleBin;
        if (!isSystemObject && !object.path.empty()) {
            const std::wstring ext = ExtensionLower(object.path);
            canRunAsAdmin = ext == L".exe" || ext == L".bat" || ext == L".cmd" ||
                             ext == L".ps1" || ext == L".vbs" || ext == L".vbe" ||
                             ext == L".js"  || ext == L".wsf" || ext == L".msc";
        }
    }
    AppendMenuW(menu, MF_STRING | (hasSelection ? MF_ENABLED : MF_GRAYED), ID_CONTEXT_OPEN, L"打开");
    AppendMenuW(menu, MF_STRING | (hasSelection ? MF_ENABLED : MF_GRAYED), ID_CONTEXT_OPEN_LOCATION, L"打开所在位置");
    AppendMenuW(menu,
                MF_STRING | (canRunAsAdmin ? MF_ENABLED : MF_GRAYED),
                ID_CONTEXT_RUN_AS_ADMIN,
                L"以管理员身份运行");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_CONTEXT_RETURN_SETTINGS, L"返回 settings");
    AppendMenuW(menu, MF_STRING, ID_CONTEXT_EXIT, L"退出 musuka");
    const UINT command = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD, x, y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
    switch (command) {
    case ID_CONTEXT_OPEN:
        if (selectedObjectIndex_ >= 0) {
            OpenObject(selectedObjectIndex_);
        }
        break;
    case ID_CONTEXT_OPEN_LOCATION:
        if (selectedObjectIndex_ >= 0) {
            OpenContainingLocation(selectedObjectIndex_);
        }
        break;
    case ID_CONTEXT_RUN_AS_ADMIN:
        if (selectedObjectIndex_ >= 0) {
            RunObjectAsAdmin(selectedObjectIndex_);
        }
        break;
    case ID_CONTEXT_RETURN_SETTINGS:
        SaveConfigQuietly();
        app_->ReturnToSettings();
        break;
    case ID_CONTEXT_EXIT:
        app_->Exit();
        break;
    }
}

void DesktopWindow::SaveConfigQuietly() {
    std::wstring error;
    app_->Store().Save(app_->Config(), error);
}

} // namespace musuka
