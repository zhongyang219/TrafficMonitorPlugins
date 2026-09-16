#include "pch.h"
#include "TimelineChart.h"
#include "ChartColors.h"
#include "Common.h"
#include "DataManager.h"
#include "SignalAnalyzer.h"
#include "StockIndicator.h"
#include "IndicatorChart.h"
#include "TechnicalIndicatorModel.h"
#include "StatusBarPanel.h"
#include <algorithm>
#include <cmath>
#include <set>

// 时间标记结构体（供分时图绘制函数共用）
struct TimeMarker {
	const TCHAR* label;
	int minutesFromStart;
};

// 辅助函数：绘制价格点标签（最高/最低价标注）
static void DrawPricePointLabel(CDC& memDC, int pointX, int pointY, int chartLeft, int chartTop, int chartWidth, int chartHeight,
	STOCK::Price price, bool isHigh, COLORREF color)
{
	CString label = CCommon::FormatFloat(price);
	CSize labelSize = memDC.GetTextExtent(label);
	const int gap = g_data.RDPI(4);
	const int arrowGap = g_data.RDPI(10);
	const int chartRight = chartLeft + chartWidth;
	const int chartBottom = chartTop + chartHeight;

	int labelX = pointX - labelSize.cx / 2;
	int labelY = isHigh ? pointY - labelSize.cy - arrowGap : pointY + arrowGap;
	bool useSideLabel = labelX < chartLeft || labelX + labelSize.cx > chartRight;

	if (useSideLabel)
	{
		if (pointX < chartLeft + chartWidth / 2)
		{
			label.Format(_T("\u2190%s"), CCommon::FormatFloat(price));
			labelSize = memDC.GetTextExtent(label);
			labelX = pointX + gap;
		}
		else
		{
			label.Format(_T("%s\u2192"), CCommon::FormatFloat(price));
			labelSize = memDC.GetTextExtent(label);
			labelX = pointX - labelSize.cx - gap;
		}
		labelY = pointY - labelSize.cy / 2;
		labelX = max(chartLeft, min(labelX, chartRight - labelSize.cx));
		labelY = max(chartTop, min(labelY, chartBottom - labelSize.cy));
		memDC.SetTextColor(color);
		memDC.TextOut(labelX, labelY, label);
		return;
	}

	labelY = max(chartTop, min(labelY, chartBottom - labelSize.cy));
	memDC.SetTextColor(color);
	memDC.TextOut(labelX, labelY, label);

	int fromX = labelX + labelSize.cx / 2;
	int fromY = isHigh ? labelY + labelSize.cy : labelY;
	if (abs(pointY - fromY) > g_data.RDPI(2))
	{
		CPen pen(PS_SOLID, 1, color);
		CPen* pOldPen = memDC.SelectObject(&pen);
		memDC.MoveTo(fromX, fromY);
		memDC.LineTo(pointX, pointY);

		int dir = (pointY >= fromY) ? 1 : -1;
		int arrowLen = g_data.RDPI(4);
		int arrowHalf = g_data.RDPI(3);
		memDC.MoveTo(pointX, pointY);
		memDC.LineTo(pointX - arrowHalf, pointY - dir * arrowLen);
		memDC.MoveTo(pointX, pointY);
		memDC.LineTo(pointX + arrowHalf, pointY - dir * arrowLen);
		memDC.SelectObject(pOldPen);
	}
}

void CTimelineChart::DrawTradingDayBands(CDC& memDC, const TimelineDrawContext& ctx, const HoverState& hover)
{
	// 仅5/30分钟K线模式绘制交易日交替底色；日K每根即一天，分时无跨日
	if (!ctx.klineData || ctx.klineData->empty() || !ctx.timelinePoint || ctx.timelinePoint->empty())
		return;
	if (hover.viewMode != UI_VIEW_MIN5_KLINE && hover.viewMode != UI_VIEW_MIN30_KLINE)
		return;

	const auto& klineData = *ctx.klineData;
	const auto& timelinePoint = *ctx.timelinePoint;
	const int totalPoints = static_cast<int>(timelinePoint.size());
	const int klineCount = static_cast<int>(klineData.size());
	int klineStartIdx = ctx.startIndex;
	if (klineStartIdx < 0 || klineStartIdx >= klineCount)
		return;
	int klineEndIdx = min(klineStartIdx + totalPoints, klineCount);

	auto klineDay = [&](int idx) -> std::string {
		// 5/30分钟day为"YYYY-MM-DD HH:MM"，日K为"YYYY-MM-DD"，均截取前10字符日期
		const auto& d = klineData[idx].day;
		return d.length() >= 10 ? d.substr(0, 10) : d;
		};

	// 可视区间首根所属交易日序号（从全局第0根累计日期切换次数），用于奇偶基准
	int runDayNo = 0;
	for (int i = 1; i <= klineStartIdx; i++)
	{
		if (klineDay(i) != klineDay(i - 1))
			runDayNo++;
	}

	// 按交易日分段填充交替底色：序号偶数日填 #EEEEEE 浅底，奇数日保持白底
	static const COLORREF BAND_COLOR = RGB(242, 242, 242); //RGB(238, 238, 238)
	CBrush bandBrush(BAND_COLOR);
	const float pxPerSlot = ctx.chartWidth / static_cast<float>(totalPoints);
	int bandTop = ctx.priceChartTop;
	int bandBottom = ctx.priceChartTop + ctx.priceChartHeight;

	int i = 0;
	while (i < totalPoints && (klineStartIdx + i) < klineEndIdx)
	{
		std::string curDay = klineDay(klineStartIdx + i);
		int runStart = i;
		while (i < totalPoints && (klineStartIdx + i) < klineEndIdx && klineDay(klineStartIdx + i) == curDay)
			i++;
		int runEnd = i;   // [runStart, runEnd)为同一交易日的可见区间

		// 偶数序日填浅底，奇数序日保持白底
		if (runDayNo % 2 == 0)
		{
			int left = static_cast<int>(pxPerSlot * runStart);
			int right = runEnd >= totalPoints ? ctx.chartWidth : static_cast<int>(pxPerSlot * runEnd);
			if (right > left)
				memDC.FillRect(CRect(left, bandTop, right, bandBottom), &bandBrush);
		}
		runDayNo++;
	}
}

void CTimelineChart::DrawTimelineHeader(CDC& memDC, const TimelineDrawContext& ctx, const HoverState& hover)
{
	CPoint origOrg = memDC.GetViewportOrg();
	memDC.SetViewportOrg(0, 0);

	CString macdSignal;
	auto stockData = g_data.GetStockData(hover.stockId);
	if (stockData && stockData->info.is_ok)
		macdSignal = stockData->macdTrendSignal;

	CStatusBarPanel::DrawMainHeader(memDC, ctx.realtimeData, ctx.windowWidth, g_data.RDPI(26));
	memDC.SetViewportOrg(origOrg);
}

void CTimelineChart::DrawTimelineBackgroundHighlights(CDC& memDC, const TimelineDrawContext& ctx)
{
	DrawTimelineBackgroundHighlightsForArea(memDC, ctx, ctx.priceChartTop, ctx.priceChartHeight);
}

void CTimelineChart::DrawTimelineBackgroundHighlightsForArea(CDC& memDC, const TimelineDrawContext& ctx, int chartTop, int chartHeight)
{
	// 仅分时模式（1分钟数据）绘制5分钟交替高亮，5分钟/30分钟/日K线模式不绘制
	if (g_data.GetCurrentViewMode() != UI_VIEW_TIMELINE)
		return;

	if (!ctx.timelinePoint || ctx.timelinePoint->empty())
		return;

	const auto& timelinePoint = *ctx.timelinePoint;
	const int totalPts = static_cast<int>(timelinePoint.size());
	const int xAxisPts = ctx.xAxisPoints > 0 ? ctx.xAxisPoints : totalPts;

	CBrush blueBrush(COLOR_LIGHT_BLUE);

	// 从9:30开盘起，每5分钟交替高亮：9:30-9:35高亮，9:35-9:40不高亮，9:40-9:45高亮...
	// A股交易时间：9:30-11:30(0~119分钟), 13:00-15:00(120~239分钟)
	for (int i = 0; i < totalPts; i++)
	{
		// 解析时间字符串 "HH:MM" 计算交易分钟序号
		const std::string& timeStr = timelinePoint[i].time;
		int hour = 0, minute = 0;
		if (timeStr.length() >= 5 && timeStr[2] == ':')
		{
			hour = (timeStr[0] - '0') * 10 + (timeStr[1] - '0');
			minute = (timeStr[3] - '0') * 10 + (timeStr[4] - '0');
		}
		else
			continue;

		int totalMinutes = hour * 60 + minute;
		// 计算从9:30开始的交易分钟序号
		int tradingMin = -1;
		if (totalMinutes >= 9 * 60 + 30 && totalMinutes <= 11 * 60 + 30)
			tradingMin = totalMinutes - (9 * 60 + 30);
		else if (totalMinutes >= 13 * 60 && totalMinutes <= 15 * 60)
			tradingMin = 120 + (totalMinutes - 13 * 60);

		if (tradingMin < 0)
			continue;

		// 每10分钟一个周期，前5分钟高亮，后5分钟不高亮
		int cyclePos = tradingMin % 10;
		if (cyclePos >= 5)
			continue;

		// 找到连续高亮区间的起点和终点
		int startIdx = i;
		while (i + 1 < totalPts)
		{
			const std::string& nextTimeStr = timelinePoint[i + 1].time;
			int nextHour = 0, nextMinute = 0;
			if (nextTimeStr.length() >= 5 && nextTimeStr[2] == ':')
			{
				nextHour = (nextTimeStr[0] - '0') * 10 + (nextTimeStr[1] - '0');
				nextMinute = (nextTimeStr[3] - '0') * 10 + (nextTimeStr[4] - '0');
			}
			else
				break;

			int nextTotalMin = nextHour * 60 + nextMinute;
			int nextTradingMin = -1;
			if (nextTotalMin >= 9 * 60 + 30 && nextTotalMin <= 11 * 60 + 30)
				nextTradingMin = nextTotalMin - (9 * 60 + 30);
			else if (nextTotalMin >= 13 * 60 && nextTotalMin <= 15 * 60)
				nextTradingMin = 120 + (nextTotalMin - 13 * 60);

			if (nextTradingMin < 0 || nextTradingMin % 10 >= 5)
				break;
			i++;
		}
		int endIdx = i;

		int xLeft = static_cast<int>(ctx.chartWidth / static_cast<float>(xAxisPts) * startIdx);
		int xRight = static_cast<int>(ctx.chartWidth / static_cast<float>(xAxisPts) * (endIdx + 1));

		memDC.FillRect(CRect(xLeft, chartTop, xRight, chartTop + chartHeight), &blueBrush);
	}
}

