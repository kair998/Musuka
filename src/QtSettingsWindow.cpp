#include "QtSettingsWindow.h"
#include "App.h"
#include "DesktopScanner.h"
#include "ImageUtil.h"
#include "SettingsLocalization.h"
#include "Util.h"
#include "WinUtil.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QColorDialog>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QImageReader>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPixmapCache>
#include <QScreen>
#include <QScrollBar>
#include <QSizePolicy>
#include <QStyle>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cwctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace musuka {

namespace {

constexpr int kPreviewMargin = 8;
constexpr int kQtThumbnailSize = 112;
constexpr int kQtThumbnailRenderSize = kQtThumbnailSize * 2;
constexpr int kConfigSaveDelayMs = 250;
constexpr uintmax_t kMaxImageFileSize = 10 * 1024 * 1024;
constexpr int kMaxImageDimension = 8192;
constexpr qint64 kMaxImagePixels = 16777216;

std::wstring LowerText(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(towlower(ch));
    });
    return value;
}

bool CandidateHasOriginalPath(const DesktopObject& object, const std::wstring& originalPath) {
    const std::wstring normalized = NormalizePathForCompare(originalPath);
    return std::any_of(object.candidates.begin(), object.candidates.end(), [&](const ImageCandidate& candidate) {
        return NormalizePathForCompare(candidate.originalPath) == normalized;
    });
}

bool SameInternalPath(const std::wstring& left, const std::wstring& right) {
    if (left.empty() || right.empty()) {
        return false;
    }
    return NormalizePathForCompare(ToAbsoluteAppPath(left)) ==
           NormalizePathForCompare(ToAbsoluteAppPath(right));
}

QString ObjectListText(const DesktopObject& object, SettingsLanguage language) {
    QString name;
    if (object.type == DesktopObjectType::ThisPC) {
        name = SettingsString(language, SettingsStringId::ThisPC);
    } else if (object.type == DesktopObjectType::RecycleBin) {
        name = SettingsString(language, SettingsStringId::RecycleBin);
    } else {
        name = QString::fromWCharArray(object.name.c_str());
    }
    if (!object.includeInDesktop) {
        name += SettingsString(language, SettingsStringId::IgnoredSuffix);
    }
    return name;
}

QPixmap LoadValidatedPixmap(const std::wstring& imagePath, const QSize& targetSize) {
    const std::wstring absolutePath = ToAbsoluteAppPath(imagePath);
    if (absolutePath.empty() || !targetSize.isValid() || targetSize.isEmpty()) {
        return QPixmap();
    }

    std::error_code ec;
    const fs::path path(absolutePath);
    const uintmax_t fileSize = fs::file_size(path, ec);
    if (ec || fileSize > kMaxImageFileSize) {
        return QPixmap();
    }
    const auto lastWriteTime = fs::last_write_time(path, ec);
    const qint64 lastWriteStamp = ec
        ? 0
        : static_cast<qint64>(lastWriteTime.time_since_epoch().count());
    const QString cacheKey = QStringLiteral("musuka:%1:%2:%3:%4x%5")
        .arg(QString::fromWCharArray(NormalizePathForCompare(absolutePath).c_str()))
        .arg(static_cast<qulonglong>(fileSize))
        .arg(lastWriteStamp)
        .arg(targetSize.width())
        .arg(targetSize.height());
    QPixmap cached;
    if (QPixmapCache::find(cacheKey, &cached)) {
        return cached;
    }

    QImageReader reader(QString::fromWCharArray(absolutePath.c_str()));
    reader.setAutoTransform(true);
    if (!reader.canRead()) {
        return QPixmap();
    }
    const QSize sourceSize = reader.size();
    if (!sourceSize.isValid() ||
        sourceSize.width() > kMaxImageDimension ||
        sourceSize.height() > kMaxImageDimension ||
        static_cast<qint64>(sourceSize.width()) * sourceSize.height() > kMaxImagePixels) {
        return QPixmap();
    }
    reader.setScaledSize(sourceSize.scaled(targetSize, Qt::KeepAspectRatio));
    const QImage image = reader.read();
    if (image.isNull()) {
        return QPixmap();
    }
    const QPixmap pixmap = QPixmap::fromImage(image);
    QPixmapCache::insert(cacheKey, pixmap);
    return pixmap;
}

QPixmap CreateBlankThumbnailPixmap(int size) {
    QPixmap pixmap(size, size);
    pixmap.fill(QColor(235, 235, 235));
    QPainter painter(&pixmap);
    painter.setPen(QColor(180, 180, 180));
    painter.drawRect(0, 0, size - 1, size - 1);
    return pixmap;
}

void AddEmptyListMessage(QListWidget* list, const QString& message) {
    auto* item = new QListWidgetItem(message);
    item->setTextAlignment(Qt::AlignCenter);
    item->setForeground(QColor(125, 135, 150));
    item->setFlags(Qt::NoItemFlags);
    list->addItem(item);
}

} // namespace

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

