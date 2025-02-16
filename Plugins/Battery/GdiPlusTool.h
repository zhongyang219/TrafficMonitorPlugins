#pragma once
#include <gdiplus.h>

class CGdiPlusTool
{
public:
    CGdiPlusTool();
    ~CGdiPlusTool();

    static Gdiplus::Color COLORREFToGdiplusColor(COLORREF color, BYTE alpha = 255);
    static COLORREF GdiplusColorToCOLORREF(Gdiplus::Color color);
    static void CreateRoundRectPath(Gdiplus::GraphicsPath& path, Gdiplus::RectF rect, float radius);
    static CRect GdiplusRectToCRect(Gdiplus::RectF rect);
    static Gdiplus::RectF CRectToGdiplusRect(CRect rect);
};
