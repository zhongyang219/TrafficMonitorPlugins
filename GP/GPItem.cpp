#include "pch.h"
#include "GPItem.h"
#include "DataManager.h"
#include "GP.h"

const wchar_t* GPItem::GetItemName() const
{
    return g_data.StringRes(IDS_PLUGIN_ITEM_NAME);
}

const wchar_t* GPItem::GetItemId() const
{
    return L"GP999SS";
}

const wchar_t* GPItem::GetItemLableText() const
{
    return g_data.StringRes(IDS_PLUGIN_ITEM_NAME);
}

const wchar_t* GPItem::GetItemValueText() const
{
    return L"";
}

const wchar_t* GPItem::GetItemValueSampleText() const
{
    return L"";
}

bool GPItem::IsCustomDraw() const
{
    return true;
}

int GPItem::GetItemWidthEx(void * hDC) const
{
    //绘图句柄
    CDC* pDC = CDC::FromHandle((HDC)hDC);
    return pDC->GetTextExtent(L"000000000000").cx;
}

void GPItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
    //绘图句柄
    CDC* pDC = CDC::FromHandle((HDC)hDC);
    //矩形区域
    CRect rect(CPoint(x, y), CSize(w, h));
    std::wstring current{ g_data.GetGPInfo().ToString() };
    pDC->DrawText(current.c_str(), rect, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

int GPItem::OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag)
{
    CWnd* pWnd = CWnd::FromHandle((HWND)hWnd);
    if (type == IPluginItem::MT_RCLICKED)
    {
        GP::Instance().ShowContextMenu(pWnd);
        return 1;
    }
    return 0;
}