QtSettingsWindow::QtSettingsWindow(App* app)
    : app_(app) {
    configSaveTimer_ = new QTimer(this);
    configSaveTimer_->setSingleShot(true);
    connect(configSaveTimer_, &QTimer::timeout, this, &QtSettingsWindow::flushConfigSave);

    setWindowTitle(QStringLiteral("musuka settings"));
    setMinimumSize(1040, 680);
    resize(1240, 780);

    QFont baseFont(QStringLiteral("Segoe UI"), 11);
    setFont(baseFont);

    buildUi();
    applyStyleSheet();
    updateNavigation();

    // Center on screen
    if (QScreen* screen = QApplication::primaryScreen()) {
        const QRect screenGeom = screen->availableGeometry();
        const QRect windowGeom = geometry();
        move(screenGeom.center().x() - (windowGeom.width() / 2),
             screenGeom.center().y() - (windowGeom.height() / 2));
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void QtSettingsWindow::showPage(int page) {
    currentPage_ = std::clamp(page, 0, 2);
    if (stackedWidget_) {
        stackedWidget_->setCurrentIndex(currentPage_);
    }
    updateNavigation();
    show();
    raise();
    activateWindow();
}

void QtSettingsWindow::hide() {
    QMainWindow::hide();
}

void QtSettingsWindow::closeEvent(QCloseEvent* event) {
    flushConfigSave();
    if (app_) {
        app_->Exit();
    }
    event->accept();
}

// ---------------------------------------------------------------------------
// String conversion helpers
// ---------------------------------------------------------------------------

QString QtSettingsWindow::toQString(const std::wstring& value) {
    return QString::fromWCharArray(value.c_str());
}

std::wstring QtSettingsWindow::fromString(const QString& value) {
    return value.toStdWString();
}

QString QtSettingsWindow::text(SettingsStringId id) const {
    return SettingsString(app_->Config().settingsLanguage, id);
}

QString QtSettingsWindow::localizedMessage(const std::wstring& message) const {
    return LocalizeSettingsMessage(app_->Config().settingsLanguage, message);
}

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------

void QtSettingsWindow::buildUi() {
    auto* centralWidget = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    stackedWidget_ = new QStackedWidget(centralWidget);
    stackedWidget_->addWidget(buildPage1());   // index 0
    stackedWidget_->addWidget(buildPage2());   // index 1
    stackedWidget_->addWidget(buildPage3());   // index 2

    mainLayout->addWidget(stackedWidget_);

    // Navigation bar at bottom
    auto* navLayout = new QHBoxLayout();
    navLayout->setContentsMargins(18, 6, 18, 10);
    auto* languageLabel = new QLabel(text(SettingsStringId::Language), centralWidget);
    languageLabel->setObjectName(QStringLiteral("settingsLanguageLabel"));
    languageCombo_ = new QComboBox(centralWidget);
    languageCombo_->setObjectName(QStringLiteral("settingsLanguageCombo"));
    for (SettingsLanguage language : {SettingsLanguage::ChineseSimplified,
                                      SettingsLanguage::English,
                                      SettingsLanguage::Japanese}) {
        languageCombo_->addItem(SettingsLanguageName(language), static_cast<int>(language));
    }
    const int languageIndex = languageCombo_->findData(
        static_cast<int>(app_->Config().settingsLanguage));
    languageCombo_->setCurrentIndex(std::max(0, languageIndex));

    navLayout->addWidget(languageLabel);
    navLayout->addWidget(languageCombo_);
    navLayout->addStretch();

    prevButton_ = new QPushButton(text(SettingsStringId::Previous), centralWidget);
    prevButton_->setProperty("nav", true);
    nextButton_ = new QPushButton(text(SettingsStringId::Next), centralWidget);
    nextButton_->setProperty("primary", true);
    runButton_ = new QPushButton(text(SettingsStringId::Run), centralWidget);
    runButton_->setProperty("action", "run");

    navLayout->addWidget(prevButton_);
    navLayout->addWidget(nextButton_);
    navLayout->addWidget(runButton_);

    connect(prevButton_, &QPushButton::clicked, this, [this]() {
        if (currentPage_ > 0) {
            --currentPage_;
            showPage(currentPage_);
        }
    });

    connect(nextButton_, &QPushButton::clicked, this, [this]() {
        if (currentPage_ == 0) {
            onPage1Next();
        } else if (currentPage_ == 1) {
            onPage2Next();
        }
    });

    connect(runButton_, &QPushButton::clicked, this, &QtSettingsWindow::onRunDesktop);
    connect(languageCombo_,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &QtSettingsWindow::onLanguageChanged);

    mainLayout->addLayout(navLayout);
    setCentralWidget(centralWidget);
}

void QtSettingsWindow::rebuildUi() {
    const int page = currentPage_;
    QWidget* oldCentralWidget = takeCentralWidget();
    resetWidgetPointers();
    if (oldCentralWidget) {
        oldCentralWidget->deleteLater();
    }
    buildUi();
    applyStyleSheet();
    showPage(page);
}

void QtSettingsWindow::resetWidgetPointers() {
    stackedWidget_ = nullptr;
    languageCombo_ = nullptr;
    pathEdit_ = nullptr;
    browseButton_ = nullptr;
    previewLabel_ = nullptr;
    searchEdit_ = nullptr;
    objectList_ = nullptr;
    candidateList_ = nullptr;
    defaultImageList_ = nullptr;
    includeButton_ = nullptr;
    includeAllButton_ = nullptr;
    replaceButton_ = nullptr;
    importSingleButton_ = nullptr;
    importFolderButton_ = nullptr;
    iconSizeSlider_ = nullptr;
    iconSizeValueLabel_ = nullptr;
    modeEngineRadio_ = nullptr;
    modeDesktopStaticCompatibilityRadio_ = nullptr;
    modeStaticVirtualDesktopRadio_ = nullptr;
    staticWallpaperOptions_ = nullptr;
    bgSystemRadio_ = nullptr;
    bgSolidRadio_ = nullptr;
    chooseColorButton_ = nullptr;
    colorPreviewLabel_ = nullptr;
    wallpaperInfoLabel_ = nullptr;
    prevButton_ = nullptr;
    nextButton_ = nullptr;
    runButton_ = nullptr;
}

QWidget* QtSettingsWindow::buildPage1() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 22, 28, 12);
    layout->setSpacing(8);

    auto* titleLabel = new QLabel(QStringLiteral("musuka settings"), page);
    titleLabel->setProperty("title", true);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    auto* subtitleLabel = new QLabel(text(SettingsStringId::Step1Title), page);
    subtitleLabel->setObjectName(QStringLiteral("page1TitleLabel"));
    subtitleLabel->setProperty("subtitle", true);
    QFont subtitleFont = subtitleLabel->font();
    subtitleFont.setPointSize(subtitleFont.pointSize() + 1);
    subtitleLabel->setFont(subtitleFont);
    layout->addWidget(subtitleLabel);

    auto* descLabel = new QLabel(
        text(SettingsStringId::Step1Description),
        page);
    descLabel->setTextFormat(Qt::PlainText);
    descLabel->setProperty("desc", true);
    descLabel->setWordWrap(app_->Config().settingsLanguage != SettingsLanguage::ChineseSimplified);
    layout->addWidget(descLabel);

    layout->addSpacing(12);

    auto* pathCard = new QFrame(page);
    pathCard->setProperty("card", true);
    auto* pathCardLayout = new QVBoxLayout(pathCard);
    pathCardLayout->setContentsMargins(18, 16, 18, 18);
    pathCardLayout->setSpacing(10);
    auto* pathTitle = new QLabel(text(SettingsStringId::DesktopScanLocation), pathCard);
    pathTitle->setProperty("section", true);
    pathCardLayout->addWidget(pathTitle);
    auto* pathHint = new QLabel(
        text(SettingsStringId::DesktopScanHint),
        pathCard);
    pathHint->setTextFormat(Qt::PlainText);
    pathHint->setProperty("desc", true);
    pathHint->setWordWrap(app_->Config().settingsLanguage != SettingsLanguage::ChineseSimplified);
    pathCardLayout->addWidget(pathHint);

    auto* pathRow = new QHBoxLayout();
    pathEdit_ = new QLineEdit(pathCard);
    std::wstring desktopPath = app_->Config().desktopPath;
    if (desktopPath.empty()) {
        desktopPath = GetKnownDesktopPath();
    }
    pathEdit_->setText(toQString(desktopPath));
    browseButton_ = new QPushButton(text(SettingsStringId::Browse), pathCard);
    browseButton_->setProperty("secondary", true);
    browseButton_->setFixedWidth(104);

    connect(browseButton_, &QPushButton::clicked, this, &QtSettingsWindow::onBrowsePath);

    pathRow->addWidget(pathEdit_, 1);
    pathRow->addWidget(browseButton_);
    pathCardLayout->addLayout(pathRow);
    layout->addWidget(pathCard);

    layout->addStretch();
    return page;
}

