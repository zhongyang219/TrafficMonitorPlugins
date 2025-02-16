//这个类用于定义用于绘图的函数
#pragma once
#include <gdiplus.h>


class CDrawCommon
{
public:
    CDrawCommon();
    ~CDrawCommon();

    virtual void Create(CDC* pDC);
    void SetDC(CDC* pDC);		//设置绘图的DC
    CDC* GetDC()
    {
        return m_pDC;
    }

    Gdiplus::Graphics* GetGraphics()
    {
        return m_pGraphics;
    }

    void DrawRoundRect(CRect rect, COLORREF color, int radius, BYTE alpha = 255);       //绘制圆角矩形（使用GDI+）
    void DrawRoundRect(Gdiplus::RectF rect, Gdiplus::Color color, int radius);       //绘制圆角矩形（使用GDI+）
    void SetDrawArea(CRect rect);

    HICON LoadIconResource(UINT id, int width, int height);

protected:
    CDC* m_pDC{};		//用于绘图的CDC类的指针
    Gdiplus::Graphics* m_pGraphics{};
};
