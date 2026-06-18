#include "pch.h"
#include "StockItem.h"
#include "DataManager.h"
#include "Stock.h"
#include "Common.h"
#include <algorithm>
#include "FloatingWnd.h"
#undef min
#undef max

const wchar_t* StockItem::GetItemName() const
{
	static std::wstring item_name;
	auto data = g_data.GetStockData(stock_id);
	if (data->info.is_ok)
	{
		if (data)
		{
			item_name = data->info.displayName;
		}
		else
		{
			item_name = g_data.StringRes(IDS_PLUGIN_ITEM_NAME).GetString();
			item_name += std::to_wstring(index);
		}
	}
	else
	{
		item_name = stock_id + L" " + g_data.StringRes(IDS_LOAD_FAIL).GetString();
	}
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
	return L"";
}

bool StockItem::IsCustomDraw() const
{
	return true;
}
int StockItem::GetItemWidthEx(void* hDC) const
{
	CDC* pDC = CDC::FromHandle((HDC)hDC);
	int width;
	if (g_data.m_setting_data.m_show_fluctuation)
	{
		width = pDC->GetTextExtent(_T("股票:99.99 +99.99%(15.55) ")).cx;
	}
	else
	{
		width = pDC->GetTextExtent(_T("股票:99.99 +99.99% ")).cx;
	}
	char buff[32];
	sprintf_s(buff, "GetItemWidthEx %d", width);

	// CCommon::WriteLog(CCommon::StrToUnicode(buff).c_str(), g_data.m_log_path.c_str());
	LogX(L"GetItemWidthEx: %d\n", width);
	return width;
}

void StockItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
	// 绘图句柄
	CDC* pDC = CDC::FromHandle((HDC)hDC);

	// 矩形区域
	auto data = g_data.GetStockData(stock_id);
	CRect rect(CPoint(x, y), CSize(w, h));

	// 文本颜色
	COLORREF color_default;
	COLORREF color_red;
	COLORREF color_green;
	COLORREF color_purple;
	COLORREF color_dark_green;
	if (dark_mode)
	{
		color_default = RGB(255, 255, 255);
		color_red = RGB(255, 121, 120);
		color_green = RGB(111, 215, 149);
		color_purple = RGB(199, 125, 255);
		color_dark_green = RGB(0, 150, 100);
	}
	else
	{
		color_default = RGB(0, 0, 0);
		color_red = RGB(195, 0, 0);
		color_green = RGB(46, 139, 87);
		color_purple = RGB(148, 0, 211);
		color_dark_green = RGB(0, 100, 0);
	}

	CRect rect_value{ rect };
	if (data->info.is_ok && g_data.m_setting_data.m_show_stock_name)
	{
		// 绘制名称
		pDC->SetTextColor(color_default);
		//CString stock_name(data->info.displayName.c_str(), 2); //CString stock_name{data->info.displayName.c_str()};
		CString stock_name = data->info.GetStockShortName();
		stock_name += _T(": ");
		CRect rect_name{ rect };
		rect_name.right = rect_name.left + pDC->GetTextExtent(stock_name).cx;
		pDC->DrawText(stock_name, rect_name, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

		rect_value.left = rect_name.right;
	}

	// 计算涨跌幅百分比
	float fluctuation_percent = 0.0f;
	if (data->info.prevClosePrice != 0)
	{
		fluctuation_percent = (data->info.currentPrice - data->info.prevClosePrice) / data->info.prevClosePrice * 100;
	}

	// 根据涨跌幅幅度设置颜色
	COLORREF price_color = color_default;
	if (fluctuation_percent > 0)
	{
		if (fluctuation_percent >= 5)
			price_color = color_purple;
		else
			price_color = color_red;
	}
	else if (fluctuation_percent < 0)
	{
		if (fluctuation_percent <= -5)
			price_color = color_dark_green;
		else
			price_color = color_green;
	}

	// 绘制价格（左对齐）
	pDC->SetTextColor(price_color);
	CString strPrice = data->info.displayPrice.c_str();
	CRect rect_price = rect_value;
	rect_price.right = rect_price.left + 33;
	pDC->DrawText(strPrice, rect_price, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

	// 设置涨跌幅/涨跌额文本颜色
	if (g_data.m_setting_data.m_color_with_price)
	{
		if (fluctuation_percent > 0)
		{
			if (fluctuation_percent >= 5)
				pDC->SetTextColor(color_purple);
			else
				pDC->SetTextColor(color_red);
		}
		else if (fluctuation_percent < 0)
		{
			if (fluctuation_percent <= -5)
				pDC->SetTextColor(color_dark_green);
			else
				pDC->SetTextColor(color_green);
		}
		else
		{
			pDC->SetTextColor(color_default);
		}
	}
	else
	{
		pDC->SetTextColor(color_default);
	}

	// 绘制涨跌幅百分比（始终显示）
	CString strDiff;
	if (fluctuation_percent >= 0)
		strDiff.Format(_T("+%s"), data->info.displayFluctuation.c_str());
	else
		strDiff.Format(_T("-%s"), data->info.displayFluctuation.c_str());
	CRect rect_diff = rect_value;
	rect_diff.left = rect_value.left + 36; // 价格结束位置
	rect_diff.right = rect_diff.left + pDC->GetTextExtent(strDiff).cx;
	pDC->DrawText(strDiff, rect_diff, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

	// 绘制涨跌额（根据配置决定是否显示）
	if (g_data.m_setting_data.m_show_fluctuation)
	{
		CRect rect_fluctuation{ rect_value };
		rect_fluctuation.left = rect_diff.right; // 紧接在涨跌幅后面
		pDC->DrawText(data->info.displayFluctuationDiff.c_str(), rect_fluctuation, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	}
}

const wchar_t* StockItem::GetItemValueSampleText() const
{
	//    if (g_data.m_setting_data.m_show_stock_name)
	//    {
	//        return L"--------: 0000000.00 +00.00%";
	//    }
	//    else
	//    {
	//        return L"0000000.00 +00.00%";
	//    }
	return L"";
}

int StockItem::OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag)
{
	CWnd* pWnd = CWnd::FromHandle((HWND)hWnd);
	LogX(L"OnMouseEvent: %d\n", type);
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