QWidget* QtSettingsWindow::buildPage2() {
    auto* page = new QWidget();
    auto* outerLayout = new QVBoxLayout(page);
    outerLayout->setContentsMargins(24, 20, 24, 8);
    outerLayout->setSpacing(8);

    auto* titleLabel = new QLabel(text(SettingsStringId::Step2Title), page);
    titleLabel->setObjectName(QStringLiteral("page2TitleLabel"));
    titleLabel->setProperty("subtitle", true);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleLabel->setFont(titleFont);
    outerLayout->addWidget(titleLabel);

    auto* descLabel = new QLabel(
        text(SettingsStringId::Step2Description),
        page);
    descLabel->setProperty("desc", true);
    outerLayout->addWidget(descLabel);

    // Three-column content area
    auto* contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(14);

    // --- Left panel: Preview ---
    auto* leftPanel = new QFrame(page);
    leftPanel->setProperty("card", true);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 14, 12, 12);
    leftLayout->setSpacing(6);

    {
        auto* previewTitle = new QLabel(text(SettingsStringId::SelectedReplacementPreview), leftPanel);
        previewTitle->setProperty("section", true);
        leftLayout->addWidget(previewTitle);
    }

    previewLabel_ = new QLabel(leftPanel);
    previewLabel_->setObjectName(QStringLiteral("previewLabel"));
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    previewLabel_->setMinimumHeight(200);
    leftLayout->addWidget(previewLabel_, 1);

    contentLayout->addWidget(leftPanel, 30);

    // --- Middle panel: Object list ---
    auto* midPanel = new QFrame(page);
    midPanel->setProperty("card", true);
    auto* midLayout = new QVBoxLayout(midPanel);
    midLayout->setContentsMargins(12, 14, 12, 12);
    midLayout->setSpacing(8);

    auto* objectTitle = new QLabel(text(SettingsStringId::DesktopObjects), midPanel);
    objectTitle->setProperty("section", true);
    midLayout->addWidget(objectTitle);

    searchEdit_ = new QLineEdit(midPanel);
    searchEdit_->setPlaceholderText(text(SettingsStringId::Search));
    searchEdit_->setText(toQString(searchText_));
    midLayout->addWidget(searchEdit_);

    objectList_ = new QListWidget(midPanel);
    objectList_->setObjectName(QStringLiteral("objectListWidget"));
    objectList_->setSelectionMode(QAbstractItemView::SingleSelection);
    objectList_->setViewMode(QListView::ListMode);
    objectList_->setMovement(QListView::Static);
    objectList_->setUniformItemSizes(true);
    objectList_->setIconSize(QSize(36, 36));
    objectList_->setWordWrap(false);
    midLayout->addWidget(objectList_, 1);

    contentLayout->addWidget(midPanel, 34);

    // --- Right panel: Detail controls ---
    auto* rightPanel = new QFrame(page);
    rightPanel->setProperty("card", true);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(14, 14, 14, 12);
    rightLayout->setSpacing(8);

    auto* placeholderLabel = new QLabel(
        text(SettingsStringId::SelectFileHint),
        rightPanel);
    placeholderLabel->setAlignment(Qt::AlignCenter);
    placeholderLabel->setWordWrap(true);
    placeholderLabel->setObjectName(QStringLiteral("placeholderLabel"));
    rightLayout->addWidget(placeholderLabel, 1);

    // Import buttons row
    auto* importRow = new QHBoxLayout();
    importSingleButton_ = new QPushButton(text(SettingsStringId::ImportImages), rightPanel);
    importSingleButton_->setProperty("secondary", true);
    importFolderButton_ = new QPushButton(text(SettingsStringId::ImportFolder), rightPanel);
    importFolderButton_->setProperty("secondary", true);
    importRow->addWidget(importSingleButton_);
    importRow->addWidget(importFolderButton_);
    rightLayout->addLayout(importRow);

    // Icon size slider row
    auto* sizeRow = new QHBoxLayout();
    auto* sizeLabel = new QLabel(text(SettingsStringId::DesktopDisplaySize), rightPanel);
    iconSizeSlider_ = new QSlider(Qt::Horizontal, rightPanel);
    iconSizeSlider_->setRange(kDesktopIconMinSize, kDesktopIconMaxSize);
    iconSizeSlider_->setTickInterval(32);
    iconSizeSlider_->setTickPosition(QSlider::TicksBelow);
    iconSizeValueLabel_ = new QLabel(rightPanel);
    iconSizeValueLabel_->setFixedWidth(62);
    iconSizeValueLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    sizeRow->addWidget(sizeLabel);
    sizeRow->addWidget(iconSizeSlider_, 1);
    sizeRow->addWidget(iconSizeValueLabel_);
    rightLayout->addLayout(sizeRow);

    // Candidate list section
    auto* candidateTitle = new QLabel(text(SettingsStringId::ObjectCandidates), rightPanel);
    candidateTitle->setProperty("section", true);
    rightLayout->addWidget(candidateTitle);

    candidateList_ = new QListWidget(rightPanel);
    candidateList_->setObjectName(QStringLiteral("candidateListWidget"));
    candidateList_->setSelectionMode(QAbstractItemView::SingleSelection);
    candidateList_->setViewMode(QListView::IconMode);
    candidateList_->setIconSize(QSize(kQtThumbnailSize, kQtThumbnailSize));
    candidateList_->setMovement(QListView::Static);
    candidateList_->setResizeMode(QListView::Adjust);
    candidateList_->setGridSize(QSize(kQtThumbnailSize + 20, kQtThumbnailSize + 34));
    candidateList_->setMinimumHeight(150);
    rightLayout->addWidget(candidateList_, 1);

    // Default image list section
    auto* defaultTitle = new QLabel(QStringLiteral("default_image"), rightPanel);
    defaultTitle->setProperty("section", true);
    rightLayout->addWidget(defaultTitle);

    defaultImageList_ = new QListWidget(rightPanel);
    defaultImageList_->setObjectName(QStringLiteral("defaultImageListWidget"));
    defaultImageList_->setSelectionMode(QAbstractItemView::SingleSelection);
    defaultImageList_->setViewMode(QListView::IconMode);
    defaultImageList_->setIconSize(QSize(kQtThumbnailSize, kQtThumbnailSize));
    defaultImageList_->setMovement(QListView::Static);
    defaultImageList_->setResizeMode(QListView::Adjust);
    defaultImageList_->setGridSize(QSize(kQtThumbnailSize + 20, kQtThumbnailSize + 34));
    defaultImageList_->setMinimumHeight(180);
    rightLayout->addWidget(defaultImageList_, 1);

    // Keep related actions together and give the primary action visual weight.
    auto* actionRow = new QHBoxLayout();
    includeButton_ = new QPushButton(text(SettingsStringId::Ignore), rightPanel);
    includeButton_->setProperty("danger", true);
    includeAllButton_ = new QPushButton(text(SettingsStringId::IgnoreAll), rightPanel);
    includeAllButton_->setProperty("danger", true);
    replaceButton_ = new QPushButton(text(SettingsStringId::Replace), rightPanel);
    replaceButton_->setProperty("primary", true);
    replaceButton_->setMinimumWidth(120);
    actionRow->addWidget(includeButton_);
    actionRow->addWidget(includeAllButton_);
    actionRow->addStretch();
    actionRow->addWidget(replaceButton_);
    rightLayout->addLayout(actionRow);

    contentLayout->addWidget(rightPanel, 36);

    outerLayout->addLayout(contentLayout, 1);

    // Connect signals for Page 2 widgets
    connect(searchEdit_, &QLineEdit::textChanged, this, &QtSettingsWindow::onSearchChanged);
    connect(objectList_, &QListWidget::currentRowChanged, this, &QtSettingsWindow::onObjectSelectionChanged);
    connect(importSingleButton_, &QPushButton::clicked, this, &QtSettingsWindow::onImportSingle);
    connect(importFolderButton_, &QPushButton::clicked, this, &QtSettingsWindow::onImportFolder);
    connect(includeButton_, &QPushButton::clicked, this, &QtSettingsWindow::onToggleInclude);
    connect(includeAllButton_, &QPushButton::clicked, this, &QtSettingsWindow::onToggleIncludeAll);
    connect(replaceButton_, &QPushButton::clicked, this, &QtSettingsWindow::onReplaceSelected);
    connect(iconSizeSlider_, &QSlider::valueChanged, this, &QtSettingsWindow::onIconSizeSliderChanged);
    connect(candidateList_, &QListWidget::currentRowChanged, this, [this](int row) {
        Q_UNUSED(row);
        selectedDefaultImage_ = false;
        selectedDefaultImageIndex_ = -1;
        selectedCandidateIndex_ = candidateList_->currentRow();
        if (defaultImageList_) {
            defaultImageList_->blockSignals(true);
            defaultImageList_->clearSelection();
            defaultImageList_->blockSignals(false);
        }
        drawPreview();
    });
    connect(defaultImageList_, &QListWidget::currentRowChanged, this, [this](int row) {
        Q_UNUSED(row);
        selectedDefaultImage_ = true;
        selectedDefaultImageIndex_ = defaultImageList_->currentRow();
        selectedCandidateIndex_ = -1;
        if (candidateList_) {
            candidateList_->blockSignals(true);
            candidateList_->clearSelection();
            candidateList_->blockSignals(false);
        }
        drawPreview();
    });

    // Initialize state
    if ((selectedObjectIndex_ < 0 ||
         selectedObjectIndex_ >= static_cast<int>(app_->Config().objects.size())) &&
        !app_->Config().objects.empty()) {
        selectedObjectIndex_ = 0;
    }

    loadDefaultImageCandidates();
    if (const DesktopObject* selObj = selectedObject()) {
        selectCurrentCandidateForObject(*selObj);
    }
    populateObjectList();
    refreshSelectedObjectControls();

    return page;
}

