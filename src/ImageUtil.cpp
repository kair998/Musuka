#include "ImageUtil.h"

#include "Util.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <commoncontrols.h>
#include <vector>

namespace fs = std::filesystem;

namespace musuka {

namespace {

int GetEncoderClsid(const WCHAR* format, CLSID* clsid) {
    UINT count = 0;
    UINT size = 0;
    Gdiplus::GetImageEncodersSize(&count, &size);
    if (size == 0) {
        return -1;
    }

    std::vector<BYTE> buffer(size);
    auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    Gdiplus::GetImageEncoders(count, size, encoders);
    for (UINT i = 0; i < count; ++i) {
        if (wcscmp(encoders[i].MimeType, format) == 0) {
            *clsid = encoders[i].Clsid;
            return static_cast<int>(i);
        }
    }
    return -1;
}

HICON LoadStockIcon(SHSTOCKICONID id, bool large) {
    SHSTOCKICONINFO info{};
    info.cbSize = sizeof(info);
    const UINT flags = SHGSI_ICON | (large ? SHGSI_LARGEICON : SHGSI_SMALLICON);
    if (SUCCEEDED(SHGetStockIconInfo(id, flags, &info))) {
        return info.hIcon;
    }
    return nullptr;
}

} // namespace

// Parse image file headers (PNG / JPEG / BMP) to get pixel dimensions
// without invoking GDI+ full decode. Returns false if dimensions exceed limits
// or format is unrecognized.
namespace {

bool CheckImageDimensionsPreDecode(const std::wstring& path,
                                    UINT maxDim, UINT64 maxPixels) {
    // For PNG/BMP, a small initial read suffices (headers at fixed offsets).
    // For JPEG, read up to kJpegMaxScan into a contiguous buffer so that
    // large APP/EXIF segments don't cause cross-buffer parsing errors.
    constexpr size_t kInitialBufSize = 4096;
    constexpr size_t kJpegMaxScan = 256 * 1024; // 256 KB total scan cap
    auto buf = std::make_unique<unsigned char[]>(kInitialBufSize);
    std::ifstream file(fs::path(path), std::ios::binary);
    if (!file) return false;

    file.read(reinterpret_cast<char*>(buf.get()), kInitialBufSize);
    const auto bytesRead = static_cast<size_t>(file.gcount());
    if (bytesRead < 8) return true; // too small for any known format

    UINT w = 0, h = 0;
    bool recognized = false;

    // PNG: signature + IHDR at fixed offset 16-23
    if (bytesRead >= 25 &&
        buf[0] == 0x89 && buf[1] == 0x50 && buf[2] == 0x4E && buf[3] == 0x47 &&
        buf[4] == 0x0D && buf[5] == 0x0A && buf[6] == 0x1A && buf[7] == 0x0A) {
        w = (static_cast<UINT>(buf[16]) << 24) | (static_cast<UINT>(buf[17]) << 16) |
            (static_cast<UINT>(buf[18]) << 8)  |  static_cast<UINT>(buf[19]);
        h = (static_cast<UINT>(buf[20]) << 24) | (static_cast<UINT>(buf[21]) << 16) |
            (static_cast<UINT>(buf[22]) << 8)  |  static_cast<UINT>(buf[23]);
        recognized = true;
    }
    // BMP: DIB header at fixed offset 18-25
    else if (bytesRead >= 26 &&
             buf[0] == 'B' && buf[1] == 'M') {
        w = static_cast<UINT>(buf[18]) | (static_cast<UINT>(buf[19]) << 8) |
            (static_cast<UINT>(buf[20]) << 16) | (static_cast<UINT>(buf[21]) << 24);
        h = static_cast<UINT>(buf[22]) | (static_cast<UINT>(buf[23]) << 8) |
            (static_cast<UINT>(buf[24]) << 16) | (static_cast<UINT>(buf[25]) << 24);
        if (h > 0x80000000u) h = 0;
        recognized = true;
    }
    // JPEG: read up to scan cap into one contiguous buffer, then scan markers
    else if (bytesRead >= 4 && buf[0] == 0xFF && buf[1] == 0xD8) {
        // If initial read was smaller than scan cap, try to read more
        size_t jpegLen = bytesRead;
        if (jpegLen < kJpegMaxScan && file.good()) {
            auto jpegBuf = std::make_unique<unsigned char[]>(kJpegMaxScan);
            // Copy what we already read
            memcpy(jpegBuf.get(), buf.get(), bytesRead);
            // Read remaining up to cap
            file.read(reinterpret_cast<char*>(jpegBuf.get() + bytesRead),
                      static_cast<std::streamsize>(kJpegMaxScan - bytesRead));
            jpegLen = bytesRead + static_cast<size_t>(file.gcount());

            // Scan the full contiguous buffer
            size_t pos = 2;
            while (pos + 9 <= jpegLen) {
                if (jpegBuf[pos] != 0xFF) break;
                if (jpegBuf[pos + 1] == 0xFF) { ++pos; continue; }
                const unsigned char marker = jpegBuf[pos + 1];
                if ((marker >= 0xC0 && marker <= 0xCF) &&
                    marker != 0xC4 && marker != 0xC8 && marker != 0xCC) {
                    h = (static_cast<UINT>(jpegBuf[pos + 5]) << 8) |
                        static_cast<UINT>(jpegBuf[pos + 6]);
                    w = (static_cast<UINT>(jpegBuf[pos + 7]) << 8) |
                        static_cast<UINT>(jpegBuf[pos + 8]);
                    recognized = true;
                    break;
                }
                if (marker == 0xD9 || marker == 0xDA) break;
                if (pos + 4 > jpegLen) break;
                const UINT segLen = (static_cast<UINT>(jpegBuf[pos + 2]) << 8) |
                                     static_cast<UINT>(jpegBuf[pos + 3]);
                if (segLen < 2) break;
                pos += 2 + segLen;
            }
        } else {
            // File fits in initial buffer — scan inline
            size_t pos = 2;
            while (pos + 9 <= jpegLen) {
                if (buf[pos] != 0xFF) break;
                if (buf[pos + 1] == 0xFF) { ++pos; continue; }
                const unsigned char marker = buf[pos + 1];
                if ((marker >= 0xC0 && marker <= 0xCF) &&
                    marker != 0xC4 && marker != 0xC8 && marker != 0xCC) {
                    h = (static_cast<UINT>(buf[pos + 5]) << 8) |
                        static_cast<UINT>(buf[pos + 6]);
                    w = (static_cast<UINT>(buf[pos + 7]) << 8) |
                        static_cast<UINT>(buf[pos + 8]);
                    recognized = true;
                    break;
                }
                if (marker == 0xD9 || marker == 0xDA) break;
                if (pos + 4 > jpegLen) break;
                const UINT segLen = (static_cast<UINT>(buf[pos + 2]) << 8) |
                                     static_cast<UINT>(buf[pos + 3]);
                if (segLen < 2) break;
                pos += 2 + segLen;
            }
        }
        if (!recognized) {
            return false; // JPEG without SOF in scan range — reject
        }
    }

    // Unknown / unrecognized format: reject rather than defer to GDI+
    if (!recognized) {
        return false;
    }
    if (w == 0 || h == 0) return false;
    return w <= maxDim && h <= maxDim && (static_cast<UINT64>(w) * static_cast<UINT64>(h)) <= maxPixels;
}

} // anonymous namespace

bool SaveHIconAsPng(HICON icon, const std::wstring& path) {
    if (!icon) {
        return false;
    }
    std::unique_ptr<Gdiplus::Bitmap> bitmap(Gdiplus::Bitmap::FromHICON(icon));
    if (!bitmap || bitmap->GetLastStatus() != Gdiplus::Ok) {
        return false;
    }
    CLSID pngClsid{};
    if (GetEncoderClsid(L"image/png", &pngClsid) < 0) {
        return false;
    }
    return bitmap->Save(path.c_str(), &pngClsid, nullptr) == Gdiplus::Ok;
}

HICON LoadShellIconForObject(const DesktopObject& object, bool large) {
    if (object.type == DesktopObjectType::ThisPC) {
        HICON icon = LoadStockIcon(SIID_DESKTOPPC, large);
        if (icon) {
            return icon;
        }
    }
    if (object.type == DesktopObjectType::RecycleBin) {
        HICON icon = LoadStockIcon(SIID_RECYCLER, large);
        if (icon) {
            return icon;
        }
    }
    if (object.type == DesktopObjectType::Folder) {
        HICON icon = LoadStockIcon(SIID_FOLDER, large);
        if (icon) {
            return icon;
        }
    }

    const std::wstring shellPath = (object.type == DesktopObjectType::ThisPC ||
                                   object.type == DesktopObjectType::RecycleBin)
        ? OpenShellIdForObject(object)
        : object.path;

    SHFILEINFOW info{};
    const UINT flags = SHGFI_ICON | (large ? SHGFI_LARGEICON : SHGFI_SMALLICON);
    if (SHGetFileInfoW(shellPath.c_str(), 0, &info, sizeof(info), flags) != 0 && info.hIcon) {
        return info.hIcon;
    }
    return LoadStockIcon(SIID_APPLICATION, large);
}

// ---------------------------------------------------------------------------
// Load a high-resolution icon for preview (256px via IImageList SHIL_JUMBO)
// This is how Windows Explorer renders crisp Large Icons.
// ---------------------------------------------------------------------------
HICON LoadShellIconForPreview(const DesktopObject& object) {
    const std::wstring shellPath = (object.type == DesktopObjectType::ThisPC ||
                                   object.type == DesktopObjectType::RecycleBin)
        ? OpenShellIdForObject(object)
        : object.path;

    // Get the icon index once, then ask the system jumbo image list for the
    // actual high-resolution icon. SHGetFileInfo size flags only return the
    // classic small/large lists and become blurry when enlarged.
    SHFILEINFOW info{};
    if (SHGetFileInfoW(shellPath.c_str(), 0, &info, sizeof(info),
                       SHGFI_SYSICONINDEX) == 0) {
        return LoadShellIconForObject(object, true);
    }

    using SHGetImageListFn = HRESULT(WINAPI*)(int, REFIID, void**);
    HMODULE shell32 = GetModuleHandleW(L"shell32.dll");
    const auto getImageList = shell32
        ? reinterpret_cast<SHGetImageListFn>(GetProcAddress(shell32, "SHGetImageList"))
        : nullptr;
    if (getImageList) {
        static const int kImageLists[] = {SHIL_JUMBO, SHIL_EXTRALARGE, SHIL_LARGE};
        for (int imageListId : kImageLists) {
            IImageList* imageList = nullptr;
            if (SUCCEEDED(getImageList(imageListId, __uuidof(IImageList),
                                       reinterpret_cast<void**>(&imageList))) &&
                imageList) {
                HICON icon = nullptr;
                const HRESULT result = imageList->GetIcon(info.iIcon, ILD_TRANSPARENT, &icon);
                imageList->Release();
                if (SUCCEEDED(result) && icon) {
                    return icon;
                }
            }
        }
    }

    return LoadShellIconForObject(object, true);
}

HBITMAP CreateThumbnailBitmap(const std::wstring& imagePath, int width, int height) {
    auto bitmap = LoadBitmapFromPath(ToAbsoluteAppPath(imagePath));
    if (!bitmap) {
        return nullptr;
    }

    Gdiplus::Bitmap canvas(width, height, PixelFormat32bppARGB);
    Gdiplus::Graphics graphics(&canvas);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
    DrawImageContain(graphics, bitmap.get(), Gdiplus::RectF(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)));

