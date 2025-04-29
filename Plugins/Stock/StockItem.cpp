#include "pch.h"
#include "StockItem.h"
#include "DataManager.h"
#include "Stock.h"
#include "Common.h"
#include <algorithm>
#include "FloatingWnd.h"
#undef min
#undef max

const wchar_t *StockItem::GetItemName() const
{
    static std::wstring item_name;
    item_name = g_data.StringRes(IDS_PLUGIN_ITEM_NAME).GetString();
    item_name += std::to_wstring(index);
    return item_name.c_str();
}

const wchar_t *StockItem::GetItemId() const
{
    static std::wstring item_id;
    item_id = L"qL0KmmYi";
    item_id += std::to_wstring(index);
    return item_id.c_str();
}

const wchar_t *StockItem::GetItemLableText() const
{
    return L"";
}

const wchar_t *StockItem::GetItemValueText() const
{
    auto data = g_data.GetStockData(stock_id);
    static std::wstring current;
    current = data->GetCurrentDisplay(g_data.m_setting_data.m_show_stock_name);
    return current.c_str();
}

bool StockItem::IsCustomDraw() const
{
    return true;
}
int StockItem::GetItemWidthEx(void *hDC) const
{
    CDC *pDC = CDC::FromHandle((HDC)hDC);
    return std::max(pDC->GetTextExtent(g_data.GetStockData(stock_id)->GetCurrentDisplay(g_data.m_setting_data.m_show_stock_name).c_str()).cx, pDC->GetTextExtent(GetItemValueSampleText()).cx);
}

void StockItem::DrawItem(void *hDC, int x, int y, int w, int h, bool dark_mode)
{
    // 绘图句柄
    CDC *pDC = CDC::FromHandle((HDC)hDC);

    // 矩形区域
    auto data = g_data.GetStockData(stock_id);
    CRect rect(CPoint(x, y), CSize(w, h));

    // 文本颜色
    COLORREF color_default;
    COLORREF color_red;
    COLORREF color_green;
    if (dark_mode)
    {
        color_default = RGB(255, 255, 255);
        color_red = RGB(255, 121, 120);
        color_green = RGB(111, 215, 149);
    }
    else
    {
        color_default = RGB(0, 0, 0);
        color_red = RGB(195, 0, 0);
        color_green = RGB(46, 139, 87);
    }

    CRect rect_value{rect};
    if (g_data.m_setting_data.m_show_stock_name)
    {
        // 绘制名称
        pDC->SetTextColor(color_default);
        CString stock_name{data->info.displayName.c_str()};
        stock_name += _T(": ");
        CRect rect_name{rect};
        rect_name.right = rect_name.left + pDC->GetTextExtent(stock_name).cx;
        pDC->DrawText(stock_name, rect_name, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        rect_value.left = rect_name.right;
    }

    if (g_data.m_setting_data.m_color_with_price)
    {
        // 绘制数值
        if (data->realTimeData.displayFluctuation.find('-') != std::wstring::npos)
            pDC->SetTextColor(color_green);
        else
            pDC->SetTextColor(color_red);
    }
    else
    {
        pDC->SetTextColor(color_default);
    }

    UINT flags = DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
    if (g_data.m_right_align)
        flags |= DT_RIGHT;
    pDC->DrawText(data->GetCurrentDisplay(false).c_str(), rect_value, flags);
}

const wchar_t *StockItem::GetItemValueSampleText() const
{
    if (g_data.m_setting_data.m_show_stock_name)
    {
        return L"--------: 0000000.00 +00.00%";
    }
    else
    {
        return L"0000000.00 +00.00%";
    }
}

void StockItem::requestMinlineData()
{
}

int StockItem::OnMouseEvent(MouseEventType type, int x, int y, void *hWnd, int flag)
{
    CWnd *pWnd = CWnd::FromHandle((HWND)hWnd);
    LogX(L"OnMouseEvent: %d", type);
    switch (type)
    {
    case IPluginItem::MT_RCLICKED:
        Stock::Instance().ShowContextMenu(pWnd);
        return 1;

    case IPluginItem::MT_LCLICKED:
    {
        if (stock_id.find(kSZ) == 0 || stock_id.find(kBJ) == 0 || stock_id.find(kSH) == 0)
        {
            CPoint ptScreen = CPoint(x, y);
            Stock::Instance().ShowFloatingWnd(hWnd, ptScreen, stock_id);
            return 1;
        }
        else
        {
            MessageBox((HWND)hWnd, g_data.StringRes(IDS_UNSUPPORT_SHOW_KLINE_STOCK_TIP), g_data.StringRes(IDS_PLUGIN_NAME), MB_ICONINFORMATION | MB_OK);
        }
    }
    default:
        break;
    }
    return 0;
}