QWidget* QtSettingsWindow::buildPage3() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(34, 28, 34, 12);
    layout->setSpacing(6);

    auto* titleLabel = new QLabel(text(SettingsStringId::Step3Title), page);
    titleLabel->setObjectName(QStringLiteral("page3TitleLabel"));
    titleLabel->setProperty("subtitle", true);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    auto* descLabel = new QLabel(
        text(SettingsStringId::Step3Description),
        page);
    descLabel->setProperty("desc", true);
    layout->addWidget(descLabel);

    // Mode radio group
    auto* modeCard = new QFrame(page);
    modeCard->setProperty("card", true);
    auto* modeLayout = new QVBoxLayout(modeCard);
    modeLayout->setContentsMargins(18, 14, 18, 16);
    modeLayout->setSpacing(6);
    auto* modeTitle = new QLabel(text(SettingsStringId::VirtualDesktopMode), modeCard);
    modeTitle->setProperty("section", true);
    modeLayout->addWidget(modeTitle);

    modeStaticVirtualDesktopRadio_ = new QRadioButton(
        text(SettingsStringId::StaticWallpaperVirtualDesktopMode), modeCard);
    modeStaticVirtualDesktopRadio_->setObjectName(
        QStringLiteral("staticWallpaperVirtualDesktopModeRadio"));

    auto* modeGroup = new QButtonGroup(page);
    modeGroup->addButton(modeStaticVirtualDesktopRadio_);

    modeLayout->addWidget(modeStaticVirtualDesktopRadio_);
    layout->addWidget(modeCard);

    auto* compatibilityCard = new QFrame(page);
    compatibilityCard->setObjectName(QStringLiteral("compatibilityModeOptions"));
    compatibilityCard->setProperty("card", true);
    auto* compatibilityLayout = new QVBoxLayout(compatibilityCard);
    compatibilityLayout->setContentsMargins(18, 14, 18, 16);
    compatibilityLayout->setSpacing(6);
    auto* compatibilityTitle = new QLabel(text(SettingsStringId::CompatibilityMode), compatibilityCard);
    compatibilityTitle->setProperty("section", true);
    compatibilityLayout->addWidget(compatibilityTitle);

    modeDesktopStaticCompatibilityRadio_ = new QRadioButton(
        text(SettingsStringId::DesktopStaticWallpaperCompatibilityMode), compatibilityCard);
    modeEngineRadio_ = new QRadioButton(
        text(SettingsStringId::WallpaperEngineCompatibilityMode), compatibilityCard);
    modeDesktopStaticCompatibilityRadio_->setObjectName(
        QStringLiteral("desktopStaticWallpaperCompatibilityModeRadio"));
    modeEngineRadio_->setObjectName(QStringLiteral("wallpaperEngineModeRadio"));
    modeGroup->addButton(modeDesktopStaticCompatibilityRadio_);
    modeGroup->addButton(modeEngineRadio_);
    compatibilityLayout->addWidget(modeDesktopStaticCompatibilityRadio_);
    compatibilityLayout->addWidget(modeEngineRadio_);
    layout->addWidget(compatibilityCard);

    switch (app_->Config().desktopMode) {
    case DesktopMode::DesktopStaticWallpaperCompatibility:
        modeDesktopStaticCompatibilityRadio_->setChecked(true);
        break;
    case DesktopMode::WallpaperEngineCompatibility:
        modeEngineRadio_->setChecked(true);
        break;
    case DesktopMode::StaticWallpaperVirtualDesktop:
        modeStaticVirtualDesktopRadio_->setChecked(true);
        break;
    }

    connect(modeEngineRadio_, &QRadioButton::toggled, this, &QtSettingsWindow::onModeEngineToggled);
    connect(modeDesktopStaticCompatibilityRadio_,
            &QRadioButton::toggled,
            this,
            &QtSettingsWindow::onModeDesktopStaticCompatibilityToggled);
    connect(modeStaticVirtualDesktopRadio_,
            &QRadioButton::toggled,
            this,
            &QtSettingsWindow::onModeStaticVirtualDesktopToggled);

    // Background source group
    auto* backgroundCard = new QFrame(page);
    staticWallpaperOptions_ = backgroundCard;
    backgroundCard->setObjectName(QStringLiteral("staticWallpaperOptions"));
    backgroundCard->setProperty("card", true);
    auto* backgroundLayout = new QVBoxLayout(backgroundCard);
    backgroundLayout->setContentsMargins(18, 14, 18, 16);
    backgroundLayout->setSpacing(8);
    auto* bgLabel = new QLabel(text(SettingsStringId::StaticWallpaperSource), backgroundCard);
    bgLabel->setProperty("section", true);
    backgroundLayout->addWidget(bgLabel);

    bgSystemRadio_ = new QRadioButton(text(SettingsStringId::UseCurrentSystemStaticWallpaper), backgroundCard);
    bgSolidRadio_ = new QRadioButton(text(SettingsStringId::UseMusukaSolidColor), backgroundCard);

    auto* bgGroup = new QButtonGroup(page);
    bgGroup->addButton(bgSystemRadio_);
    bgGroup->addButton(bgSolidRadio_);

    backgroundLayout->addWidget(bgSystemRadio_);
    backgroundLayout->addWidget(bgSolidRadio_);

    if (app_->Config().backgroundSource == BackgroundSource::SolidColor) {
        bgSolidRadio_->setChecked(true);
    } else {
        bgSystemRadio_->setChecked(true);
    }

    connect(bgSystemRadio_, &QRadioButton::toggled, this, &QtSettingsWindow::onBgSystemToggled);
    connect(bgSolidRadio_, &QRadioButton::toggled, this, &QtSettingsWindow::onBgSolidToggled);

    // Color chooser row
    auto* colorRow = new QHBoxLayout();
    colorRow->setContentsMargins(28, 0, 0, 0);
    chooseColorButton_ = new QPushButton(text(SettingsStringId::ChooseColor), backgroundCard);
    chooseColorButton_->setProperty("secondary", true);
    chooseColorButton_->setFixedWidth(110);

    colorPreviewLabel_ = new QLabel(backgroundCard);
    colorPreviewLabel_->setObjectName(QStringLiteral("colorPreviewLabel"));
    colorPreviewLabel_->setFrameShape(QFrame::Box);
    colorPreviewLabel_->setLineWidth(1);
    colorPreviewLabel_->setFixedSize(36, 36);

    colorRow->addWidget(chooseColorButton_);
    colorRow->addWidget(colorPreviewLabel_);
    colorRow->addStretch();
    backgroundLayout->addLayout(colorRow);

    connect(chooseColorButton_, &QPushButton::clicked, this, &QtSettingsWindow::onChooseColor);

    // Wallpaper info
    std::wstring wallpaperPath;
    const bool hasWallpaper = TryGetSystemWallpaperPath(wallpaperPath);
    if (hasWallpaper) {
        app_->Config().systemWallpaperPath = wallpaperPath;
    }

    wallpaperInfoLabel_ = new QLabel(
        hasWallpaper ? text(SettingsStringId::CurrentSystemStaticWallpaperPrefix) + toQString(wallpaperPath)
                     : text(SettingsStringId::CurrentSystemStaticWallpaperReadFailed),
        backgroundCard);
    wallpaperInfoLabel_->setObjectName(QStringLiteral("infoLabel"));
    wallpaperInfoLabel_->setWordWrap(true);
    auto* infoIndentLayout = new QHBoxLayout();
    infoIndentLayout->setContentsMargins(28, 8, 0, 0);
    infoIndentLayout->addWidget(wallpaperInfoLabel_);
    backgroundLayout->addLayout(infoIndentLayout);
    layout->addWidget(backgroundCard);
    backgroundCard->setVisible(
        app_->Config().desktopMode == DesktopMode::StaticWallpaperVirtualDesktop);

    layout->addStretch();

    // Update color preview
    COLORREF solidColor = app_->Config().solidColor;
    QString colorStyle = QStringLiteral("background-color: rgb(%1, %2, %3); border: 1px solid #505050;")
                             .arg(GetRValue(solidColor))
                             .arg(GetGValue(solidColor))
                             .arg(GetBValue(solidColor));
    colorPreviewLabel_->setStyleSheet(colorStyle);

    return page;
}

void QtSettingsWindow::updateNavigation() {
    if (!prevButton_) { return; }
    prevButton_->setVisible(currentPage_ > 0);
    nextButton_->setVisible(currentPage_ < 2);
    runButton_->setVisible(currentPage_ == 2);
}

// ---------------------------------------------------------------------------
// Slot implementations — Page 1
// ---------------------------------------------------------------------------

void QtSettingsWindow::onBrowsePath() {
    const std::wstring current = pathEdit_
        ? fromString(pathEdit_->text())
        : app_->Config().desktopPath;
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        text(SettingsStringId::SelectDesktopFolder),
        toQString(current),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty() && pathEdit_) {
        pathEdit_->setText(dir);
    }
}

void QtSettingsWindow::onPage1Next() {
    const std::wstring path = pathEdit_
        ? fromString(pathEdit_->text())
        : app_->Config().desktopPath;
    if (!DirectoryExists(path)) {
        QMessageBox::warning(this, QStringLiteral("musuka"), text(SettingsStringId::DesktopPathMissing));
        return;
    }

    app_->Config().desktopPath = path;
    DesktopScanner scanner;
    std::wstring error;
    std::wstring warning;
    if (!scanner.ScanAndPrepare(app_->Config(), error, warning)) {
        QMessageBox::critical(this, QStringLiteral("musuka"), localizedMessage(error));
        return;
    }
    saveConfigQuietly();
    if (!warning.empty()) {
        QMessageBox::information(this, QStringLiteral("musuka"), localizedMessage(warning));
    }
    currentPage_ = 1;
    selectedObjectIndex_ = app_->Config().objects.empty() ? -1 : 0;
    loadDefaultImageCandidates();
    if (const DesktopObject* object = selectedObject()) {
        selectCurrentCandidateForObject(*object);
    }
    // Rebuild page 2 with fresh data
    // Note: we need to rebuild page 2 since it was built at startup without objects.
    // For simplicity we just switch pages and let existing widget populate.
    showPage(currentPage_);
    populateObjectList();
    refreshSelectedObjectControls();
}

// ---------------------------------------------------------------------------
// Slot implementations — Page 2 → Page 3 transition
// ---------------------------------------------------------------------------

void QtSettingsWindow::onPage2Next() {
    saveConfigQuietly();
    currentPage_ = 2;
    showPage(currentPage_);
}

// ---------------------------------------------------------------------------
// Slot implementations — Run desktop
// ---------------------------------------------------------------------------

void QtSettingsWindow::onRunDesktop() {
    AppConfig& config = app_->Config();
    if (config.desktopMode == DesktopMode::StaticWallpaperVirtualDesktop &&
        config.backgroundSource == BackgroundSource::SystemWallpaper) {
        std::wstring wallpaperPath;
        if (!TryGetSystemWallpaperPath(wallpaperPath)) {
            QMessageBox::critical(this, QStringLiteral("musuka"),
                text(SettingsStringId::SystemStaticWallpaperReadFailed));
            config.backgroundSource = BackgroundSource::SolidColor;
            if (bgSolidRadio_) {
                bgSolidRadio_->setChecked(true);
            }
            return;
        }
        config.systemWallpaperPath = wallpaperPath;
    }

    configSaveTimer_->stop();
    std::wstring error;
    if (!app_->Store().Save(config, error)) {
        QMessageBox::critical(this, QStringLiteral("musuka"), localizedMessage(error));
        return;
    }
    app_->ShowDesktop();
}

// ---------------------------------------------------------------------------
// Slot implementations — Search and selection
// ---------------------------------------------------------------------------

void QtSettingsWindow::onSearchChanged() {
    searchText_ = searchEdit_ ? fromString(searchEdit_->text()) : L"";
    populateObjectList();
}

void QtSettingsWindow::onObjectSelectionChanged() {
    int row = objectList_ ? objectList_->currentRow() : -1;
    if (row >= 0 && row < static_cast<int>(filteredObjects_.size())) {
        const int objectIndex = filteredObjects_[static_cast<size_t>(row)];
        onObjectSelectedImpl(objectIndex);
    }
}