    HBITMAP handle = nullptr;
    canvas.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &handle);
    return handle;
}

bool ImageCanBeLoaded(const std::wstring& imagePath) {
    auto bitmap = LoadBitmapFromPath(imagePath);
    return bitmap != nullptr;
}

std::unique_ptr<Gdiplus::Bitmap> LoadBitmapFromPath(const std::wstring& imagePath) {
    if (imagePath.empty() || !FileExists(imagePath)) {
        return nullptr;
    }
    // Reject oversized files before GDI+ decodes them (decompression bomb defense).
    {
        std::error_code ec;
        const auto fileSize = fs::file_size(fs::path(imagePath), ec);
        if (!ec && fileSize > 10 * 1024 * 1024) { // 10 MB raw file cap
            return nullptr;
        }
    }
    // Pre-decode header check: parse PNG/JPEG/BMP headers to get dimensions
    // without invoking GDI+ full decompression.
    {
        constexpr UINT kPreMaxDim = 8192;
        constexpr UINT64 kPreMaxPixels = 16777216ULL; // 16 MP
        if (!CheckImageDimensionsPreDecode(imagePath, kPreMaxDim, kPreMaxPixels)) {
            return nullptr;
        }
    }
    auto bitmap = std::make_unique<Gdiplus::Bitmap>(imagePath.c_str(), FALSE);
    if (!bitmap || bitmap->GetLastStatus() != Gdiplus::Ok ||
        bitmap->GetWidth() == 0 || bitmap->GetHeight() == 0) {
        return nullptr;
    }
    constexpr UINT kMaxDimension = 8192;
    constexpr UINT64 kMaxPixels = 16777216ULL; // 16 megapixels (~64 MB at ARGB)
    const UINT w = bitmap->GetWidth();
    const UINT h = bitmap->GetHeight();
    if (w > kMaxDimension || h > kMaxDimension ||
        static_cast<UINT64>(w) * static_cast<UINT64>(h) > kMaxPixels) {
        return nullptr;
    }
    return bitmap;
}

