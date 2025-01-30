#include "pch.h"
#include "StockItem.h"
#include "DataManager.h"
#include "Stock.h"
#include "Common.h"

const wchar_t* StockItem::GetItemName() const
{
    static std::wstring item_name;
    item_name = g_data.StringRes(IDS_PLUGIN_ITEM_NAME).GetString();
    item_name += std::to_wstring(index);
    return item_name.c_str();
}

const wchar_t* StockItem::GetItemId() const
{
    static std::wstring item_id;
    item_id = L"Stock999SS";
    item_id += std::to_wstring(index);
    return item_id.c_str();
}

const wchar_t* StockItem::GetItemLableText() const
{
    if (index >= 0 && index < g_data.m_setting_data.m_Stock_codes.size())
        return g_data.m_setting_data.m_Stock_codes[index].GetString();
    else
        return GetItemName();
}

const wchar_t* StockItem::GetItemValueText() const
{
    return L"";
    //GupiaoInfo& StockInfo = g_data.GetStockInfo(Stock_id);
    //static std::wstring current;
    //current = StockInfo.ToString();
    //return current.c_str();
}

const wchar_t* StockItem::GetItemValueSampleText() const
{
    return L"";
}

bool StockItem::IsCustomDraw() const
{
    return true;
}

int StockItem::GetItemWidthEx(void * hDC) const
{
    if (enable)
    {
        //绘图句柄
        CDC* pDC = CDC::FromHandle((HDC)hDC);
        return pDC->GetTextExtent(L"000000000000").cx;
    }
    return 0;
}

void StockItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
    if (enable)
    {
        //绘图句柄
        CDC* pDC = CDC::FromHandle((HDC)hDC);
        //矩形区域
        CRect rect(CPoint(x, y), CSize(w, h));
        GupiaoInfo& StockInfo = g_data.GetStockInfo(Stock_id);
        std::wstring current{ StockInfo.ToString() };
        //pDC->DrawText(StockInfo.name.c_str(), rect, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        pDC->DrawText(current.c_str(), rect, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        // TODO: 暂时没想好怎么展示name
        //CFont font;
        //font.CreatePointFont(100, _T("微软雅黑"));//创建字体属性：字体大小100
        //CFont* pOldFont = pDC->SelectObject(&font);
        //pDC->SetBkMode(TRANSPARENT);//这样文字的背后就不会有难看的背景颜色了
        //pDC->SetTextColor(RGB(255, 0, 0));//设置文字的颜色为红色
        //pDC->DrawText(StockInfo.name.c_str(), rect, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        //pDC->SelectObject(pOldFont);
    }
}

int StockItem::OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag)
{
    CWnd* pWnd = CWnd::FromHandle((HWND)hWnd);
    if (type == IPluginItem::MT_RCLICKED)
    {
        Stock::Instance().ShowContextMenu(pWnd);
        return 1;
    }
    return 0;
}