void CTimelineChart::DrawTimelineGridLines(CDC& memDC, const TimelineDrawContext& ctx)
{
	CPen pGrid(PS_DOT, 1, COLOR_GRAY_GRID);
	CPen* pOldPen = memDC.SelectObject(&pGrid);

	if (ctx.timelinePoint && !ctx.timelinePoint->empty())
	{
		// 竖向虚线所在日期与时间标签同规则交替变色（仅5/30分钟K线模式生效）
		// X坐标对齐柱中心（与K线/鼠标十字线一致）：槽宽 = chartWidth / xAxisPts，线画在 (i+0.5)*槽宽
		const auto& tl = *ctx.timelinePoint;
		const int totalPts = static_cast<int>(tl.size());
		const int numVLines = 6;
		const int xAxisPts = ctx.xAxisPoints > 0 ? ctx.xAxisPoints : totalPts;
		const DayColorCtx dayCtx = CalcDayColorCtx(tl);
		CPen blueGridPen(PS_DOT, 1, COLOR_BLACK);
		for (int i = 0; i <= numVLines; i++)
		{
			int idx = totalPts * i / numVLines;
			if (idx >= totalPts) idx = totalPts - 1;
			int xPos = static_cast<int>(ctx.chartWidth / static_cast<float>(xAxisPts) * (static_cast<float>(idx) + 0.5f))
				+ (i == 0 ? g_data.RDPI(1) : 0);   // 首线的中心会超出左边界少许，右移1px保证可见
			memDC.SelectObject(IsBlueDayAt(tl, idx, dayCtx) ? &blueGridPen : &pGrid);
			memDC.MoveTo(xPos, ctx.priceChartTop);
			memDC.LineTo(xPos, ctx.priceChartTop + ctx.priceChartHeight);
		}
	}

	memDC.MoveTo(0, ctx.priceChartTop + ctx.priceChartHeight);
	memDC.LineTo(ctx.chartWidth, ctx.priceChartTop + ctx.priceChartHeight);

	if (ctx.maxPrice > 0 && ctx.minPrice >= 0 && ctx.maxPrice > ctx.minPrice && ctx.niceStep > 0)
	{
		CPen pGridLine(PS_DOT, 1, COLOR_GRAY_GRID);
		memDC.SelectObject(&pGridLine);
		double priceRange = ctx.maxPrice - ctx.minPrice;
		double unitY = ctx.unitY;
		int labelCount = static_cast<int>(round(priceRange / ctx.niceStep));
		for (int i = 0; i <= labelCount; i++)
		{
			double labelPrice = round((ctx.minPrice + i * ctx.niceStep) * 1000.0) / 1000.0;
			int y = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((labelPrice - ctx.minPrice) * unitY));
			memDC.MoveTo(0, y);
			memDC.LineTo(ctx.chartWidth, y);
		}
	}

	if (ctx.maxPrice > 0 && ctx.minPrice >= 0 && ctx.maxPrice > ctx.minPrice && ctx.realtimeData.prevClosePrice > 0
		&& ctx.realtimeData.prevClosePrice >= ctx.minPrice && ctx.realtimeData.prevClosePrice <= ctx.maxPrice)
	{
		CPen pMiddleLine(PS_DASHDOT, 1, COLOR_GRAY_MIDDLE);
		memDC.SelectObject(&pMiddleLine);
		int prevCloseY = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((ctx.realtimeData.prevClosePrice - ctx.minPrice) * ctx.unitY));
		prevCloseY = max(ctx.priceChartTop, min(prevCloseY, ctx.priceChartTop + ctx.priceChartHeight));
		memDC.MoveTo(0, prevCloseY);
		memDC.LineTo(ctx.chartWidth, prevCloseY);
	}

	memDC.SelectObject(pOldPen);
}

void CTimelineChart::DrawTimelinePriceLabels(CDC& memDC, const TimelineDrawContext& ctx)
{
	if (ctx.maxPrice > 0 && ctx.minPrice >= 0 && ctx.maxPrice > ctx.minPrice && ctx.niceStep > 0)
	{
		int oldBkMode = memDC.SetBkMode(TRANSPARENT);
		double priceRange = ctx.maxPrice - ctx.minPrice;
		double unitY = ctx.unitY;

		int labelCount = static_cast<int>(round(priceRange / ctx.niceStep));
		for (int i = 0; i <= labelCount; i++)
		{
			double labelPrice = round((ctx.minPrice + i * ctx.niceStep) * 1000.0) / 1000.0;
			int y = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((labelPrice - ctx.minPrice) * unitY));
			CString priceTxt = ctx.realtimeData.IsETF() ? CCommon::FormatETFPrice(labelPrice) : CCommon::FormatFloat(labelPrice);
			CSize sz = memDC.GetTextExtent(priceTxt);
			int labelX = -sz.cx - g_data.RDPI(4);
			int labelY = y - sz.cy / 2;
			if (ctx.realtimeData.prevClosePrice > 0 && labelPrice > ctx.realtimeData.prevClosePrice + ctx.niceStep * 0.01)
				memDC.SetTextColor(COLOR_RED_UP);
			else if (ctx.realtimeData.prevClosePrice > 0 && labelPrice < ctx.realtimeData.prevClosePrice - ctx.niceStep * 0.01)
				memDC.SetTextColor(COLOR_GREEN_DOWN);
			else
				memDC.SetTextColor(COLOR_BLACK);
			memDC.TextOut(labelX, labelY, priceTxt);
		}

		if (ctx.realtimeData.prevClosePrice > 0
			&& ctx.realtimeData.prevClosePrice >= ctx.minPrice && ctx.realtimeData.prevClosePrice <= ctx.maxPrice)
		{
			memDC.SetTextColor(COLOR_GRAY_PURPLE);
			CString prevTxt = ctx.realtimeData.IsETF() ? CCommon::FormatETFPrice(ctx.realtimeData.prevClosePrice) : CCommon::FormatFloat(ctx.realtimeData.prevClosePrice);
			CSize prevSize = memDC.GetTextExtent(prevTxt);
			int prevY = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((ctx.realtimeData.prevClosePrice - ctx.minPrice) * unitY));
			int labelY = prevY - prevSize.cy / 2;
			memDC.TextOut(-prevSize.cx - g_data.RDPI(4), labelY, prevTxt);
		}
		memDC.SetBkMode(oldBkMode);
		return;
	}

	STOCK::Price priceLimit = ctx.realtimeData.priceLimit;
	int oldBkMode = memDC.SetBkMode(TRANSPARENT);

	memDC.SetTextColor(COLOR_RED_UP);
	CString upperLimitTxt = CCommon::FormatFloat(ctx.realtimeData.prevClosePrice + priceLimit);
	CSize upperSize = memDC.GetTextExtent(upperLimitTxt);
	int upperY = ctx.priceChartTop - upperSize.cy / 2;
	upperY = max(ctx.priceChartTop, min(upperY, ctx.priceChartTop + ctx.priceChartHeight - upperSize.cy));
	memDC.TextOut(-upperSize.cx - g_data.RDPI(4), upperY, upperLimitTxt);

	memDC.SetTextColor(COLOR_GREEN_DOWN);
	CString lowerLimitTxt = CCommon::FormatFloat(ctx.realtimeData.prevClosePrice - priceLimit);
	CSize lowerSize = memDC.GetTextExtent(lowerLimitTxt);
	int lowerY = ctx.priceChartTop + ctx.priceChartHeight - lowerSize.cy / 2;
	lowerY = max(ctx.priceChartTop, min(lowerY, ctx.priceChartTop + ctx.priceChartHeight - lowerSize.cy));
	memDC.TextOut(-lowerSize.cx - g_data.RDPI(4), lowerY, lowerLimitTxt);

	memDC.SetTextColor(COLOR_GRAY_PURPLE);
	CString middleTxt = CCommon::FormatFloat(ctx.realtimeData.prevClosePrice);
	CSize midSize = memDC.GetTextExtent(middleTxt);
	int midY = ctx.priceChartTop + ctx.priceChartHeight / 2 - midSize.cy / 2;
	midY = max(ctx.priceChartTop, min(midY, ctx.priceChartTop + ctx.priceChartHeight - midSize.cy));
	memDC.TextOut(-midSize.cx - g_data.RDPI(4), midY, middleTxt);

	memDC.SetBkMode(oldBkMode);
}

void CTimelineChart::DrawTimelineCostAndProfitLines(CDC& memDC, const TimelineDrawContext& ctx, const HoverState& hover)
{
	(void)memDC;
	(void)ctx;
	(void)hover;
}

void CTimelineChart::DrawTimelineGridAndLines(CDC& memDC, const TimelineDrawContext& ctx, const HoverState& hover)
{
	//DrawTimelineBackgroundHighlights(memDC, ctx);
	DrawTimelineGridLines(memDC, ctx);
	DrawTimelinePriceLabels(memDC, ctx);
	DrawTimelineCostAndProfitLines(memDC, ctx, hover);
}

