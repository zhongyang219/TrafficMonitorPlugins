#include "pch.h"
#include "ChipPeakPanel.h"
#include "ChartColors.h"
#include "Common.h"
#include "DataManager.h"
#include <algorithm>
#include <cmath>
#include <set>

void CChipPeakPanel::Draw(CDC& memDC, int left, int right, int height, const STOCK::StockInfo& stockInfo,
	const STOCK::ChipDistribution& chipData, const std::vector<STOCK::TimelinePoint>& timelinePoint,
	UIViewMode viewMode)
{
	// 按比例分配行高，与盘口面板一致
	const int totalRows = 19;
	const int headerHeight = g_data.RDPI(26) + g_data.RDPI(20);  // 主标题栏+管理股票栏高度
	const int obTitleH = g_data.RDPI(16);       // 盘口标题栏高度，与走势图标题栏一致
	const int topOffset = headerHeight + obTitleH;  // 内容从主标题栏+盘口标题栏下方开始
	const int panelW = right - left;
	// 绘制盘口标题栏背景（在主标题栏下方）
	memDC.FillSolidRect(left, headerHeight, panelW, obTitleH, RGB(245, 245, 245));
	const int rowHeight = (height - obTitleH) / totalRows;
	const int panelH = height - obTitleH;
	if (panelW <= 0 || panelH <= 0)
		return;
	const int rem = panelH % totalRows;    // 余数：前rem行多1px
	// 辅助：计算第i行(0-based)的Y坐标
	auto rowY = [&](int i) -> int {
		if (i < rem) return topOffset + i * (rowHeight + 1);
		else return topOffset + rem * (rowHeight + 1) + (i - rem) * rowHeight;
		};
	// 辅助：计算第i行的高度
	auto rowH = [&](int i) -> int {
		return (i < rem) ? (rowHeight + 1) : rowHeight;
		};

	memDC.FillSolidRect(left, topOffset, panelW, panelH, RGB(250, 250, 250));
	memDC.SetBkMode(TRANSPARENT);

	if (!chipData.IsValid())
	{
		memDC.SetTextColor(COLOR_GRAY_TEXT);
		memDC.TextOut(left + g_data.RDPI(5), topOffset + max(0, (panelH - memDC.GetTextExtent(_T("暂无筹码数据")).cy) / 2), _T("暂无筹码数据"));
		return;
	}

	std::vector<STOCK::ChipPoint> points = chipData.points;
	if (viewMode < UI_VIEW_MIN5_KLINE && stockInfo.circulatingAShares > 0 && !timelinePoint.empty())
	{
		const double CHIP_ATTRITION_N = 1.3;
		const double MAX_EFFECT_TURN = 0.85;
		const double PRICE_STEP = 0.01;
		const double FLOAT_CORRECT_THRESHOLD = 0.01;

		double yMin = 999999.0;
		double yMax = 0.0;
		for (const auto& point : chipData.points)
		{
			if (point.price > 0)
			{
				yMin = min(yMin, point.price);
				yMax = max(yMax, point.price);
			}
		}

		double limitDown = stockInfo.prevClosePrice > 0 && stockInfo.priceLimit > 0 ? stockInfo.prevClosePrice - stockInfo.priceLimit : yMin;
		double limitUp = stockInfo.prevClosePrice > 0 && stockInfo.priceLimit > 0 ? stockInfo.prevClosePrice + stockInfo.priceLimit : yMax;
		double gridMin = floor(min(yMin, limitDown) * 100.0) / 100.0;
		double gridMax = ceil(max(yMax, limitUp) * 100.0) / 100.0;
		if (gridMax > gridMin)
		{
			std::vector<double> priceLevels;
			for (double cur = gridMin; cur <= gridMax + PRICE_STEP / 2; cur += PRICE_STEP)
				priceLevels.push_back(round(cur * 100.0) / 100.0);
			std::vector<double> chipArray(priceLevels.size(), 0.0);
			auto findPriceIndex = [&](double price) -> int {
				auto it = std::lower_bound(priceLevels.begin(), priceLevels.end(), round(price * 100.0) / 100.0);
				return static_cast<int>(it - priceLevels.begin());
				};

			for (const auto& point : chipData.points)
			{
				int idx = findPriceIndex(point.price);
				if (idx >= 0 && idx < static_cast<int>(chipArray.size()))
					chipArray[idx] += point.percent * stockInfo.circulatingAShares;
			}

			std::set<std::string> processedMinuteSet;
			for (const auto& item : timelinePoint)
			{
				if (item.volume <= 0 || processedMinuteSet.find(item.time) != processedMinuteSet.end())
					continue;
				processedMinuteSet.insert(item.time);

				double minuteTurn = static_cast<double>(item.volume) / static_cast<double>(stockInfo.circulatingAShares);
				double effTurn = min(MAX_EFFECT_TURN, minuteTurn * CHIP_ATTRITION_N);
				double retainRate = 1.0 - effTurn;
				double addTotalShare = effTurn * stockInfo.circulatingAShares;
				for (auto& val : chipArray)
					val *= retainRate;

				double price = item.price > 0 ? item.price : item.averagePrice;
				int idx = findPriceIndex(price);
				if (idx >= 0 && idx < static_cast<int>(chipArray.size()))
					chipArray[idx] += addTotalShare;

				double sumAll = 0.0;
				for (auto val : chipArray)
					sumAll += val;
				if (sumAll > 0 && fabs(sumAll - stockInfo.circulatingAShares) > FLOAT_CORRECT_THRESHOLD)
				{
					double scale = static_cast<double>(stockInfo.circulatingAShares) / sumAll;
					for (auto& val : chipArray)
						val *= scale;
				}
			}

			double totalShares = static_cast<double>(stockInfo.circulatingAShares);
			points.clear();
			points.reserve(priceLevels.size());
			double weightSum = 0.0;
			double profitShare = 0.0;
			double currentPrice = stockInfo.currentPrice > 0 ? stockInfo.currentPrice : stockInfo.prevClosePrice;
			for (size_t i = 0; i < priceLevels.size(); ++i)
			{
				weightSum += priceLevels[i] * chipArray[i];
				if (priceLevels[i] < currentPrice)
					profitShare += chipArray[i];
				STOCK::ChipPoint point;
				point.price = priceLevels[i];
				point.percent = totalShares > 0 ? chipArray[i] / totalShares : 0.0;
				points.push_back(point);
			}
		}
	}

	double totalPercent = 0.0;
	double weightSum = 0.0;
	double profitPercent = 0.0;
	double maxPercent = 0.0;
	double currentPrice = stockInfo.currentPrice > 0 ? stockInfo.currentPrice : stockInfo.prevClosePrice;
	for (const auto& point : points)
	{
		if (point.percent <= 0) continue;
		totalPercent += point.percent;
		weightSum += point.price * point.percent;
		if (point.price < currentPrice)
			profitPercent += point.percent;
		maxPercent = max(maxPercent, point.percent);
	}
	if (totalPercent <= 0 || maxPercent <= 0)
		return;

	double avgCost = weightSum / totalPercent;
	double cumPercent = 0.0;
	double chip90Low = 0.0;
	double chip90High = 0.0;
	bool findLow = false;
	std::sort(points.begin(), points.end(), [](const STOCK::ChipPoint& a, const STOCK::ChipPoint& b) { return a.price < b.price; });
	for (const auto& point : points)
	{
		cumPercent += point.percent;
		if (!findLow && cumPercent >= totalPercent * 0.05)
		{
			chip90Low = point.price;
			findLow = true;
		}
		if (cumPercent >= totalPercent * 0.95)
		{
			chip90High = point.price;
			break;
		}
	}

	// 筹码峰图：从行0开始，到底部倒数第3行
	const int chartRowEnd = totalRows - 3;
	const int chartTop = topOffset + g_data.RDPI(2);
	const int chartBottom = rowY(chartRowEnd);
	const int chartLeft = left + g_data.RDPI(6);
	const int chartRight = right - g_data.RDPI(6);
	const int chartH = chartBottom - chartTop;
	const int chartW = chartRight - chartLeft;
	if (chartH <= 0 || chartW <= 0)
		return;

	double minPrice = points.front().price;
	double maxPrice = points.back().price;
	if (maxPrice <= minPrice)
		return;

	CPen borderPen(PS_SOLID, 1, RGB(220, 220, 220));
	CPen* oldPen = memDC.SelectObject(&borderPen);
	memDC.Rectangle(chartLeft, chartTop, chartRight, chartBottom);
	memDC.SelectObject(oldPen);

	auto priceToY = [&](double price) -> int {
		return chartBottom - static_cast<int>((price - minPrice) / (maxPrice - minPrice) * chartH);
		};

	for (const auto& point : points)
	{
		if (point.percent <= 0) continue;
		int y = priceToY(point.price);
		int barW = max(1, static_cast<int>(point.percent / maxPercent * chartW));
		COLORREF color = point.price < currentPrice ? COLOR_RED_UP : COLOR_GREEN_DOWN;
		memDC.FillSolidRect(chartLeft, y, barW, max(1, g_data.RDPI(1)), color);
	}

	if (avgCost > minPrice && avgCost < maxPrice)
	{
		int avgY = priceToY(avgCost);
		CPen avgPen(PS_DOT, 1, RGB(0, 80, 204));
		oldPen = memDC.SelectObject(&avgPen);
		memDC.MoveTo(chartLeft, avgY);
		memDC.LineTo(chartRight, avgY);
		memDC.SelectObject(oldPen);
		// 均价标签绘制在线条右侧
		CString avgLabel;
		avgLabel.Format(_T("均:%s"), CCommon::FormatFloat(avgCost));
		memDC.SetTextColor(RGB(0, 80, 204));
		CSize avgLabelSize = memDC.GetTextExtent(avgLabel);
		int avgLabelX = chartRight - avgLabelSize.cx - g_data.RDPI(2);
		int avgLabelY = avgY - avgLabelSize.cy / 2;
		// 避免标签超出图表区域
		avgLabelY = max(chartTop, min(avgLabelY, chartBottom - avgLabelSize.cy));
		memDC.TextOut(avgLabelX, avgLabelY, avgLabel);
	}

	if (currentPrice > minPrice && currentPrice < maxPrice)
	{
		int y = priceToY(currentPrice);
		CPen curPen(PS_DOT, 1, RGB(112, 32, 176));
		oldPen = memDC.SelectObject(&curPen);
		memDC.MoveTo(chartLeft, y);
		memDC.LineTo(chartRight, y);
		memDC.SelectObject(oldPen);
		CString priceTxt;
		priceTxt.Format(_T("现 %s"), CCommon::FormatFloat(currentPrice));
		CSize txtSize = memDC.GetTextExtent(priceTxt);
		int paddingX = g_data.RDPI(4);
		int paddingY = g_data.RDPI(2);
		int labelW = txtSize.cx + paddingX * 2;
		int labelH = txtSize.cy + paddingY * 2;
		int labelLeft = chartRight - labelW - g_data.RDPI(2);
		int labelTop = min(max(chartTop, y - labelH / 2), chartBottom - labelH);
		CRect labelRect(labelLeft, labelTop, labelLeft + labelW, labelTop + labelH);
		memDC.SetTextColor(COLOR_BLACK);
		memDC.TextOut(labelRect.left + paddingX, labelRect.top + paddingY, priceTxt);
	}

	CString highTxt;
	highTxt = CCommon::FormatFloat(maxPrice);
	CString lowTxt;
	lowTxt = CCommon::FormatFloat(minPrice);
	memDC.SetTextColor(COLOR_GRAY_TEXT);
	memDC.TextOut(chartRight - memDC.GetTextExtent(highTxt).cx, chartTop, highTxt);
	memDC.TextOut(chartRight - memDC.GetTextExtent(lowTxt).cx, chartBottom - memDC.GetTextExtent(lowTxt).cy, lowTxt);

	// 文字信息绘制在筹码峰图下方，拆分为3行
	memDC.SetTextColor(COLOR_GRAY_TEXT);
	// 获利比例：标签 + 带红绿背景的数字
	// 左红（亏损比例）右绿（获利比例），分界点由获利比例决定，红绿各自长度按比例分配
	CString profitLabelTxt = _T("获利比例:");
	double profitRatio = totalPercent > 0 ? (profitPercent / totalPercent * 100.0) : 0.0;
	CString profitNumTxt;
	profitNumTxt.Format(_T("%.1f%%"), profitRatio);
	int profitY = rowY(chartRowEnd) + max(0, (rowH(chartRowEnd) - memDC.GetTextExtent(profitLabelTxt).cy) / 2);
	int profitX = left + g_data.RDPI(5);
	memDC.TextOut(profitX, profitY, profitLabelTxt);
	{
		int labelW = memDC.GetTextExtent(profitLabelTxt).cx;
		int numH = memDC.GetTextExtent(profitNumTxt).cy;
		int barX = profitX + labelW;
		// 总长度固定：从标签后到面板右边界，留少量右边距
		int barW = max(0, right - barX - g_data.RDPI(4));
		int barH = numH + g_data.RDPI(2);
		int barY = profitY - g_data.RDPI(1);

		// 红色长度 = 总宽度 × X/100（获利比例）；绿色长度 = 总宽度 × (100-X)/100（套牢比例）
		// 红色用标准红，绿色与筹码峰 COLOR_GREEN_DOWN 一致
		int redW = static_cast<int>(barW * profitRatio / 100.0 + 0.5);
		int greenW = barW - redW;
		if (redW > 0)
		{
			CRect redRect(barX, barY, barX + redW, barY + barH);
			CBrush redBrush(RGB(179, 64, 65));
			CBrush* pOldBrush = memDC.SelectObject(&redBrush);
			memDC.FillRect(&redRect, &redBrush);
			memDC.SelectObject(pOldBrush);
		}
		if (greenW > 0)
		{
			CRect greenRect(barX + redW, barY, barX + redW + greenW, barY + barH);
			CBrush greenBrush(COLOR_GREEN_DOWN);
			CBrush* pOldBrush = memDC.SelectObject(&greenBrush);
			memDC.FillRect(&greenRect, &greenBrush);
			memDC.SelectObject(pOldBrush);
		}

		// 数字绘制在背景条左侧（白色文字，带少量内边距）
		memDC.SetTextColor(RGB(255, 255, 255));
		memDC.TextOut(barX + g_data.RDPI(4), profitY, profitNumTxt);
		memDC.SetTextColor(COLOR_GRAY_TEXT);
	}
	CString avgCostTxt;
	avgCostTxt.Format(_T("平均成本:%s"), CCommon::FormatFloat(avgCost));
	memDC.TextOut(left + g_data.RDPI(5), rowY(chartRowEnd + 1) + max(0, (rowH(chartRowEnd + 1) - memDC.GetTextExtent(avgCostTxt).cy) / 2), avgCostTxt);

	CString rangeTxt;
	rangeTxt.Format(_T("90%%成本:%s-%s"), CCommon::FormatFloat(chip90Low), CCommon::FormatFloat(chip90High));
	memDC.TextOut(left + g_data.RDPI(5), rowY(chartRowEnd + 2) + max(0, (rowH(chartRowEnd + 2) - memDC.GetTextExtent(rangeTxt).cy) / 2), rangeTxt);
}