bool BitmapHasAlpha(Gdiplus::Bitmap* bitmap) {
    if (!bitmap) {
        return false;
    }
    const UINT flags = bitmap->GetFlags();
    if ((flags & Gdiplus::ImageFlagsHasAlpha) != 0) {
        return true;
    }
    const Gdiplus::PixelFormat format = bitmap->GetPixelFormat();
    return (format & PixelFormatAlpha) != 0 || (format & PixelFormatPAlpha) != 0;
}

Gdiplus::RectF CalculateContainRect(Gdiplus::Image* image,
                                    float x,
                                    float y,
                                    float maxWidth,
                                    float maxHeight) {
    if (!image || image->GetWidth() == 0 || image->GetHeight() == 0) {
        return Gdiplus::RectF(x, y, maxWidth, maxHeight);
    }
    const float imageWidth = static_cast<float>(image->GetWidth());
    const float imageHeight = static_cast<float>(image->GetHeight());
    const float scale = std::min(maxWidth / imageWidth, maxHeight / imageHeight);
    const float width = imageWidth * scale;
    const float height = imageHeight * scale;
    return Gdiplus::RectF(x + (maxWidth - width) * 0.5f,
                          y + (maxHeight - height) * 0.5f,
                          width,
                          height);
}

void DrawImageContain(Gdiplus::Graphics& graphics,
                      Gdiplus::Image* image,
                      const Gdiplus::RectF& bounds) {
    if (!image) {
        return;
    }
    const Gdiplus::RectF rect = CalculateContainRect(image, bounds.X, bounds.Y, bounds.Width, bounds.Height);
    graphics.DrawImage(image, rect);
}