void CTimelineChart::DrawTimelinePriceCurve(CDC& memDC, const TimelineDrawContext& ctx, const HoverState& hover)
{
	const auto& timelinePoint = *ctx.timelinePoint;
	if (timelinePoint.empty())
		return;

	const int totalPoints = static_cast<int>(timelinePoint.size());
	const int xAxisPts = ctx.xAxisPoints > 0 ? ctx.xAxisPoints : totalPoints;

	STOCK::Price maxPrice = ctx.maxPrice;
	STOCK::Price minPrice = ctx.minPrice;
	double unitY = ctx.unitY;
	if (maxPrice <= 0 || minPrice < 0 || maxPrice <= minPrice || unitY <= 0)
	{
		STOCK::Price priceLimit = ctx.realtimeData.priceLimit;
		maxPrice = ctx.realtimeData.prevClosePrice + priceLimit;
		minPrice = ctx.realtimeData.prevClosePrice - priceLimit;
		const int pricePaddingY = g_data.RDPI(10);
		double paddingPrice = (maxPrice - minPrice) * pricePaddingY / ctx.priceChartHeight;
		maxPrice += paddingPrice;
		minPrice -= paddingPrice;
		unitY = ctx.priceChartHeight / (maxPrice - minPrice);
	}

	std::vector<CPoint> dataPoints;
	dataPoints.reserve(timelinePoint.size());
	for (int i = 0; i < totalPoints; i++)
	{
		const auto& item = timelinePoint[i];
		int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(xAxisPts) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(xAxisPts) / 2);
		double yVal = (item.price - minPrice) * unitY;
		dataPoints.push_back(CPoint(pointX, static_cast<int>(round(yVal))));
	}

	// 均价线（点虚线，先画）
	{
		CPen avgLinePen(PS_SOLID, 1, COLOR_BLUE_COST);
		CPen* pOldAvgPen = memDC.SelectObject(&avgLinePen);
		bool firstAvgPoint = true;
		for (int i = 0; i < totalPoints; i++)
		{
			const auto& item = timelinePoint[i];
			if (item.averagePrice <= 0)
			{
				firstAvgPoint = true;
				continue;
			}
			int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(xAxisPts) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(xAxisPts) / 2);
			int py = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((item.averagePrice - minPrice) * unitY));
			py = max(ctx.priceChartTop, min(py, ctx.priceChartTop + ctx.priceChartHeight));
			if (firstAvgPoint)
			{
				memDC.MoveTo(pointX, py);
				firstAvgPoint = false;
			}
			else
			{
				memDC.LineTo(pointX, py);
			}
		}
		memDC.SelectObject(pOldAvgPen);
	}

	// MA5/MA10/MA20均线配色
	if (hover.showMA)
	{
		const COLORREF ma5Color = RGB(240, 117, 40);
		const COLORREF ma10Color = RGB(21, 101, 192);
		const COLORREF ma20Color = RGB(128, 40, 149);

		auto drawMALine = [&](int fieldOffset, COLORREF color) {
			CPen maPen(PS_SOLID, 1, color);
			memDC.SelectObject(&maPen);
			bool first = true;
			for (int i = 0; i < totalPoints; i++)
			{
				const auto& item = timelinePoint[i];
				STOCK::Price maVal = 0;
				switch (fieldOffset)
				{
				case 5: maVal = item.ma5; break;
				case 10: maVal = item.ma10; break;
				case 20: maVal = item.ma20; break;
				}
				if (maVal <= 0) { first = true; continue; }
				int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(xAxisPts) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(xAxisPts) / 2);
				double yVal = (maVal - minPrice) * unitY;
				int py = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round(yVal));
				if (first)
				{
					memDC.MoveTo(pointX, py);
					first = false;
				}
				else
				{
					memDC.LineTo(pointX, py);
				}
			}
			};

		drawMALine(5, ma5Color);
		drawMALine(10, ma10Color);
		drawMALine(20, ma20Color);
	}

	// 布林带（短横线虚线）——直接读模型缓存的上中下轨序列，不做逐点计算
	if (hover.showBollBands && ctx.indicatorModel)
	{
		const auto& upperBand = ctx.indicatorModel->GetBollUpper();
		const auto& middleBand = ctx.indicatorModel->GetBollMid();
		const auto& lowerBand = ctx.indicatorModel->GetBollLower();

		auto priceToY = [&](double price) -> int {
			return ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((price - minPrice) * unitY));
			};

		auto drawBandLine = [&](const std::vector<double>& band, COLORREF color) {
			CPen bandPen(PS_DASH, 1, color);
			memDC.SelectObject(&bandPen);
			bool first = true;
			for (int i = 0; i < totalPoints; i++)
			{
				int globalIdx = ctx.startIndex + i;
				if (globalIdx >= static_cast<int>(band.size()) || band[globalIdx] <= 0) continue;
				int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(xAxisPts) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(xAxisPts) / 2);
				int py = priceToY(band[globalIdx]);
				if (first)
				{
					memDC.MoveTo(pointX, py);
					first = false;
				}
				else
				{
					memDC.LineTo(pointX, py);
				}
			}
			};

		drawBandLine(upperBand, COLOR_RED_UP);
		drawBandLine(middleBand, RGB(85, 85, 85));
		drawBandLine(lowerBand, COLOR_GREEN_DOWN);
	}

	// 绘制基金净值曲线
	if (ctx.realtimeData.IsETF())
	{
		auto priceToY = [&](double price) -> int {
			return ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((price - minPrice) * unitY));
			};

		const auto& fullTimeline = *ctx.fullTimeline;
		std::map<int, double> iopvByIndex;

		// 优先使用内存中fullTimeline的iopv字段（ApplyTimeline/ApplyFundIOPV已填充）
		for (int i = 0; i < static_cast<int>(fullTimeline.size()); i++)
		{
			if (fullTimeline[i].iopv > 0)
			{
				iopvByIndex[i] = fullTimeline[i].iopv;
			}
		}

		// 始终从数据库补充净值数据（LoadLatestFundNavCache已过滤非交易时段，含午休11:30-13:00）
		// 不依赖内存iopv的覆盖度判断，避免上午数据已占满一半时跳过数据库，导致下午曲线缺失
		{
			auto navPoints = g_data.GetDbManager().LoadLatestFundNavCache(hover.stockId);
			if (!navPoints.empty())
			{
				std::map<std::string, int> fullTimeIndexMap;
				for (int i = 0; i < static_cast<int>(fullTimeline.size()); i++)
				{
					std::string hhmm = fullTimeline[i].time.substr(0, 5);
					fullTimeIndexMap[hhmm] = i;
				}

				for (const auto& nav : navPoints)
				{
					auto it = fullTimeIndexMap.find(nav.time);
					if (it != fullTimeIndexMap.end())
					{
						iopvByIndex[it->second] = nav.iopv;
					}
				}
			}
		}

		// 追加实时IOPV到最后一个分时点
		if (ctx.realtimeData.iopv > 0 && !fullTimeline.empty())
		{
			int lastIdx = static_cast<int>(fullTimeline.size()) - 1;
			iopvByIndex[lastIdx] = ctx.realtimeData.iopv;
		}
		if (!iopvByIndex.empty())
		{
			const COLORREF navColor = RGB(160, 32, 240);
			CPen navPen(PS_SOLID, 1, navColor);
			CPen* pOldPen = memDC.SelectObject(&navPen);
			bool firstNavPoint = true;

			int startIdx = ctx.startIndex;
			int visCount = ctx.visibleCount;
			int drawnCount = 0;

			for (const auto& kv : iopvByIndex)
			{
				int fullIdx = kv.first;
				double iopvVal = kv.second;

				if (fullIdx < startIdx || fullIdx >= startIdx + visCount)
					continue;

				int relIdx = fullIdx - startIdx;
				int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(xAxisPts) * relIdx) + static_cast<int>(ctx.chartWidth / static_cast<float>(xAxisPts) / 2);
				int pointY = priceToY(iopvVal);
				if (firstNavPoint)
				{
					memDC.MoveTo(pointX, pointY);
					firstNavPoint = false;
				}
				else
				{
					memDC.LineTo(pointX, pointY);
				}
				drawnCount++;
			}

			memDC.SelectObject(pOldPen);
		}
	}

	// 走势线（黑色实线，最后画，覆盖在均价线和布林带之上）
	if (!dataPoints.empty())
	{
		CPen pKLine(PS_SOLID, 2, COLOR_DARK_GRAY_BORDER);
		memDC.SelectObject(&pKLine);
		memDC.MoveTo(dataPoints[0].x, ctx.priceChartTop + ctx.priceChartHeight - dataPoints[0].y);
		for (int i = 1; i < static_cast<int>(dataPoints.size()); i++)
			memDC.LineTo(dataPoints[i].x, ctx.priceChartTop + ctx.priceChartHeight - dataPoints[i].y);

		// 最高/最低价标签
		{
			STOCK::Price hiPrice = 0, loPrice = (std::numeric_limits<STOCK::Price>::max)();
			int hiIdx = -1, loIdx = -1;
			for (int i = 0; i < totalPoints; i++)
			{
				STOCK::Price p = timelinePoint[i].price;
				if (p > 0)
				{
					// 仅严格大于/小于更新索引，保留第一次出现的位置
					if (p > hiPrice) { hiPrice = p; hiIdx = i; }
					if (p < loPrice) { loPrice = p; loIdx = i; }
				}
			}

			STOCK::Price prevClose = ctx.realtimeData.prevClosePrice;

			if (hiIdx >= 0 && hiPrice > 0)
			{
				int hiX = dataPoints[hiIdx].x;
				int hiY = ctx.priceChartTop + ctx.priceChartHeight - dataPoints[hiIdx].y;
				DrawPricePointLabel(memDC, hiX, hiY, 0, ctx.priceChartTop, ctx.chartWidth, ctx.priceChartHeight,
					hiPrice, true, COLOR_RED_UP);
			}

			if (loIdx >= 0 && loPrice > 0 && loIdx != hiIdx)
			{
				int loX = dataPoints[loIdx].x;
				int loY = ctx.priceChartTop + ctx.priceChartHeight - dataPoints[loIdx].y;
				DrawPricePointLabel(memDC, loX, loY, 0, ctx.priceChartTop, ctx.chartWidth, ctx.priceChartHeight,
					loPrice, false, COLOR_GREEN_DOWN);
			}
		}
	}

	// 绘制智能分析买卖点标记
	{
		auto stockData = g_data.GetStockData(hover.stockId);
		if (stockData)
		{
			auto min30KLineObj = stockData->getMin30KLineData();

			if (min30KLineObj && min30KLineObj->data.size() >= 22)
			{
				std::vector<STOCK::Bar> bars30;
				bars30.reserve(min30KLineObj->data.size());
				for (const auto& kp : min30KLineObj->data) bars30.push_back(STOCK::Bar::FromKLinePoint(kp));

				auto buySignals = std::vector<bool>(totalPoints, false);
				auto sellSignals = std::vector<bool>(totalPoints, false);
				auto forbidSignals = std::vector<bool>(totalPoints, false);
				auto buyReasons = std::vector<CString>(totalPoints);
				auto sellReasons = std::vector<CString>(totalPoints);
				auto noLabelSignals = std::vector<bool>(totalPoints, false);

				if (hover.viewMode < UI_VIEW_MIN5_KLINE && ctx.fullTimeline && ctx.fullTimeline->size() >= 120)
				{
					std::vector<STOCK::Bar> bars1m = CSignalAnalyzer::ConvertTimelineToBars(*ctx.fullTimeline);
					auto ar = CSignalAnalyzer::AnalyzeSignalAtFromTimeline(bars1m, bars30, static_cast<int>(bars1m.size()) - 1);
					auto& allSignals = ar.batchSignals;

					int fullToVisibleOffset = ctx.startIndex;

					std::vector<bool> filteredSignals(allSignals.size(), false);
					{
						int lastDrawnBuyBar = -10, lastDrawnSellBar = -10;
						for (size_t i = 0; i < allSignals.size(); i++)
						{
							if (allSignals[i].isForbid) continue;
							int bi = allSignals[i].barIndex;
							if (allSignals[i].isBuy)
							{
								if (bi - lastDrawnBuyBar < 5)
									filteredSignals[i] = true;
								else
									lastDrawnBuyBar = bi;
							}
							else
							{
								if (bi - lastDrawnSellBar < 5)
									filteredSignals[i] = true;
								else
									lastDrawnSellBar = bi;
							}
						}
					}

					for (size_t si = 0; si < allSignals.size(); si++)
					{
						const auto& sig = allSignals[si];
						int k = sig.barIndex - fullToVisibleOffset;
						if (k < 0 || k >= totalPoints) continue;
						if (sig.isForbid)
						{
							forbidSignals[k] = true;
							buySignals[k] = false;
						}
						else if (!forbidSignals[k] || !sig.isBuy)
						{
							if (sig.isBuy)
							{
								if (!forbidSignals[k])
								{
									buySignals[k] = true;
									buyReasons[k] = sig.reason;
									if (filteredSignals[si])
										noLabelSignals[k] = true;
								}
							}
							else
							{
								sellSignals[k] = true;
								sellReasons[k] = sig.reason;
								if (filteredSignals[si])
									noLabelSignals[k] = true;
							}
						}
					}
				}
				else if (hover.viewMode >= UI_VIEW_MIN5_KLINE)
				{
					auto min5KLineObj = stockData->getMin5KLineData();
					if (min5KLineObj && min5KLineObj->data.size() >= 60)
					{
						std::string latestBarDate;
						for (auto it5 = min5KLineObj->data.rbegin(); it5 != min5KLineObj->data.rend(); ++it5)
						{
							auto sp = it5->day.find(' ');
							if (sp != std::string::npos)
							{
								latestBarDate = it5->day.substr(0, sp);
								break;
							}
						}

						std::vector<STOCK::Bar> bars5;
						bars5.reserve(min5KLineObj->data.size());
						for (const auto& kp : min5KLineObj->data) bars5.push_back(STOCK::Bar::FromKLinePoint(kp));

						auto ar = CSignalAnalyzer::AnalyzeSignalAt(bars5, bars30, static_cast<int>(bars5.size()) - 1);
						auto& allSignals = ar.batchSignals;

						std::vector<CSignalAnalyzer::SmartSignalPoint> signals;
						for (const auto& sig : allSignals)
						{
							if (sig.barIndex < 0 || sig.barIndex >= static_cast<int>(min5KLineObj->data.size()))
								continue;
							if (!latestBarDate.empty())
							{
								const auto& bar5Time = min5KLineObj->data[sig.barIndex].day;
								auto sp = bar5Time.find(' ');
								if (sp != std::string::npos && bar5Time.substr(0, sp) != latestBarDate)
									continue;
							}
							signals.push_back(sig);
						}

						std::map<std::string, int> timeIndexMap;
						for (int k = 0; k < totalPoints; k++)
						{
							const auto& t = timelinePoint[k].time;
							timeIndexMap[t] = k;
							if (t.length() > 5 && t[5] == ':')
								timeIndexMap[t.substr(0, 5)] = k;
						}

						std::set<int> klineFilteredBarIndices;
						{
							bool lastDirIsBuy = false;
							bool hasLastDir = false;
							int lastBarIdx = -1;
							for (const auto& sig : signals)
							{
								if (sig.isForbid) { hasLastDir = false; continue; }
								int bi = sig.barIndex;
								if (bi == lastBarIdx) continue;
								if (hasLastDir && sig.isBuy == lastDirIsBuy)
									klineFilteredBarIndices.insert(bi);
								else
								{
									lastDirIsBuy = sig.isBuy;
									hasLastDir = true;
								}
								lastBarIdx = bi;
							}
						}

						for (const auto& sig : signals)
						{
							const auto& bar5Time = min5KLineObj->data[sig.barIndex].day;
							std::string timeStr;
							auto spacePos = bar5Time.find(' ');
							if (spacePos != std::string::npos && bar5Time.length() > spacePos + 5)
								timeStr = bar5Time.substr(spacePos + 1, 5);
							else if (bar5Time.length() >= 5 && bar5Time[2] == ':')
								timeStr = bar5Time.substr(0, 5);
							else
								timeStr = bar5Time;

							auto it = timeIndexMap.find(timeStr);
							if (it == timeIndexMap.end() && timeStr.length() >= 8)
								it = timeIndexMap.find(timeStr.substr(0, 5));
							if (it != timeIndexMap.end())
							{
								int k = it->second;
								if (sig.isForbid)
								{
									forbidSignals[k] = true;
									buySignals[k] = false;
								}
								else if (!forbidSignals[k] || !sig.isBuy)
								{
									if (sig.isBuy)
									{
										if (!forbidSignals[k])
										{
											buySignals[k] = true;
											buyReasons[k] = sig.reason;
											if (klineFilteredBarIndices.count(sig.barIndex))
												noLabelSignals[k] = true;
										}
									}
									else
									{
										sellSignals[k] = true;
										sellReasons[k] = sig.reason;
										if (klineFilteredBarIndices.count(sig.barIndex))
											noLabelSignals[k] = true;
									}
								}
							}
						}
					}
				}

				const int dotR = g_data.RDPI(3);
				const int labelOff = g_data.RDPI(8);
				int oldBkMode = memDC.SetBkMode(TRANSPARENT);
				auto drawSignalArrow = [&](int x, int fromY, int toY, COLORREF color) {
					CPen pen(PS_SOLID, 1, color);
					CPen* pOldP = memDC.SelectObject(&pen);
					memDC.MoveTo(x, fromY);
					memDC.LineTo(x, toY);

					int dir = (toY >= fromY) ? 1 : -1;
					int arrowLen = g_data.RDPI(4);
					int arrowHalf = g_data.RDPI(3);
					memDC.MoveTo(x, toY);
					memDC.LineTo(x - arrowHalf, toY - dir * arrowLen);
					memDC.MoveTo(x, toY);
					memDC.LineTo(x + arrowHalf, toY - dir * arrowLen);
					memDC.SelectObject(pOldP);
					};

				for (int i = 0; i < totalPoints; i++)
				{
					if (!buySignals[i] && !sellSignals[i] && !forbidSignals[i])
						continue;
					if (i >= static_cast<int>(dataPoints.size()))
						continue;

					int ptX = dataPoints[i].x;
					int ptY = ctx.priceChartTop + ctx.priceChartHeight - dataPoints[i].y;

					if (buySignals[i])
					{
						CBrush brush(COLOR_GREEN_DOWN);
						CPen pen(PS_SOLID, 1, COLOR_GREEN_DOWN);
						CBrush* pOldB = memDC.SelectObject(&brush);
						CPen* pOldP = memDC.SelectObject(&pen);
						memDC.Ellipse(ptX - dotR, ptY - dotR, ptX + dotR, ptY + dotR);
						if (!noLabelSignals[i])
						{
							memDC.SetTextColor(COLOR_GREEN_DOWN);
							CString label = buyReasons[i].IsEmpty() ? _T("B") : buyReasons[i];
							CSize sz = memDC.GetTextExtent(label);
							int labelY = ptY + dotR + labelOff;
							memDC.TextOut(ptX - sz.cx / 2, labelY, label);
							drawSignalArrow(ptX, labelY, ptY + dotR, COLOR_GREEN_DOWN);
						}
						memDC.SelectObject(pOldB);
						memDC.SelectObject(pOldP);
					}
					else if (sellSignals[i])
					{
						CBrush brush(COLOR_RED_UP);
						CPen pen(PS_SOLID, 1, COLOR_RED_UP);
						CBrush* pOldB = memDC.SelectObject(&brush);
						CPen* pOldP = memDC.SelectObject(&pen);
						memDC.Ellipse(ptX - dotR, ptY - dotR, ptX + dotR, ptY + dotR);
						if (!noLabelSignals[i])
						{
							memDC.SetTextColor(COLOR_RED_UP);
							CString label = sellReasons[i].IsEmpty() ? _T("S") : sellReasons[i];
							CSize sz = memDC.GetTextExtent(label);
							int labelY = ptY - dotR - labelOff - sz.cy;
							memDC.TextOut(ptX - sz.cx / 2, labelY, label);
							drawSignalArrow(ptX, labelY + sz.cy, ptY - dotR, COLOR_RED_UP);
						}
						memDC.SelectObject(pOldB);
						memDC.SelectObject(pOldP);
					}
				}
				memDC.SetBkMode(oldBkMode);
			}
		}
	}
}

