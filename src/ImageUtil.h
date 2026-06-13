#pragma once

#include "Models.h"

#include <windows.h>
#include <objidl.h>
#include <propidl.h>
#include <gdiplus.h>

#include <memory>
#include <string>

namespace musuka {

constexpr int kThumbnailSize = 96;

bool SaveHIconAsPng(HICON icon, const std::wstring& path);
HICON LoadShellIconForObject(const DesktopObject& object, bool large);
HICON LoadShellIconForPreview(const DesktopObject& object);
HBITMAP CreatePreviewBitmap(const DesktopObject& object, int size);
HBITMAP CreateThumbnailBitmap(const std::wstring& imagePath, int width, int height);
bool ImageCanBeLoaded(const std::wstring& imagePath);
std::unique_ptr<Gdiplus::Bitmap> LoadBitmapFromPath(const std::wstring& imagePath);
std::unique_ptr<Gdiplus::Bitmap> PrepareBitmapForScaling(std::unique_ptr<Gdiplus::Bitmap> bitmap);
bool BitmapHasAlpha(Gdiplus::Bitmap* bitmap);
void DrawImageContain(Gdiplus::Graphics& graphics,
                      Gdiplus::Image* image,
                      const Gdiplus::RectF& bounds);
void DrawImageCover(Gdiplus::Graphics& graphics,
                    Gdiplus::Image* image,
                    const Gdiplus::RectF& bounds);
Gdiplus::RectF CalculateContainRect(Gdiplus::Image* image,
                                    float x,
                                    float y,
                                    float maxWidth,
                                    float maxHeight);
bool AlphaHitTest(Gdiplus::Bitmap* bitmap,
                  const Gdiplus::RectF& drawRect,
                  int screenX,
                  int screenY,
                  BYTE threshold);

} // namespace musuka
