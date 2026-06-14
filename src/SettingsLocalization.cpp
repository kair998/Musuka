#include "SettingsLocalization.h"

#include <array>

namespace musuka {

namespace {

struct Translation {
    const char* zh;
    const char* en;
    const char* ja;
};

constexpr std::array<Translation, static_cast<size_t>(SettingsStringId::Count)> kTranslations{{
    {"语言", "Language", "言語"},
    {"上一步", "Previous", "戻る"},
    {"下一步", "Next", "次へ"},
    {"运行", "Run", "実行"},
    {"第一步：选择桌面路径", "Step 1: Select desktop path", "ステップ1：デスクトップのパスを選択"},
    {"请选择主要桌面路径。Musuka 会同时扫描该路径、当前用户桌面、公共桌面，\n并加入“此电脑”和“回收站”。",
     "Select the primary desktop path. Musuka will also scan the current user's desktop and the public desktop, then add This PC and Recycle Bin.",
     "メインのデスクトップパスを選択してください。Musuka は現在のユーザーとパブリックのデスクトップもスキャンし、「PC」と「ごみ箱」を追加します。"},
    {"桌面扫描位置", "Desktop scan location", "デスクトップのスキャン場所"},
    {"选择常用桌面目录。继续后会扫描对象，\n并生成可编辑的替代图片配置。",
     "Choose the desktop directory you normally use. Continuing will scan objects and create editable replacement-image settings.",
     "通常使用するデスクトップフォルダーを選択してください。続行するとオブジェクトをスキャンし、編集可能な置換画像設定を作成します。"},
    {"浏览...", "Browse...", "参照..."},
    {"是", "Yes", "はい"},
    {"否", "No", "いいえ"},
    {"第二步：文件选择和替代图片配置", "Step 2: Select objects and replacement images", "ステップ2：オブジェクトと置換画像を設定"},
    {"选择桌面对象，预览并配置它在拟桌面中的显示图片。导入图片不会修改原始文件。",
     "Select a desktop object, then preview and configure its displayed image. Imported images do not modify the original files.",
     "デスクトップオブジェクトを選択し、表示画像をプレビューして設定します。画像をインポートしても元のファイルは変更されません。"},
    {"已选文件替代图片预览", "Selected replacement image preview", "選択した置換画像のプレビュー"},
    {"桌面对象", "Desktop objects", "デスクトップオブジェクト"},
    {"搜索...", "Search...", "検索..."},
    {"选定文件后，在此导入图片并配置替代图。",
     "Select an object to import and configure its replacement image here.",
     "オブジェクトを選択すると、ここで置換画像をインポートして設定できます。"},
    {"导入单张图片", "Import images", "画像をインポート"},
    {"导入整个图片文件夹", "Import image folder", "画像フォルダーをインポート"},
    {"桌面显示尺寸", "Desktop display size", "デスクトップ表示サイズ"},
    {"对象候选图", "Object image candidates", "オブジェクト画像候補"},
    {"忽略", "Exclude", "除外"},
    {"忽略全部", "Exclude all", "すべて除外"},
    {"替换", "Replace", "置換"},
    {"第三步：Desktop 模式选择页面", "Step 3: Select desktop mode", "ステップ3：デスクトップモードを選択"},
    {"拟桌面模式由 Musuka 绘制背景；兼容模式直接在当前桌面壁纸上显示 Musuka 图标。",
     "Virtual desktop mode draws the background in Musuka. Compatibility modes display Musuka icons directly over the current desktop wallpaper.",
     "仮想デスクトップモードでは Musuka が背景を描画します。互換モードでは現在の壁紙上に Musuka アイコンを直接表示します。"},
    {"拟桌面模式", "Virtual desktop mode", "仮想デスクトップモード"},
    {"静态壁纸拟桌面模式", "Static wallpaper virtual desktop mode", "静止画壁紙の仮想デスクトップモード"},
    {"兼容模式", "Compatibility modes", "互換モード"},
    {"桌面静态壁纸兼容模式", "Desktop static wallpaper compatibility mode", "デスクトップ静止画壁紙互換モード"},
    {"Wallpaper Engine 动态壁纸兼容模式", "Wallpaper Engine dynamic wallpaper compatibility mode", "Wallpaper Engine 動く壁紙互換モード"},
    {"静态壁纸来源", "Static wallpaper source", "静止画壁紙のソース"},
    {"使用当前系统静态壁纸", "Use current system static wallpaper", "現在のシステム静止画壁紙を使用"},
    {"使用 Musuka 纯色背景", "Use Musuka solid-color background", "Musuka の単色背景を使用"},
    {"选择颜色", "Choose color", "色を選択"},
    {"当前系统静态壁纸：", "Current system static wallpaper: ", "現在のシステム静止画壁紙："},
    {"当前系统静态壁纸读取失败，可改用 Musuka 纯色背景。",
     "Could not read the current system static wallpaper. Use a Musuka solid-color background instead.",
     "現在のシステム静止画壁紙を読み込めませんでした。Musuka の単色背景を使用してください。"},
    {"选择桌面文件夹", "Select desktop folder", "デスクトップフォルダーを選択"},
    {"桌面路径不存在，请重新选择。", "The desktop path does not exist. Select another path.", "デスクトップパスが存在しません。選択し直してください。"},
    {"读取系统静态壁纸失败，请选择 Musuka 纯色背景。",
     "Could not read the system static wallpaper. Select the Musuka solid-color background.",
     "システム静止画壁紙を読み込めませんでした。Musuka の単色背景を選択してください。"},
    {"选择图片文件", "Select image files", "画像ファイルを選択"},
    {"Images (*.png *.jpg *.jpeg *.bmp);;All Files (*)", "Images (*.png *.jpg *.jpeg *.bmp);;All Files (*)", "画像 (*.png *.jpg *.jpeg *.bmp);;すべてのファイル (*)"},
    {"导入整个图片文件夹", "Import image folder", "画像フォルダーをインポート"},
    {"该文件夹中没有支持的图片格式。当前只导入所选文件夹内的图片，不递归子目录。",
     "The folder contains no supported images. Only images directly inside the selected folder are imported.",
     "このフォルダーに対応画像がありません。選択したフォルダー直下の画像のみをインポートします。"},
    {"该文件夹内有 %1 张图片。确认批量导入这些图片吗？",
     "The folder contains %1 images. Import them all?",
     "このフォルダーには %1 枚の画像があります。すべてインポートしますか？"},
    {"所选文件夹中的图片已经全部导入过，本次未新增候选图片。",
     "All images in the selected folder have already been imported.",
     "選択したフォルダーの画像はすべてインポート済みです。"},
    {"图片导入失败。", "Image import failed.", "画像のインポートに失敗しました。"},
    {"已导入 %1 张图片，跳过 %2 张重复图片。",
     "Imported %1 images and skipped %2 duplicates.",
     "%1 枚の画像をインポートし、重複する %2 枚をスキップしました。"},
    {"请先选择一个候选图片。", "Select an image candidate first.", "先に画像候補を選択してください。"},
    {"未扫描到桌面对象", "No desktop objects were found", "デスクトップオブジェクトが見つかりません"},
    {"没有匹配的桌面对象", "No matching desktop objects", "一致するデスクトップオブジェクトがありません"},
    {"请先选择桌面对象", "Select a desktop object first", "先にデスクトップオブジェクトを選択してください"},
    {"[当前] ", "[Current] ", "[現在] "},
    {"暂无对象专属候选图", "No object-specific image candidates", "オブジェクト専用の画像候補がありません"},
    {"default_image 目录中暂无图片", "No images in the default_image directory", "default_image フォルダーに画像がありません"},
    {"未选中文件", "No object selected", "オブジェクトが選択されていません"},
    {"原始图标", "Original icon", "元のアイコン"},
    {"图片读取失败", "Could not read image", "画像を読み込めませんでした"},
    {"带入", "Include", "含める"},
    {"带入全部", "Include all", "すべて含める"},
    {"此电脑", "This PC", "PC"},
    {"回收站", "Recycle Bin", "ごみ箱"},
    {"  [忽略]", "  [Excluded]", "  [除外]"},
    {"图片格式不支持。当前支持 PNG/JPG/JPEG/BMP。",
     "Unsupported image format. Supported formats: PNG/JPG/JPEG/BMP.",
     "対応していない画像形式です。PNG/JPG/JPEG/BMP に対応しています。"},
    {"图片文件过大（超过 10 MB）。", "The image file is larger than 10 MB.", "画像ファイルが 10 MB を超えています。"},
    {"图片读取失败，可能不是有效图片文件。", "Could not read the image. It may not be a valid image file.", "画像を読み込めません。有効な画像ファイルではない可能性があります。"},
    {"对象目录路径异常，导入已拒绝。", "The object directory path is unsafe. Import was rejected.", "オブジェクトフォルダーのパスが安全ではないため、インポートを拒否しました。"},
    {"对象目录为重解析点，导入已拒绝。", "The object directory is a reparse point. Import was rejected.", "オブジェクトフォルダーが再解析ポイントのため、インポートを拒否しました。"}
}};

const char* TranslationFor(SettingsLanguage language, const Translation& translation) {
    switch (language) {
    case SettingsLanguage::ChineseSimplified:
        return translation.zh;
    case SettingsLanguage::English:
        return translation.en;
    case SettingsLanguage::Japanese:
        return translation.ja;
    }
    return translation.zh;
}

} // namespace

QString SettingsString(SettingsLanguage language, SettingsStringId id) {
    const size_t index = static_cast<size_t>(id);
    if (index >= kTranslations.size()) {
        return {};
    }
    return QString::fromUtf8(TranslationFor(language, kTranslations[index]));
}

QString SettingsLanguageName(SettingsLanguage language) {
    switch (language) {
    case SettingsLanguage::ChineseSimplified:
        return QString::fromUtf8("中文");
    case SettingsLanguage::English:
        return QStringLiteral("English");
    case SettingsLanguage::Japanese:
        return QString::fromUtf8("日本語");
    }
    return QString::fromUtf8("中文");
}

QString LocalizeSettingsMessage(SettingsLanguage language, const std::wstring& message) {
    static constexpr Translation phrases[] = {
        {"图片格式不支持。当前支持 PNG/JPG/JPEG/BMP。",
         "Unsupported image format. Supported formats: PNG/JPG/JPEG/BMP.",
         "対応していない画像形式です。PNG/JPG/JPEG/BMP に対応しています。"},
        {"图片文件过大（超过 10 MB）。", "The image file is larger than 10 MB.", "画像ファイルが 10 MB を超えています。"},
        {"图片读取失败，可能不是有效图片文件。", "Could not read the image. It may not be a valid image file.", "画像を読み込めません。有効な画像ファイルではない可能性があります。"},
        {"对象目录路径异常，导入已拒绝。", "The object directory path is unsafe. Import was rejected.", "オブジェクトフォルダーのパスが安全ではないため、インポートを拒否しました。"},
        {"对象目录为重解析点，导入已拒绝。", "The object directory is a reparse point. Import was rejected.", "オブジェクトフォルダーが再解析ポイントのため、インポートを拒否しました。"},
        {"图片导入失败。", "Image import failed.", "画像のインポートに失敗しました。"},
        {"桌面路径不存在。", "The desktop path does not exist.", "デスクトップパスが存在しません。"},
        {"扫描桌面路径失败：", "Failed to scan desktop path: ", "デスクトップパスのスキャンに失敗しました："},
        {"对象目录路径异常，已跳过图片初始化。", "The object directory path is unsafe. Image initialization was skipped.", "オブジェクトフォルダーのパスが安全ではないため、画像の初期化をスキップしました。"},
        {"对象目录为重解析点，已跳过图片初始化。", "The object directory is a reparse point. Image initialization was skipped.", "オブジェクトフォルダーが再解析ポイントのため、画像の初期化をスキップしました。"},
        {"default_image 目录不存在，默认图片为空。", "The default_image directory does not exist. No default images are available.", "default_image フォルダーが存在しないため、既定画像を使用できません。"},
        {"default_image 目录中没有可用图片。", "The default_image directory contains no usable images.", "default_image フォルダーに使用可能な画像がありません。"},
        {"源图片不存在。", "The source image does not exist.", "元の画像が存在しません。"},
        {"不支持的图片格式。", "Unsupported image format.", "対応していない画像形式です。"},
        {"无法创建 musuka 内部图片目录。", "Could not create the internal Musuka image directory.", "Musuka の内部画像フォルダーを作成できませんでした。"},
        {"图片复制失败。", "Could not copy the image.", "画像をコピーできませんでした。"},
        {"无法创建 data 目录。", "Could not create the data directory.", "data フォルダーを作成できませんでした。"},
        {"无法创建 data\\icons 目录。", "Could not create the data\\icons directory.", "data\\icons フォルダーを作成できませんでした。"},
        {"无法获取配置写入锁。", "Could not acquire the configuration write lock.", "設定書き込みロックを取得できませんでした。"},
        {"无法写入临时配置文件。", "Could not open the temporary configuration file for writing.", "一時設定ファイルを開いて書き込めませんでした。"},
        {"写入临时配置文件失败。", "Could not write the temporary configuration file.", "一時設定ファイルに書き込めませんでした。"},
        {"无法替换配置文件。", "Could not replace the configuration file.", "設定ファイルを置き換えられませんでした。"}
    };
    QString result = QString::fromWCharArray(message.c_str());
    if (language == SettingsLanguage::ChineseSimplified) {
        return result;
    }
    for (const auto& phrase : phrases) {
        result.replace(QString::fromUtf8(phrase.zh),
                       QString::fromUtf8(TranslationFor(language, phrase)));
    }
    return result;
}

} // namespace musuka