void CTimelineChart::DrawTimelineHoverOverlay(CDC& memDC, const TimelineDrawContext& ctx, const HoverState& hover)
{
	if (!hover.isHoveringVolume || hover.hoveredBarIndex < 0)
		return;

	const auto& timelinePoint = *ctx.timelinePoint;
	if (hover.hoveredBarIndex >= static_cast<int>(timelinePoint.size()))
		return;

	STOCK::Price maxPrice = ctx.maxPrice;
	STOCK::Price minPrice = ctx.minPrice;
	double unitY = ctx.unitY;
	if (maxPrice <= 0 || minPrice < 0 || maxPrice <= minPrice || unitY <= 0)
	{
		STOCK::Price priceLimit = ctx.realtimeData.priceLimit;
		maxPrice = ctx.realtimeData.prevClosePrice + priceLimit;
		minPrice = ctx.realtimeData.prevClosePrice - priceLimit;
		const int pricePaddingY = g_data.RDPI(10);
		double paddingPrice = (maxPrice - minPrice) * pricePaddingY / ctx.priceChartHeight;
		maxPrice += paddingPrice;
		minPrice -= paddingPrice;
		unitY = ctx.priceChartHeight / (maxPrice - minPrice);
	}

	const int xSlots = ctx.xAxisPoints > 0 ? ctx.xAxisPoints : static_cast<int>(timelinePoint.size());
	const auto& item = timelinePoint[hover.hoveredBarIndex];
	int hoverX = static_cast<int>(ctx.chartWidth / static_cast<float>(xSlots) * hover.hoveredBarIndex + ctx.chartWidth / static_cast<float>(xSlots) / 2);

	int dotY = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((item.price - minPrice) * unitY));

	COLORREF dotColor = (item.price >= ctx.realtimeData.prevClosePrice) ? COLOR_RED_UP : COLOR_GREEN_DOWN;

	CPen crossPen(PS_DOT, 1, RGB(70, 130, 210));
	memDC.SelectObject(&crossPen);
	memDC.MoveTo(hoverX, ctx.priceChartTop);
	memDC.LineTo(hoverX, ctx.positionY);

	memDC.MoveTo(0, dotY);
	memDC.LineTo(ctx.chartWidth, dotY);

	int yAxisW = g_data.RDPI(50);
	CPoint origOrg = memDC.GetViewportOrg();
	memDC.OffsetViewportOrg(-yAxisW, 0);

	CString hoverPriceStr = ctx.realtimeData.IsETF() ? CCommon::FormatETFPrice(item.price) : CCommon::FormatFloat(item.price);
	CSize hoverPriceSize = memDC.GetTextExtent(hoverPriceStr);
	int priceLabelX = yAxisW - hoverPriceSize.cx - g_data.RDPI(3);
	int priceLabelY = dotY - hoverPriceSize.cy / 2;
	priceLabelY = max(ctx.priceChartTop, min(priceLabelY, ctx.priceChartTop + ctx.priceChartHeight - hoverPriceSize.cy));
	CRect priceBgRect(priceLabelX - g_data.RDPI(2), priceLabelY, priceLabelX + hoverPriceSize.cx + g_data.RDPI(2), priceLabelY + hoverPriceSize.cy);
	memDC.FillSolidRect(priceBgRect, RGB(200, 220, 255));
	memDC.SetTextColor(dotColor);
	memDC.TextOut(priceLabelX, priceLabelY, hoverPriceStr);

	memDC.SetViewportOrg(origOrg);

	{
		STOCK::Volume maxVol = 0;
		for (const auto& tp : timelinePoint)
		{
			if (tp.volume > maxVol)
				maxVol = tp.volume;
		}
		if (maxVol > 0 && item.volume > 0)
		{
			float volRatio = static_cast<float>(item.volume) / static_cast<float>(maxVol);
			int volBarY = ctx.volumeChartTop + ctx.volumeChartHeight - static_cast<int>(volRatio * ctx.volumeChartHeight);
			CPen volCrossPen(PS_DOT, 1, RGB(70, 130, 210));
			memDC.SelectObject(&volCrossPen);
			memDC.MoveTo(0, volBarY);
			memDC.LineTo(ctx.chartWidth, volBarY);

			memDC.OffsetViewportOrg(-yAxisW, 0);
			STOCK::Volume volInLots = item.volume / 100;
			CString volLabel = CCommon::FormatVolumeInt(volInLots);
			CSize volLabelSize = memDC.GetTextExtent(volLabel);
			int volLabelX = yAxisW - volLabelSize.cx - g_data.RDPI(3);
			int volLabelY = volBarY - volLabelSize.cy / 2;
			volLabelY = max(ctx.volumeChartTop, min(volLabelY, ctx.volumeChartTop + ctx.volumeChartHeight - volLabelSize.cy));
			CRect volBgRect(volLabelX - g_data.RDPI(2), volLabelY, volLabelX + volLabelSize.cx + g_data.RDPI(2), volLabelY + volLabelSize.cy);
			memDC.FillSolidRect(volBgRect, RGB(200, 220, 255));
			memDC.SetTextColor(COLOR_GRAY_TEXT);
			memDC.TextOut(volLabelX, volLabelY, volLabel);
			memDC.SetViewportOrg(origOrg);
		}
	}

	CString timeStr;
	if (!item.fullTime.empty() && (hover.viewMode == UI_VIEW_MIN5_KLINE || hover.viewMode == UI_VIEW_MIN30_KLINE))
	{
		// 不显示年份，仅 mm-dd hh:mm
		timeStr = CString(item.fullTime.c_str());
		if (timeStr.GetLength() >= 16)
			timeStr = timeStr.Mid(5, 11);
	}
	else if (!item.fullTime.empty() && hover.viewMode == UI_VIEW_DAY_KLINE)
	{
		// 日K线模式：悬停高亮显示完整日期 yyyy-mm-dd
		timeStr = CString(item.fullTime.c_str());
	}
	else
	{
		timeStr = CString(item.time.c_str());
		if (timeStr.GetLength() >= 5)
			timeStr = timeStr.Left(5);
	}
	CSize timeSize = memDC.GetTextExtent(timeStr);
	int timeLabelX = hoverX - timeSize.cx / 2;
	timeLabelX = max(g_data.RDPI(2), min(timeLabelX, ctx.chartWidth - timeSize.cx - g_data.RDPI(2)));
	int timeLabelY = ctx.positionY;
	CRect timeBgRect(timeLabelX - g_data.RDPI(3), timeLabelY, timeLabelX + timeSize.cx + g_data.RDPI(3), timeLabelY + timeSize.cy);
	memDC.FillSolidRect(timeBgRect, RGB(220, 235, 250));
	memDC.SetTextColor(COLOR_BLACK);
	memDC.SetBkMode(TRANSPARENT);
	memDC.TextOut(timeLabelX, timeLabelY, timeStr);
}

// 合并各来源的基金净值数据：key为"HH:MM"，value为该分钟净值
// 来源优先级（后者覆盖前者）：内存分时iopv → 数据库净值缓存 → 实时IOPV(取分时最后时间点)
// 注意：LoadLatestFundNavCache已过滤非交易时段；数据库晚于内存接口刷新，故放最后保证最新
static std::map<std::string, double> BuildFundNavTimeMap(const std::wstring& stockId,
	const STOCK::StockInfo& realtimeData, const std::vector<STOCK::TimelinePoint>& fullTimeline)
{
	std::map<std::string, double> iopvByTime;

	for (const auto& pt : fullTimeline)
	{
		if (pt.iopv > 0 && pt.time.length() >= 5)
			iopvByTime[pt.time.substr(0, 5)] = pt.iopv;
	}

	auto navPoints = g_data.GetDbManager().LoadLatestFundNavCache(stockId);
	for (const auto& nav : navPoints)
		iopvByTime[nav.time] = nav.iopv;

	if (realtimeData.iopv > 0 && !fullTimeline.empty() && fullTimeline.back().time.length() >= 5)
		iopvByTime[fullTimeline.back().time.substr(0, 5)] = realtimeData.iopv;

	return iopvByTime;
}

// 5分钟K线净值曲线：逐根K线按bar结束时刻（day的HH:MM部分）查净值连线
static void DrawMin5KLineNavCurve(CDC& memDC, const TimelineDrawContext& ctx,
	const std::vector<STOCK::KLinePoint>& klineData, const std::map<std::string, double>& iopvByTime,
	int klineStartIdx, int klineEndIdx, int totalPoints, float barTotalWidth,
	STOCK::Price minPrice, double unitY)
{
	const COLORREF navColor = RGB(160, 32, 240);
	CPen navPen(PS_SOLID, 1, navColor);
	CPen* pOldPen = memDC.SelectObject(&navPen);
	bool firstNavPoint = true;

	auto priceToY = [&](double price) -> int {
		int py = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((price - minPrice) * unitY));
		return max(ctx.priceChartTop, min(py, ctx.priceChartTop + ctx.priceChartHeight));
		};

	for (int i = 0; i < totalPoints && (klineStartIdx + i) < klineEndIdx; i++)
	{
		// K线day为"YYYY-MM-DD HH:MM"，时间为bar结束时刻，直接查该分钟净值
		const auto& kp = klineData[klineStartIdx + i];
		if (kp.day.length() < 16 || kp.day[10] != ' ')
			continue;
		auto it = iopvByTime.find(kp.day.substr(11, 5));
		if (it == iopvByTime.end())
			continue;

		int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(barTotalWidth / 2);
		int pointY = priceToY(it->second);
		if (firstNavPoint)
		{
			memDC.MoveTo(pointX, pointY);
			firstNavPoint = false;
		}
		else
		{
			memDC.LineTo(pointX, pointY);
		}
	}

	memDC.SelectObject(pOldPen);
}

