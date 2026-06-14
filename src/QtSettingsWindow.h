#pragma once

#include "Models.h"
#include "SettingsLocalization.h"

#include <QMainWindow>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QListWidget>
#include <QStackedWidget>
#include <QTimer>

#include <string>
#include <vector>

namespace musuka {

class App;

class QtSettingsWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit QtSettingsWindow(App* app);
    ~QtSettingsWindow() override = default;

    void showPage(int page);
    void hide();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onBrowsePath();
    void onPage1Next();
    void onPage2Next();
    void onRunDesktop();
    void onSearchChanged();
    void onObjectSelectionChanged();
    void onImportSingle();
    void onImportFolder();
    void onToggleInclude();
    void onToggleIncludeAll();
    void onReplaceSelected();
    void onIconSizeSliderChanged();
    void onModeEngineToggled(bool checked);
    void onModeDesktopStaticCompatibilityToggled(bool checked);
    void onModeStaticVirtualDesktopToggled(bool checked);
    void onBgSystemToggled(bool checked);
    void onBgSolidToggled(bool checked);
    void onChooseColor();
    void onLanguageChanged(int index);

private:
    void buildUi();
    void rebuildUi();
    void resetWidgetPointers();
    QWidget* buildPage1();
    QWidget* buildPage2();
    QWidget* buildPage3();
    void updateNavigation();

    void populateObjectList();
    void populateCandidateList();
    void populateDefaultImageList();
    void refreshSelectedObjectControls();
    void updateSelectionDetailControls();
    void loadDefaultImageCandidates();
    void selectCurrentCandidateForObject(const DesktopObject& object);
    int findDefaultImageCandidateByPath(const std::wstring& internalPath) const;
    bool resolveCurrentCandidate(const DesktopObject& object, ImageCandidate& outCandidate) const;
    bool addCandidateFromFile(DesktopObject& object, const std::wstring& imagePath, std::wstring& error);
    void saveConfigQuietly();
    void flushConfigSave();
    void drawPreview();
    void applyStyleSheet();
    QString text(SettingsStringId id) const;
    QString localizedMessage(const std::wstring& message) const;

    void onObjectSelectedImpl(int objectIndex);
    void updateObjectListItem(int objectIndex);
    void updateVisibleObjectItems();

    DesktopObject* selectedObject();
    const DesktopObject* selectedObject() const;
    ImageCandidate* selectedCandidate();

    static QString toQString(const std::wstring& value);
    static std::wstring fromString(const QString& value);

    App* app_ = nullptr;
    int currentPage_ = 0;
    int selectedObjectIndex_ = -1;
    int selectedCandidateIndex_ = -1;
    int selectedDefaultImageIndex_ = -1;
    bool selectedDefaultImage_ = false;
    std::wstring searchText_;
    std::vector<int> filteredObjects_;
    std::vector<ImageCandidate> defaultImageCandidates_;
    QTimer* configSaveTimer_ = nullptr;

    QStackedWidget* stackedWidget_ = nullptr;
    QComboBox* languageCombo_ = nullptr;

    // Page 1 widgets
    QLineEdit* pathEdit_ = nullptr;
    QPushButton* browseButton_ = nullptr;

    // Page 2 widgets
    QLabel* previewLabel_ = nullptr;
    QLineEdit* searchEdit_ = nullptr;
    QListWidget* objectList_ = nullptr;
    QListWidget* candidateList_ = nullptr;
    QListWidget* defaultImageList_ = nullptr;
    QPushButton* includeButton_ = nullptr;
    QPushButton* includeAllButton_ = nullptr;
    QPushButton* replaceButton_ = nullptr;
    QPushButton* importSingleButton_ = nullptr;
    QPushButton* importFolderButton_ = nullptr;
    QSlider* iconSizeSlider_ = nullptr;
    QLabel* iconSizeValueLabel_ = nullptr;

    // Page 3 widgets
    QRadioButton* modeEngineRadio_ = nullptr;
    QRadioButton* modeDesktopStaticCompatibilityRadio_ = nullptr;
    QRadioButton* modeStaticVirtualDesktopRadio_ = nullptr;
    QWidget* staticWallpaperOptions_ = nullptr;
    QRadioButton* bgSystemRadio_ = nullptr;
    QRadioButton* bgSolidRadio_ = nullptr;
    QPushButton* chooseColorButton_ = nullptr;
    QLabel* colorPreviewLabel_ = nullptr;
    QLabel* wallpaperInfoLabel_ = nullptr;

    // Navigation buttons (shared)
    QPushButton* prevButton_ = nullptr;
    QPushButton* nextButton_ = nullptr;
    QPushButton* runButton_ = nullptr;
};

} // namespace musuka
