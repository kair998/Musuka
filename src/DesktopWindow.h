#pragma once

#include "Models.h"

#include <windows.h>
#include <objidl.h>
#include <propidl.h>
#include <gdiplus.h>

#include <memory>
#include <vector>

namespace musuka {

class App;

class DesktopWindow {
public:
    explicit DesktopWindow(App* app);
    ~DesktopWindow();

    bool Create();
    void Hide();

private:
    struct RenderItem {
        int objectIndex = -1;
        std::unique_ptr<Gdiplus::Bitmap> bitmap;
        Gdiplus::RectF rect;
        Gdiplus::RectF bounds;
        Gdiplus::RectF labelRect;
        std::wstring label;
        bool showLabel = false;
        int layerPriority = kDefaultImageLayerPriority;
    };

    struct DragOrigin {
        int objectIndex = -1;
        int x = 0;
        int y = 0;
    };

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void RegisterWindowClass();
    void PositionDesktopWindow();
    bool AttachToDesktopHost(HWND host);
    void RefreshWallpaperEngineIntegration();
    void RestoreDesktopIcons();
    void LoadAssets();
    void AutoArrangeMissingPositions();
    void RecalculateRects();
    void RecalculateItemRect(RenderItem& item);
    void Paint();
    void DrawBackground(Gdiplus::Graphics& graphics, const RECT& rc);
    void DrawItemLabel(HDC dc, const RenderItem& item, const RECT& dirtyRect);
    void DrawSelectionBox(Gdiplus::Graphics& graphics);
    int HitTest(int x, int y) const;
    int FindItemByObjectIndex(int objectIndex) const;
    bool IsObjectSelected(int objectIndex) const;
    void SetSingleSelection(int objectIndex);
    void ClearSelection();
    void SelectObjectsInBox();
    RECT CurrentSelectionBoxRect() const;
    void InvalidateRenderItem(const RenderItem& item);
    void InvalidateRenderRect(const Gdiplus::RectF& rect);
    void InvalidateSelectionBox(const RECT& rect);
    void ScaleSelectedObjects(int delta);
    void OpenObject(int objectIndex);
    void OpenContainingLocation(int objectIndex);
    void RunObjectAsAdmin(int objectIndex);
    void ShowContextMenu(int x, int y);
    void SaveConfigQuietly();

    App* app_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND desktopHost_ = nullptr;
    HWND hiddenDesktopIconList_ = nullptr;
    bool restoreDesktopIconList_ = false;
    bool wallpaperEngineMode_ = false;
    std::vector<RenderItem> items_;
    std::unique_ptr<Gdiplus::Bitmap> wallpaper_;
    int selectedObjectIndex_ = -1;
    std::vector<int> selectedObjectIndices_;
    int draggingItem_ = -1;
    POINT dragStart_{};
    std::vector<DragOrigin> dragOrigins_;
    bool movedDuringDrag_ = false;
    bool selectingBox_ = false;
    POINT selectionStart_{};
    POINT selectionCurrent_{};
};

} // namespace musuka