void QtSettingsWindow::onObjectSelectedImpl(int objectIndex) {
    if (objectIndex < 0 || objectIndex >= static_cast<int>(app_->Config().objects.size())) {
        return;
    }
    selectedObjectIndex_ = objectIndex;
    loadDefaultImageCandidates();
    selectCurrentCandidateForObject(app_->Config().objects[static_cast<size_t>(objectIndex)]);
    refreshSelectedObjectControls();
}

// ---------------------------------------------------------------------------
// Slot implementations — Import
// ---------------------------------------------------------------------------

void QtSettingsWindow::onImportSingle() {
    DesktopObject* object = selectedObject();
    if (!object) {
        return;
    }
    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        text(SettingsStringId::SelectImageFiles),
        QString(),
        text(SettingsStringId::ImageFileFilter));
    if (files.isEmpty()) {
        return;
    }
    for (const QString& filePath : files) {
        std::wstring error;
        if (!addCandidateFromFile(*object, fromString(filePath), error)) {
            QMessageBox::critical(this, QStringLiteral("musuka"), localizedMessage(error));
            break;
        }
    }
    saveConfigQuietly();
    refreshSelectedObjectControls();
}

void QtSettingsWindow::onImportFolder() {
    DesktopObject* object = selectedObject();
    if (!object) {
        return;
    }
    const QString folder = QFileDialog::getExistingDirectory(
        this,
        text(SettingsStringId::ImportImageFolder),
        QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (folder.isEmpty()) {
        return;
    }
    const auto images = EnumerateImageFiles(fromString(folder), false);
    if (images.empty()) {
        QMessageBox::critical(this, QStringLiteral("musuka"),
            text(SettingsStringId::NoSupportedImagesInFolder));
        return;
    }
    if (images.size() > 50) {
        const QString message = text(SettingsStringId::BulkImportConfirmation)
                                    .arg(static_cast<qulonglong>(images.size()));
        QMessageBox confirmation(QMessageBox::Question,
                                 QStringLiteral("musuka"),
                                 message,
                                 QMessageBox::NoButton,
                                 this);
        QPushButton* yesButton = confirmation.addButton(text(SettingsStringId::Yes),
                                                        QMessageBox::YesRole);
        confirmation.addButton(text(SettingsStringId::No), QMessageBox::NoRole);
        confirmation.exec();
        if (confirmation.clickedButton() != yesButton) {
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
        if (addCandidateFromFile(*object, image, error)) {
            ++imported;
        } else {
            lastError = error;
        }
    }
    if (imported == 0) {
        if (skipped > 0) {
            QMessageBox::information(this, QStringLiteral("musuka"),
                text(SettingsStringId::FolderImagesAlreadyImported));
            return;
        }
        QMessageBox::critical(this, QStringLiteral("musuka"),
            lastError.empty() ? text(SettingsStringId::ImageImportFailed) : localizedMessage(lastError));
        return;
    }
    saveConfigQuietly();
    refreshSelectedObjectControls();
    if (images.size() > 1 || skipped > 0) {
        QMessageBox::information(this, QStringLiteral("musuka"),
            text(SettingsStringId::ImportSummary).arg(imported).arg(skipped));
    }
}

// ---------------------------------------------------------------------------
// Slot implementations — Include toggle / Replace
// ---------------------------------------------------------------------------

void QtSettingsWindow::onToggleInclude() {
    DesktopObject* object = selectedObject();
    if (!object) {
        return;
    }
    object->includeInDesktop = !object->includeInDesktop;
    saveConfigQuietly();
    updateObjectListItem(selectedObjectIndex_);
    updateSelectionDetailControls();
}

void QtSettingsWindow::onToggleIncludeAll() {
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
    saveConfigQuietly();
    updateVisibleObjectItems();
    updateSelectionDetailControls();
}

void QtSettingsWindow::onReplaceSelected() {
    DesktopObject* object = selectedObject();
    if (!object) {
        QMessageBox::warning(this, QStringLiteral("musuka"), text(SettingsStringId::SelectCandidateFirst));
        return;
    }
    const ImageCandidate* candidate = nullptr;
    if (selectedDefaultImage_) {
        if (selectedDefaultImageIndex_ >= 0 &&
            selectedDefaultImageIndex_ < static_cast<int>(defaultImageCandidates_.size())) {
            candidate = &defaultImageCandidates_[static_cast<size_t>(selectedDefaultImageIndex_)];
            SelectExternalCandidate(*object, *candidate);
        }
    } else if (selectedCandidateIndex_ >= 0 &&
               selectedCandidateIndex_ < static_cast<int>(object->candidates.size())) {
        candidate = &object->candidates[static_cast<size_t>(selectedCandidateIndex_)];
        SelectObjectCandidate(*object, selectedCandidateIndex_);
    }
    if (!candidate) {
        QMessageBox::warning(this, QStringLiteral("musuka"), text(SettingsStringId::SelectCandidateFirst));
        return;
    }
    object->iconSize = PreferredIconSizeForCandidate(*candidate);
    saveConfigQuietly();
    drawPreview();
    populateCandidateList();
    populateDefaultImageList();
    updateSelectionDetailControls();
}

// ---------------------------------------------------------------------------
// Slot implementations — Icon size slider
// ---------------------------------------------------------------------------

void QtSettingsWindow::onIconSizeSliderChanged() {
    DesktopObject* object = selectedObject();
    if (!object || !iconSizeSlider_) {
        return;
    }
    const int size = std::clamp(iconSizeSlider_->value(), kDesktopIconMinSize, kDesktopIconMaxSize);
    if (object->iconSize == size) {
        return;
    }
    object->iconSize = size;
    saveConfigQuietly();
    updateSelectionDetailControls();
}

// ---------------------------------------------------------------------------
// Slot implementations — Page 3 mode / background / color
// ---------------------------------------------------------------------------

void QtSettingsWindow::onModeEngineToggled(bool checked) {
    if (!checked) {
        return;
    }
    app_->Config().desktopMode = DesktopMode::WallpaperEngineCompatibility;
    if (staticWallpaperOptions_) {
        staticWallpaperOptions_->setVisible(false);
    }
    saveConfigQuietly();
}

void QtSettingsWindow::onModeDesktopStaticCompatibilityToggled(bool checked) {
    if (!checked) {
        return;
    }
    app_->Config().desktopMode = DesktopMode::DesktopStaticWallpaperCompatibility;
    if (staticWallpaperOptions_) {
        staticWallpaperOptions_->setVisible(false);
    }
    saveConfigQuietly();
}

void QtSettingsWindow::onModeStaticVirtualDesktopToggled(bool checked) {
    if (!checked) {
        return;
    }
    app_->Config().desktopMode = DesktopMode::StaticWallpaperVirtualDesktop;
    if (staticWallpaperOptions_) {
        staticWallpaperOptions_->setVisible(true);
    }
    saveConfigQuietly();
}

void QtSettingsWindow::onBgSystemToggled(bool checked) {
    if (!checked) {
        return;
    }
    app_->Config().backgroundSource = BackgroundSource::SystemWallpaper;
    saveConfigQuietly();
}

void QtSettingsWindow::onBgSolidToggled(bool checked) {
    if (!checked) {
        return;
    }
    app_->Config().backgroundSource = BackgroundSource::SolidColor;
    saveConfigQuietly();
}

void QtSettingsWindow::onChooseColor() {
    QColor initialColor = QColor(
        GetRValue(app_->Config().solidColor),
        GetGValue(app_->Config().solidColor),
        GetBValue(app_->Config().solidColor));
    QColor chosen = QColorDialog::getColor(initialColor, this, text(SettingsStringId::ChooseColor));
    if (!chosen.isValid()) {
        return;
    }
    app_->Config().solidColor = RGB(chosen.red(), chosen.green(), chosen.blue());
    app_->Config().backgroundSource = BackgroundSource::SolidColor;
    if (bgSolidRadio_) {
        bgSolidRadio_->blockSignals(true);
        bgSolidRadio_->setChecked(true);
        bgSolidRadio_->blockSignals(false);
    }
    saveConfigQuietly();
    if (colorPreviewLabel_) {
        const COLORREF c = app_->Config().solidColor;
        colorPreviewLabel_->setStyleSheet(
            QStringLiteral("background-color: rgb(%1, %2, %3); border: 1px solid #505050;")
                .arg(GetRValue(c)).arg(GetGValue(c)).arg(GetBValue(c)));
    }
}

void QtSettingsWindow::onLanguageChanged(int index) {
    if (!languageCombo_ || index < 0) {
        return;
    }
    const auto language = static_cast<SettingsLanguage>(
        languageCombo_->itemData(index).toInt());
    if (language == app_->Config().settingsLanguage) {
        return;
    }
    app_->Config().settingsLanguage = language;
    flushConfigSave();
    rebuildUi();
}

// ---------------------------------------------------------------------------
// Populate / refresh methods
// ---------------------------------------------------------------------------

void QtSettingsWindow::populateObjectList() {
    if (!objectList_) {
        return;
    }
    objectList_->blockSignals(true);
    objectList_->clear();

    filteredObjects_.clear();
    const std::wstring query = LowerText(searchText_);
    auto& objects = app_->Config().objects;
    for (size_t i = 0; i < objects.size(); ++i) {
        if (!query.empty() && LowerText(objects[i].name).find(query) == std::wstring::npos) {
            continue;
        }
        filteredObjects_.push_back(static_cast<int>(i));
    }

    for (size_t row = 0; row < filteredObjects_.size(); ++row) {
        const int objectIndex = filteredObjects_[row];
        DesktopObject& object = objects[static_cast<size_t>(objectIndex)];
        HICON icon = LoadShellIconForObject(object, true);
        QListWidgetItem* item = new QListWidgetItem();
        item->setText(ObjectListText(object, app_->Config().settingsLanguage));
        item->setData(Qt::UserRole, objectIndex);
        if (icon) {
            item->setIcon(QIcon(QPixmap::fromImage(QImage::fromHICON(icon))));
            DestroyIcon(icon);
        }
        if (!object.includeInDesktop) {
            QFont font = item->font();
            font.setStrikeOut(true);
            item->setFont(font);
            item->setForeground(QBrush(QColor(140, 140, 140)));
        }
        if (objectIndex == selectedObjectIndex_) {
            item->setSelected(true);
        }
        objectList_->addItem(item);
    }
    if (filteredObjects_.empty()) {
        AddEmptyListMessage(objectList_,
            query.empty() ? text(SettingsStringId::NoDesktopObjects)
                          : text(SettingsStringId::NoMatchingDesktopObjects));
    }
    objectList_->blockSignals(false);
}

void QtSettingsWindow::updateObjectListItem(int objectIndex) {
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
    QListWidgetItem* item = objectList_->item(row);
    if (!item) {
        return;
    }
    item->setText(ObjectListText(app_->Config().objects[static_cast<size_t>(objectIndex)],
                                 app_->Config().settingsLanguage));
    QFont font = item->font();
    font.setStrikeOut(!app_->Config().objects[static_cast<size_t>(objectIndex)].includeInDesktop);
    item->setFont(font);
    item->setForeground(QBrush(app_->Config().objects[static_cast<size_t>(objectIndex)].includeInDesktop
                      ? palette().text().color()
                      : QColor(140, 140, 140)));
    objectList_->viewport()->update();
}

void QtSettingsWindow::updateVisibleObjectItems() {
    if (!objectList_) {
        return;
    }
    for (int row = 0; row < objectList_->count(); ++row) {
        QListWidgetItem* item = objectList_->item(row);
        if (!item) {
            continue;
        }
        const int objectIndex = item->data(Qt::UserRole).toInt();
        if (objectIndex < 0 || objectIndex >= static_cast<int>(app_->Config().objects.size())) {
            continue;
        }
        item->setText(ObjectListText(app_->Config().objects[static_cast<size_t>(objectIndex)],
                                     app_->Config().settingsLanguage));
        QFont font = item->font();
        font.setStrikeOut(!app_->Config().objects[static_cast<size_t>(objectIndex)].includeInDesktop);
        item->setFont(font);
        item->setForeground(QBrush(app_->Config().objects[static_cast<size_t>(objectIndex)].includeInDesktop
                          ? palette().text().color()
                          : QColor(140, 140, 140)));
    }
    objectList_->viewport()->update();
}

void QtSettingsWindow::populateCandidateList() {
    if (!candidateList_) {
        return;
    }
    candidateList_->blockSignals(true);
    candidateList_->clear();

    DesktopObject* object = selectedObject();
    if (!object) {
        AddEmptyListMessage(candidateList_, text(SettingsStringId::SelectDesktopObjectFirst));
        candidateList_->blockSignals(false);
        return;
    }

    if (!selectedDefaultImage_ &&
        (selectedCandidateIndex_ < 0 ||
         selectedCandidateIndex_ >= static_cast<int>(object->candidates.size()))) {
        selectedCandidateIndex_ = object->selectedCandidate;
    }

    for (int i = 0; i < static_cast<int>(object->candidates.size()); ++i) {
        const auto& candidate = object->candidates[static_cast<size_t>(i)];
        QPixmap pix = LoadValidatedPixmap(candidate.internalPath,
                                          QSize(kQtThumbnailRenderSize, kQtThumbnailRenderSize));
        if (pix.isNull()) {
            pix = CreateBlankThumbnailPixmap(kQtThumbnailRenderSize);
        }

        QString displayText = candidate.originalIcon
            ? text(SettingsStringId::OriginalIcon)
            : toQString(candidate.displayName.empty()
                ? FileNameFromPath(candidate.internalPath)
                : candidate.displayName);
        if (SameInternalPath(candidate.internalPath, object->selectedImageInternalPath)) {
            displayText = text(SettingsStringId::CurrentPrefix) + displayText;
        }

        QListWidgetItem* item = new QListWidgetItem(QIcon(pix), displayText);
        item->setData(Qt::UserRole, i);
        if (!selectedDefaultImage_ && i == selectedCandidateIndex_) {
            item->setSelected(true);
        }
        candidateList_->addItem(item);
    }
    if (object->candidates.empty()) {
        AddEmptyListMessage(candidateList_, text(SettingsStringId::NoObjectCandidates));
    }
    candidateList_->blockSignals(false);
}

void QtSettingsWindow::loadDefaultImageCandidates() {
    defaultImageCandidates_.clear();
    const auto images = EnumerateImageFiles(GetDefaultImageDirectory(), false);
    defaultImageCandidates_.reserve(images.size());
    for (const auto& imagePath : images) {
        const std::wstring relativePath = ToAppRelativePath(imagePath);
        ImageCandidate candidate;
        candidate.displayName = FileNameFromPath(imagePath);
        candidate.originalPath = relativePath;
        candidate.internalPath = relativePath;
        candidate.originalIcon = false;
        candidate.layerPriority = kDefaultImageLayerPriority;
        defaultImageCandidates_.push_back(std::move(candidate));
    }
}

int QtSettingsWindow::findDefaultImageCandidateByPath(const std::wstring& internalPath) const {
    if (internalPath.empty()) {
        return -1;
    }
    for (int i = 0; i < static_cast<int>(defaultImageCandidates_.size()); ++i) {
        if (SameInternalPath(defaultImageCandidates_[static_cast<size_t>(i)].internalPath, internalPath)) {
            return i;
        }
    }
    return -1;
}

void QtSettingsWindow::selectCurrentCandidateForObject(const DesktopObject& object) {
    selectedCandidateIndex_ = -1;
    selectedDefaultImageIndex_ = -1;
    selectedDefaultImage_ = false;

    std::wstring selectedPath = object.selectedImageInternalPath;
    if (selectedPath.empty() &&
        object.selectedCandidate >= 0 &&
        object.selectedCandidate < static_cast<int>(object.candidates.size())) {
        selectedPath = object.candidates[static_cast<size_t>(object.selectedCandidate)].internalPath;
    }

    for (int i = 0; i < static_cast<int>(object.candidates.size()); ++i) {
        if (SameInternalPath(object.candidates[static_cast<size_t>(i)].internalPath, selectedPath)) {
            selectedCandidateIndex_ = i;
            return;
        }
    }

    const int defaultIndex = findDefaultImageCandidateByPath(selectedPath);
    if (defaultIndex >= 0) {
        selectedDefaultImage_ = true;
        selectedDefaultImageIndex_ = defaultIndex;
        return;
    }

    if (object.selectedCandidate >= 0 &&
        object.selectedCandidate < static_cast<int>(object.candidates.size())) {
        selectedCandidateIndex_ = object.selectedCandidate;
    } else if (!object.candidates.empty()) {
        selectedCandidateIndex_ = 0;
    }
}

bool QtSettingsWindow::resolveCurrentCandidate(const DesktopObject& object, ImageCandidate& outCandidate) const {
    std::wstring selectedPath = object.selectedImageInternalPath;
    if (selectedPath.empty() &&
        object.selectedCandidate >= 0 &&
        object.selectedCandidate < static_cast<int>(object.candidates.size())) {
        selectedPath = object.candidates[static_cast<size_t>(object.selectedCandidate)].internalPath;
    }

    for (const auto& candidate : object.candidates) {
        if (SameInternalPath(candidate.internalPath, selectedPath)) {
            outCandidate = candidate;
            return true;
        }
    }

    const int defaultIndex = findDefaultImageCandidateByPath(selectedPath);
    if (defaultIndex >= 0) {
        outCandidate = defaultImageCandidates_[static_cast<size_t>(defaultIndex)];
        return true;
    }

    if (object.selectedCandidate >= 0 &&
        object.selectedCandidate < static_cast<int>(object.candidates.size())) {
        outCandidate = object.candidates[static_cast<size_t>(object.selectedCandidate)];
        return true;
    }
    if (!object.candidates.empty()) {
        outCandidate = object.candidates.front();
        return true;
    }
    return false;
}

void QtSettingsWindow::populateDefaultImageList() {
    if (!defaultImageList_) {
        return;
    }
    defaultImageList_->blockSignals(true);
    defaultImageList_->clear();

    DesktopObject* object = selectedObject();
    if (!object) {
        AddEmptyListMessage(defaultImageList_, text(SettingsStringId::SelectDesktopObjectFirst));
        defaultImageList_->blockSignals(false);
        return;
    }

    if (selectedDefaultImage_ &&
        (selectedDefaultImageIndex_ < 0 ||
         selectedDefaultImageIndex_ >= static_cast<int>(defaultImageCandidates_.size()))) {
        selectedDefaultImageIndex_ = findDefaultImageCandidateByPath(object->selectedImageInternalPath);
    }

    for (int i = 0; i < static_cast<int>(defaultImageCandidates_.size()); ++i) {
        const auto& candidate = defaultImageCandidates_[static_cast<size_t>(i)];
        QPixmap pix = LoadValidatedPixmap(candidate.internalPath,
                                          QSize(kQtThumbnailRenderSize, kQtThumbnailRenderSize));
        if (pix.isNull()) {
            pix = CreateBlankThumbnailPixmap(kQtThumbnailRenderSize);
        }

        QString displayText = toQString(candidate.displayName.empty()
            ? FileNameFromPath(candidate.internalPath)
            : candidate.displayName);
        if (SameInternalPath(candidate.internalPath, object->selectedImageInternalPath)) {
            displayText = text(SettingsStringId::CurrentPrefix) + displayText;
        }

        QListWidgetItem* item = new QListWidgetItem(QIcon(pix), displayText);
        item->setData(Qt::UserRole, i);
        if (selectedDefaultImage_ && i == selectedDefaultImageIndex_) {
            item->setSelected(true);
        }
        defaultImageList_->addItem(item);
    }
    if (defaultImageCandidates_.empty()) {
        AddEmptyListMessage(defaultImageList_, text(SettingsStringId::NoDefaultImages));
    }
    defaultImageList_->blockSignals(false);
}

// ---------------------------------------------------------------------------
// Preview drawing
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Global stylesheet — modern clean theme
// ---------------------------------------------------------------------------

void QtSettingsWindow::applyStyleSheet() {
    // clang-format off
    const QString sheet = QStringLiteral(R"(
        /* ── Global ─────────────────────────── */
        QMainWindow, QWidget {
            background-color: #f5f6fa;
            color: #2c3e50;
            font-family: "Segoe UI", "Microsoft YaHei UI", sans-serif;
            font-size: 11pt;
        }

        /* ── Page titles (large bold) ─────── */
        QLabel[title="true"] {
            font-size: 20pt;
            font-weight: bold;
            color: #1a1a2e;
            padding: 4px 0px 8px 0px;
        }

        /* ── Subtitles / step labels ──────── */
        QLabel[subtitle="true"] {
            font-size: 13pt;
            font-weight: bold;
            color: #16213e;
            padding: 0px 0px 8px 0px;
        }

        /* ── Description text ─────────────── */
        QLabel[desc="true"] {
            color: #555;
            font-size: 10pt;
            padding: 2px 0px 12px 0px;
        }

        /* ── Section headers in Page 2 ─────── */
        QLabel[section="true"] {
            font-weight: bold;
            color: #34495e;
            font-size: 10pt;
            padding: 6px 0px 4px 0px;
            border-bottom: 1px solid #dcdde1;
            margin-bottom: 4px;
        }

        /* ── Primary action buttons ────────── */
        QPushButton[primary="true"] {
            background-color: #4a90d9;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 8px 24px;
            font-weight: bold;
            font-size: 11pt;
            min-width: 90px;
        }
        QPushButton[primary="true"]:hover {
            background-color: #357abd;
        }
        QPushButton[primary="true"]:pressed {
            background-color: #2968a3;
        }

        /* ── Secondary buttons (browse etc) ── */
        QPushButton[secondary="true"] {
            background-color: #ffffff;
            color: #4a90d9;
            border: 1px solid #4a90d9;
            border-radius: 6px;
            padding: 7px 18px;
            font-size: 10pt;
            min-width: 70px;
        }
        QPushButton[secondary="true"]:hover {
            background-color: #eaf2fb;
            border-color: #357abd;
        }

        /* ── Navigation buttons ────────────── */
        QPushButton[nav="true"] {
            background-color: #ffffff;
            color: #333;
            border: 1px solid #ccc;
            border-radius: 6px;
            padding: 7px 22px;
            font-size: 10pt;
            min-width: 80px;
        }
        QPushButton[nav="true"]:hover {
            background-color: #f0f0f0;
            border-color: #999;
        }
        QPushButton[nav="true"]:disabled {
            color: #aaa;
            background-color: #fafafa;
            border-color: #e0e0e0;
        }

        /* ── Run button (accent green) ─────── */
        QPushButton[action="run"] {
            background-color: #27ae60;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 8px 28px;
            font-weight: bold;
            font-size: 11pt;
            min-width: 100px;
        }
        QPushButton[action="run"]:hover {
            background-color: #219a52;
        }
        QPushButton[action="run"]:pressed {
            background-color: #1e8449;
        }

        /* ── Danger / toggle buttons ───────── */
        QPushButton[danger="true"] {
            background-color: #fff5f5;
            color: #c0392b;
            border: 1px solid #e74c3c;
            border-radius: 5px;
            padding: 5px 14px;
            font-size: 9pt;
        }
        QPushButton[danger="true"]:hover {
            background-color: #fadbd8;
        }

        /* ── Line edits (path input, search) ─*/
        QLineEdit {
            background-color: white;
            border: 1px solid #ccd1d9;
            border-radius: 6px;
            padding: 7px 12px;
            font-size: 10pt;
            selection-background-color: #4a90d9;
            selection-color: white;
        }
        QLineEdit:focus {
            border-color: #4a90d9;
        }
        QLineEdit:read-only {
            background-color: #f8f9fa;
            color: #555;
        }
        QComboBox#settingsLanguageCombo {
            background-color: white;
            border: 1px solid #ccd1d9;
            border-radius: 5px;
            padding: 4px 24px 4px 8px;
            min-width: 92px;
        }
        QComboBox#settingsLanguageCombo:hover,
        QComboBox#settingsLanguageCombo:focus {
            border-color: #4a90d9;
        }

        /* ── Object list (Page 2 left-center) ─*/
        QListWidget#objectListWidget {
            background-color: white;
            border: 1px solid #dcdde1;
            border-radius: 8px;
            outline: none;
            padding: 4px;
        }
        QListWidget#objectListWidget::item {
            padding: 6px 10px;
            border-radius: 4px;
            margin: 1px 2px;
            font-size: 10pt;
        }
        QListWidget#objectListWidget::item:selected {
            background-color: #e8f0fe;
            color: #1a73e8;
            border-left: 3px solid #1a73e8;
        }
        QListWidget#objectListWidget::item:hover:!selected {
            background-color: #f5f7fa;
        }

        /* ── Candidate thumbnail lists ──────── */
        QListWidget#candidateListWidget,
        QListWidget#defaultImageListWidget {
            background-color: #fafbfc;
            border: 1px solid #e1e4e8;
            border-radius: 8px;
            outline: none;
        }
        QListWidget#candidateListWidget::item,
        QListWidget#defaultImageListWidget::item {
            margin: 3px;
            border-radius: 6px;
            padding: 4px;
            background-color: white;
            border: 1px solid transparent;
        }
        QListWidget#candidateListWidget::item:selected,
        QListWidget#defaultImageListWidget::item:selected {
            border: 2px solid #4a90d9;
            background-color: #eef4fc;
        }
        QListWidget#candidateListWidget::item:hover:!selected,
        QListWidget#defaultImageListWidget::item:hover:!selected {
            border-color: #ccc;
        }

        /* ── Preview area (card style) ──────── */
        QLabel#previewLabel {
            background-color: white;
            border: 2px dashed #dcdde1;
            border-radius: 10px;
            qproperty-alignment: AlignCenter;
            min-height: 200px;
        }

        /* ── Color preview label ────────────── */
        QLabel#colorPreviewLabel {
            border: 2px solid #bbb;
            border-radius: 6px;
            min-width: 36px;
            max-width: 36px;
            min-height: 36px;
            max-height: 36px;
        }

        /* ── Radio buttons (modern) ─────────── */
        QRadioButton {
            spacing: 8px;
            font-size: 11pt;
            color: #333;
            padding: 6px 4px;
        }
        QRadioButton::indicator {
            width: 18px;
            height: 18px;
            border-radius: 9px;
            border: 2px solid #aaa;
            background: white;
        }
        QRadioButton::indicator:checked {
            border-color: #4a90d9;
            background: qradialgradient(cx:0.5, cy:0.5, radius:0.6, fx:0.5, fy:0.5,
                stop:0 #4a90d9, stop:1 #4a90d9);
        }
        QRadioButton::indicator:hover {
            border-color: #4a90d9;
        }

        /* ── Slider (custom track & handle) ─── */
        QSlider::groove:horizontal {
            height: 6px;
            background: #dcdde1;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            width: 18px;
            height: 18px;
            margin: -6px 0;
            background: #4a90d9;
            border-radius: 9px;
            border: 2px solid white;
        }
        QSlider::handle:horizontal:hover {
            background: #357abd;
        }
        QSlider::sub-page:horizontal {
            background: #4a90d9;
            border-radius: 3px;
        }

        /* ── Scrollbar (thin modern) ────────── */
        QScrollBar:vertical {
            width: 10px;
            background: transparent;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #ccc;
            border-radius: 5px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: #aaa;
        }
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar::add-page:vertical,
        QScrollBar::sub-page:vertical {
            background: transparent;
        }
        QScrollBar:horizontal {
            height: 10px;
            background: transparent;
        }
        QScrollBar::handle:horizontal {
            background: #ccc;
            border-radius: 5px;
            min-width: 30px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #aaa;
        }
        QScrollBar::add-line:horizontal,
        QScrollBar::sub-line:horizontal {
            width: 0;
        }
        QScrollBar::add-page:horizontal,
        QScrollBar::sub-page:horizontal {
            background: transparent;
        }

        /* ── Info label (wallpaper path etc) ── */
        QLabel#infoLabel {
            color: #777;
            font-size: 9pt;
            padding: 4px 8px;
            background-color: #f0f2f5;
            border-radius: 4px;
        }

        /* ── Group box / frame styling ──────── */
        QFrame[card="true"] {
            background-color: white;
            border: 1px solid #e1e4e8;
            border-radius: 10px;
            padding: 8px;
        }
    )");
    // clang-format on
    setStyleSheet(sheet);
}

