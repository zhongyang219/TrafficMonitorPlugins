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
