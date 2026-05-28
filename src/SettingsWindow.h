#pragma once

#include "Models.h"

#include <commctrl.h>
#include <windows.h>

#include <string>
#include <vector>

namespace musuka {

class App;

class SettingsWindow {
public:
    explicit SettingsWindow(App* app);
    ~SettingsWindow();

    bool Create(int initialPage);
    void ShowPage(int page);
    void Hide();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK PreviewProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleNotify(LPARAM lParam);
    LRESULT HandleCustomDraw(NMLVCUSTOMDRAW* customDraw);

    void RegisterClasses();
    void DestroyChildControls();
    void BuildPage();
    void BuildPage1();
    void BuildPage2();
    void BuildPage3();
    void BuildNavigation(bool previousVisible, bool nextVisible, bool runVisible);

    HWND CreateStatic(const std::wstring& text, int x, int y, int w, int h, DWORD style = 0);
    HWND CreateButton(const std::wstring& text, int id, int x, int y, int w, int h, DWORD style = 0);
    HWND CreateEdit(const std::wstring& text, int id, int x, int y, int w, int h);

    void OnBrowseDesktopPath();
    void OnPage1Next();
    void OnPage2Next();
    void OnRunDesktop();
    void OnSearchChanged();
    void OnObjectSelected(int objectIndex);
    void ImportSingleImage();
    void ImportImageFolder();
    void ToggleIncludeSelected();
    void ToggleIncludeAll();
    void ReplaceSelectedImage();
    void ChooseSolidColor();
    void OnIconSizeSliderChanged();

    bool AddCandidateFromFile(DesktopObject& object, const std::wstring& imagePath, std::wstring& error);
    void RefreshSelectedObjectControls();
    void UpdateSelectionDetailControls();
    void PopulateObjectList();
    void UpdateObjectListRow(int objectIndex);
    void UpdateVisibleObjectRows();
    void PopulateCandidateList();
    void DrawPreview(HWND hwnd);
    void DrawColorPreview(HDC dc, const RECT& rect);
    void SaveConfigQuietly();

    DesktopObject* SelectedObject();
    const DesktopObject* SelectedObject() const;
    ImageCandidate* SelectedCandidate();

    App* app_ = nullptr;
    HWND hwnd_ = nullptr;
    int page_ = 0;
    int selectedObjectIndex_ = -1;
    int selectedCandidateIndex_ = -1;
    std::wstring searchText_;
    std::vector<int> filteredObjects_;

    HWND pathEdit_ = nullptr;
    HWND searchEdit_ = nullptr;
    HWND objectList_ = nullptr;
    HWND candidateList_ = nullptr;
    HWND previewPane_ = nullptr;
    HWND colorPreview_ = nullptr;
    HWND includeButton_ = nullptr;
    HWND includeAllButton_ = nullptr;
    HWND iconSizeSlider_ = nullptr;
    HWND iconSizeValue_ = nullptr;

    HIMAGELIST objectImages_ = nullptr;
    HIMAGELIST candidateImages_ = nullptr;
    HFONT strikeFont_ = nullptr;
    bool suppressNotifications_ = false;
};

} // namespace musuka
