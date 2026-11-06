#include "pch.h"
#include "OverviewPanel.h"
#include "ChartColors.h"
#include "Common.h"
#include "DataManager.h"
#include "Stock.h"
#include <algorithm>
#include <mutex>

bool CompareStockPriority(const std::wstring& a, const std::wstring& b)
{
	int priA = GetStockPriority(a);
	int priB = GetStockPriority(b);
	if (priA != priB)
		return priA < priB;
	// 同为个股时，按总成本（成本×持股）从大到小排序
	if (priA >= 200)
	{
		double totalCostA = g_data.GetCostPrice(a) * g_data.GetHoldingCount(a);
		double totalCostB = g_data.GetCostPrice(b) * g_data.GetHoldingCount(b);
		if (totalCostA != totalCostB)
			return totalCostA > totalCostB;
	}
	return false;
}

// 根据百分比值获取背景颜色（用于涨幅和盈亏列）
COLORREF GetCellBgColor(double percent)
{
	if (percent >= 10.0)
		return COLOR_BG_PURPLE;
	else if (percent >= 5.0)
		return COLOR_BG_RED;
	else if (percent <= -10.0)
		return COLOR_BG_DARK_GREEN;
	else if (percent <= -5.0)
		return COLOR_BG_GREEN;
	return COLOR_WHITE;  // 默认白色背景
}

// ========== DrawIndexSection ==========
// 绘制大盘指数区域：每个指数占一列，显示名称(黑色)、当前价格(红涨绿跌)、涨跌额和涨跌幅(红涨绿跌)
void COverviewPanel::DrawIndexSection(CDC& memDC, int x, int y, int w, const std::vector<std::pair<std::wstring, STOCK::StockInfo>>& indices)
{
	if (indices.empty())
		return;

	const int indexCount = (int)indices.size();
	const int colWidth = w / indexCount;
	const int sectionHeight = g_data.RDPI(56);

	// 背景
	memDC.FillSolidRect(x, y, w, sectionHeight, RGB(245, 245, 245));

	memDC.SetBkMode(TRANSPARENT);

	// 创建大号字体用于显示价格
	LOGFONT lf;
	memset(&lf, 0, sizeof(LOGFONT));
	lf.lfHeight = g_data.RDPI(22);
	lf.lfWeight = FW_BOLD;
	wcscpy_s(lf.lfFaceName, _T("Microsoft YaHei"));
	CFont largeFont;
	largeFont.CreateFontIndirect(&lf);

	// 获取两种字体的高度，用于均匀分配垂直空间
	TEXTMETRIC tmLarge, tmNormal;
	CFont* pOldFont = memDC.SelectObject(&largeFont);
	memDC.GetTextMetrics(&tmLarge);
	int largeFontHeight = tmLarge.tmHeight;

	memDC.SelectObject(pOldFont);
	memDC.GetTextMetrics(&tmNormal);
	int normalFontHeight = tmNormal.tmHeight;

	for (int i = 0; i < indexCount; i++)
	{
		const auto& code = indices[i].first;
		const auto& info = indices[i].second;

		int colX = x + i * colWidth;
		int centerX = colX + colWidth / 2;

		double displayPrice = info.currentPrice > 0 ? info.currentPrice : info.prevClosePrice;
		double diff = displayPrice - info.prevClosePrice;
		double diffPercent = info.prevClosePrice != 0 ? (diff / info.prevClosePrice) * 100 : 0;

		COLORREF priceColor = CCommon::GetProfitLossColor(diffPercent);

		// 均匀分配垂直空间：名称、价格、涨跌幅三者等间距排列
		int totalTextHeight = normalFontHeight + largeFontHeight + normalFontHeight;
		int gap = (sectionHeight - totalTextHeight) / 3;
		int nameY = y + gap;
		int priceY = nameY + normalFontHeight + gap;
		int changeY = priceY + largeFontHeight + gap;

		// 名称（黑色，普通字体）
		CString name = info.GetStockListName();
		memDC.SelectObject(pOldFont);
		CSize nameSz = memDC.GetTextExtent(name);
		memDC.SetTextColor(COLOR_BLACK);
		memDC.TextOut(centerX - nameSz.cx / 2, nameY, name);

		// 当前价格（红涨绿跌，大号字体）
		memDC.SelectObject(&largeFont);
		CString priceStr;
		priceStr.Format(_T("%.2f"), displayPrice);
		CSize priceSz = memDC.GetTextExtent(priceStr);
		memDC.SetTextColor(priceColor);
		memDC.TextOut(centerX - priceSz.cx / 2, priceY, priceStr);

		// 涨跌额和涨跌幅（红涨绿跌，普通字体）
		memDC.SelectObject(pOldFont);
		CString changeStr;
		if (diff >= 0)
			changeStr.Format(_T("+%.2f  +%.2f%%"), diff, diffPercent);
		else
			changeStr.Format(_T("%.2f  %.2f%%"), diff, diffPercent);

		CSize changeSz = memDC.GetTextExtent(changeStr);
		memDC.SetTextColor(priceColor);
		memDC.TextOut(centerX - changeSz.cx / 2, changeY, changeStr);
	}

	memDC.SelectObject(pOldFont);
}

