#include "pch.h"
#include "StockItem.h"
#include "DataManager.h"
#include "Stock.h"
#include "Common.h"
#include <algorithm>
#undef min
#undef max

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
    item_id = L"qL0KmmYi";
    item_id += std::to_wstring(index);
    return item_id.c_str();
}

const wchar_t* StockItem::GetItemLableText() const
{
    return L"";
}

const wchar_t* StockItem::GetItemValueText() const
{
    StockInfo& StockInfo = g_data.GetStockInfo(stock_id);
    static std::wstring current;
    current = StockInfo.ToString();
    return current.c_str();
}

bool StockItem::IsCustomDraw() const
{
	return true;
}
int StockItem::GetItemWidthEx(void* hDC) const
{
	CDC* pDC = CDC::FromHandle((HDC)hDC);
    return std::max(pDC->GetTextExtent(g_data.GetStockInfo(stock_id).ToString().c_str()).cx, pDC->GetTextExtent(GetItemValueSampleText()).cx);
}

void StockItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
	//绘图句柄
	CDC* pDC = CDC::FromHandle((HDC)hDC);
	//矩形区域

	StockInfo& gpInfo = g_data.GetStockInfo(stock_id);
	size_t _long = gpInfo.name.length();

	CRect rect(CPoint(x, y), CSize(w, h));
	std::wstring current{ gpInfo.ToString() };

	if (gpInfo.ToString().find('-') != -1) {
		pDC->SetTextColor(RGB(46, 139, 87));//设置文字的颜色为绿色
	}
	else {
		pDC->SetTextColor(RGB(255, 0, 0));//设置文字的颜色为红色
	}

	pDC->DrawText(current.c_str(), rect, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

const wchar_t* StockItem::GetItemValueSampleText() const
{
    return L"----------: 0000000.00 00.00%";
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
