#include "pch.h"
#include "GdiPlusTool.h"


CGdiPlusTool::CGdiPlusTool()
{
}


CGdiPlusTool::~CGdiPlusTool()
{
}

Gdiplus::Color CGdiPlusTool::COLORREFToGdiplusColor(COLORREF color, BYTE alpha /*= 255*/)
{
    return Gdiplus::Color(alpha, GetRValue(color), GetGValue(color), GetBValue(color));
}

COLORREF CGdiPlusTool::GdiplusColorToCOLORREF(Gdiplus::Color color)
{
    return RGB(color.GetR(), color.GetG(), color.GetB());
}

void CGdiPlusTool::CreateRoundRectPath(Gdiplus::GraphicsPath& path, Gdiplus::RectF rect, float radius)
{
    Gdiplus::REAL diam{ 2 * radius };
    path.AddArc(rect.X, rect.Y, diam, diam, 180, 90); // 左上角圆弧
    path.AddLine(rect.X + radius, rect.Y, rect.GetRight() - radius, rect.Y); // 上边

    path.AddArc(rect.GetRight() - diam, rect.Y, diam, diam, 270, 90); // 右上角圆弧
    path.AddLine(rect.GetRight(), rect.Y + radius, rect.GetRight(), rect.GetBottom() - radius);// 右边

    path.AddArc(rect.GetRight() - diam, rect.GetBottom() - diam, diam, diam, 0, 90); // 右下角圆弧
    path.AddLine(rect.GetRight() - radius, rect.GetBottom(), rect.X + radius, rect.GetBottom()); // 下边

    path.AddArc(rect.X, rect.GetBottom() - diam, diam, diam, 90, 90);
    path.AddLine(rect.X, rect.Y + radius, rect.X, rect.GetBottom() - radius);

}

CRect CGdiPlusTool::GdiplusRectToCRect(Gdiplus::RectF rect)
{
    return CRect(static_cast<int>(rect.GetLeft()), static_cast<int>(rect.GetTop()),
        static_cast<int>(rect.GetRight()), static_cast<int>(rect.GetBottom()));
}

Gdiplus::RectF CGdiPlusTool::CRectToGdiplusRect(CRect rect)
{
    return Gdiplus::RectF(static_cast<Gdiplus::REAL>(rect.left), static_cast<Gdiplus::REAL>(rect.top),
        static_cast<Gdiplus::REAL>(rect.Width()), static_cast<Gdiplus::REAL>(rect.Height()));
}