void DrawImageCover(Gdiplus::Graphics& graphics,
                    Gdiplus::Image* image,
                    const Gdiplus::RectF& bounds) {
    if (!image || image->GetWidth() == 0 || image->GetHeight() == 0) {
        return;
    }
    const float imageWidth = static_cast<float>(image->GetWidth());
    const float imageHeight = static_cast<float>(image->GetHeight());
    const float scale = std::max(bounds.Width / imageWidth, bounds.Height / imageHeight);
    const float width = imageWidth * scale;
    const float height = imageHeight * scale;
    const float x = bounds.X + (bounds.Width - width) * 0.5f;
    const float y = bounds.Y + (bounds.Height - height) * 0.5f;
    graphics.DrawImage(image, Gdiplus::RectF(x, y, width, height));
}

bool AlphaHitTest(Gdiplus::Bitmap* bitmap,
                  const Gdiplus::RectF& drawRect,
                  int screenX,
                  int screenY,
                  BYTE threshold) {
    if (!bitmap) {
        return false;
    }
    if (screenX < drawRect.X || screenY < drawRect.Y ||
        screenX >= drawRect.X + drawRect.Width ||
        screenY >= drawRect.Y + drawRect.Height) {
        return false;
    }
    if (!BitmapHasAlpha(bitmap)) {
        return true;
    }

    const double localX = (static_cast<double>(screenX) - drawRect.X) / drawRect.Width;
    const double localY = (static_cast<double>(screenY) - drawRect.Y) / drawRect.Height;
    const UINT px = static_cast<UINT>(std::clamp(localX, 0.0, 0.999999) * bitmap->GetWidth());
    const UINT py = static_cast<UINT>(std::clamp(localY, 0.0, 0.999999) * bitmap->GetHeight());

    Gdiplus::Color color;
    if (bitmap->GetPixel(px, py, &color) != Gdiplus::Ok) {
        return true;
    }
    return color.GetAlpha() > threshold;
}

} // namespace musuka