// ---------------------------------------------------------------------------
// Preview rendering
// ---------------------------------------------------------------------------

void QtSettingsWindow::drawPreview() {
    if (!previewLabel_) {
        return;
    }

    DesktopObject* object = selectedObject();
    ImageCandidate candidate;
    if (!object || !resolveCurrentCandidate(*object, candidate)) {
        previewLabel_->setPixmap(QPixmap());
        previewLabel_->setText(text(SettingsStringId::NoFileSelected));
        return;
    }

    // If this is the original shell icon, render it with GDI+ for
    // high-quality scaling and proper alpha (no white background/border).
    if (candidate.originalIcon) {
        constexpr int kPreviewIconSize = 220;
        const HBITMAP hBmp = CreatePreviewBitmap(*object, kPreviewIconSize);
        if (hBmp) {
            // GDI+ exports a bottom-up HBITMAP, while Qt displays scanlines top-down.
            const QImage img = QImage::fromHBITMAP(hBmp).mirrored(false, true);
            DeleteObject(hBmp);
            if (!img.isNull()) {
                previewLabel_->setPixmap(QPixmap::fromImage(img));
                previewLabel_->setAlignment(Qt::AlignCenter);
                previewLabel_->setText(QString());
            } else {
                previewLabel_->setPixmap(QPixmap());
                previewLabel_->setText(text(SettingsStringId::OriginalIcon));
            }
        } else {
            previewLabel_->setPixmap(QPixmap());
            previewLabel_->setText(text(SettingsStringId::OriginalIcon));
        }
        return;
    }

    const QSize labelSize = previewLabel_->size();
    const QPixmap source = LoadValidatedPixmap(
        candidate.internalPath,
        QSize(labelSize.width() - kPreviewMargin * 2,
              labelSize.height() - kPreviewMargin * 2));
    if (!source.isNull()) {
        previewLabel_->setPixmap(source);
        previewLabel_->setAlignment(Qt::AlignCenter);
        previewLabel_->setText(QString());
    } else {
        previewLabel_->setPixmap(QPixmap());
        previewLabel_->setText(text(SettingsStringId::ImageReadFailed));
    }
}

