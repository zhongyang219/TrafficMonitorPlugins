#include "pch.h"
#include "StockListPanel.h"
#include "ChartColors.h"
#include "Common.h"
#include "DataManager.h"
#include <Stock.h>
#include <mutex>
#include <vector>

void CStockListPanel::Draw(CDC& memDC, int x, int y, int w, int h, const std::wstring& currentStockId)
{
	// 绘制背景
	memDC.FillSolidRect(x, y, w, h, RGB(245, 245, 245));

	// 绘制标题栏（与走势图标题栏高度一致）
	const int titleH = g_data.RDPI(16);
	memDC.FillSolidRect(x, y, w, titleH, RGB(230, 230, 230));
	memDC.SetTextColor(COLOR_BLACK);
	memDC.SetBkMode(TRANSPARENT);
	memDC.TextOut(x + g_data.RDPI(4), y + g_data.RDPI(2), _T("股票列表"));

	// 绘制分隔线
	CPen linePen(PS_SOLID, 1, RGB(200, 200, 200));
	CPen* pOldPen = memDC.SelectObject(&linePen);
	memDC.MoveTo(x, y + titleH);
	memDC.LineTo(x + w, y + titleH);

	// 获取所有股票列表（加锁访问，过滤掉大盘指数和港股）
	std::vector<std::wstring> stockCodes;
	{
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		for (const auto& code : g_data.m_setting_data.m_stock_codes)
		{
			if (GetStockPriority(code) >= 200 && code.find(kHK) != 0)  // 只保留非指数、非港股股票
				stockCodes.push_back(code);
		}
	}

	if (stockCodes.empty())
	{
		// 没有股票时显示提示
		memDC.SetTextColor(RGB(150, 150, 150));
		memDC.TextOut(x + g_data.RDPI(4), y + titleH + g_data.RDPI(10), _T("暂无股票"));
		memDC.SelectObject(pOldPen);
		return;
	}

	// 每行高度固定35像素
	const int rowHeight = g_data.RDPI(35);
	const int nameHeight = g_data.RDPI(14);
	const int codeHeight = g_data.RDPI(7);

	// 绘制股票列表
	int currentY = y + titleH + g_data.RDPI(2);
	for (const auto& code : stockCodes)
	{
		if (currentY + rowHeight > y + h)
			break;  // 超出区域

		// 获取股票名称（GetStockData 内部无需额外加锁）
		std::wstring stockName = code;  // 默认使用代码作为名称
		auto stockData = g_data.GetStockData(code);
		if (stockData && !stockData->info.displayName.empty())
		{
			stockName = stockData->info.displayName;
		}

		// 高亮当前股票
		bool isCurrent = (code == currentStockId);
		if (isCurrent)
		{
			memDC.FillSolidRect(x + 1, currentY, w - 2, rowHeight, RGB(200, 220, 255));
		}

		// 文字垂直居中：内容总高度 = nameHeight + codeHeight，在rowHeight内居中
		int contentH = nameHeight + codeHeight;
		int textOffsetY = (rowHeight - contentH) / 2;

		// 绘制股票名称（上方，超出宽度截断）
		memDC.SetTextColor(isCurrent ? RGB(0, 0, 180) : COLOR_BLACK);
		CFont nameFont;
		nameFont.CreatePointFont(90, _T("Microsoft YaHei"), &memDC);
		CFont* pOldFont = memDC.SelectObject(&nameFont);
		CRect nameRect(x + g_data.RDPI(4), currentY + textOffsetY, x + w - g_data.RDPI(4), currentY + textOffsetY + nameHeight);
		memDC.DrawText(stockName.c_str(), stockName.length(), &nameRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
		memDC.SelectObject(pOldFont);
		nameFont.DeleteObject();

		// 绘制股票代码（下方，字体较小）
		memDC.SetTextColor(isCurrent ? RGB(80, 80, 180) : RGB(100, 100, 100));
		CFont codeFont;
		codeFont.CreatePointFont(70, _T("Microsoft YaHei"), &memDC);
		pOldFont = memDC.SelectObject(&codeFont);
		memDC.TextOut(x + g_data.RDPI(4), currentY + textOffsetY + nameHeight, code.c_str());
		memDC.SelectObject(pOldFont);
		codeFont.DeleteObject();

		// 绘制分隔线
		currentY += rowHeight;
		memDC.MoveTo(x, currentY);
		memDC.LineTo(x + w, currentY);
	}

	memDC.SelectObject(pOldPen);
}