void CTimelineChart::DrawMin5KLinePriceChart(CDC& memDC, const TimelineDrawContext& ctx, const HoverState& hover)
{
	const auto& timelinePoint = *ctx.timelinePoint;
	if (timelinePoint.empty())
		return;

	const int totalPoints = static_cast<int>(timelinePoint.size());
	const int xAxisPts = ctx.xAxisPoints > 0 ? ctx.xAxisPoints : totalPoints;

	STOCK::Price maxPrice = ctx.maxPrice;
	STOCK::Price minPrice = ctx.minPrice;
	double unitY = ctx.unitY;
	if (maxPrice <= 0 || minPrice < 0 || maxPrice <= minPrice || unitY <= 0)
	{
		STOCK::Price priceLimit = ctx.realtimeData.priceLimit;
		maxPrice = ctx.realtimeData.prevClosePrice + priceLimit;
		minPrice = ctx.realtimeData.prevClosePrice - priceLimit;
		const int pricePaddingY = g_data.RDPI(10);
		double paddingPrice = (maxPrice - minPrice) * pricePaddingY / ctx.priceChartHeight;
		maxPrice += paddingPrice;
		minPrice -= paddingPrice;
		unitY = ctx.priceChartHeight / (maxPrice - minPrice);
	}

	const auto& klineData = *ctx.klineData;
	int klineStartIdx = ctx.startIndex;
	int klineEndIdx = klineStartIdx + totalPoints;
	if (klineEndIdx > static_cast<int>(klineData.size()))
		klineEndIdx = static_cast<int>(klineData.size());

	if (klineStartIdx >= static_cast<int>(klineData.size()))
		return;

	float barTotalWidth = static_cast<float>(ctx.chartWidth) / xAxisPts;
	int barWidth = max(1, static_cast<int>(barTotalWidth * 0.7));
	int gap = static_cast<int>(barTotalWidth) - barWidth;
	if (gap < 1) gap = 1;

	auto priceToY = [&](STOCK::Price price) -> int {
		return ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>((price - minPrice) * unitY);
		};

	//// 最新一天K线背景高亮,先频闭
	//{
	//	if (!klineData.empty() && (hover.viewMode == UI_VIEW_MIN5_KLINE || hover.viewMode == UI_VIEW_MIN30_KLINE))
	//	{
	//		const auto& lastKp = klineData.back();
	//		std::string lastDate;
	//		auto spacePos = lastKp.day.find(' ');
	//		if (spacePos != std::string::npos)
	//			lastDate = lastKp.day.substr(0, spacePos);
	//		else
	//			lastDate = lastKp.day;

	//		if (!lastDate.empty())
	//		{
	//			int firstIdx = -1, lastIdx = -1;
	//			for (int i = 0; i < totalPoints && (klineStartIdx + i) < klineEndIdx; i++)
	//			{
	//				const auto& kp = klineData[klineStartIdx + i];
	//				std::string kpDate;
	//				auto sp = kp.day.find(' ');
	//				if (sp != std::string::npos)
	//					kpDate = kp.day.substr(0, sp);
	//				else
	//					kpDate = kp.day;
	//				if (kpDate == lastDate)
	//				{
	//					if (firstIdx < 0) firstIdx = i;
	//					lastIdx = i;
	//				}
	//			}

	//			if (firstIdx >= 0 && lastIdx >= 0)
	//			{
	//				int xLeft = static_cast<int>(ctx.chartWidth / static_cast<float>(xAxisPts) * firstIdx);
	//				int xRight = static_cast<int>(ctx.chartWidth / static_cast<float>(xAxisPts) * (lastIdx + 1));

	//				CBrush highlightBrush(COLOR_LIGHT_BLUE);
	//				memDC.FillRect(CRect(xLeft, ctx.priceChartTop, xRight, ctx.priceChartTop + ctx.priceChartHeight), &highlightBrush);
	//				memDC.FillRect(CRect(xLeft, ctx.volumeChartTop, xRight, ctx.volumeChartTop + ctx.volumeChartHeight), &highlightBrush);
	//				memDC.FillRect(CRect(xLeft, ctx.macdChartTop, xRight, ctx.macdChartTop + ctx.macdChartHeight), &highlightBrush);

	//				DrawTimelineGridLines(memDC, ctx);
	//			}
	//		}
	//	}
	//}

	STOCK::Price prevClose = ctx.realtimeData.prevClosePrice;

	for (int i = 0; i < totalPoints && (klineStartIdx + i) < klineEndIdx; i++)
	{
		const auto& kp = klineData[klineStartIdx + i];
		if (kp.close <= 0) continue;

		int centerX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(barTotalWidth / 2);
		int leftX = centerX - barWidth / 2;

		bool isUp = (kp.close >= kp.open);
		COLORREF barColor = isUp ? COLOR_RED_UP : COLOR_GREEN_DOWN;

		int openY = priceToY(kp.open);
		int closeY = priceToY(kp.close);
		int highY = priceToY(kp.high);
		int lowY = priceToY(kp.low);

		openY = max(ctx.priceChartTop, min(openY, ctx.priceChartTop + ctx.priceChartHeight));
		closeY = max(ctx.priceChartTop, min(closeY, ctx.priceChartTop + ctx.priceChartHeight));
		highY = max(ctx.priceChartTop, min(highY, ctx.priceChartTop + ctx.priceChartHeight));
		lowY = max(ctx.priceChartTop, min(lowY, ctx.priceChartTop + ctx.priceChartHeight));

		CPen barPen(PS_SOLID, 1, barColor);
		memDC.SelectObject(&barPen);
		memDC.MoveTo(centerX, highY);
		memDC.LineTo(centerX, lowY);

		int bodyTop = min(openY, closeY);
		int bodyBottom = max(openY, closeY);
		int bodyHeight = bodyBottom - bodyTop;
		if (bodyHeight < 1) bodyHeight = 1;

		if (isUp)
		{
			CBrush brush(barColor);
			CBrush* pOldBrush = memDC.SelectObject(&brush);
			memDC.Rectangle(leftX, bodyTop, leftX + barWidth, bodyBottom + 1);
			memDC.SelectObject(pOldBrush);
		}
		else
		{
			CBrush brush(barColor);
			CBrush* pOldBrush = memDC.SelectObject(&brush);
			memDC.Rectangle(leftX, bodyTop, leftX + barWidth, bodyBottom + 1);
			memDC.SelectObject(pOldBrush);
		}
	}

	if (hover.showMA)
	{
		// MA5/MA10/MA20均线配色
		const COLORREF ma5Color = RGB(240, 117, 40);
		const COLORREF ma10Color = RGB(21, 101, 192);
		const COLORREF ma20Color = RGB(128, 40, 149);

		auto drawMALine = [&](int fieldOffset, COLORREF color) {
			CPen maPen(PS_SOLID, 1, color);
			memDC.SelectObject(&maPen);
			bool first = true;
			for (int i = 0; i < totalPoints; i++)
			{
				const auto& item = timelinePoint[i];
				STOCK::Price maVal = 0;
				switch (fieldOffset)
				{
				case 5: maVal = item.ma5; break;
				case 10: maVal = item.ma10; break;
				case 20: maVal = item.ma20; break;
				}
				if (maVal <= 0) { first = true; continue; }
				int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) / 2);
				double yVal = (maVal - minPrice) * unitY;
				int py = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(yVal);
				if (first)
				{
					memDC.MoveTo(pointX, py);
					first = false;
				}
				else
				{
					memDC.LineTo(pointX, py);
				}
			}
			};

		drawMALine(5, ma5Color);
		drawMALine(10, ma10Color);
		drawMALine(20, ma20Color);
	}

	// 布林带（短横线虚线）——直接读模型缓存的上中下轨序列
	if (hover.showBollBands && ctx.indicatorModel)
	{
		const auto& upperBand = ctx.indicatorModel->GetBollUpper();
		const auto& middleBand = ctx.indicatorModel->GetBollMid();
		const auto& lowerBand = ctx.indicatorModel->GetBollLower();

		auto bandPriceToY = [&](double price) -> int {
			int py = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((price - minPrice) * unitY));
			return max(ctx.priceChartTop, min(py, ctx.priceChartTop + ctx.priceChartHeight));
			};

		auto drawBandLine = [&](const std::vector<double>& band, COLORREF color) {
			CPen bandPen(PS_DASH, 1, color);
			memDC.SelectObject(&bandPen);
			bool first = true;
			for (int i = 0; i < totalPoints; i++)
			{
				int globalIdx = ctx.startIndex + i;
				if (globalIdx >= static_cast<int>(band.size()) || band[globalIdx] <= 0) continue;
				int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) / 2);
				int py = bandPriceToY(band[globalIdx]);
				if (first)
				{
					memDC.MoveTo(pointX, py);
					first = false;
				}
				else
				{
					memDC.LineTo(pointX, py);
				}
			}
			};

		drawBandLine(upperBand, COLOR_RED_UP);
		drawBandLine(middleBand, RGB(0, 0, 230));
		drawBandLine(lowerBand, COLOR_GREEN_DOWN);
	}

	// 绘制基金净值曲线：数据取每根5分钟K线最后时间点（bar时间即结束时刻）的净值
	if (ctx.realtimeData.IsETF() && hover.viewMode == UI_VIEW_MIN5_KLINE
		&& ctx.fullTimeline && !ctx.fullTimeline->empty())
	{
		std::map<std::string, double> iopvByTime = BuildFundNavTimeMap(hover.stockId, ctx.realtimeData, *ctx.fullTimeline);
		if (!iopvByTime.empty())
			DrawMin5KLineNavCurve(memDC, ctx, klineData, iopvByTime, klineStartIdx, klineEndIdx, totalPoints, barTotalWidth, minPrice, unitY);
	}

	// 最高/最低价标签：优先展示最后一个交易日（>=5分钟K线：末尾同日期的最后N根）的最高/最低，
	// 可视区间内没有该日数据时退回整个可视区间的极值
	{
		STOCK::Price hiPrice = 0, loPrice = (std::numeric_limits<STOCK::Price>::max)();
		int hiIdx = -1, loIdx = -1;

		// 确定"最后一天"的数据日期：以可视区间最后一根K线的日期为界
		// 注意：5/30分钟K线的day为"YYYY-MM-DD HH:MM"，须截取前10字符日期比较，否则只能匹配到最后一根
		std::string lastDay;
		if (klineStartIdx < klineEndIdx && klineData[klineEndIdx - 1].day.length() >= 10)
			lastDay = klineData[klineEndIdx - 1].day.substr(0, 10);

		auto scanRange = [&](int from, int to) {
			for (int i = from; i < to; i++)
			{
				const auto& kp = klineData[klineStartIdx + i];
				if (kp.high > 0 && kp.high > hiPrice) { hiPrice = kp.high; hiIdx = i; }  // 先到先得
				if (kp.low > 0 && kp.low < loPrice) { loPrice = kp.low; loIdx = i; }
			}
			};

		// 最后一天在可视区间内的数据范围：从末尾向前找连续同日的第一根
		int dayFirst = klineEndIdx;   // 未找到任何同日数据时的哨兵
		if (!lastDay.empty())
		{
			for (int i = klineEndIdx - 1; i >= klineStartIdx; --i)
			{
				std::string d = klineData[i].day.substr(0, 10);
				if (d == lastDay)
					dayFirst = i;
				else
					break;
			}
		}
		if (!lastDay.empty() && dayFirst < klineEndIdx)
			scanRange(dayFirst - klineStartIdx, klineEndIdx - klineStartIdx);   // 最后一天
		else
			scanRange(0, klineEndIdx - klineStartIdx);                          // 回退：整个可视区间

		if (hiIdx >= 0 && hiPrice > 0)
		{
			int hiX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * hiIdx) + static_cast<int>(barTotalWidth / 2);
			int hiY = priceToY(hiPrice);
			DrawPricePointLabel(memDC, hiX, hiY, 0, ctx.priceChartTop, ctx.chartWidth, ctx.priceChartHeight,
				hiPrice, true, COLOR_RED_UP);
		}

		// 最低点标签：即使与最高点在同一根K线（loIdx==hiIdx）也绘制，仅价格相同时跳过
		if (loIdx >= 0 && loPrice > 0 && loPrice != hiPrice)
		{
			int loX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * loIdx) + static_cast<int>(barTotalWidth / 2);
			int loY = priceToY(loPrice);
			DrawPricePointLabel(memDC, loX, loY, 0, ctx.priceChartTop, ctx.chartWidth, ctx.priceChartHeight,
				loPrice, false, COLOR_GREEN_DOWN);
		}
	}

	// 智能分析买卖点标记（仅5分钟K线模式）
	if (hover.viewMode == UI_VIEW_MIN5_KLINE)
	{
		auto stockData = g_data.GetStockData(hover.stockId);
		if (stockData)
		{
			auto min5KLineObj = stockData->getMin5KLineData();
			auto min30KLineObj = stockData->getMin30KLineData();
			if (min5KLineObj && min5KLineObj->data.size() >= 120 &&
				min30KLineObj && min30KLineObj->data.size() >= 22)
			{
				std::vector<STOCK::Bar> bars5, bars30;
				bars5.reserve(min5KLineObj->data.size());
				for (const auto& kp : min5KLineObj->data) bars5.push_back(STOCK::Bar::FromKLinePoint(kp));
				bars30.reserve(min30KLineObj->data.size());
				for (const auto& kp : min30KLineObj->data) bars30.push_back(STOCK::Bar::FromKLinePoint(kp));

				auto ar = CSignalAnalyzer::AnalyzeSignalAt(bars5, bars30, static_cast<int>(bars5.size()) - 1);
				auto& signals = ar.batchSignals;

				std::set<int> kline5FilteredBarIndices;
				{
					bool lastDirIsBuy = false;
					bool hasLastDir = false;
					int lastBarIdx = -1;
					for (const auto& sig : signals)
					{
						if (sig.isForbid) { hasLastDir = false; continue; }
						int bi = sig.barIndex;
						if (bi == lastBarIdx) continue;
						if (hasLastDir && sig.isBuy == lastDirIsBuy)
							kline5FilteredBarIndices.insert(bi);
						else
						{
							lastDirIsBuy = sig.isBuy;
							hasLastDir = true;
						}
						lastBarIdx = bi;
					}
				}

				int oldBkMode = memDC.SetBkMode(TRANSPARENT);
				auto drawSignalArrow = [&](int x, int fromY, int toY, COLORREF color) {
					CPen pen(PS_SOLID, 1, color);
					CPen* pOldP = memDC.SelectObject(&pen);
					memDC.MoveTo(x, fromY);
					memDC.LineTo(x, toY);

					int dir = (toY >= fromY) ? 1 : -1;
					int arrowLen = g_data.RDPI(4);
					int arrowHalf = g_data.RDPI(3);
					memDC.MoveTo(x, toY);
					memDC.LineTo(x - arrowHalf, toY - dir * arrowLen);
					memDC.MoveTo(x, toY);
					memDC.LineTo(x + arrowHalf, toY - dir * arrowLen);
					memDC.SelectObject(pOldP);
					};

				for (const auto& sig : signals)
				{
					int klineIdx = sig.barIndex;
					if (klineIdx < klineStartIdx || klineIdx >= klineEndIdx)
						continue;

					int visibleIdx = klineIdx - klineStartIdx;
					int barX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * visibleIdx) + static_cast<int>(barTotalWidth / 2);

					if (sig.isForbid)
					{
						int barY = priceToY(klineData[klineIdx].close);
						CPen pen(PS_SOLID, 2, COLOR_GRAY_TEXT);
						CPen* pOldP = memDC.SelectObject(&pen);
						int r = g_data.RDPI(3);
						memDC.MoveTo(barX - r, barY - r); memDC.LineTo(barX + r, barY + r);
						memDC.MoveTo(barX + r, barY - r); memDC.LineTo(barX - r, barY + r);
						memDC.SelectObject(pOldP);
						continue;
					}

					if (sig.isBuy)
					{
						int barY = priceToY(klineData[klineIdx].low) + g_data.RDPI(2);
						CBrush brush(COLOR_GREEN_DOWN);
						CPen pen(PS_SOLID, 1, COLOR_GREEN_DOWN);
						CBrush* pOldB = memDC.SelectObject(&brush);
						CPen* pOldP = memDC.SelectObject(&pen);
						int r = g_data.RDPI(3);
						int labelOff = g_data.RDPI(8);
						memDC.Ellipse(barX - r, barY, barX + r, barY + 2 * r);
						if (!kline5FilteredBarIndices.count(klineIdx))
						{
							memDC.SetTextColor(COLOR_GREEN_DOWN);
							CSize sz = memDC.GetTextExtent(sig.reason);
							int labelY = barY + 2 * r + labelOff;
							memDC.TextOut(barX - sz.cx / 2, labelY, sig.reason);
							drawSignalArrow(barX, labelY, barY + 2 * r, COLOR_GREEN_DOWN);
						}
						memDC.SelectObject(pOldB);
						memDC.SelectObject(pOldP);
					}
					else
					{
						int barY = priceToY(klineData[klineIdx].high) - g_data.RDPI(2);
						CBrush brush(COLOR_RED_UP);
						CPen pen(PS_SOLID, 1, COLOR_RED_UP);
						CBrush* pOldB = memDC.SelectObject(&brush);
						CPen* pOldP = memDC.SelectObject(&pen);
						int r = g_data.RDPI(3);
						int labelOff = g_data.RDPI(8);
						memDC.Ellipse(barX - r, barY - 2 * r, barX + r, barY);
						if (!kline5FilteredBarIndices.count(klineIdx))
						{
							memDC.SetTextColor(COLOR_RED_UP);
							CSize sz = memDC.GetTextExtent(sig.reason);
							int labelY = barY - 2 * r - sz.cy - labelOff;
							memDC.TextOut(barX - sz.cx / 2, labelY, sig.reason);
							drawSignalArrow(barX, labelY + sz.cy, barY - 2 * r, COLOR_RED_UP);
						}
						memDC.SelectObject(pOldB);
						memDC.SelectObject(pOldP);
					}
				}
				memDC.SetBkMode(oldBkMode);
			}
		}
	}
}