// ---------------------------------------------------------------------------
// Refresh helpers
// ---------------------------------------------------------------------------

void QtSettingsWindow::refreshSelectedObjectControls() {
    if (!candidateList_ || !previewLabel_) {
        // Full rebuild not needed with QStackedWidget; re-populate instead
    }
    loadDefaultImageCandidates();
    populateCandidateList();
    populateDefaultImageList();
    updateSelectionDetailControls();
    drawPreview();
}

void QtSettingsWindow::updateSelectionDetailControls() {
    DesktopObject* object = selectedObject();

    // Show/hide detail controls based on whether an object is selected
    QWidget* ph = findChild<QWidget*>(QStringLiteral("placeholderLabel"));
    if (ph) {
        ph->setVisible(!object);
    }
    if (importSingleButton_) importSingleButton_->setVisible(!!object);
    if (importFolderButton_) importFolderButton_->setVisible(!!object);
    if (iconSizeSlider_) iconSizeSlider_->setVisible(!!object);
    if (iconSizeValueLabel_) iconSizeValueLabel_->setVisible(!!object);
    if (candidateList_) candidateList_->setVisible(!!object);
    if (defaultImageList_) defaultImageList_->setVisible(!!object);
    if (includeButton_) includeButton_->setVisible(!!object);
    if (includeAllButton_) includeAllButton_->setVisible(!!object);
    if (replaceButton_) replaceButton_->setVisible(!!object);

    if (includeButton_ && object) {
        includeButton_->setText(object->includeInDesktop
            ? text(SettingsStringId::Ignore)
            : text(SettingsStringId::Include));
    }
    if (includeAllButton_) {
        const auto& objects = app_->Config().objects;
        const bool anyIncluded = std::any_of(objects.begin(), objects.end(), [](const DesktopObject& item) {
            return item.includeInDesktop;
        });
        includeAllButton_->setText(anyIncluded
            ? text(SettingsStringId::IgnoreAll)
            : text(SettingsStringId::IncludeAll));
    }
    if (iconSizeSlider_ && object) {
        const int size = std::clamp(object->iconSize, kDesktopIconMinSize, kDesktopIconMaxSize);
        iconSizeSlider_->blockSignals(true);
        iconSizeSlider_->setValue(size);
        iconSizeSlider_->blockSignals(false);
    }
    if (iconSizeValueLabel_ && object) {
        const int size = std::clamp(object->iconSize, kDesktopIconMinSize, kDesktopIconMaxSize);
        iconSizeValueLabel_->setText(QString("%1 px").arg(size));
    }
}

