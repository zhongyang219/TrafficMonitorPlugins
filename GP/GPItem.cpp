#include "pch.h"
#include "GPItem.h"
#include "DataManager.h"
#include "GP.h"
#include "Common.h"

const wchar_t* GPItem::GetItemName() const
{
    // TODO: TrafficMonitor 暂不支持动态创建
    return g_data.StringRes(IDS_PLUGIN_ITEM_NAME);
}

const wchar_t* GPItem::GetItemId() const
{
    // TODO: TrafficMonitor 暂不支持动态创建
    return L"GP999SS";
}

const wchar_t* GPItem::GetItemLableText() const
{
    // TODO: TrafficMonitor 暂不支持动态创建
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
    if (enable)
    {
        //绘图句柄
        CDC* pDC = CDC::FromHandle((HDC)hDC);
        return pDC->GetTextExtent(L"000000000000").cx;
    }
    return 0;
}

void GPItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
    if (enable)
    {
        //绘图句柄
        CDC* pDC = CDC::FromHandle((HDC)hDC);
        //矩形区域
        CRect rect(CPoint(x, y), CSize(w, h));
        GupiaoInfo& gpInfo = g_data.GetGPInfo(gp_id);
        std::wstring current{ gpInfo.ToString() };
        //pDC->DrawText(gpInfo.name.c_str(), rect, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        pDC->DrawText(current.c_str(), rect, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        // TODO: 暂时没想好怎么展示name
        //CFont font;
        //font.CreatePointFont(100, _T("微软雅黑"));//创建字体属性：字体大小100
        //CFont* pOldFont = pDC->SelectObject(&font);
        //pDC->SetBkMode(TRANSPARENT);//这样文字的背后就不会有难看的背景颜色了
        //pDC->SetTextColor(RGB(255, 0, 0));//设置文字的颜色为红色
        //pDC->DrawText(gpInfo.name.c_str(), rect, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        //pDC->SelectObject(pOldFont);
    }
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