void CTimelineChart::DrawDayKLinePriceChart(CDC& memDC, const TimelineDrawContext& ctx, const HoverState& hover)
{
	const auto& timelinePoint = *ctx.timelinePoint;
	if (timelinePoint.empty())
		return;

	const int totalPoints = static_cast<int>(timelinePoint.size());

	STOCK::Price maxPrice = ctx.maxPrice;
	STOCK::Price minPrice = ctx.minPrice;
	double unitY = ctx.unitY;
	if (maxPrice <= 0 || minPrice < 0 || maxPrice <= minPrice || unitY <= 0)
	{
		STOCK::Price priceLimit = ctx.realtimeData.priceLimit;
		maxPrice = ctx.realtimeData.prevClosePrice + priceLimit;
		minPrice = ctx.realtimeData.prevClosePrice - priceLimit;
		const int pricePaddingY = g_data.RDPI(10);
		double paddingPrice = (maxPrice - minPrice) * pricePaddingY / ctx.priceChartHeight;
		maxPrice += paddingPrice;
		minPrice -= paddingPrice;
		unitY = ctx.priceChartHeight / (maxPrice - minPrice);
	}

	const auto& klineData = *ctx.klineData;
	int klineStartIdx = ctx.startIndex;
	int klineEndIdx = klineStartIdx + totalPoints;
	if (klineEndIdx > static_cast<int>(klineData.size()))
		klineEndIdx = static_cast<int>(klineData.size());

	if (klineStartIdx >= static_cast<int>(klineData.size()))
		return;

	float barTotalWidth = static_cast<float>(ctx.chartWidth) / totalPoints;
	int barWidth = max(1, static_cast<int>(barTotalWidth * 0.7));
	int gap = static_cast<int>(barTotalWidth) - barWidth;
	if (gap < 1) gap = 1;

	auto priceToY = [&](STOCK::Price price) -> int {
		return ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>((price - minPrice) * unitY);
		};

	STOCK::Price prevClose = ctx.realtimeData.prevClosePrice;

	for (int i = 0; i < totalPoints && (klineStartIdx + i) < klineEndIdx; i++)
	{
		const auto& kp = klineData[klineStartIdx + i];
		if (kp.close <= 0) continue;

		int centerX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(barTotalWidth / 2);
		int leftX = centerX - barWidth / 2;

		bool isUp = (kp.close >= kp.open);
		COLORREF barColor = isUp ? COLOR_RED_UP : COLOR_GREEN_DOWN;

		int openY = priceToY(kp.open);
		int closeY = priceToY(kp.close);
		int highY = priceToY(kp.high);
		int lowY = priceToY(kp.low);

		openY = max(ctx.priceChartTop, min(openY, ctx.priceChartTop + ctx.priceChartHeight));
		closeY = max(ctx.priceChartTop, min(closeY, ctx.priceChartTop + ctx.priceChartHeight));
		highY = max(ctx.priceChartTop, min(highY, ctx.priceChartTop + ctx.priceChartHeight));
		lowY = max(ctx.priceChartTop, min(lowY, ctx.priceChartTop + ctx.priceChartHeight));

		CPen barPen(PS_SOLID, 1, barColor);
		memDC.SelectObject(&barPen);
		memDC.MoveTo(centerX, highY);
		memDC.LineTo(centerX, lowY);

		int bodyTop = min(openY, closeY);
		int bodyBottom = max(openY, closeY);
		int bodyHeight = bodyBottom - bodyTop;
		if (bodyHeight < 1) bodyHeight = 1;

		if (isUp)
		{
			CBrush* pOldBrush = memDC.SelectObject((CBrush*)CBrush::FromHandle((HBRUSH)GetStockObject(NULL_BRUSH)));
			memDC.Rectangle(leftX, bodyTop, leftX + barWidth, bodyBottom + 1);
			memDC.SelectObject(pOldBrush);
		}
		else
		{
			CBrush brush(barColor);
			CBrush* pOldBrush = memDC.SelectObject(&brush);
			memDC.Rectangle(leftX, bodyTop, leftX + barWidth, bodyBottom + 1);
			memDC.SelectObject(pOldBrush);
		}
	}

	if (hover.showMA)
	{
		// 日K线MA均线配色
		const COLORREF ma5Color = RGB(240, 117, 40);
		const COLORREF ma10Color = RGB(21, 101, 192);
		const COLORREF ma20Color = RGB(128, 40, 149);

		auto drawMALine = [&](int fieldOffset, COLORREF color) {
			CPen maPen(PS_SOLID, 1, color);
			memDC.SelectObject(&maPen);
			bool first = true;
			for (int i = 0; i < totalPoints; i++)
			{
				const auto& item = timelinePoint[i];
				STOCK::Price maVal = 0;
				switch (fieldOffset)
				{
				case 5: maVal = item.ma5; break;
				case 10: maVal = item.ma10; break;
				case 20: maVal = item.ma20; break;
				}
				if (maVal <= 0) { first = true; continue; }
				int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) / 2);
				double yVal = (maVal - minPrice) * unitY;
				int py = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(yVal);
				if (first)
				{
					memDC.MoveTo(pointX, py);
					first = false;
				}
				else
				{
					memDC.LineTo(pointX, py);
				}
			}
			};

		drawMALine(5, ma5Color);
		drawMALine(10, ma10Color);
		drawMALine(20, ma20Color);
	}

	// 布林带（短横线虚线）——直接读模型缓存的上中下轨序列
	if (hover.showBollBands && ctx.indicatorModel)
	{
		const auto& upperBand = ctx.indicatorModel->GetBollUpper();
		const auto& middleBand = ctx.indicatorModel->GetBollMid();
		const auto& lowerBand = ctx.indicatorModel->GetBollLower();

		auto bandPriceToY = [&](double price) -> int {
			int py = ctx.priceChartTop + ctx.priceChartHeight - static_cast<int>(round((price - minPrice) * unitY));
			return max(ctx.priceChartTop, min(py, ctx.priceChartTop + ctx.priceChartHeight));
			};

		auto drawBandLine = [&](const std::vector<double>& band, COLORREF color) {
			CPen bandPen(PS_DASH, 1, color);
			memDC.SelectObject(&bandPen);
			bool first = true;
			for (int i = 0; i < totalPoints; i++)
			{
				int globalIdx = ctx.startIndex + i;
				if (globalIdx >= static_cast<int>(band.size()) || band[globalIdx] <= 0) continue;
				int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) / 2);
				int py = bandPriceToY(band[globalIdx]);
				if (first)
				{
					memDC.MoveTo(pointX, py);
					first = false;
				}
				else
				{
					memDC.LineTo(pointX, py);
				}
			}
			};

		drawBandLine(upperBand, COLOR_RED_UP);
		drawBandLine(middleBand, RGB(0, 0, 230));
		drawBandLine(lowerBand, COLOR_GREEN_DOWN);
	}

	// 最高/最低价标签：日K图每根K线=一天，直接扫描当前可视范围内的极值
	{
		STOCK::Price hiPrice = 0, loPrice = (std::numeric_limits<STOCK::Price>::max)();
		int hiIdx = -1, loIdx = -1;

		auto scanRange = [&](int from, int to) {
			for (int i = from; i < to; i++)
			{
				const auto& kp = klineData[klineStartIdx + i];
				if (kp.high > 0 && kp.high > hiPrice) { hiPrice = kp.high; hiIdx = i; }  // 先到先得
				if (kp.low > 0 && kp.low < loPrice) { loPrice = kp.low; loIdx = i; }
			}
			};

		scanRange(0, klineEndIdx - klineStartIdx);

		if (hiIdx >= 0 && hiPrice > 0)
		{
			int hiX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * hiIdx) + static_cast<int>(barTotalWidth / 2);
			int hiY = priceToY(hiPrice);
			DrawPricePointLabel(memDC, hiX, hiY, 0, ctx.priceChartTop, ctx.chartWidth, ctx.priceChartHeight,
				hiPrice, true, COLOR_RED_UP);
		}

		// 最低点标签：即使与最高点在同一根K线（loIdx==hiIdx）也绘制，仅价格相同时跳过
		if (loIdx >= 0 && loPrice > 0 && loPrice != hiPrice)
		{
			int loX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * loIdx) + static_cast<int>(barTotalWidth / 2);
			int loY = priceToY(loPrice);
			DrawPricePointLabel(memDC, loX, loY, 0, ctx.priceChartTop, ctx.chartWidth, ctx.priceChartHeight,
				loPrice, false, COLOR_GREEN_DOWN);
		}
	}
}