// ---------------------------------------------------------------------------
// Add candidate from file (identical business logic to original)
// ---------------------------------------------------------------------------

bool QtSettingsWindow::addCandidateFromFile(DesktopObject& object, const std::wstring& imagePath, std::wstring& error) {
    error.clear();
    if (!IsSupportedImageFile(imagePath)) {
        error = L"图片格式不支持。当前支持 PNG/JPG/JPEG/BMP。";
        return false;
    }
    {
        std::error_code ec;
        const auto fileSize = fs::file_size(imagePath, ec);
        if (!ec && fileSize > 10 * 1024 * 1024) { // 10 MB
            error = L"图片文件过大（超过 10 MB）。";
            return false;
        }
    }
    if (!ImageCanBeLoaded(imagePath)) {
        error = L"图片读取失败，可能不是有效图片文件。";
        return false;
    }
    const std::wstring objectDir = CombinePath(GetIconsDirectory(), object.id);
    if (!IsPathInsideIconsRoot(objectDir)) {
        error = L"对象目录路径异常，导入已拒绝。";
        return false;
    }
    EnsureDirectory(objectDir);
    // TOCTOU defense: verify objectDir is not a reparse point before writing.
    if (IsReparsePoint(objectDir)) {
        error = L"对象目录为重解析点，导入已拒绝。";
        return false;
    }
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
    selectedDefaultImage_ = false;
    selectedDefaultImageIndex_ = -1;
    return true;
}

// ---------------------------------------------------------------------------
// Save config quietly
// ---------------------------------------------------------------------------

void QtSettingsWindow::saveConfigQuietly() {
    configSaveTimer_->start(kConfigSaveDelayMs);
}

void QtSettingsWindow::flushConfigSave() {
    configSaveTimer_->stop();
    std::wstring error;
    app_->Store().Save(app_->Config(), error);
}

// ---------------------------------------------------------------------------
// Accessor helpers
// ---------------------------------------------------------------------------

DesktopObject* QtSettingsWindow::selectedObject() {
    if (selectedObjectIndex_ < 0 ||
        selectedObjectIndex_ >= static_cast<int>(app_->Config().objects.size())) {
        return nullptr;
    }
    return &app_->Config().objects[static_cast<size_t>(selectedObjectIndex_)];
}

const DesktopObject* QtSettingsWindow::selectedObject() const {
    if (selectedObjectIndex_ < 0 ||
        selectedObjectIndex_ >= static_cast<int>(app_->Config().objects.size())) {
        return nullptr;
    }
    return &app_->Config().objects[static_cast<size_t>(selectedObjectIndex_)];
}

ImageCandidate* QtSettingsWindow::selectedCandidate() {
    DesktopObject* object = selectedObject();
    if (!object || selectedCandidateIndex_ < 0 ||
        selectedCandidateIndex_ >= static_cast<int>(object->candidates.size())) {
        return nullptr;
    }
    return &object->candidates[static_cast<size_t>(selectedCandidateIndex_)];
}

} // namespace musuka
