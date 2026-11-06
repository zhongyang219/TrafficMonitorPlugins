#include "pch.h"
#include "CallAuctionChart.h"
#include "ChartColors.h"
#include "Common.h"
#include "DataManager.h"
#include <algorithm>
#include <ctime>

void CCallAuctionChart::Draw(CDC& memDC, const TimelineDrawContext& ctx, const STOCK::CallAuctionData& callAuctionData)
{
	const auto& snapshots = callAuctionData.snapshots;
	int totalPoints = static_cast<int>(snapshots.size());

	if (snapshots.empty() && !callAuctionData.isValid)
	{
		memDC.SetTextColor(COLOR_GRAY_TEXT);
		CString noDataText = _T("暂无集合竞价数据");
		CSize textSize = memDC.GetTextExtent(noDataText);
		int textX = (ctx.chartWidth - textSize.cx) / 2;
		int textY = ctx.priceChartTop + (ctx.priceChartHeight - textSize.cy) / 2;
		memDC.TextOut(textX, textY, noDataText);
		return;
	}

	// X轴时间范围：9:15-9:25
	const int startMinute = 9 * 60 + 15;
	const int endMinute = 9 * 60 + 25;
	const int totalMinutes = endMinute - startMinute;  // 10分钟

	// 将快照时间映射到X坐标的辅助函数
	auto timeToX = [&](time_t t) -> int {
		struct tm tmBuf;
		localtime_s(&tmBuf, &t);
		int minute = tmBuf.tm_hour * 60 + tmBuf.tm_min;
		double ratio = static_cast<double>(minute - startMinute) / totalMinutes;
		ratio = max(0.0, min(1.0, ratio));
		return static_cast<int>(ratio * ctx.chartWidth);
		};

	// ==================== 竖线网格（9:15-9:25，每5分钟） ====================
	{
		CPen gridPen(PS_SOLID, 1, RGB(220, 220, 220));
		CPen* pOldPen = memDC.SelectObject(&gridPen);
		for (int minute = startMinute; minute <= endMinute; minute += 5)
		{
			int xPos = static_cast<int>(static_cast<double>(minute - startMinute) / totalMinutes * ctx.chartWidth);
			memDC.MoveTo(xPos, ctx.priceChartTop);
			memDC.LineTo(xPos, ctx.volumeChartTop + ctx.volumeChartHeight);
		}
		memDC.SelectObject(pOldPen);
	}

	// ==================== 主图：价格走势 ====================
	// 昨收参考线
	if (callAuctionData.prevClosePrice > 0 && ctx.unitY > 0)
	{
		STOCK::Price prevClose = callAuctionData.prevClosePrice;
		int prevCloseY = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>((prevClose - ctx.minPrice) * ctx.unitY);
		CPen dashPen(PS_DASH, 1, RGB(128, 128, 128));
		CPen* pOldPen = memDC.SelectObject(&dashPen);
		memDC.MoveTo(0, prevCloseY);
		memDC.LineTo(ctx.chartWidth, prevCloseY);
		memDC.SelectObject(pOldPen);

		CString prevCloseLabel;
		prevCloseLabel.Format(_T("昨收 %.2f"), prevClose);
		memDC.SetTextColor(RGB(128, 128, 128));
		memDC.TextOut(2, prevCloseY - memDC.GetTextExtent(prevCloseLabel).cy - 1, prevCloseLabel);
	}

	// 涨停/跌停线
	if (callAuctionData.limitUpPrice > 0 && ctx.unitY > 0)
	{
		int limitUpY = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>((callAuctionData.limitUpPrice - ctx.minPrice) * ctx.unitY);
		CPen dashPen(PS_DOT, 1, RGB(200, 0, 200));
		CPen* pOldPen = memDC.SelectObject(&dashPen);
		memDC.MoveTo(0, limitUpY);
		memDC.LineTo(ctx.chartWidth, limitUpY);
		memDC.SelectObject(pOldPen);
	}
	if (callAuctionData.limitDownPrice > 0 && ctx.unitY > 0)
	{
		int limitDownY = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>((callAuctionData.limitDownPrice - ctx.minPrice) * ctx.unitY);
		CPen dashPen(PS_DOT, 1, RGB(0, 128, 0));
		CPen* pOldPen = memDC.SelectObject(&dashPen);
		memDC.MoveTo(0, limitDownY);
		memDC.LineTo(ctx.chartWidth, limitDownY);
		memDC.SelectObject(pOldPen);
	}

	// 价格走势曲线
	if (totalPoints > 0 && ctx.unitY > 0)
	{
		CPen pricePen(PS_SOLID, 2, RGB(0, 0, 180));
		CPen* pOldPen = memDC.SelectObject(&pricePen);
		bool firstPoint = true;
		int prevX = 0, prevY = 0;
		for (int i = 0; i < totalPoints; i++)
		{
			if (snapshots[i].matchPrice <= 0) continue;
			int x = timeToX(snapshots[i].timestamp);
			int y = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>((snapshots[i].matchPrice - ctx.minPrice) * ctx.unitY);
			if (firstPoint)
			{
				prevX = x;
				prevY = y;
				firstPoint = false;
			}
			else
			{
				memDC.MoveTo(prevX, prevY);
				memDC.LineTo(x, y);
				prevX = x;
				prevY = y;
			}
		}
		memDC.SelectObject(pOldPen);

		// 当前价格标签（右侧）
		if (callAuctionData.matchPrice > 0)
		{
			int lastX = timeToX(snapshots[totalPoints - 1].timestamp);
			int lastY = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>((callAuctionData.matchPrice - ctx.minPrice) * ctx.unitY);
			CString priceLabel = CCommon::FormatFloat(callAuctionData.matchPrice);
			memDC.SetTextColor(callAuctionData.matchPrice >= callAuctionData.prevClosePrice ? COLOR_RED_UP : COLOR_GREEN_DOWN);
			CSize labelSize = memDC.GetTextExtent(priceLabel);
			memDC.TextOut(ctx.chartWidth + 2 - labelSize.cx, lastY - labelSize.cy / 2, priceLabel);
		}
	}

	// 主图信息文本（左上角）
	{
		CString infoText;
		if (callAuctionData.matchPrice > 0)
		{
			double changePercent = callAuctionData.prevClosePrice > 0 ?
				(callAuctionData.matchPrice - callAuctionData.prevClosePrice) / callAuctionData.prevClosePrice * 100 : 0;
			CString priceStr = CCommon::FormatFloat(callAuctionData.matchPrice);
			CString changeStr = CCommon::FormatSignedValue(changePercent, _T("%.2f"));
			infoText.Format(_T("撮合价 %s  %s%%"), priceStr, changeStr);
			memDC.SetTextColor(callAuctionData.matchPrice >= callAuctionData.prevClosePrice ? COLOR_RED_UP : COLOR_GREEN_DOWN);
		}
		else
		{
			infoText = _T("暂无撮合价");
			memDC.SetTextColor(COLOR_GRAY_TEXT);
		}
		memDC.TextOut(g_data.RDPI(4), ctx.priceChartTop + g_data.RDPI(2), infoText);
	}

	// 主图Y轴价格刻度
	if (ctx.maxPrice > 0 && ctx.minPrice >= 0 && ctx.maxPrice > ctx.minPrice && ctx.niceStep > 0)
	{
		memDC.SetTextColor(COLOR_GRAY_TEXT);
		double priceRange = ctx.maxPrice - ctx.minPrice;
		int labelCount = static_cast<int>(round(priceRange / ctx.niceStep));
		for (int i = 0; i <= labelCount; i++)
		{
			double p = round((ctx.minPrice + i * ctx.niceStep) * 1000.0) / 1000.0;
			int y = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>((p - ctx.minPrice) * ctx.unitY);
			CString label = CCommon::FormatFloat(p);
			CSize sz = memDC.GetTextExtent(label);
			memDC.TextOut(-sz.cx - g_data.RDPI(4), y - sz.cy / 2, label);
		}
	}

	// ==================== 副图：成交量（上半未匹配量倒置，下半已匹配量正置） ====================
	if (totalPoints > 0)
	{
		// 找最大增量成交量（addVol）和最大未匹配量
		STOCK::Volume maxAddVol = 0;
		STOCK::Volume maxUnmatchVol = 0;
		for (const auto& snap : snapshots)
		{
			maxAddVol = (std::max)(maxAddVol, snap.addVol);
			maxUnmatchVol = (std::max)(maxUnmatchVol, (std::max)(snap.unmatchBidVol, snap.unmatchAskVol));
		}
		if (maxAddVol <= 0) maxAddVol = 1;
		if (maxUnmatchVol <= 0) maxUnmatchVol = 1;

		// 副图分为上下两半：上半绘制未匹配量（倒置），下半绘制已匹配量（正置）
		int volHalfHeight = ctx.volumeChartHeight / 2;
		int volMidY = ctx.volumeChartTop + volHalfHeight;  // 中线

		// 中线分隔线
		CPen midPen(PS_SOLID, 1, RGB(200, 200, 200));
		CPen* pOldPen = memDC.SelectObject(&midPen);
		memDC.MoveTo(0, volMidY);
		memDC.LineTo(ctx.chartWidth, volMidY);
		memDC.SelectObject(pOldPen);

		double barWidth = static_cast<double>(ctx.chartWidth) / totalPoints;
		int actualBarWidth = max(1, static_cast<int>(barWidth) - 1);

		for (int i = 0; i < totalPoints; i++)
		{
			int x = timeToX(snapshots[i].timestamp) - actualBarWidth / 2;
			bool isUp = snapshots[i].matchPrice >= callAuctionData.prevClosePrice;
			COLORREF matchColor = isUp ? COLOR_RED_UP : COLOR_GREEN_DOWN;
			COLORREF unmatchColor = isUp ? RGB(255, 150, 150) : RGB(150, 255, 150);

			// 下半部分：已匹配增量成交量（正置，从中线向下生长）
			if (snapshots[i].addVol > 0)
			{
				int barHeight = static_cast<int>(static_cast<double>(snapshots[i].addVol) / maxAddVol * volHalfHeight * 0.9);
				barHeight = max(1, barHeight);
				memDC.FillSolidRect(x, volMidY, actualBarWidth, barHeight, matchColor);
			}

			// 上半部分：未匹配量（倒置，从中线向上生长）
			// 未匹配买量（红色，左侧半柱）
			if (snapshots[i].unmatchBidVol > 0)
			{
				int barHeight = static_cast<int>(static_cast<double>(snapshots[i].unmatchBidVol) / maxUnmatchVol * volHalfHeight * 0.9);
				barHeight = max(1, barHeight);
				int halfW = max(1, actualBarWidth / 2);
				memDC.FillSolidRect(x, volMidY - barHeight, halfW, barHeight, unmatchColor);
			}
			// 未匹配卖量（绿色，右侧半柱）
			if (snapshots[i].unmatchAskVol > 0)
			{
				int barHeight = static_cast<int>(static_cast<double>(snapshots[i].unmatchAskVol) / maxUnmatchVol * volHalfHeight * 0.9);
				barHeight = max(1, barHeight);
				int halfW = max(1, actualBarWidth / 2);
				memDC.FillSolidRect(x + halfW, volMidY - barHeight, halfW, barHeight, unmatchColor);
			}
		}

		// 副图标题
		CString volTitle;
		volTitle.Format(_T("成交量  匹配 %s  未匹配买 %s / 卖 %s"),
			CCommon::FormatVolume(static_cast<double>(callAuctionData.matchVolume)),
			CCommon::FormatVolume(static_cast<double>(callAuctionData.totalBidVolume)),
			CCommon::FormatVolume(static_cast<double>(callAuctionData.totalAskVolume)));
		memDC.SetTextColor(COLOR_GRAY_TEXT);
		memDC.TextOut(g_data.RDPI(4), ctx.volumeChartTop + g_data.RDPI(1), volTitle);
	}

	// ==================== X轴时间标签 ====================
	{
		memDC.SetTextColor(COLOR_GRAY_TEXT);
		for (int minute = startMinute; minute <= endMinute; minute += 5)
		{
			int xPos = static_cast<int>(static_cast<double>(minute - startMinute) / totalMinutes * ctx.chartWidth);
			int hour = minute / 60;
			int min = minute % 60;
			CString timeLabel;
			timeLabel.Format(_T("%02d:%02d"), hour, min);
			CSize labelSize = memDC.GetTextExtent(timeLabel);
			int labelX = max(0, min(xPos - labelSize.cx / 2, ctx.chartWidth - labelSize.cx));
			memDC.TextOut(labelX, ctx.positionY, timeLabel);
		}
	}
}