void CTimelineChart::DrawPriceChartArea(CDC& memDC, const TimelineDrawContext& ctx, int areaTop, int areaHeight, HoverState& hover)
{
	const auto& timelinePoint = *ctx.timelinePoint;
	int titleH = g_data.RDPI(16);
	int oldBkMode = memDC.SetBkMode(TRANSPARENT);

	CRect priceTitleRect(0, areaTop, ctx.chartWidth, areaTop + titleH);
	memDC.FillSolidRect(priceTitleRect, RGB(245, 245, 245));

	if (!hover.timelinePriceTitleTip.IsEmpty())
	{
		memDC.SetTextColor(COLOR_BLACK);
		memDC.DrawText(hover.timelinePriceTitleTip, priceTitleRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
	}
	else if (hover.viewMode == UI_VIEW_DAY_KLINE && ctx.klineData && !ctx.klineData->empty())
	{
		int xPos = g_data.RDPI(4);
		int centerY = areaTop + titleH / 2;

		const auto& klineData = *ctx.klineData;
		int klineIdx = -1;
		if (hover.hoveredBarIndex >= 0 && hover.isHoveringVolume)
		{
			klineIdx = ctx.startIndex + hover.hoveredBarIndex;
		}
		else
		{
			klineIdx = ctx.startIndex + static_cast<int>(timelinePoint.size()) - 1;
		}
		if (klineIdx < 0 || klineIdx >= static_cast<int>(klineData.size()))
			klineIdx = static_cast<int>(klineData.size()) - 1;

		if (klineIdx >= 0 && klineIdx < static_cast<int>(klineData.size()))
		{
			const auto& kp = klineData[klineIdx];
			STOCK::Price prevClose = ctx.realtimeData.prevClosePrice;

			auto drawKLineLabel = [&](const CString& label, STOCK::Price value, COLORREF labelColor, COLORREF valueColor) {
				CString valStr = CCommon::FormatFloat(value);
				memDC.SetTextColor(labelColor);
				CSize ls = memDC.GetTextExtent(label);
				memDC.TextOut(xPos, centerY - ls.cy / 2, label);
				xPos += ls.cx;
				memDC.SetTextColor(valueColor);
				CSize vs = memDC.GetTextExtent(valStr);
				memDC.TextOut(xPos, centerY - vs.cy / 2, valStr);
				xPos += vs.cx + g_data.RDPI(4);
				};

			drawKLineLabel(_T("开:"), kp.open, COLOR_BLACK, (kp.open >= prevClose ? COLOR_RED_UP : COLOR_GREEN_DOWN));
			drawKLineLabel(_T("收:"), kp.close, COLOR_BLACK, (kp.close >= prevClose ? COLOR_RED_UP : COLOR_GREEN_DOWN));
		}
	}
	else if ((hover.viewMode == UI_VIEW_MIN5_KLINE || hover.viewMode == UI_VIEW_MIN30_KLINE) && ctx.klineData && !ctx.klineData->empty())
	{
		// 5分钟/30分钟K线模式：标题栏与分时模式一致，显示现价/IOPV/均价
		STOCK::Price prevClose = ctx.realtimeData.prevClosePrice;

		bool isHovering = (hover.hoveredBarIndex >= 0 && hover.isHoveringVolume);
		STOCK::Price dispAvgPrice = isHovering ? hover.hoveredData.averagePrice : timelinePoint.back().averagePrice;
		if (dispAvgPrice <= 0)
			dispAvgPrice = isHovering ? hover.hoverMa1 : ctx.ma1;

		int xPos = g_data.RDPI(4);
		int centerY = areaTop + titleH / 2;

		auto drawLabelValue = [&](const CString& labelText, STOCK::Price value, COLORREF labelColor, COLORREF valueColor) {
			CString valStr = CCommon::FormatFloat(value);
			memDC.SetTextColor(labelColor);
			CSize ls = memDC.GetTextExtent(labelText);
			memDC.TextOut(xPos, centerY - ls.cy / 2, labelText);
			xPos += ls.cx;
			memDC.SetTextColor(valueColor);
			CSize vs = memDC.GetTextExtent(valStr);
			memDC.TextOut(xPos, centerY - vs.cy / 2, valStr);
			xPos += vs.cx + g_data.RDPI(4);
			};

		auto cmpPrevClose = [prevClose](STOCK::Price p) -> COLORREF {
			if (prevClose <= 0) return COLOR_BLACK;
			if (p > prevClose) return COLOR_RED_UP;
			if (p < prevClose) return COLOR_GREEN_DOWN;
			return COLOR_BLACK;
			};

		// 左侧：现价
		drawLabelValue(_T("现:"), ctx.realtimeData.currentPrice, COLOR_BLACK, cmpPrevClose(ctx.realtimeData.currentPrice));

		// 右侧：ETF显示净:xx 溢:+/-xx%，股票显示均:xx
		if (ctx.realtimeData.IsETF())
		{
			COLORREF iopvColor = COLOR_BLACK;
			if (ctx.realtimeData.iopv > ctx.realtimeData.currentPrice)
				iopvColor = COLOR_RED_UP;
			else if (ctx.realtimeData.iopv < ctx.realtimeData.currentPrice)
				iopvColor = COLOR_GREEN_DOWN;

			CString iopvLabel = _T("净:");
			CString iopvVal;
			iopvVal.Format(_T("%.4f"), ctx.realtimeData.iopv);
			CSize iopvLs = memDC.GetTextExtent(iopvLabel);
			CSize iopvVs = memDC.GetTextExtent(iopvVal);

			CString premLabel = _T(" 溢:");
			CString premVal;
			double premRate = ctx.realtimeData.iopvPremiumRate;
			if (premRate >= 0)
				premVal.Format(_T("+%.2f%%"), premRate);
			else
				premVal.Format(_T("%.2f%%"), premRate);
			COLORREF premColor = premRate > 0 ? COLOR_RED_UP : (premRate < 0 ? COLOR_GREEN_DOWN : COLOR_BLACK);
			CSize premLs = memDC.GetTextExtent(premLabel);
			CSize premVs = memDC.GetTextExtent(premVal);

			int rightX = ctx.chartWidth - g_data.RDPI(4) - iopvLs.cx - iopvVs.cx - premLs.cx - premVs.cx;
			memDC.SetTextColor(COLOR_BLACK);
			memDC.TextOut(rightX, centerY - premLs.cy / 2, premLabel);
			rightX += premLs.cx;
			memDC.SetTextColor(premColor);
			memDC.TextOut(rightX, centerY - premVs.cy / 2, premVal);
			rightX += premVs.cx;
			memDC.SetTextColor(COLOR_BLACK);
			memDC.TextOut(rightX, centerY - iopvLs.cy / 2, iopvLabel);
			rightX += iopvLs.cx;
			memDC.SetTextColor(iopvColor);
			memDC.TextOut(rightX, centerY - iopvVs.cy / 2, iopvVal);
		}
		else
		{
			CString avgLabel = _T("均:");
			CString avgVal = CCommon::FormatFloat(dispAvgPrice);
			COLORREF avgColor = cmpPrevClose(dispAvgPrice);
			CSize avgLs = memDC.GetTextExtent(avgLabel);
			CSize avgVs = memDC.GetTextExtent(avgVal);
			int rightX = ctx.chartWidth - g_data.RDPI(4) - avgLs.cx - avgVs.cx;
			memDC.SetTextColor(COLOR_BLACK);
			memDC.TextOut(rightX, centerY - avgLs.cy / 2, avgLabel);
			rightX += avgLs.cx;
			memDC.SetTextColor(avgColor);
			memDC.TextOut(rightX, centerY - avgVs.cy / 2, avgVal);
		}
	}
	else if (!timelinePoint.empty())
	{
		STOCK::Price prevClose = ctx.realtimeData.prevClosePrice;

		bool isHovering = (hover.hoveredBarIndex >= 0 && hover.isHoveringVolume);
		STOCK::Price dispAvgPrice = isHovering ? hover.hoveredData.averagePrice : timelinePoint.back().averagePrice;
		if (dispAvgPrice <= 0)
			dispAvgPrice = isHovering ? hover.hoverMa1 : ctx.ma1;

		int xPos = g_data.RDPI(4);
		int centerY = areaTop + titleH / 2;

		auto drawLabelValue = [&](const CString& labelText, STOCK::Price value, COLORREF labelColor, COLORREF valueColor) {
			CString valStr = CCommon::FormatFloat(value);
			memDC.SetTextColor(labelColor);
			CSize ls = memDC.GetTextExtent(labelText);
			memDC.TextOut(xPos, centerY - ls.cy / 2, labelText);
			xPos += ls.cx;
			memDC.SetTextColor(valueColor);
			CSize vs = memDC.GetTextExtent(valStr);
			memDC.TextOut(xPos, centerY - vs.cy / 2, valStr);
			xPos += vs.cx + g_data.RDPI(4);
			};

		auto cmpPrevClose = [prevClose](STOCK::Price p) -> COLORREF {
			if (prevClose <= 0) return COLOR_BLACK;
			if (p > prevClose) return COLOR_RED_UP;
			if (p < prevClose) return COLOR_GREEN_DOWN;
			return COLOR_BLACK;
			};

		// 左侧：现价
		drawLabelValue(_T("现:"), ctx.realtimeData.currentPrice, COLOR_BLACK, cmpPrevClose(ctx.realtimeData.currentPrice));

		// 右侧：ETF显示净:xx 溢:+/-xx%，股票显示均:xx
		if (ctx.realtimeData.IsETF())
		{
			COLORREF iopvColor = COLOR_BLACK;
			if (ctx.realtimeData.iopv > ctx.realtimeData.currentPrice)
				iopvColor = COLOR_RED_UP;
			else if (ctx.realtimeData.iopv < ctx.realtimeData.currentPrice)
				iopvColor = COLOR_GREEN_DOWN;

			CString iopvLabel = _T("净:");
			CString iopvVal;
			iopvVal.Format(_T("%.4f"), ctx.realtimeData.iopv);
			CSize iopvLs = memDC.GetTextExtent(iopvLabel);
			CSize iopvVs = memDC.GetTextExtent(iopvVal);

			CString premLabel = _T(" 溢:");
			CString premVal;
			double premRate = ctx.realtimeData.iopvPremiumRate;
			if (premRate >= 0)
				premVal.Format(_T("+%.2f%%"), premRate);
			else
				premVal.Format(_T("%.2f%%"), premRate);
			COLORREF premColor = premRate > 0 ? COLOR_RED_UP : (premRate < 0 ? COLOR_GREEN_DOWN : COLOR_BLACK);
			CSize premLs = memDC.GetTextExtent(premLabel);
			CSize premVs = memDC.GetTextExtent(premVal);

			int rightX = ctx.chartWidth - g_data.RDPI(4) - iopvLs.cx - iopvVs.cx - premLs.cx - premVs.cx;

			memDC.SetTextColor(COLOR_BLACK);
			memDC.TextOut(rightX, centerY - premLs.cy / 2, premLabel);
			rightX += premLs.cx;
			memDC.SetTextColor(premColor);
			memDC.TextOut(rightX, centerY - premVs.cy / 2, premVal);
			rightX += premVs.cx;
			memDC.SetTextColor(COLOR_BLACK);
			memDC.TextOut(rightX, centerY - iopvLs.cy / 2, iopvLabel);
			rightX += iopvLs.cx;
			memDC.SetTextColor(iopvColor);
			memDC.TextOut(rightX, centerY - iopvVs.cy / 2, iopvVal);
		}
		else
		{
			CString avgLabel = _T("均:");
			CString avgVal = CCommon::FormatFloat(dispAvgPrice);
			COLORREF avgColor = cmpPrevClose(dispAvgPrice);
			CSize avgLs = memDC.GetTextExtent(avgLabel);
			CSize avgVs = memDC.GetTextExtent(avgVal);
			int rightX = ctx.chartWidth - g_data.RDPI(4) - avgLs.cx - avgVs.cx;
			memDC.SetTextColor(COLOR_BLACK);
			memDC.TextOut(rightX, centerY - avgLs.cy / 2, avgLabel);
			rightX += avgLs.cx;
			memDC.SetTextColor(avgColor);
			memDC.TextOut(rightX, centerY - avgVs.cy / 2, avgVal);
		}

		// 分时模式：使用分时数据计算实时指标信号，设置按钮信号颜色
		// 将分时TimelinePoint转为1分钟Bar序列（open=high=low=close=price），各模式用各自数据
		{
			// 使用完整分时数据（ctx.fullTimeline），而非可见区域子集
			const auto& fullTl = ctx.fullTimeline ? *ctx.fullTimeline : timelinePoint;
			if (fullTl.size() >= 26)
			{
				std::vector<STOCK::Bar> bars1m = CSignalAnalyzer::ConvertTimelineToBars(fullTl);

				int signalEndIndex = -1;
				if (isHovering)
				{
					// ctx.startIndex + hover.hoveredBarIndex 是分时数据的全局索引，与bars1m索引一致
					int hoverIdx = ctx.startIndex + hover.hoveredBarIndex;
					if (hoverIdx >= 25 && hoverIdx < static_cast<int>(bars1m.size()))
						signalEndIndex = hoverIdx;
				}

				auto rtSig = CSignalAnalyzer::CalcRealtimeSignals(bars1m, signalEndIndex);

				static const COLORREF BUY_COLORS[] = {
					RGB(40, 240, 40),
					RGB(50, 180, 50),
					RGB(20, 130, 40)
				};
				static const COLORREF SELL_COLORS[] = {
					RGB(240, 40, 40),
					RGB(180, 50, 50),
					RGB(130, 20, 40)
				};

				// 超卖(买入信号=-1)→红色(要涨)，超买(卖出信号=1)→绿色(要跌)
				if (rtSig.boll != 0) hover.bollSignalColor = rtSig.boll == -1 ? SELL_COLORS[rtSig.bollStr - 1] : BUY_COLORS[rtSig.bollStr - 1];
				if (rtSig.macd != 0) hover.macdSignalColor = rtSig.macd == -1 ? SELL_COLORS[rtSig.macdStr - 1] : BUY_COLORS[rtSig.macdStr - 1];
				if (rtSig.kdj != 0) hover.kdjSignalColor = rtSig.kdj == -1 ? SELL_COLORS[rtSig.kdjStr - 1] : BUY_COLORS[rtSig.kdjStr - 1];
				if (rtSig.rsi != 0) hover.rsiSignalColor = rtSig.rsi == -1 ? SELL_COLORS[rtSig.rsiStr - 1] : BUY_COLORS[rtSig.rsiStr - 1];
				if (rtSig.wr != 0) hover.wrSignalColor = rtSig.wr == -1 ? SELL_COLORS[rtSig.wrStr - 1] : BUY_COLORS[rtSig.wrStr - 1];
				if (rtSig.ma != 0) hover.maSignalColor = rtSig.ma == -1 ? SELL_COLORS[rtSig.maStr - 1] : BUY_COLORS[rtSig.maStr - 1];
			}
		}
	}

	if (hover.viewMode >= UI_VIEW_MIN5_KLINE && !timelinePoint.empty())
	{
		bool isHovering = (hover.hoveredBarIndex >= 0 && hover.isHoveringVolume);
		int displayIdx = isHovering ? hover.hoveredBarIndex : static_cast<int>(timelinePoint.size()) - 1;
		displayIdx = max(0, min(displayIdx, static_cast<int>(timelinePoint.size()) - 1));

		const int rightPadding = g_data.RDPI(4);
		const int itemGap = g_data.RDPI(6);
		int centerY = areaTop + titleH / 2;

		auto formatPrice = [](STOCK::Price value) -> CString {
			return CCommon::FormatFloat(value);
			};
		auto drawRightLabelValues = [&](const std::vector<std::pair<CString, COLORREF>>& items) {
			if (items.empty())
				return;

			int totalWidth = 0;
			for (const auto& item : items)
				totalWidth += memDC.GetTextExtent(item.first).cx + itemGap;
			totalWidth -= itemGap;

			int xPos = ctx.chartWidth - rightPadding - totalWidth;
			xPos = max(g_data.RDPI(4), xPos);
			for (const auto& item : items)
			{
				memDC.SetTextColor(item.second);
				CSize sz = memDC.GetTextExtent(item.first);
				memDC.TextOut(xPos, centerY - sz.cy / 2, item.first);
				xPos += sz.cx + itemGap;
			}
			};

		if (hover.viewMode == UI_VIEW_MIN5_KLINE)
		{
			auto stockData = g_data.GetStockData(hover.stockId);
			if (stockData)
			{
				auto min5KLineObj = stockData->getMin5KLineData();
				if (min5KLineObj && min5KLineObj->data.size() >= 26)
				{
					std::vector<STOCK::Bar> bars5;
					bars5.reserve(min5KLineObj->data.size());
					for (const auto& kp : min5KLineObj->data) bars5.push_back(STOCK::Bar::FromKLinePoint(kp));

					int signalEndIndex = -1;
					if (isHovering)
					{
						int hoverKlineIdx = ctx.startIndex + hover.hoveredBarIndex;
						if (hoverKlineIdx >= 25 && hoverKlineIdx < static_cast<int>(bars5.size()))
							signalEndIndex = hoverKlineIdx;
					}

					auto rtSig = CSignalAnalyzer::CalcRealtimeSignals(bars5, signalEndIndex);

					static const COLORREF BUY_COLORS[] = {
						RGB(40, 240, 40),
						RGB(50, 180, 50),
						RGB(20, 130, 40)
					};
					static const COLORREF SELL_COLORS[] = {
						RGB(240, 40, 40),
						RGB(180, 50, 50),
						RGB(130, 20, 40)
					};

					// 超卖(买入信号=-1)→红色(要涨)，超买(卖出信号=1)→绿色(要跌)
					if (rtSig.boll != 0) hover.bollSignalColor = rtSig.boll == -1 ? SELL_COLORS[rtSig.bollStr - 1] : BUY_COLORS[rtSig.bollStr - 1];
					if (rtSig.macd != 0) hover.macdSignalColor = rtSig.macd == -1 ? SELL_COLORS[rtSig.macdStr - 1] : BUY_COLORS[rtSig.macdStr - 1];
					if (rtSig.kdj != 0) hover.kdjSignalColor = rtSig.kdj == -1 ? SELL_COLORS[rtSig.kdjStr - 1] : BUY_COLORS[rtSig.kdjStr - 1];
					if (rtSig.rsi != 0) hover.rsiSignalColor = rtSig.rsi == -1 ? SELL_COLORS[rtSig.rsiStr - 1] : BUY_COLORS[rtSig.rsiStr - 1];
					if (rtSig.wr != 0) hover.wrSignalColor = rtSig.wr == -1 ? SELL_COLORS[rtSig.wrStr - 1] : BUY_COLORS[rtSig.wrStr - 1];
					if (rtSig.ma != 0) hover.maSignalColor = rtSig.ma == -1 ? SELL_COLORS[rtSig.maStr - 1] : BUY_COLORS[rtSig.maStr - 1];
				}
			}
		}

		else
		{
			// 30分钟K线/日K线模式：计算信号颜色
			auto stockData = g_data.GetStockData(hover.stockId);
			if (stockData)
			{
				std::vector<STOCK::Bar> bars;
				if (hover.viewMode == UI_VIEW_MIN30_KLINE)
				{
					auto min30KLineObj = stockData->getMin30KLineData();
					if (min30KLineObj && min30KLineObj->data.size() >= 26)
					{
						bars.reserve(min30KLineObj->data.size());
						for (const auto& kp : min30KLineObj->data) bars.push_back(STOCK::Bar::FromKLinePoint(kp));
					}
				}
				else if (hover.viewMode == UI_VIEW_DAY_KLINE)
				{
					auto klineObj = stockData->getKLineData();
					if (klineObj && klineObj->data.size() >= 26)
					{
						bars.reserve(klineObj->data.size());
						for (const auto& kp : klineObj->data) bars.push_back(STOCK::Bar::FromKLinePoint(kp));
					}
				}

				if (bars.size() >= 26)
				{
					int signalEndIndex = -1;
					if (isHovering)
					{
						int hoverKlineIdx = ctx.startIndex + hover.hoveredBarIndex;
						if (hoverKlineIdx >= 25 && hoverKlineIdx < static_cast<int>(bars.size()))
							signalEndIndex = hoverKlineIdx;
					}

					auto rtSig = CSignalAnalyzer::CalcRealtimeSignals(bars, signalEndIndex);

					static const COLORREF BUY_COLORS[] = { RGB(40, 240, 40), RGB(50, 180, 50), RGB(20, 130, 40) };
					static const COLORREF SELL_COLORS[] = { RGB(240, 40, 40), RGB(180, 50, 50), RGB(130, 20, 40) };

					if (rtSig.boll != 0) hover.bollSignalColor = rtSig.boll == -1 ? SELL_COLORS[rtSig.bollStr - 1] : BUY_COLORS[rtSig.bollStr - 1];
					if (rtSig.macd != 0) hover.macdSignalColor = rtSig.macd == -1 ? SELL_COLORS[rtSig.macdStr - 1] : BUY_COLORS[rtSig.macdStr - 1];
					if (rtSig.kdj != 0) hover.kdjSignalColor = rtSig.kdj == -1 ? SELL_COLORS[rtSig.kdjStr - 1] : BUY_COLORS[rtSig.kdjStr - 1];
					if (rtSig.rsi != 0) hover.rsiSignalColor = rtSig.rsi == -1 ? SELL_COLORS[rtSig.rsiStr - 1] : BUY_COLORS[rtSig.rsiStr - 1];
					if (rtSig.wr != 0) hover.wrSignalColor = rtSig.wr == -1 ? SELL_COLORS[rtSig.wrStr - 1] : BUY_COLORS[rtSig.wrStr - 1];
					if (rtSig.ma != 0) hover.maSignalColor = rtSig.ma == -1 ? SELL_COLORS[rtSig.maStr - 1] : BUY_COLORS[rtSig.maStr - 1];
				}
			}

			if (hover.showMA)
			{
				STOCK::Price dispMa5 = isHovering ? hover.hoverMa5 : ctx.ma5;
				STOCK::Price dispMa10 = isHovering ? hover.hoverMa10 : ctx.ma10;
				STOCK::Price dispMa20 = isHovering ? hover.hoverMa20 : ctx.ma20;
				std::vector<std::pair<CString, COLORREF>> items;
				// 30分钟/日K线MA均线配色
				const COLORREF ma5TextColor = RGB(240, 117, 40);
				const COLORREF ma10TextColor = RGB(21, 101, 192);
				const COLORREF ma20TextColor = RGB(128, 40, 149);
				if (dispMa5 > 0) items.push_back({ _T("MA5:") + formatPrice(dispMa5), ma5TextColor });
				if (dispMa10 > 0) items.push_back({ _T("MA10:") + formatPrice(dispMa10), ma10TextColor });
				if (dispMa20 > 0) items.push_back({ _T("MA20:") + formatPrice(dispMa20), ma20TextColor });
				drawRightLabelValues(items);
			}
		}
	}

	memDC.SetBkMode(oldBkMode);

	TimelineDrawContext tmpCtx = ctx;
	tmpCtx.priceChartTop = areaTop + titleH;
	tmpCtx.priceChartHeight = areaHeight - titleH;

	if (hover.viewMode == UI_VIEW_DAY_KLINE)
	{
		if (hover.showTrendView)
			DrawTimelinePriceCurve(memDC, tmpCtx, hover);
		else
			DrawDayKLinePriceChart(memDC, tmpCtx, hover);
	}
	else if (hover.viewMode == UI_VIEW_MIN5_KLINE)
		DrawMin5KLinePriceChart(memDC, tmpCtx, hover);
	else if (hover.viewMode == UI_VIEW_MIN30_KLINE)
		DrawMin5KLinePriceChart(memDC, tmpCtx, hover);
	else
		DrawTimelinePriceCurve(memDC, tmpCtx, hover);
}