void COverviewPanel::DrawOverviewTable(CDC& memDC, int x, int y, int w, int h, int vScrollOffset, int totalHeight,
	std::vector<OverviewRowInfo>& outRows)
{
	const int headerHeight = g_data.RDPI(26);
	const int colCount = 12;
	const CString headers[] = {
		_T("名称"), _T("昨收"), _T("现价"), _T("涨额"), _T("涨幅"),
		_T("成本"), _T("持股"), _T("仓值"), _T("市值"), _T("收益"), _T("收益率"), _T("操作")
	};

	// 绘制表头背景
	memDC.FillSolidRect(x, y, w, headerHeight, RGB(230, 230, 230));

	// 计算列宽
	int colWidths[12] = { 0 };
	colWidths[11] = g_data.RDPI(40);
	int totalWidth = colWidths[11];

	for (int i = 0; i < colCount - 1; i++)
	{
		CSize sz = memDC.GetTextExtent(headers[i]);
		colWidths[i] = sz.cx + g_data.RDPI(10);
		totalWidth += colWidths[i];
	}

	// 按内容调整列宽
	struct StockRowData {
		std::wstring code;  // 股票代码
		CString name;
		CString prevClose;
		CString current;
		CString changeAmount;
		CString changePercent;
		CString cost;
		CString holding;
		CString totalCost;
		CString marketValue;
		CString profitLoss;
		CString profitLossPercent;
		COLORREF changeColor;
		COLORREF profitColor;
		bool isIndex;  // 标记是否是指数
		double changePercentValue;  // 涨幅百分比数值
		double profitLossPercentValue;  // 盈亏百分比数值
	};

	std::vector<StockRowData> rows;
	auto stockCodes = g_data.m_setting_data.m_stock_codes;  // 拷贝一份用于排序

	// 对股票列表进行排序，优先展示大盘指数
	std::sort(stockCodes.begin(), stockCodes.end(), CompareStockPriority);

	{
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);

		for (const auto& code : stockCodes)
		{
			auto stockData = g_data.GetStockData(code);
			if (!stockData)
				continue;

			const auto& info = stockData->info;
			if (!info.is_ok)
				continue;

			// 跳过指数（已在顶部独立区域显示）
			if (GetStockPriority(code) < 200)
				continue;

			StockRowData row;
			row.code = code;  // 保存股票代码
			row.name = info.GetStockListName();
			row.isIndex = false;  // 表格中不再包含指数

			double costPrice = g_data.GetCostPrice(code);
			double holdingCount = g_data.GetHoldingCount(code);

			// 当现价为0时，使用昨收价作为显示价格
			double displayPrice = info.currentPrice > 0 ? info.currentPrice : info.prevClosePrice;

			// 大盘股（指数）价格格式化为整数显示
			if (row.isIndex)
			{
				row.prevClose.Format(_T("%.0f"), info.prevClosePrice);
				row.current.Format(_T("%.0f"), displayPrice);
			}
			else
			{
				row.prevClose = CCommon::FormatFloat(info.prevClosePrice);
				row.current = CCommon::FormatFloat(displayPrice);
			}

			double diff = displayPrice - info.prevClosePrice;
			double diffPercent = info.prevClosePrice != 0 ? (diff / info.prevClosePrice) * 100 : 0;
			row.changeColor = diff > 0 ? COLOR_RED_UP : (diff < 0 ? COLOR_GREEN_DOWN : COLOR_BLACK);
			row.changePercentValue = diffPercent;  // 保存涨幅数值

			if (diff >= 0)
				row.changePercent.Format(_T("+%.2f%%"), diffPercent);
			else
				row.changePercent.Format(_T("%.2f%%"), diffPercent);

			// 大盘股（指数）涨额格式化为整数显示
			if (row.isIndex)
			{
				if (diff >= 0)
					row.changeAmount.Format(_T("+%.0f"), diff);
				else
					row.changeAmount.Format(_T("%.0f"), diff);
			}
			else
			{
				if (diff >= 0)
					row.changeAmount.Format(_T("+%s"), CCommon::FormatFloat(diff));
				else
					row.changeAmount.Format(_T("%s"), CCommon::FormatFloat(diff));
			}

			if (costPrice > 0)
				row.cost = CCommon::FormatFloat(costPrice);
			else
				row.cost = _T("--");

			if (holdingCount > 0)
				row.holding = CCommon::FormatVolumeInt(holdingCount);
			else
				row.holding = _T("--");

			if (costPrice > 0 && holdingCount > 0)
				row.totalCost = CCommon::FormatAmount(costPrice * holdingCount);
			else
				row.totalCost = _T("--");

			if (holdingCount > 0 && displayPrice > 0)
				row.marketValue = CCommon::FormatAmount(displayPrice * holdingCount);
			else
				row.marketValue = _T("--");

			if (costPrice > 0 && holdingCount > 0 && displayPrice > 0)
			{
				double totalCost = costPrice * holdingCount;
				double marketValue = displayPrice * holdingCount;
				double profitLoss = marketValue - totalCost;
				double profitLossPercent = totalCost != 0 ? (profitLoss / totalCost) * 100 : 0;
				row.profitColor = profitLoss > 0 ? COLOR_RED_UP : (profitLoss < 0 ? COLOR_GREEN_DOWN : COLOR_BLACK);
				row.profitLossPercentValue = profitLossPercent;  // 保存盈亏数值

				CString formattedAmount = CCommon::FormatAmount(abs(profitLoss));
				if (profitLoss >= 0)
					row.profitLoss = _T("+") + formattedAmount;
				else
					row.profitLoss = _T("-") + formattedAmount;

				if (profitLossPercent >= 0)
					row.profitLossPercent.Format(_T("+%.2f%%"), profitLossPercent);
				else
					row.profitLossPercent.Format(_T("%.2f%%"), profitLossPercent);
			}
			else
			{
				row.profitLoss = _T("--");
				row.profitLossPercent = _T("--");
				row.profitColor = COLOR_BLACK;
				row.profitLossPercentValue = 0;  // 默认0
			}

			rows.push_back(row);

			// 更新列宽
			CString allFields[] = { row.name, row.prevClose, row.current,
				row.changeAmount, row.changePercent, row.cost, row.holding,
				row.totalCost, row.marketValue, row.profitLoss, row.profitLossPercent };
			for (int i = 0; i < colCount - 1; i++)
			{
				CSize sz = memDC.GetTextExtent(allFields[i]);
				int needed = sz.cx + g_data.RDPI(10);
				if (needed > colWidths[i])
					colWidths[i] = needed;
			}
		}
	}

	totalWidth = 0;
	for (int i = 0; i < colCount; i++)
		totalWidth += colWidths[i];

	// 将剩余空间分配给各列，铺满窗口宽度
	if (totalWidth < w)
	{
		int extra = w - totalWidth;
		for (int i = 0; i < colCount - 1; i++)  // 操作列不参与分配
		{
			colWidths[i] += extra / (colCount - 1);
		}
		// 余数分配给第一列（名称列）
		colWidths[0] += extra % (colCount - 1);
		totalWidth = w;
	}

	// 计算总行数和总高度
	int totalRows = (int)rows.size();
	int totalTableH = headerHeight + totalRows * headerHeight;

	// 计算可滚动范围
	int maxScrollOffset = max(0, totalTableH - h);

	// 限制滚动偏移
	if (vScrollOffset < 0) vScrollOffset = 0;
	if (vScrollOffset > maxScrollOffset) vScrollOffset = maxScrollOffset;

	// 绘制表头
	int currentX = x;
	memDC.SetTextColor(COLOR_BLACK);
	memDC.SetBkMode(TRANSPARENT);
	CPen gridPen(PS_SOLID, 1, COLOR_GRAY_GRID);
	memDC.SelectObject(&gridPen);

	// 计算列起始位置，用于后续绘制竖线
	std::vector<int> colStartX;
	colStartX.reserve(colCount);
	for (int i = 0; i < colCount; i++)
	{
		colStartX.push_back(currentX);
		CSize sz = memDC.GetTextExtent(headers[i]);
		int txtX = currentX + (colWidths[i] - sz.cx) / 2;
		memDC.TextOut(txtX, y + g_data.RDPI(3), headers[i]);
		currentX += colWidths[i];
	}

	// 绘制数据行（裁剪到表头以下区域，防止滚动时覆盖表头）
	CRect dataClipRect(x, y + headerHeight, x + w, y + h);
	int savedDC = memDC.SaveDC();
	memDC.IntersectClipRect(&dataClipRect);

	int rowY = y + headerHeight - vScrollOffset;
	int rowIndex = 0;

	// 记录行信息用于双击处理
	outRows.clear();
	outRows.reserve(rows.size());

	for (const auto& row : rows)
	{
		// 行高
		int rowH = headerHeight;

		// 计算删除按钮位置
		int deleteBtnStartX = totalWidth - colWidths[colCount - 1];
		int deleteBtnEndX = totalWidth;

		// 保存行信息（考虑滚动偏移）
		OverviewRowInfo info;
		info.code = row.code;
		info.rowY = rowY + vScrollOffset;
		info.rowH = rowH;
		info.nameColWidth = colWidths[0];
		info.deleteBtnStartX = deleteBtnStartX;
		info.deleteBtnEndX = deleteBtnEndX;
		outRows.push_back(info);

		// 如果当前行超出可视区域，跳过绘制
		if (rowY + rowH < y || rowY >= y + h)
		{
			rowY += rowH;
			rowIndex++;
			continue;
		}

		// 交替行背景
		if (rowIndex % 2 == 1)
		{
			memDC.FillSolidRect(x, rowY, w, rowH, RGB(250, 250, 250));
		}

		currentX = x;
		CString fields[] = { row.name, row.prevClose, row.current,
			row.changeAmount, row.changePercent, row.cost, row.holding,
			row.totalCost, row.marketValue, row.profitLoss, row.profitLossPercent };

		for (int i = 0; i < colCount; i++)
		{
			COLORREF textColor = COLOR_BLACK;
			COLORREF bgColor = COLOR_WHITE;  // 默认白色背景

			if (i == colCount - 1) // 操作列（删除按钮）
			{
				// 绘制删除按钮背景
				CBrush btnBrush(RGB(230, 230, 230));
				memDC.FillRect(CRect(currentX + g_data.RDPI(4), rowY + g_data.RDPI(3),
					currentX + colWidths[i] - g_data.RDPI(4), rowY + rowH - g_data.RDPI(3)), &btnBrush);

				// 绘制删除文字
				CString delText = _T("删除");
				memDC.SetTextColor(COLOR_BLACK);
				CSize sz = memDC.GetTextExtent(delText);
				int txtX = currentX + (colWidths[i] - sz.cx) / 2;
				memDC.TextOut(txtX, rowY + g_data.RDPI(3), delText);
				currentX += colWidths[i];
				continue;
			}

			if (i == 3) // 涨额列
			{
				textColor = row.changeColor;
			}
			else if (i == 4) // 涨幅列
			{
				textColor = row.changeColor;
				bgColor = GetCellBgColor(row.changePercentValue);
			}
			else if (i == 9) // 盈亏列
			{
				textColor = row.profitColor;
			}
			else if (i == 10) // 盈比列
			{
				textColor = row.profitColor;
				bgColor = GetCellBgColor(row.profitLossPercentValue);
			}

			// 如果需要特殊背景色，先绘制背景
			if (bgColor != COLOR_WHITE)
			{
				CBrush bgBrush(bgColor);
				int paddingX = (i == 0) ? g_data.RDPI(3) : 0;
				memDC.FillRect(CRect(currentX + paddingX, rowY, currentX + colWidths[i] - paddingX, rowY + rowH), &bgBrush);
				textColor = COLOR_WHITE;  // 有背景色时使用白色文字
			}

			memDC.SetTextColor(textColor);

			CSize sz = memDC.GetTextExtent(fields[i]);
			int txtX;
			if (i == 0) // 名称列左对齐
				txtX = currentX + g_data.RDPI(3);
			else // 其余数字列右对齐
				txtX = currentX + colWidths[i] - sz.cx - g_data.RDPI(3);

			memDC.TextOut(txtX, rowY + g_data.RDPI(3), fields[i]);
			currentX += colWidths[i];
		}

		rowY += rowH;
		rowIndex++;
	}

	// 恢复DC（取消裁剪区域）
	memDC.RestoreDC(savedDC);

	// 绘制汇总行：总成本、总市值、总盈亏、收益率（在横线之前绘制，避免覆盖横线）
	double sumCost = 0, sumMarket = 0;
	for (const auto& row : rows)
	{
		if (row.isIndex) continue;
		auto stockData = g_data.GetStockData(row.code);
		if (!stockData) continue;
		double costPrice = g_data.GetCostPrice(row.code);
		double holdingCount = g_data.GetHoldingCount(row.code);
		double displayPrice = stockData->info.currentPrice > 0 ? stockData->info.currentPrice : stockData->info.prevClosePrice;
		if (costPrice > 0 && holdingCount > 0)
		{
			sumCost += costPrice * holdingCount;
			if (displayPrice > 0)
				sumMarket += displayPrice * holdingCount;
		}
	}

	if (sumCost > 0 || sumMarket > 0)
	{
		double sumProfit = sumMarket - sumCost;
		double profitRate = sumCost > 0 ? (sumProfit / sumCost) * 100 : 0;

		CString costStr = CCommon::FormatAmount(sumCost);
		CString marketStr = CCommon::FormatAmount(sumMarket);
		CString profitStr = CCommon::FormatAmount(abs(sumProfit));
		if (sumProfit >= 0)
			profitStr = _T("+") + profitStr;
		else
			profitStr = _T("-") + profitStr;

		CString rateStr;
		if (profitRate >= 0)
			rateStr.Format(_T("+%.2f%%"), profitRate);
		else
			rateStr.Format(_T("%.2f%%"), profitRate);

		COLORREF rateColor = CCommon::GetProfitLossColor(profitRate);

		// 总盈亏颜色
		COLORREF profitColor = sumProfit > 0 ? COLOR_RED_UP : (sumProfit < 0 ? COLOR_GREEN_DOWN : COLOR_BLACK);

		// 分段计算总宽度以实现居中
		struct TextSeg { CString text; COLORREF color; };
		TextSeg segs[] = {
			{ _T("总成本: "), COLOR_BLACK }, { costStr, COLOR_BLACK },
			{ _T("  总市值: "), COLOR_BLACK }, { marketStr, COLOR_BLACK },
			{ _T("  总收益: "), COLOR_BLACK }, { profitStr, profitColor },
			{ _T("  收益率: "), COLOR_BLACK }, { rateStr, rateColor }
		};
		int totalWidth = 0;
		for (const auto& seg : segs)
			totalWidth += memDC.GetTextExtent(seg.text).cx;

		// 绘制到对话框最底部作为状态栏（使用 totalHeight 确保紧贴底部）
		CSize textSize = memDC.GetTextExtent(_T("Ay"));
		const int statusBarHeight = textSize.cy + g_data.RDPI(6);
		int summaryY = totalHeight - statusBarHeight;
		memDC.FillSolidRect(x, summaryY, w, statusBarHeight, RGB(240, 240, 240));
		memDC.SetBkMode(TRANSPARENT);
		int drawX = x + max(0, (w - totalWidth) / 2);
		int textY = summaryY + g_data.RDPI(3);
		for (const auto& seg : segs)
		{
			memDC.SetTextColor(seg.color);
			memDC.TextOut(drawX, textY, seg.text);
			drawX += memDC.GetTextExtent(seg.text).cx;
		}
	}

	// 在所有背景和文本绘制完成后，统一画横线和竖线（避免被FillSolidRect覆盖）
	memDC.SelectObject(&gridPen);

	// 画横线（从表头顶部开始，按headerHeight间隔绘制）
	for (int lineY = y; lineY <= y + h; lineY += headerHeight)
	{
		memDC.MoveTo(x, lineY);
		memDC.LineTo(x + w, lineY);
	}

	// 画竖线（画到表格区域底部，与横线对齐）
	for (int i = 1; i <= colCount; i++)
	{
		int lineX = colStartX[0];
		for (int j = 1; j <= i; j++)
			lineX += colWidths[j - 1];
		memDC.MoveTo(lineX, y);
		memDC.LineTo(lineX, y + h);
	}
}