#include "pch.h"
#include "IndicatorChart.h"
#include "TimelineChart.h"
#include "ChartColors.h"
#include "Common.h"
#include "DataManager.h"
#include "TechnicalIndicatorModel.h"
#include <algorithm>
#include <functional>

// ========== 隔天交替上色公共逻辑（时间标签+各区域竖向虚线共用） ==========

// DayColorCtx 定义在 IndicatorChart.h

// 由可视区间首点日期起统计交易日序号（不依赖日历天数，假期/长周末不破坏交替）
DayColorCtx CalcDayColorCtx(const std::vector<STOCK::TimelinePoint>& timelinePoint)
{
	DayColorCtx ctx;
	int n = static_cast<int>(timelinePoint.size());
	if (n == 0)
		return ctx;
	if (timelinePoint[0].fullTime.length() >= 11 && timelinePoint[0].fullTime[10] == ' ')
		ctx.klineMode = true;
	if (!ctx.klineMode)
		return ctx;

	ctx.dayIdx.assign(n, 0);
	std::string curDay = timelinePoint[0].fullTime.substr(0, 10);
	int runNo = 0;
	for (int i = 1; i < n; i++)
	{
		if (timelinePoint[i].fullTime.length() < 10)
			continue;
		std::string d = timelinePoint[i].fullTime.substr(0, 10);
		if (d != curDay)
		{
			curDay = d;
			runNo++;
		}
		ctx.dayIdx[i] = runNo;
	}
	// 最新交易日序号=runNo；要求它与相邻前一个交易日异色，即奇偶自然成立；
	// 再把"最新那天不标色"翻转一次
	bool lastIsOdd = (runNo % 2 != 0);
	ctx.flipColor = lastIsOdd;   // dayIdx与runNo同奇偶的颜色 == 标色，取反保证最新日为灰
	return ctx;
}

// 第idx个点所属交易日序号是否标色（首个交易日=基准0）
bool IsBlueDayAt(const std::vector<STOCK::TimelinePoint>& timelinePoint, int idx, const DayColorCtx& ctx)
{
	if (!ctx.klineMode || idx < 0 || idx >= static_cast<int>(ctx.dayIdx.size()))
		return false;
	// 可视区间首日为灰（序号0），其后每个真实交易日交替标色；
	// flip保证最新交易日不标色
	bool isOdd = (ctx.dayIdx[idx] % 2 != 0);
	return ctx.flipColor ? !isOdd : isOdd;
}

// ========== 竖向虚线公共绘制（各图表区域共用） ==========

void DrawDayColoredVLines(CDC& memDC, const std::vector<STOCK::TimelinePoint>& timelinePoint,
	int chartWidth, int xAxisPoints, int top, int height, CPen& grayPen)
{
	const int totalPts = static_cast<int>(timelinePoint.size());
	if (totalPts <= 0 || height <= 0)
		return;

	const int numVLines = 6;
	const int slots = (xAxisPoints > 0) ? xAxisPoints : totalPts;
	const DayColorCtx dayCtx = CalcDayColorCtx(timelinePoint);
	CPen blackGridPen(PS_DOT, 1, COLOR_BLACK);
	for (int i = 0; i <= numVLines; i++)
	{
		int idx = totalPts * i / numVLines;
		if (idx >= totalPts) idx = totalPts - 1;
		// 对齐柱中心：槽宽 = chartWidth/slots，中心 = (idx+0.5)*槽宽
		int xPos = static_cast<int>(chartWidth / static_cast<float>(slots) * (static_cast<float>(idx) + 0.5f));
		if (xPos < 0) xPos = 0;
		memDC.SelectObject(IsBlueDayAt(timelinePoint, idx, dayCtx) ? &blackGridPen : &grayPen);
		memDC.MoveTo(xPos, top);
		memDC.LineTo(xPos, top + height);
	}
	// 还原为调用方画笔，避免局部黑笔析构时仍被选入DC
	memDC.SelectObject(&grayPen);
}

// ========== 通用绘图小工具（本文件内各指标区域共用） ==========

namespace
{
	// 指标值 → Y坐标（KDJ/WR/RSI等振荡指标共用）
	using ValueToYFn = std::function<int(double)>;

	// 振荡指标第i个可视槽位的中心X（曲线与金叉死叉圆点对齐）
	int OscSlotCenterX(int x, int width, int xSlots, int i)
	{
		const float slot = width / static_cast<float>(xSlots);
		return x + static_cast<int>(slot * i) + static_cast<int>(slot / 2);
	}

	// 绘制量程参考线（KDJ的0/100边界与20/50/80参考线、WR/RSI参考线共用）
	// lo/hi：只画落在量程内的参考线（J线叠加需要，其余默认全画）
	void DrawRefLines(CDC& memDC, int x, int width, const double* values, int count,
		const ValueToYFn& valueToY, int penStyle, COLORREF color,
		double lo = -1e308, double hi = 1e308)
	{
		CPen pen(penStyle, 1, color);
		CPen* pOldPen = memDC.SelectObject(&pen);
		for (int i = 0; i < count; i++)
		{
			if (values[i] < lo || values[i] > hi)
				continue;
			int gy = valueToY(values[i]);
			memDC.MoveTo(x, gy);
			memDC.LineTo(x + width, gy);
		}
		memDC.SelectObject(pOldPen);
	}

	// 绘制一条振荡指标折线：无效点断笔（下个有效点重新MoveTo）
	// getter: 数据点 → 取哪条曲线（K/D/J、WR1/WR2、RSI1/RSI2）
	template <typename DataT, typename GetterT>
	void DrawOscillatorSeries(CDC& memDC, int x, int width, int xSlots, int totalPts,
		int startIndex, const std::vector<DataT>& data, GetterT getter,
		const ValueToYFn& valueToY, COLORREF color)
	{
		CPen pen(PS_SOLID, 1, color);
		CPen* pOldPen = memDC.SelectObject(&pen);
		bool first = true;
		for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(data.size()); i++)
		{
			int idx = startIndex + i;
			if (!data[idx].valid)
				continue;
			int pointX = OscSlotCenterX(x, width, xSlots, i);
			int pointY = valueToY(getter(data[idx]));
			if (first) { memDC.MoveTo(pointX, pointY); first = false; }
			else memDC.LineTo(pointX, pointY);
		}
		memDC.SelectObject(pOldPen);
	}

	// 金叉死叉标记几何：圆点位置 + 箭头画法
	// MACD的箭头贴0轴上/下边缘、KDJ的箭头贴圆点边缘，故箭头画法由调用方注入
	struct CrossMarkGeom
	{
		std::function<int(int)> markX;                                   // 可视索引 → 圆点X
		std::function<int(int)> markY;                                   // 可视索引 → 圆点Y
		std::function<void(int, int, int, bool)> drawArrow;              // (可视索引, markX, markY, 金叉?)
	};

	// 金叉死叉标记：方向反转时画箭头，同方向重复只画圆点（MACD/KDJ共用）
	// 圆点大小与颜色、"重复信号是否画箭头"的规则在两处完全一致，故整体下沉
	void DrawCrossSignalMarks(CDC& memDC,
		const std::vector<CStockIndicator::MACDCrossSignal>& crossSignals,
		int startIndex, int totalPts, const CrossMarkGeom& geom)
	{
		const int dotRadius = g_data.RDPI(3);
		const int smallDotRadius = g_data.RDPI(2);
		int oldBkMode = memDC.SetBkMode(TRANSPARENT);
		int lastDrawnDir = 0; // 0=无, 1=金叉, -1=死叉

		for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(crossSignals.size()); i++)
		{
			int idx = startIndex + i;
			if (crossSignals[idx] == CStockIndicator::MACDCrossSignal::None)
				continue;

			int markX = geom.markX(i);
			int markY = geom.markY(i);

			bool isGolden = (crossSignals[idx] == CStockIndicator::MACDCrossSignal::GoldenCross
				|| crossSignals[idx] == CStockIndicator::MACDCrossSignal::RepeatedGoldenCross);
			bool isRepeated = (crossSignals[idx] == CStockIndicator::MACDCrossSignal::RepeatedGoldenCross
				|| crossSignals[idx] == CStockIndicator::MACDCrossSignal::RepeatedDeathCross);
			int curDir = isGolden ? 1 : -1;

			// 方向反转时始终绘制箭头；同方向重复则只画圆点
			bool drawArrow = !isRepeated || (curDir != lastDrawnDir);
			if (drawArrow)
			{
				geom.drawArrow(i, markX, markY, isGolden);
				lastDrawnDir = curDir;
			}

			// 绘制圆点
			int r = (isRepeated && !drawArrow) ? smallDotRadius : dotRadius;
			COLORREF dotColor = isGolden ? COLOR_RED_UP : COLOR_GREEN_DOWN;
			CBrush dotBrush(dotColor);
			CPen dotPen(PS_SOLID, 1, dotColor);
			CBrush* pOldBrush = memDC.SelectObject(&dotBrush);
			CPen* pOldDotPen = memDC.SelectObject(&dotPen);
			memDC.Ellipse(markX - r, markY - r, markX + r, markY + r);

			memDC.SelectObject(pOldBrush);
			memDC.SelectObject(pOldDotPen);
		}
		memDC.SetBkMode(oldBkMode);
	}

	// 趋势箭头回看根数：1分钟6根，5分钟8根，30分钟和日K10根
	int GetTrendLookback(UIViewMode viewMode)
	{
		if (viewMode == UI_VIEW_MIN5_KLINE)
			return 8;
		if (viewMode == UI_VIEW_TIMELINE)
			return 6;
		return 10;
	}

	// 趋势计算的结束索引：悬停位置优先（越界钳到末尾），否则最后一个有效点
	int ResolveTrendEndIdx(const CString& titleTip, int hoveredBarIndex, int startIndex,
		int dataCount, const std::function<bool(int)>& isValid)
	{
		if (!titleTip.IsEmpty() && hoveredBarIndex >= 0)
		{
			int endIdx = startIndex + hoveredBarIndex;
			if (endIdx >= dataCount)
				endIdx = dataCount - 1;
			return endIdx;
		}
		for (int i = dataCount - 1; i >= 0; i--)
		{
			if (isValid(i))
				return i;
		}
		return -1;
	}

	// 绘制趋势箭头（↑/↓按斜率方向，个数由调用方按阈值算出）
	void DrawTrendArrows(CDC& memDC, int xPos, int centerY, bool isUp, int arrowCount, COLORREF color)
	{
		if (arrowCount <= 0)
			return;
		CString arrows;
		for (int a = 0; a < arrowCount; a++)
			arrows += isUp ? _T("↑") : _T("↓");
		memDC.SetTextColor(color);
		CSize arrowSize = memDC.GetTextExtent(arrows);
		memDC.TextOut(xPos, centerY - arrowSize.cy / 2, arrows);
	}

	// 解析悬停tip "DIF:xxx DEA:xxx" → (dif, dea)；解析失败返回false
	bool ParseMacdTip(const CString& tip, double& difVal, double& deaVal)
	{
		int difPos = tip.Find(_T("DIF:"));
		int deaPos = tip.Find(_T("DEA:"));
		if (difPos < 0 || deaPos < 0)
			return false;
		CString difStr = tip.Mid(difPos + 4, deaPos - difPos - 4);
		difStr.TrimRight();
		CString deaStr = tip.Mid(deaPos + 4);
		deaStr.TrimRight();
		difVal = _ttof(difStr);
		deaVal = _ttof(deaStr);
		return true;
	}

	// 解析悬停tip "K:xx.x D:xx.x J:xx.x" → (k, d, j)；解析失败返回false
	bool ParseKDJTip(const CString& tip, double& kVal, double& dVal, double& jVal)
	{
		int kPos = tip.Find(_T("K:"));
		int dPos = tip.Find(_T("D:"));
		int jPos = tip.Find(_T("J:"));
		if (kPos < 0 || dPos < 0 || jPos < 0)
			return false;
		CString kStr = tip.Mid(kPos + 2, dPos - kPos - 2); kStr.TrimRight();
		CString dStr = tip.Mid(dPos + 2, jPos - dPos - 2); dStr.TrimRight();
		CString jStr = tip.Mid(jPos + 2); jStr.TrimRight();
		kVal = _ttof(kStr);
		dVal = _ttof(dStr);
		jVal = _ttof(jStr);
		return true;
	}
}

// ========== DrawMACDChart（分时数据版本） ==========

namespace
{
	using MACDData = CIndicatorChart::MACDData;
	using MACDCrossSignal = CIndicatorChart::MACDCrossSignal;

	// MACD绘图几何：双轴缩放 + 槽位尺寸（背景/柱/线/标记共用，算一次到处传）
	struct MacdGeom
	{
		float unitY{ 0 };      // DIF/DEA 像素/值（共用轴）
		float barUnitY{ 0 };   // MACD柱 像素/值（独立轴）
		int zeroY{ 0 };        // 0轴Y坐标
		int xSlots{ 0 };       // X轴槽位数
		int halfSlot{ 0 };     // 槽位半宽
		int barWidth{ 0 };     // 柱宽
	};

	// DIF/DEA折线采样点（双线绘制与极值标记共用）
	struct MacdPoint { int x; int yDif; int yDea; double dif; double dea; };

	// 第i个可视槽位的中心X（柱/线/交叉标记/竖线共用同一套对齐规则）
	int SlotCenterX(const MacdGeom& geom, int x, int width, int i)
	{
		return x + static_cast<int>(width / static_cast<float>(geom.xSlots) * i) + geom.halfSlot;
	}

	// ===== Y轴自适应缩放（仅在绘图层做，指标数据保持原值不动）=====
	// 双轴方案：DIF/DEA 共用一套Y轴，MACD柱单独一套独立Y轴。
	// 柱值(DIF-DEA的2倍差)通常远小于DIF/DEA本身，共用一套轴时柱子会被压扁；
	// 柱单独按自己可视区间的极值缩放后可以撑满子图（行情软件通用做法）
	MacdGeom CalcMacdGeom(const std::vector<MACDData>& macdData, int totalPts, int startIndex,
		int x, int y, int width, int height, int xAxisPoints)
	{
		double lineMaxAbs = 0;   // DIF/DEA 共用轴半径
		double barMaxAbs = 0;    // MACD柱独立轴半径
		for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(macdData.size()); i++)
		{
			int idx = startIndex + i;
			if (macdData[idx].valid)
			{
				lineMaxAbs = (std::max)(lineMaxAbs, std::abs(macdData[idx].dif));
				lineMaxAbs = (std::max)(lineMaxAbs, std::abs(macdData[idx].dea));
				barMaxAbs = (std::max)(barMaxAbs, std::abs(macdData[idx].bar));
			}
		}
		// 10%边距，避免极值线贴图表边框
		lineMaxAbs *= 1.1;
		barMaxAbs *= 1.1;

		MacdGeom geom;
		geom.zeroY = y + height / 2;
		// 自适应Y轴：单位 = 可用半高 / 可视区间最大绝对值（不再固定量程）
		// 极值为0时（区间内全为0）回退固定量程1，避免除0
		geom.unitY = (height / 2.0f - g_data.RDPI(2)) / static_cast<float>(lineMaxAbs > 0 ? lineMaxAbs : 1.0);
		geom.barUnitY = (height / 2.0f - g_data.RDPI(2)) / static_cast<float>(barMaxAbs > 0 ? barMaxAbs : 1.0);
		geom.xSlots = (xAxisPoints > 0) ? xAxisPoints : totalPts;
		const int fixedGap = 1;
		int slotWidth = geom.xSlots > 0 ? width / geom.xSlots : 1;
		geom.barWidth = max(2, static_cast<int>((slotWidth - fixedGap)) * 0.6);
		geom.halfSlot = slotWidth / 2;
		return geom;
	}

	// ===== DIF上行区间浅红背景（读模型缓存区间序列，渲染层不做计算）=====
	// MVC：区间提取在 TechnicalIndicatorModel::UpdateAll 完成并缓存，
	// 此处只把索引对换算成x坐标绘制背景（先画背景，柱/线后画不被覆盖）
	// DIF在0轴上方时背景占满全高；DIF在0轴下方时只画零轴以下半段，
	// 背景高度随DIF水下/水上分段变化
	void DrawMacdDIFUpBackground(CDC& memDC, const std::vector<MACDData>& macdData,
		const TimelineDrawContext* modelData, const MacdGeom& geom,
		int x, int y, int width, int height, int startIndex, int totalPts)
	{
		static const std::vector<std::pair<int, int>> emptyIntervals;
		const auto& upIntervals = (modelData && modelData->indicatorModel)
			? modelData->indicatorModel->GetDIFUpIntervals() : emptyIntervals;
		if (upIntervals.empty())
			return;

		CBrush bgBrush(COLOR_LIGHT_RED_BG);
		const int zeroY = geom.zeroY;
		const int halfSlot = geom.halfSlot;
		const float pxPerSlot = width / static_cast<float>(geom.xSlots);
		for (const auto& range : upIntervals)
		{
			// 全局索引 → 可视槽位索引；区间与可视窗口的交集部分绘制
			int from = max(range.first - startIndex, 0);
			int to = min(range.second - startIndex, totalPts - 1);
			if (from > to)
				continue;
			int left = max(x + static_cast<int>(pxPerSlot * from) - halfSlot / 2, x);
			int right = min(x + static_cast<int>(pxPerSlot * (to + 1)) + halfSlot / 2, x + width);
			if (right <= left)
				continue;
			// 按槽位遍历可视交集，DIF同在水上/水下的连续槽位合并成一块矩形：
			// 水上→全高 [y, y+height]；水下→下半区 [zeroY, y+height]
			int segStart = from;
			bool segAboveZero = (startIndex + from < static_cast<int>(macdData.size())
				&& macdData[startIndex + from].valid && macdData[startIndex + from].dif >= 0);
			for (int i = from + 1; i <= to + 1; ++i)
			{
				bool aboveZero = (i <= to && startIndex + i < static_cast<int>(macdData.size())
					&& macdData[startIndex + i].valid && macdData[startIndex + i].dif >= 0);
				// 槽位终点(i==to+1)或状态切换时flush当前段
				if (i > to || aboveZero != segAboveZero)
				{
					int segLeft = (segStart == from) ? left
						: max(x + static_cast<int>(pxPerSlot * segStart) - halfSlot / 2, x);
					int segRight = (i == to + 1) ? right
						: min(x + static_cast<int>(pxPerSlot * i) + halfSlot / 2, x + width);
					if (segRight > segLeft)
					{
						if (segAboveZero)
							memDC.FillRect(CRect(segLeft, y, segRight, y + height), &bgBrush);
						else
							memDC.FillRect(CRect(segLeft, zeroY, segRight, y + height), &bgBrush);
					}
					segStart = i;
					if (i <= to) segAboveZero = aboveZero;
				}
			}
		}
	}

	// 水上/水下最长柱索引（可视区间内正/负极值柱，分别标紫色/黑色）
	void FindMacdExtremeBars(const std::vector<MACDData>& macdData, int totalPts, int startIndex,
		int& maxPosIdx, int& maxNegIdx)
	{
		maxPosIdx = -1;
		maxNegIdx = -1;
		for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(macdData.size()); i++)
		{
			int idx = startIndex + i;
			if (!macdData[idx].valid)
				continue;
			double barVal = macdData[idx].bar;
			if (barVal > 0 && (maxPosIdx < 0 || barVal > macdData[maxPosIdx].bar))
				maxPosIdx = idx;
			if (barVal < 0 && (maxNegIdx < 0 || barVal < macdData[maxNegIdx].bar))
				maxNegIdx = idx;
		}
	}

	// 绘制 MACD 柱状图（独立Y轴缩放）
	void DrawMacdBars(CDC& memDC, const std::vector<MACDData>& macdData, const MacdGeom& geom,
		int x, int y, int width, int height, int startIndex, int totalPts)
	{
		int maxPosIdx = -1, maxNegIdx = -1;
		FindMacdExtremeBars(macdData, totalPts, startIndex, maxPosIdx, maxNegIdx);

		for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(macdData.size()); i++)
		{
			int idx = startIndex + i;
			if (!macdData[idx].valid)
				continue;

			int barX = SlotCenterX(geom, x, width, i) - geom.barWidth / 2;
			double barVal = macdData[idx].bar;
			int barHeight = static_cast<int>(std::abs(barVal) * geom.barUnitY);
			// 最小高度保护：数值极小时柱子至少2px，避免肉眼不可见
			barHeight = max(2, barHeight);

			COLORREF color;
			if (maxPosIdx == idx)			// 水上最长柱：紫色
				color = RGB(128, 0, 128);
			else if (maxNegIdx == idx)		// 水下最长柱：黑色
				color = RGB(0, 0, 0);
			else
				color = (barVal >= 0) ? COLOR_RED_UP : COLOR_GREEN_DOWN;
			CBrush brush(color);
			int barY = (barVal >= 0) ? geom.zeroY - barHeight : geom.zeroY;
			memDC.FillRect(CRect(barX, barY, barX + geom.barWidth,
				geom.zeroY + (barVal >= 0 ? 0 : barHeight)), &brush);
		}
	}

	// 收集DIF/DEA有效采样点（双线与极值标记共用同一份采样）
	std::vector<MacdPoint> CollectMacdPoints(const std::vector<MACDData>& macdData, const MacdGeom& geom,
		int x, int width, int startIndex, int totalPts)
	{
		std::vector<MacdPoint> pts;
		pts.reserve(totalPts);
		for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(macdData.size()); i++)
		{
			int idx = startIndex + i;
			if (!macdData[idx].valid)
				continue;
			pts.push_back({ SlotCenterX(geom, x, width, i),
				geom.zeroY - static_cast<int>(macdData[idx].dif * geom.unitY),
				geom.zeroY - static_cast<int>(macdData[idx].dea * geom.unitY),
				macdData[idx].dif, macdData[idx].dea });
		}
		return pts;
	}

	// 绘制 DIF/DEA 双线（分段动态画笔：多头DIF粗橙实线+DEA细黑实线，空头DEA粗蓝实线+DIF细黑实线）
	void DrawMacdDIFDEALines(CDC& memDC, const std::vector<MacdPoint>& pts)
	{
		if (pts.size() >= 2)
		{
			// 切分连续同状态区间（交叉点搭接1根bar，消除间隙）
			enum EMacdState { STATE_LONG, STATE_SHORT };
			struct Seg { int from; int to; EMacdState state; };
			std::vector<Seg> segList;
			int segStart = 0;
			EMacdState curState = (pts[0].dif >= pts[0].dea) ? STATE_LONG : STATE_SHORT;
			for (size_t i = 1; i < pts.size(); i++)
			{
				EMacdState st = (pts[i].dif >= pts[i].dea) ? STATE_LONG : STATE_SHORT;
				if (st != curState)
				{
					segList.push_back({ segStart, static_cast<int>(i), curState });
					segStart = static_cast<int>(i);
					curState = st;
				}
			}
			segList.push_back({ segStart, static_cast<int>(pts.size()) - 1, curState });

			// 分段绘制
			for (const auto& seg : segList)
			{
				std::vector<CPoint> ptsDif, ptsDea;
				ptsDif.reserve(seg.to - seg.from + 1);
				ptsDea.reserve(seg.to - seg.from + 1);
				for (int i = seg.from; i <= seg.to; i++)
				{
					ptsDif.push_back(CPoint(pts[i].x, pts[i].yDif));
					ptsDea.push_back(CPoint(pts[i].x, pts[i].yDea));
				}

				if (seg.state == STATE_LONG)
				{
					// 多头：DIF粗橙色实线（主线），DEA细黑色实线（辅助）
					CPen penMain(PS_SOLID, 2, COLOR_DARK_ORANGE);
					CPen penSub(PS_SOLID, 1, COLOR_BLACK);
					memDC.SelectObject(&penMain);
					memDC.Polyline(&ptsDif[0], static_cast<int>(ptsDif.size()));
					memDC.SelectObject(&penSub);
					memDC.Polyline(&ptsDea[0], static_cast<int>(ptsDea.size()));
				}
				else
				{
					// 空头：DEA粗蓝色实线（主线），DIF细黑色实线（辅助）
					CPen penMain(PS_SOLID, 2, COLOR_BLUE_AVG1);
					CPen penSub(PS_SOLID, 1, COLOR_BLACK);
					memDC.SelectObject(&penMain);
					memDC.Polyline(&ptsDea[0], static_cast<int>(ptsDea.size()));
					memDC.SelectObject(&penSub);
					memDC.Polyline(&ptsDif[0], static_cast<int>(ptsDif.size()));
				}
			}
		}
		else if (pts.size() == 1)
		{
			// 单点：画小圆点
			CPen penMain(PS_SOLID, 2, (pts[0].dif >= pts[0].dea) ? COLOR_DARK_ORANGE : COLOR_BLUE_AVG1);
			memDC.SelectObject(&penMain);
			memDC.SetPixel(pts[0].x, pts[0].yDif, (pts[0].dif >= pts[0].dea) ? COLOR_DARK_ORANGE : COLOR_BLACK);
			memDC.SetPixel(pts[0].x, pts[0].yDea, (pts[0].dif >= pts[0].dea) ? COLOR_BLACK : COLOR_BLUE_AVG1);
		}
	}

	// ===== 叠加KDJ的J线（绿色）：点击KDJ按钮后开启（ctx.overlayKDJInMacd）=====
	// J值范围(通常-10~110)与DIF量级无关，独立线性映射到图表全高：
	// J按其可视区间极值自适应缩放，高低极值各留10%边距
	void DrawMacdKDJOverlay(CDC& memDC, const TimelineDrawContext* modelData, const MacdGeom& geom,
		int x, int y, int width, int height, int startIndex, int totalPts)
	{
		if (!modelData || !modelData->overlayKDJInMacd || !modelData->indicatorModel)
			return;
		const auto& kdjData = modelData->indicatorModel->GetKDJ();
		if (kdjData.empty())
			return;

		double jMin = 1e308, jMax = -1e308;
		for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(kdjData.size()); i++)
		{
			const auto& kd = kdjData[startIndex + i];
			if (!kd.valid)
				continue;
			jMin = (std::min)(jMin, kd.j);
			jMax = (std::max)(jMax, kd.j);
		}
		if (jMax <= jMin)
			return;

		const int jPadding = g_data.RDPI(3);
		const float jUnit = (height - jPadding * 2) / static_cast<float>(jMax - jMin);
		auto jToY = [&](double jval) {
			return y + jPadding + static_cast<int>((jMax - jval) * jUnit);
			};

		// J线量程刻度：0/100两条灰色虚线（量程含0~100时才画）
		const double jRefValues[] = { 0.0, 100.0 };
		DrawRefLines(memDC, x, width, jRefValues, 2, jToY, PS_DOT, COLOR_GRAY_MIDDLE, jMin, jMax);

		CPen jPen(PS_SOLID, 1, COLOR_GREEN_DOWN);
		CPen* pOldJPen = memDC.SelectObject(&jPen);
		bool jFirst = true;
		for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(kdjData.size()); i++)
		{
			const auto& kd = kdjData[startIndex + i];
			if (!kd.valid) { jFirst = true; continue; }
			int px = SlotCenterX(geom, x, width, i);
			int py = jToY(kd.j);
			if (jFirst) { memDC.MoveTo(px, py); jFirst = false; }
			else memDC.LineTo(px, py);
		}
		memDC.SelectObject(pOldJPen);
	}

	// DIF 极值标记的实际绘制（不依赖类状态，故下沉为本文件内自由函数，
	// 供 DrawMacdDIFExtremeMarks 与 CIndicatorChart::DrawDifExtremeMark 共用）
	// 最高点红色圆圈H，最低点绿色圆圈L
	void DrawDifMark(CDC& memDC, int markX, int markY, bool isHigh)
	{
		COLORREF color = isHigh ? COLOR_RED_UP : COLOR_GREEN_DOWN;
		const int r = g_data.RDPI(8);   // 圆圈半径（足够包住H/L字母）

		CString label = isHigh ? _T("H") : _T("L");
		int oldBkMode = memDC.SetBkMode(TRANSPARENT);

		// 红绿实心圆圈
		CBrush brush(color);
		CPen pen(PS_SOLID, 1, color);
		CBrush* pOldBrush = memDC.SelectObject(&brush);
		CPen* pOldPen = memDC.SelectObject(&pen);
		memDC.Ellipse(markX - r, markY - r, markX + r, markY + r);
		memDC.SelectObject(pOldBrush);
		memDC.SelectObject(pOldPen);

		// 白色字母居中
		memDC.SetTextColor(RGB(255, 255, 255));
		memDC.TextOut(markX - memDC.GetTextExtent(label).cx / 2,
			markY - memDC.GetTextExtent(label).cy / 2, label);
		memDC.SetBkMode(oldBkMode);
	}

	// DIF 最高/最低点标记：最高点红色圆圈H标记，最低点绿色圆圈L标记
	// 5分钟/30分钟模式优先展示最后一个交易日（可视序列末尾连续同fullTime日期）的极值，
	// 可视序列无法界定日期（分时模式）时退回整个可视区间
	void DrawMacdDIFExtremeMarks(CDC& memDC, const std::vector<MacdPoint>& pts,
		const std::vector<STOCK::TimelinePoint>& timelinePoint, int totalPts)
	{
		if (pts.size() < 2)
			return;

		// 最后交易日界：从末尾向前找连续同fullTime日期（如"2026-09-11"）的点
		// 分时点fullTime=0的用time（无日期）→ 最后全区间
		std::string lastDay;
		for (int i = totalPts - 1; i >= 0; --i)
		{
			const auto& tp = timelinePoint[i];
			const std::string& ft = tp.fullTime.empty() ? tp.time : tp.fullTime;
			if (ft.length() >= 10 && ft[4] == '-' && ft[7] == '-')
			{
				lastDay = ft.substr(0, 10);
				break;
			}
		}
		int dayFirst = totalPts;
		if (!lastDay.empty())
		{
			for (int i = totalPts - 1; i >= 0; --i)
			{
				const std::string& ft = timelinePoint[i].fullTime;
				if (ft.substr(0, 10) == lastDay) dayFirst = i;
				else break;
			}
		}

		int difHiIdx = -1, difLoIdx = -1;
		double difHi = -1e308, difLo = 1e308;
		// 优先扫描最后交易日区间；区间内不足2点时扫描整个可视区间
		int scanFrom = (!lastDay.empty() && dayFirst < totalPts - 1) ? dayFirst : 0;
		for (int i = scanFrom; i < static_cast<int>(pts.size()); i++)
		{
			if (pts[i].dif > difHi) { difHi = pts[i].dif; difHiIdx = i; }
			if (pts[i].dif < difLo) { difLo = pts[i].dif; difLoIdx = i; }
		}
		if (difHiIdx >= 0)
			DrawDifMark(memDC, pts[difHiIdx].x, pts[difHiIdx].yDif, true);
		if (difLoIdx >= 0 && difLoIdx != difHiIdx)
			DrawDifMark(memDC, pts[difLoIdx].x, pts[difLoIdx].yDif, false);
	}
}

void CIndicatorChart::DrawMACDChart(CDC& memDC, int x, int y, int width, int height,
	const std::vector<STOCK::TimelinePoint>& timelinePoint,
	const std::vector<MACDData>& macdData, int startIndex /* = 0 */, int visibleCount /* = -1 */, int xAxisPoints /* = 0 */,
	const TimelineDrawContext* modelData /* = nullptr */)
{
	if (timelinePoint.empty() || macdData.empty())
		return;

	const int totalPts = static_cast<int>(timelinePoint.size());
	const MacdGeom geom = CalcMacdGeom(macdData, totalPts, startIndex, x, y, width, height, xAxisPoints);

	// 零轴画笔：整个区域绘制期间保持选中，函数末尾还原
	CPen zeroPen(PS_SOLID, 1, COLOR_GRAY_MIDDLE);
	CPen* pOldPen = memDC.SelectObject(&zeroPen);

	// 背景在最底层，柱/线后画不会被覆盖
	DrawMacdDIFUpBackground(memDC, macdData, modelData, geom, x, y, width, height, startIndex, totalPts);

	memDC.MoveTo(x, geom.zeroY);
	memDC.LineTo(x + width, geom.zeroY);

	DrawMacdBars(memDC, macdData, geom, x, y, width, height, startIndex, totalPts);

	// DIF/DEA双线、KDJ的J线叠加、DIF极值标记
	const std::vector<MacdPoint> pts = CollectMacdPoints(macdData, geom, x, width, startIndex, totalPts);
	DrawMacdDIFDEALines(memDC, pts);
	DrawMacdKDJOverlay(memDC, modelData, geom, x, y, width, height, startIndex, totalPts);
	DrawMacdDIFExtremeMarks(memDC, pts, timelinePoint, totalPts);

	memDC.SelectObject(pOldPen);

	// 绘制金叉死叉标记：金叉红色↑，死叉绿色↓
	// 金叉死叉信号：直接读模型缓存序列，不再每帧重算
	static const std::vector<MACDCrossSignal> emptyCross;
	const auto& crossSignals = (modelData && modelData->indicatorModel)
		? modelData->indicatorModel->GetMACDCross() : emptyCross;
	const int dotRadius = g_data.RDPI(3);

	CrossMarkGeom markGeom;
	markGeom.markX = [&](int i) { return SlotCenterX(geom, x, width, i); };
	markGeom.markY = [&](int i) { return geom.zeroY - static_cast<int>(macdData[startIndex + i].dif * geom.unitY); };
	// 箭头语义：金叉恒为红↑、死叉恒为绿↓（箭头头朝交叉方向）；
	// 位置按交叉点DIF所在0轴上/下半区：0轴上方贴图表顶边，0轴下方贴底边，
	// 箭头全部画在图表内部、不越过MACD图像边缘
	markGeom.drawArrow = [&](int i, int markX, int /*markY*/, bool isGolden) {
		bool aboveZero = (macdData[startIndex + i].dif >= 0);
		DrawCrossArrow2(memDC, markX, aboveZero ? y : (y + height), dotRadius, isGolden, aboveZero);
	};
	DrawCrossSignalMarks(memDC, crossSignals, startIndex, totalPts, markGeom);
}

// ========== DrawVolumeChartArea ==========

void CIndicatorChart::DrawVolumeChartArea(CDC& memDC, const TimelineDrawContext& ctx, int areaTop, int areaHeight, bool drawTimeLabels, const HoverState& hover)
{
	const int titleH = g_data.RDPI(16);
	int oldBkMode = memDC.SetBkMode(TRANSPARENT);

	// 标题栏背景 + 文字
	memDC.FillSolidRect(CRect(0, areaTop, ctx.chartWidth, areaTop + titleH), RGB(245, 245, 245));
	DrawVolumeTitleBar(memDC, ctx, areaTop, titleH, hover);

	memDC.SetBkMode(oldBkMode);

	const int chartTop = areaTop + titleH;
	const int chartHeight = areaHeight - titleH;
	DrawVolumeChartBody(memDC, ctx, chartTop, chartHeight, hover);

	if (drawTimeLabels && !ctx.timelinePoint->empty())
	{
		// 时间标签：柱中心对齐，5/30分钟隔天交替黑色（公共函数DrawTimeLabelsAt）
		DrawTimeLabelsAt(memDC, *ctx.timelinePoint, ctx.chartWidth,
			chartTop + chartHeight + g_data.RDPI(2), ctx.xAxisPoints);
	}
}

// ========== 成交量标题栏（CJL区域与成交量区域共用） ==========

void CIndicatorChart::DrawVolumeTitleBar(CDC& memDC, const TimelineDrawContext& ctx, int areaTop, int titleH, const HoverState& hover)
{
	const auto& timelinePoint = *ctx.timelinePoint;

	CString perVolStr, perAmtStr;
	bool isVolHovering = !hover.timelineVolumeTitleTip.IsEmpty();
	int displayVolIdx = isVolHovering ? hover.hoveredBarIndex : static_cast<int>(timelinePoint.size()) - 1;
	displayVolIdx = max(0, min(displayVolIdx, static_cast<int>(timelinePoint.size()) - 1));
	if (displayVolIdx >= 0 && displayVolIdx < static_cast<int>(timelinePoint.size()))
	{
		const auto& displayPt = timelinePoint[displayVolIdx];
		perVolStr = CCommon::FormatVolumeInt(displayPt.volume / 100);
		double displayAmount = static_cast<double>(displayPt.volume) * displayPt.price;
		perAmtStr = CCommon::FormatAmount(displayAmount);
	}

	auto calcVolumeMA = [&](int globalIndex, int period) -> double {
		if (!ctx.fullTimeline || globalIndex < period - 1 || globalIndex >= static_cast<int>(ctx.fullTimeline->size()))
			return 0.0;
		double sum = 0.0;
		for (int i = globalIndex - period + 1; i <= globalIndex; i++)
			sum += static_cast<double>((*ctx.fullTimeline)[i].volume);
		return sum / period;
		};

	auto formatVolumeMA = [](double volume) -> CString {
		return CCommon::FormatVolumeInt(volume / 100.0);
		};

	const int globalVolIdx = ctx.startIndex + displayVolIdx;
	double volMa5 = calcVolumeMA(globalVolIdx, 5);
	double prevVolMa5 = calcVolumeMA(globalVolIdx - 1, 5);
	double volMa10 = calcVolumeMA(globalVolIdx, 10);
	double prevVolMa10 = calcVolumeMA(globalVolIdx - 1, 10);

	int xPos = g_data.RDPI(4);
	int centerY = areaTop + titleH / 2;
	const COLORREF hoverBgColor = RGB(200, 220, 255);

	auto drawLabelValueHL = [&](const CString& labelText, const CString& valStr, COLORREF labelColor, COLORREF valueColor, bool highlight) {
		memDC.SetTextColor(labelColor);
		CSize ls = memDC.GetTextExtent(labelText);
		memDC.TextOut(xPos, centerY - ls.cy / 2, labelText);
		xPos += ls.cx;
		memDC.SetTextColor(valueColor);
		CSize vs = memDC.GetTextExtent(valStr);
		if (highlight)
		{
			CRect hlRect(xPos, areaTop + 1, xPos + vs.cx, areaTop + titleH - 1);
			memDC.FillSolidRect(hlRect, hoverBgColor);
		}
		memDC.TextOut(xPos, centerY - vs.cy / 2, valStr);
		xPos += vs.cx + g_data.RDPI(4);
		};

	auto drawVolumeMA = [&](const CString& labelText, double maValue, double prevMaValue, COLORREF maColor) {
		if (maValue <= 0)
			return;
		CString valueText = formatVolumeMA(maValue);
		memDC.SetTextColor(maColor);
		CString text = labelText + valueText;
		CSize textSize = memDC.GetTextExtent(text);
		memDC.TextOut(xPos, centerY - textSize.cy / 2, text);
		xPos += textSize.cx;
		if (prevMaValue > 0 && maValue != prevMaValue)
		{
			CString arrowText = maValue > prevMaValue ? _T("↑") : _T("↓");
			memDC.SetTextColor(maValue > prevMaValue ? COLOR_RED_UP : COLOR_GREEN_DOWN);
			CSize arrowSize = memDC.GetTextExtent(arrowText);
			memDC.TextOut(xPos, centerY - arrowSize.cy / 2, arrowText);
			xPos += arrowSize.cx;
		}
		xPos += g_data.RDPI(4);
		};

	memDC.SetTextColor(COLOR_GRAY_TEXT);
	if (!perVolStr.IsEmpty())
		drawLabelValueHL(_T("分量:"), perVolStr, COLOR_GRAY_TEXT, COLOR_GRAY_TEXT, isVolHovering);
	if (!perAmtStr.IsEmpty())
		drawLabelValueHL(_T("分额:"), perAmtStr, COLOR_GRAY_TEXT, COLOR_GRAY_TEXT, isVolHovering);
	if (hover.viewMode == UI_VIEW_MIN5_KLINE)
	{
		drawVolumeMA(_T("MA5:"), volMa5, prevVolMa5, RGB(0, 0, 230));
		drawVolumeMA(_T("MA10:"), volMa10, prevVolMa10, RGB(0, 166, 235));
	}
}

// ========== 成交量区域正文 ==========

void CIndicatorChart::DrawVolumeChartBody(CDC& memDC, const TimelineDrawContext& ctx, int chartTop, int chartHeight, const HoverState& hover)
{
	// 先绘制背景高亮，确保数据不被覆盖
	//CTimelineChart::DrawTimelineBackgroundHighlightsForArea(memDC, tmpCtx, chartTop, chartHeight);

	DrawVolumeChart(memDC, 0, chartTop, ctx.chartWidth, chartHeight,
		*ctx.timelinePoint, &ctx.realtimeData, 0, -1, ctx.xAxisPoints);
	DrawVolumeMALines(memDC, ctx, chartTop, chartHeight, hover);
	DrawVolumeGrid(memDC, ctx, chartTop, chartHeight);
}

// 5/10分钟成交量均线叠加（仅5分钟K线模式）
void CIndicatorChart::DrawVolumeMALines(CDC& memDC, const TimelineDrawContext& ctx, int chartTop, int chartHeight, const HoverState& hover)
{
	if (hover.viewMode != UI_VIEW_MIN5_KLINE || !ctx.fullTimeline || ctx.fullTimeline->empty()
		|| !ctx.timelinePoint || ctx.timelinePoint->empty())
		return;

	const auto& fullData = *ctx.fullTimeline;
	const int totalPoints = static_cast<int>(ctx.timelinePoint->size());
	const int startIndex = ctx.startIndex;
	const int endIndex = min(static_cast<int>(fullData.size()), startIndex + totalPoints);

	STOCK::Volume maxVolume = 0;
	for (int i = startIndex; i < endIndex; i++)
		maxVolume = max(maxVolume, fullData[i].volume);
	if (maxVolume == 0)
		return;

	// 成交量均线直接读模型缓存序列（fullTimeline等长），不再逐点重算
	static const std::vector<double> emptySeq;
	const auto& ma5 = ctx.indicatorModel ? ctx.indicatorModel->GetVolumeMA5() : emptySeq;
	const auto& ma10 = ctx.indicatorModel ? ctx.indicatorModel->GetVolumeMA10() : emptySeq;

	auto volumeToY = [&](double volume) -> int {
		int py = chartTop + chartHeight - static_cast<int>(volume / static_cast<double>(maxVolume) * chartHeight);
		return max(chartTop, min(py, chartTop + chartHeight));
		};

	auto drawVolumeMALine = [&](const std::vector<double>& values, COLORREF color) {
		CPen pen(PS_SOLID, 1, color);
		CPen* pOldPen = memDC.SelectObject(&pen);
		bool first = true;
		for (int i = 0; i < totalPoints; i++)
		{
			int globalIndex = startIndex + i;
			if (globalIndex >= static_cast<int>(values.size()) || values[globalIndex] <= 0) { first = true; continue; }
			int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i)
				+ static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) / 2);
			int pointY = volumeToY(values[globalIndex]);
			if (first) { memDC.MoveTo(pointX, pointY); first = false; }
			else memDC.LineTo(pointX, pointY);
		}
		memDC.SelectObject(pOldPen);
		};

	drawVolumeMALine(ma5, RGB(0, 0, 230));
	drawVolumeMALine(ma10, RGB(0, 166, 235));
}

// 成交量网格线（含左侧刻度标签与隔天交替竖向虚线）
void CIndicatorChart::DrawVolumeGrid(CDC& memDC, const TimelineDrawContext& ctx, int chartTop, int chartHeight)
{
	const auto& timelinePoint = *ctx.timelinePoint;

	CPen pGrid(PS_DOT, 1, COLOR_GRAY_GRID);
	CPen* pOldVolPen = memDC.SelectObject(&pGrid);
	memDC.MoveTo(0, chartTop);
	memDC.LineTo(ctx.chartWidth, chartTop);

	if (!timelinePoint.empty())
	{
		STOCK::Volume maxVolume = 0;
		for (const auto& item : timelinePoint)
		{
			if (item.volume > maxVolume)
				maxVolume = item.volume;
		}
		if (maxVolume > 0)
		{
			CPen dotPen(PS_DOT, 1, COLOR_GRAY_MIDDLE);
			memDC.SelectObject(&dotPen);
			memDC.SetTextColor(COLOR_GRAY_TEXT);
			for (int i = 1; i <= 2; i++)
			{
				int yPos = chartTop + chartHeight * i / 3;
				memDC.MoveTo(0, yPos);
				memDC.LineTo(ctx.chartWidth, yPos);
				STOCK::Volume volAtLine = maxVolume * (3 - i) / 3;
				CString volLabel = CCommon::FormatVolumeInt(volAtLine / 100);
				CSize labelSize = memDC.GetTextExtent(volLabel);
				memDC.TextOut(-labelSize.cx - g_data.RDPI(3), yPos - labelSize.cy / 2, volLabel);
			}
			memDC.SelectObject(&pGrid);
		}
	}

	if (!timelinePoint.empty())
	{
		// 竖向虚线：柱中心对齐，5/30分钟隔天交替黑色，其余灰色
		DrawDayColoredVLines(memDC, timelinePoint, ctx.chartWidth, ctx.xAxisPoints,
			chartTop, chartHeight, pGrid);
	}
	memDC.SelectObject(pOldVolPen);
}

// ========== DrawMacdChartArea ==========

void CIndicatorChart::DrawMacdChartArea(CDC& memDC, const TimelineDrawContext& ctx, int areaTop, int areaHeight, const CString& macdTitleTip, const HoverState& hover)
{
	const int titleH = g_data.RDPI(16);
	int oldBkMode = memDC.SetBkMode(TRANSPARENT);

	// 标题栏背景 + DIF/DEA数值与趋势箭头
	memDC.FillSolidRect(CRect(0, areaTop, ctx.chartWidth, areaTop + titleH), RGB(245, 245, 245));
	DrawMacdTitleBar(memDC, ctx, areaTop, titleH, hover);

	memDC.SetBkMode(oldBkMode);

	TimelineDrawContext tmpCtx = ctx;
	tmpCtx.macdChartTop = areaTop + titleH;
	tmpCtx.macdChartHeight = areaHeight - titleH;

	// 指标序列由 TechnicalIndicatorModel 在序列变化时计算并缓存，此处只读
	static const std::vector<MACDData> emptyMacd;
	const std::vector<MACDData>& macdData =
		ctx.indicatorModel ? ctx.indicatorModel->GetMACD() : emptyMacd;

	// 先绘制背景高亮，确保数据不被覆盖
	//CTimelineChart::DrawTimelineBackgroundHighlightsForArea(memDC, tmpCtx, tmpCtx.macdChartTop, tmpCtx.macdChartHeight);

	DrawMACDChart(memDC, 0, tmpCtx.macdChartTop, ctx.chartWidth, tmpCtx.macdChartHeight,
		*ctx.timelinePoint, macdData, ctx.startIndex, -1, ctx.xAxisPoints, &ctx);

	// 上下边框线 + 竖向虚线（不含时间标签）
	DrawSectionGridAndTimeLabels(memDC, tmpCtx, tmpCtx.macdChartTop, tmpCtx.macdChartHeight, false);
}

// ========== MACD标题栏 ==========

void CIndicatorChart::DrawMacdTitleBar(CDC& memDC, const TimelineDrawContext& ctx, int areaTop, int titleH, const HoverState& hover)
{
	// 指标序列由 TechnicalIndicatorModel 缓存，此处只读
	static const std::vector<MACDData> emptyMacd;
	const std::vector<MACDData>& macdData = ctx.indicatorModel ? ctx.indicatorModel->GetMACD() : emptyMacd;

	int xPos = g_data.RDPI(4);
	int centerY = areaTop + titleH / 2;
	// 分别绘制标签和数值，标签和数值可以不同颜色
	auto drawLabel = [&](const CString& label, COLORREF labelColor) {
		memDC.SetTextColor(labelColor);
		CSize ts = memDC.GetTextExtent(label);
		memDC.TextOut(xPos, centerY - ts.cy / 2, label);
		xPos += ts.cx;
		};
	auto drawValue = [&](const CString& value, double val) {
		COLORREF valColor = (val >= 0) ? COLOR_RED_UP : COLOR_GREEN_DOWN;
		memDC.SetTextColor(valColor);
		CSize ts = memDC.GetTextExtent(value);
		memDC.TextOut(xPos, centerY - ts.cy / 2, value);
		xPos += ts.cx + g_data.RDPI(4);
		};
	auto formatMACDValue = [](double val) -> CString {
		CString s;
		double absVal = std::abs(val);
		if (absVal < 0.001 && absVal > 0)
			s.Format(_T("%.5f"), val);
		else if (absVal < 0.01)
			s.Format(_T("%.4f"), val);
		else
			s.Format(_T("%.3f"), val);
		return s;
		};

	// 获取当前显示的DIF/DEA值（悬停时从tip解析，非悬停时直接读模型缓存）
	double difVal = 0, deaVal = 0;
	if (!hover.timelineMacdTitleTip.IsEmpty())
		ParseMacdTip(hover.timelineMacdTitleTip, difVal, deaVal);
	else if (ctx.indicatorModel)
		ctx.indicatorModel->GetLastMACD(difVal, deaVal);

	drawLabel(_T("DIF:"), COLOR_DARK_ORANGE);
	drawValue(formatMACDValue(difVal), difVal);
	drawLabel(_T("DEA:"), COLOR_BLUE_AVG1);
	drawValue(formatMACDValue(deaVal), deaVal);

	// DIF趋势箭头：随悬停位置实时截断计算（lookback≤10，开销可忽略，保留每帧计算
	// 以保证悬停时箭头跟随鼠标位置变化；非悬停时基于最新数据）
	const int trendEndIdx = ResolveTrendEndIdx(hover.timelineMacdTitleTip, hover.hoveredBarIndex,
		ctx.startIndex, static_cast<int>(macdData.size()),
		[&macdData](int i) { return macdData[i].valid; });
	if (trendEndIdx < 0)
		return;

	// 截取到悬停位置为止的子序列计算趋势
	std::vector<MACDData> subMacd(macdData.begin(), macdData.begin() + trendEndIdx + 1);
	auto trend = CStockIndicator::CalcDIFTrend(subMacd, GetTrendLookback(hover.viewMode));
	if (!trend.valid || trend.slope == 0.0)
		return;

	// 斜率相对DIF绝对值的占比决定箭头个数
	double refDif = std::abs(macdData[trendEndIdx].dif);
	double base = (refDif > 0.001) ? refDif : 0.001;
	double absSlope = std::abs(trend.slope);
	int arrowCount = 1;
	if (absSlope >= base * 0.3) arrowCount = 3;
	else if (absSlope >= base * 0.15) arrowCount = 2;

	COLORREF arrowColor = (macdData[trendEndIdx].dea >= 0) ? COLOR_RED_UP : COLOR_GREEN_DOWN;
	DrawTrendArrows(memDC, xPos, centerY, trend.slope > 0, arrowCount, arrowColor);
}

// ========== 指标标题栏 ==========

namespace
{
	// W&R / RSI 标题栏共用实现：悬停时按空格切分tip逐段上色，否则画默认标签
	void DrawTwoPeriodTitleBar(CDC& memDC, int areaTop, int titleH,
		const CString& titleTip, const CString& defaultLabel1, const CString& defaultLabel2)
	{
		int xPos = g_data.RDPI(4);
		int centerY = areaTop + titleH / 2;
		auto drawLabel = [&](const CString& label, const CString& value, COLORREF color) {
			memDC.SetTextColor(color);
			CString text = label + value;
			CSize ts = memDC.GetTextExtent(text);
			memDC.TextOut(xPos, centerY - ts.cy / 2, text);
			xPos += ts.cx + g_data.RDPI(4);
			};

		if (!titleTip.IsEmpty())
		{
			CString tip = titleTip;
			int pos = 0;
			CString token;
			COLORREF colors[] = { RGB(0, 68, 204), RGB(204, 34, 34) };
			int colorIdx = 0;
			while ((token = tip.Tokenize(_T(" "), pos)) != _T(""))
			{
				drawLabel(_T(""), token, colors[min(colorIdx, 1)]);
				colorIdx++;
			}
		}
		else
		{
			drawLabel(defaultLabel1, _T(""), RGB(0, 68, 204));
			drawLabel(defaultLabel2, _T(""), RGB(204, 34, 34));
		}
	}
}

void CIndicatorChart::DrawKDJTitleBar(CDC& memDC, const TimelineDrawContext& ctx, int areaTop, int titleH, const HoverState& hover)
{
	// KDJ序列由 TechnicalIndicatorModel 缓存，此处只读
	static const std::vector<KDJData> emptyKdjTitle;
	const auto& kdjData = ctx.indicatorModel ? ctx.indicatorModel->GetKDJ() : emptyKdjTitle;

	int xPos = g_data.RDPI(4);
	int centerY = areaTop + titleH / 2;
	auto drawKDJLabel = [&](const CString& label, COLORREF labelColor) {
		memDC.SetTextColor(labelColor);
		CSize ts = memDC.GetTextExtent(label);
		memDC.TextOut(xPos, centerY - ts.cy / 2, label);
		xPos += ts.cx;
		};
	auto drawKDJValue = [&](const CString& value, double val) {
		// KDJ值：>50红色，<50绿色，=50灰色
		COLORREF valColor = (val > 50) ? COLOR_RED_UP : ((val < 50) ? COLOR_GREEN_DOWN : COLOR_GRAY_TEXT);
		memDC.SetTextColor(valColor);
		CSize ts = memDC.GetTextExtent(value);
		memDC.TextOut(xPos, centerY - ts.cy / 2, value);
		xPos += ts.cx + g_data.RDPI(4);
		};

	// 获取当前K/D/J值（非悬停时直接读模型缓存）
	double kVal = 0, dVal = 0, jVal = 0;
	if (!hover.timelineKdjTitleTip.IsEmpty())
		ParseKDJTip(hover.timelineKdjTitleTip, kVal, dVal, jVal);
	else if (ctx.indicatorModel)
		ctx.indicatorModel->GetLastKDJ(kVal, dVal, jVal);

	drawKDJLabel(_T("K:"), COLOR_RED_UP);
	CString kStr; kStr.Format(_T("%.1f"), kVal);
	drawKDJValue(kStr, kVal);
	drawKDJLabel(_T("D:"), RGB(0, 68, 204));
	CString dStr; dStr.Format(_T("%.1f"), dVal);
	drawKDJValue(dStr, dVal);
	drawKDJLabel(_T("J:"), RGB(0, 136, 34));
	CString jStr; jStr.Format(_T("%.1f"), jVal);
	drawKDJValue(jStr, jVal);

	// J值趋势箭头（随悬停位置截断，规则与MACD的DIF箭头一致）
	const int trendEndIdx = ResolveTrendEndIdx(hover.timelineKdjTitleTip, hover.hoveredBarIndex,
		ctx.startIndex, static_cast<int>(kdjData.size()),
		[&kdjData](int i) { return kdjData[i].valid; });
	if (trendEndIdx < 0)
		return;

	std::vector<KDJData> subKdj(kdjData.begin(), kdjData.begin() + trendEndIdx + 1);
	auto trend = CStockIndicator::CalcKDJTrend(subKdj, GetTrendLookback(hover.viewMode));
	if (!trend.valid || trend.slope == 0.0)
		return;

	// K-D差值范围通常-20~20，阈值：1个箭头=slope>=0.5, 2个>=1.5, 3个>=3
	double absSlope = std::abs(trend.slope);
	int arrowCount = 1;
	if (absSlope >= 3) arrowCount = 3;
	else if (absSlope >= 1.5) arrowCount = 2;

	// K>=D用红色，K<D用绿色
	COLORREF arrowColor = (kdjData[trendEndIdx].k >= kdjData[trendEndIdx].d) ? COLOR_RED_UP : COLOR_GREEN_DOWN;
	DrawTrendArrows(memDC, xPos, centerY, trend.slope > 0, arrowCount, arrowColor);
}

void CIndicatorChart::DrawWRTitleBar(CDC& memDC, int areaTop, int titleH, const HoverState& hover)
{
	DrawTwoPeriodTitleBar(memDC, areaTop, titleH, hover.timelineWrTitleTip, _T("WR6:"), _T("WR14:"));
}

void CIndicatorChart::DrawRSITitleBar(CDC& memDC, int areaTop, int titleH, const HoverState& hover)
{
	DrawTwoPeriodTitleBar(memDC, areaTop, titleH, hover.timelineRsiTitleTip, _T("RSI6:"), _T("RSI14:"));
}

// ========== DrawIndicatorChartArea ==========

void CIndicatorChart::DrawIndicatorChartArea(CDC& memDC, const TimelineDrawContext& ctx, int areaTop, int areaHeight, bool drawTimeLabels, TimelineIndicator indicator, const HoverState& hover)
{
	const int titleH = g_data.RDPI(16);
	int oldBkMode = memDC.SetBkMode(TRANSPARENT);

	// 标题栏背景 + 各指标专属标题文字
	memDC.FillSolidRect(CRect(0, areaTop, ctx.chartWidth, areaTop + titleH), RGB(245, 245, 245));
	if (indicator == TimelineIndicator::KDJ)
		DrawKDJTitleBar(memDC, ctx, areaTop, titleH, hover);
	else if (indicator == TimelineIndicator::WR)
		DrawWRTitleBar(memDC, areaTop, titleH, hover);
	else if (indicator == TimelineIndicator::RSI)
		DrawRSITitleBar(memDC, areaTop, titleH, hover);
	else
		DrawVolumeTitleBar(memDC, ctx, areaTop, titleH, hover);

	memDC.SetBkMode(oldBkMode);

	const int chartTop = areaTop + titleH;
	const int chartHeight = areaHeight - titleH;
	TimelineDrawContext tmpCtx = ctx;

	if (indicator == TimelineIndicator::KDJ)
	{
		tmpCtx.macdChartTop = chartTop;
		tmpCtx.macdChartHeight = chartHeight;
		//CTimelineChart::DrawTimelineBackgroundHighlightsForArea(memDC, tmpCtx, tmpCtx.macdChartTop, tmpCtx.macdChartHeight);
		DrawTimelineKDJSection(memDC, tmpCtx, hover);
	}
	else if (indicator == TimelineIndicator::WR)
	{
		tmpCtx.macdChartTop = chartTop;
		tmpCtx.macdChartHeight = chartHeight;
		//CTimelineChart::DrawTimelineBackgroundHighlightsForArea(memDC, tmpCtx, tmpCtx.macdChartTop, tmpCtx.macdChartHeight);
		DrawTimelineWRSection(memDC, tmpCtx, hover);
	}
	else if (indicator == TimelineIndicator::RSI)
	{
		tmpCtx.macdChartTop = chartTop;
		tmpCtx.macdChartHeight = chartHeight;
		//CTimelineChart::DrawTimelineBackgroundHighlightsForArea(memDC, tmpCtx, tmpCtx.macdChartTop, tmpCtx.macdChartHeight);
		DrawTimelineRSISection(memDC, tmpCtx, hover);
	}
	else
	{
		// 成交量模式（MACD枚举值现在表示成交量）
		//CTimelineChart::DrawTimelineBackgroundHighlightsForArea(memDC, tmpCtx, chartTop, chartHeight);
		DrawVolumeChartBody(memDC, ctx, chartTop, chartHeight, hover);
	}

	// X轴时间标签已提取为独立的 DrawTimeLabels（由CFloatingWnd统一调用，
	// 不再依附于KDJ区域——该区域隐藏时标签仍需显示）
}

// ========== DrawCrossArrow ==========

void CIndicatorChart::DrawCrossArrow(CDC& memDC, int markX, int markY, int dotRadius, bool isGolden)
{
	COLORREF arrowColor = isGolden ? COLOR_RED_UP : COLOR_GREEN_DOWN;
	CBrush arrowBrush(arrowColor);
	CPen arrowPen(PS_SOLID, 1, arrowColor);
	CBrush* pOldBrush = memDC.SelectObject(&arrowBrush);
	CPen* pOldPen = memDC.SelectObject(&arrowPen);
	int aw = g_data.RDPI(3);    // 箭头头部半宽
	int ah = g_data.RDPI(5);    // 箭头头部高度
	int shaft = g_data.RDPI(7); // 箭头竖线长度
	int sw = g_data.RDPI(1);    // 箭头竖线半宽
	if (isGolden)
	{
		// ↑箭头：从圆点上边缘向上绘制（箭头头+竖线杆）
		int base = markY - dotRadius;
		CPoint pts[7] = {
			CPoint(markX, base - ah - shaft),
			CPoint(markX - aw, base - shaft),
			CPoint(markX - sw, base - shaft),
			CPoint(markX - sw, base),
			CPoint(markX + sw, base),
			CPoint(markX + sw, base - shaft),
			CPoint(markX + aw, base - shaft)
		};
		memDC.Polygon(pts, 7);
	}
	else
	{
		// ↓箭头：从圆点下边缘向下绘制（箭头头+竖线杆）
		int base = markY + dotRadius;
		CPoint pts[7] = {
			CPoint(markX, base + ah + shaft),
			CPoint(markX - aw, base + shaft),
			CPoint(markX - sw, base + shaft),
			CPoint(markX - sw, base),
			CPoint(markX + sw, base),
			CPoint(markX + sw, base + shaft),
			CPoint(markX + aw, base + shaft)
		};
		memDC.Polygon(pts, 7);
	}
	memDC.SelectObject(pOldBrush);
	memDC.SelectObject(pOldPen);
}

// ========== DrawCrossArrow2 ==========
// 金叉红↑/死叉绿↓（方向与颜色绑定语义），位置贴图表顶边/底边：
// 同一边缘的金叉/死叉箭头占据同一水平带（高度水平一致）——
// 死叉头在带内下端指向图内，金叉头在带内上端指向图外方向，
// 全部画在图表内部，不越过MACD图像边缘

void CIndicatorChart::DrawCrossArrow2(CDC& memDC, int markX, int markY, int dotRadius, bool isGolden, bool arrowAtTop)
{
	COLORREF arrowColor = isGolden ? COLOR_RED_UP : COLOR_GREEN_DOWN;
	CBrush arrowBrush(arrowColor);
	CPen arrowPen(PS_SOLID, 1, arrowColor);
	CBrush* pOldBrush = memDC.SelectObject(&arrowBrush);
	CPen* pOldPen = memDC.SelectObject(&arrowPen);
	int aw = g_data.RDPI(3);    // 箭头头部半宽
	int ah = g_data.RDPI(5);    // 箭头头部高度
	int shaft = g_data.RDPI(7); // 箭头竖线长度
	int sw = g_data.RDPI(1);    // 箭头竖线半宽
	// 同一水平带的下端（顶边模式：带从边缘dotRadius处向下延伸ah+shaft；
	// 底边模式：带从边缘dotRadius处向上延伸ah+shaft）
	int spanBot = arrowAtTop ? (markY + dotRadius + ah + shaft) : (markY - dotRadius);
	// 金叉↑：杆底=spanBot、头在带内上端；死叉↓：杆顶=带内上端、头在下端
	// （两种箭头基线不同但占据同一区间，视觉高度水平对齐）
	int base = isGolden ? spanBot : (spanBot - ah - shaft);
	if (isGolden)
	{
		// ↑箭头：头朝上、竖线杆向下连到base
		CPoint pts[7] = {
			CPoint(markX, base - ah - shaft),
			CPoint(markX - aw, base - shaft),
			CPoint(markX - sw, base - shaft),
			CPoint(markX - sw, base),
			CPoint(markX + sw, base),
			CPoint(markX + sw, base - shaft),
			CPoint(markX + aw, base - shaft)
		};
		memDC.Polygon(pts, 7);
	}
	else
	{
		// ↓箭头：头朝下、竖线杆向上连到base
		CPoint pts[7] = {
			CPoint(markX, base + ah + shaft),
			CPoint(markX - aw, base + shaft),
			CPoint(markX - sw, base + shaft),
			CPoint(markX - sw, base),
			CPoint(markX + sw, base),
			CPoint(markX + sw, base + shaft),
			CPoint(markX + aw, base + shaft)
		};
		memDC.Polygon(pts, 7);
	}
	memDC.SelectObject(pOldBrush);
	memDC.SelectObject(pOldPen);
}

// ========== DrawSectionGridAndTimeLabels ==========

void CIndicatorChart::DrawSectionGridAndTimeLabels(CDC& memDC, const TimelineDrawContext& ctx, int chartTop, int chartHeight, bool drawTimeLabels)
{
	const auto& timelinePoint = *ctx.timelinePoint;
	CPen pGrid(PS_DOT, 1, COLOR_GRAY_GRID);
	CPen* pOldPen = memDC.SelectObject(&pGrid);
	memDC.MoveTo(0, chartTop);
	memDC.LineTo(ctx.chartWidth, chartTop);
	memDC.MoveTo(0, chartTop + chartHeight);
	memDC.LineTo(ctx.chartWidth, chartTop + chartHeight);
	const int totalPts = static_cast<int>(timelinePoint.size());
	if (totalPts > 0)
	{
		// 竖向虚线：柱中心对齐，5/30分钟隔天交替黑色，其余灰色
		DrawDayColoredVLines(memDC, timelinePoint, ctx.chartWidth, ctx.xAxisPoints,
			chartTop, chartHeight, pGrid);
	}
	memDC.SelectObject(pOldPen);
	if (drawTimeLabels && totalPts > 0)
	{
		DrawTimeLabelsAt(memDC, timelinePoint, ctx.chartWidth,
			chartTop + chartHeight + g_data.RDPI(2), ctx.xAxisPoints);
	}
}

// ========== DrawDifExtremeMark ==========
// 在DIF线的极值点（最高/最低）绘制标记：空心圆圈包住H/L字母，直接落在DIF线上
// 最高点红色H，最低点绿色L

void CIndicatorChart::DrawDifExtremeMark(CDC& memDC, int markX, int markY, bool isHigh)
{
	// 实现在文件内匿名命名空间 DrawDifMark（与DrawMacdDIFExtremeMarks共用）
	DrawDifMark(memDC, markX, markY, isHigh);
}

// ========== DrawTimeLabels ==========
// 不依附任何指标区域——KDJ区域隐藏（MACD扩展）时标签仍然绘制在图表底部
// 隔天标签变色逻辑在 DrawTimeLabelsAt（两处标签绘制路径共用）

void CIndicatorChart::DrawTimeLabels(CDC& memDC, const TimelineDrawContext& ctx)
{
	if (!ctx.timelinePoint || ctx.timelinePoint->empty())
		return;
	DrawTimeLabelsAt(memDC, *ctx.timelinePoint, ctx.chartWidth, ctx.positionY, ctx.xAxisPoints);
}

void CIndicatorChart::DrawTimeLabelsAt(CDC& memDC, const std::vector<STOCK::TimelinePoint>& timelinePoint,
	int xAxisWidth, int labelY, int xAxisPts /* = 0 */)
{
	const int totalPts = static_cast<int>(timelinePoint.size());
	const int numVLines = 6;
	// 标签X坐标对齐柱中心（与K线/鼠标十字线/竖线一致）
	const int slots = (xAxisPts > 0) ? xAxisPts : totalPts;
	// 隔天标签交替变色（仅5/30分钟K线模式生效）：绝对天数奇偶决定颜色，最新交易日为蓝
	const DayColorCtx dayCtx = CalcDayColorCtx(timelinePoint);
	const COLORREF crossDayColor = COLOR_BLACK;   // 隔天标签颜色
	for (int i = 0; i <= numVLines; i++)
	{
		int idx = totalPts * i / numVLines;
		if (idx >= totalPts) idx = totalPts - 1;
		int xPos = static_cast<int>(xAxisWidth / static_cast<float>(slots) * (static_cast<float>(idx) + 0.5f));
		int xPosClamp = max(0, xPos);   // 首标签中心可能为负，钳到0
		CString timeLabel(timelinePoint[idx].time.c_str());
		if (timeLabel.GetLength() >= 5) timeLabel = timeLabel.Left(5);
		CSize labelSize = memDC.GetTextExtent(timeLabel);
		int labelX = max(0, min(xPosClamp - labelSize.cx / 2, xAxisWidth - labelSize.cx));
		memDC.SetTextColor(IsBlueDayAt(timelinePoint, idx, dayCtx) ? crossDayColor : COLOR_GRAY_TEXT);
		memDC.TextOut(labelX, labelY, timeLabel);
	}
}

// ========== DrawTimelineKDJSection ==========

void CIndicatorChart::DrawTimelineKDJSection(CDC& memDC, const TimelineDrawContext& ctx, const HoverState& hover)
{
	const auto& timelinePoint = *ctx.timelinePoint;
	// KDJ序列由 TechnicalIndicatorModel 缓存，此处只读
	static const std::vector<KDJData> emptyKdj;
	const auto& kdjData = ctx.indicatorModel ? ctx.indicatorModel->GetKDJ() : emptyKdj;
	DrawTimelineKDJChart(memDC, 0, ctx.macdChartTop, ctx.chartWidth, ctx.macdChartHeight, timelinePoint, kdjData, ctx.startIndex, ctx.xAxisPoints, &ctx);
	DrawSectionGridAndTimeLabels(memDC, ctx, ctx.macdChartTop, ctx.macdChartHeight, true);
}

// ========== DrawTimelineKDJChart ==========

void CIndicatorChart::DrawTimelineKDJChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::TimelinePoint>& timelinePoint, const std::vector<KDJData>& kdjData, int startIndex /* = 0 */, int xAxisPoints /* = 0 */, const TimelineDrawContext* modelCtx /* = nullptr */)
{
	if (timelinePoint.empty() || kdjData.empty())
		return;

	// 量程：以J值的可视极值为准，但至少覆盖0~100
	double minVal = 0;
	double maxVal = 100;
	int drawStart = startIndex;
	int drawEnd = startIndex + static_cast<int>(timelinePoint.size());
	if (drawEnd > static_cast<int>(kdjData.size())) drawEnd = static_cast<int>(kdjData.size());
	for (int i = drawStart; i < drawEnd; i++)
	{
		if (!kdjData[i].valid) continue;
		minVal = (std::min)(minVal, kdjData[i].j);
		maxVal = (std::max)(maxVal, kdjData[i].j);
	}
	if (minVal > 0) minVal = 0;
	if (maxVal < 100) maxVal = 100;

	const int padding = g_data.RDPI(4);
	int drawHeight = height - padding * 2;
	if (drawHeight <= 0) return;

	const ValueToYFn valueToY = [&](double val) {
		double ratio = (val - minVal) / (maxVal - minVal);
		return y + padding + static_cast<int>((1.0 - ratio) * drawHeight);
		};

	// 0/100量程边界（虚线）与20/50/80参考线（点线）
	const double borderValues[] = { 0.0, 100.0 };
	DrawRefLines(memDC, x, width, borderValues, 2, valueToY, PS_DASH, COLOR_GRAY_GRID);
	const double refValues[] = { 20.0, 50.0, 80.0 };
	DrawRefLines(memDC, x, width, refValues, 3, valueToY, PS_DOT, COLOR_GRAY_GRID);

	const int xSlots = (xAxisPoints > 0) ? xAxisPoints : static_cast<int>(timelinePoint.size());
	const int totalPts = static_cast<int>(timelinePoint.size());

	// K/D/J三条曲线
	DrawOscillatorSeries(memDC, x, width, xSlots, totalPts, startIndex, kdjData,
		[](const KDJData& d) { return d.k; }, valueToY, COLOR_RED_UP);
	DrawOscillatorSeries(memDC, x, width, xSlots, totalPts, startIndex, kdjData,
		[](const KDJData& d) { return d.d; }, valueToY, RGB(0, 68, 204));
	DrawOscillatorSeries(memDC, x, width, xSlots, totalPts, startIndex, kdjData,
		[](const KDJData& d) { return d.j; }, valueToY, RGB(0, 136, 34));

	// KDJ金叉死叉信号：直接读模型缓存序列
	static const std::vector<MACDCrossSignal> emptyCross2;
	const auto& crossSignals = (modelCtx && modelCtx->indicatorModel)
		? modelCtx->indicatorModel->GetKDJCross() : emptyCross2;

	// 圆点落在K线上，箭头从圆点边缘伸出
	CrossMarkGeom markGeom;
	markGeom.markX = [&](int i) { return OscSlotCenterX(x, width, xSlots, i); };
	markGeom.markY = [&](int i) { return valueToY(kdjData[startIndex + i].k); };
	markGeom.drawArrow = [&](int /*i*/, int markX, int markY, bool isGolden) {
		DrawCrossArrow(memDC, markX, markY, g_data.RDPI(3), isGolden);
	};
	DrawCrossSignalMarks(memDC, crossSignals, startIndex, totalPts, markGeom);
}

// ========== DrawTimelineWRChart ==========

void CIndicatorChart::DrawTimelineWRChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::TimelinePoint>& timelinePoint, const std::vector<WRData>& wrData, int startIndex /* = 0 */, int xAxisPoints /* = 0 */)
{
	if (timelinePoint.empty() || wrData.empty())
		return;

	// 量程固定0~100
	const double minVal = 0;
	const double maxVal = 100;

	const int padding = g_data.RDPI(4);
	int drawHeight = height - padding * 2;
	if (drawHeight <= 0) return;

	const ValueToYFn valueToY = [&](double val) {
		double ratio = (val - minVal) / (maxVal - minVal);
		return y + padding + static_cast<int>((1.0 - ratio) * drawHeight);
		};

	const double refValues[] = { 20.0, 50.0, 80.0 };
	DrawRefLines(memDC, x, width, refValues, 3, valueToY, PS_DOT, COLOR_GRAY_GRID);

	const int xSlots = (xAxisPoints > 0) ? xAxisPoints : static_cast<int>(timelinePoint.size());
	const int totalPts = static_cast<int>(timelinePoint.size());

	// WR6 / WR14 两条曲线
	DrawOscillatorSeries(memDC, x, width, xSlots, totalPts, startIndex, wrData,
		[](const WRData& d) { return d.wr1; }, valueToY, RGB(0, 68, 204));
	DrawOscillatorSeries(memDC, x, width, xSlots, totalPts, startIndex, wrData,
		[](const WRData& d) { return d.wr2; }, valueToY, RGB(204, 34, 34));
}

// ========== DrawTimelineWRSection ==========

void CIndicatorChart::DrawTimelineWRSection(CDC& memDC, const TimelineDrawContext& ctx, const HoverState& hover)
{
	const auto& timelinePoint = *ctx.timelinePoint;

	// W&R序列由 TechnicalIndicatorModel 缓存（K线/分时模式在Update时自动选择对应计算）
	static const std::vector<WRData> emptyWr;
	const auto& wrData = ctx.indicatorModel ? ctx.indicatorModel->GetWR() : emptyWr;

	DrawTimelineWRChart(memDC, 0, ctx.macdChartTop, ctx.chartWidth, ctx.macdChartHeight, timelinePoint, wrData, ctx.startIndex, ctx.xAxisPoints);
	DrawSectionGridAndTimeLabels(memDC, ctx, ctx.macdChartTop, ctx.macdChartHeight, true);
}

// ========== DrawTimelineRSIChart ==========

void CIndicatorChart::DrawTimelineRSIChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::TimelinePoint>& timelinePoint, const std::vector<RSIData>& rsiData, int startIndex /* = 0 */, int xAxisPoints /* = 0 */)
{
	if (timelinePoint.empty() || rsiData.empty())
		return;

	// 量程固定0~100
	const double minVal = 0;
	const double maxVal = 100;

	const int padding = g_data.RDPI(4);
	int drawHeight = height - padding * 2;
	if (drawHeight <= 0) return;

	const ValueToYFn valueToY = [&](double val) {
		double ratio = (val - minVal) / (maxVal - minVal);
		return y + padding + static_cast<int>((1.0 - ratio) * drawHeight);
		};

	const double refValues[] = { 30.0, 50.0, 70.0 };
	DrawRefLines(memDC, x, width, refValues, 3, valueToY, PS_DOT, COLOR_GRAY_GRID);

	const int xSlots = (xAxisPoints > 0) ? xAxisPoints : static_cast<int>(timelinePoint.size());
	const int totalPts = static_cast<int>(timelinePoint.size());

	// RSI6 / RSI14 两条曲线
	DrawOscillatorSeries(memDC, x, width, xSlots, totalPts, startIndex, rsiData,
		[](const RSIData& d) { return d.rsi1; }, valueToY, RGB(0, 68, 204));
	DrawOscillatorSeries(memDC, x, width, xSlots, totalPts, startIndex, rsiData,
		[](const RSIData& d) { return d.rsi2; }, valueToY, RGB(204, 34, 34));
}

// ========== DrawTimelineRSISection ==========

void CIndicatorChart::DrawTimelineRSISection(CDC& memDC, const TimelineDrawContext& ctx, const HoverState& hover)
{
	const auto& timelinePoint = *ctx.timelinePoint;

	// RSI序列由 TechnicalIndicatorModel 缓存（K线/分时模式在Update时自动选择对应计算）
	static const std::vector<RSIData> emptyRsi;
	const auto& rsiData = ctx.indicatorModel ? ctx.indicatorModel->GetRSI() : emptyRsi;

	DrawTimelineRSIChart(memDC, 0, ctx.macdChartTop, ctx.chartWidth, ctx.macdChartHeight, timelinePoint, rsiData, ctx.startIndex, ctx.xAxisPoints);
	DrawSectionGridAndTimeLabels(memDC, ctx, ctx.macdChartTop, ctx.macdChartHeight, true);
}

// ========== 成交量柱的拆分/配色计算（DrawVolumeChart 专用） ==========

namespace
{
	// 可视区间归一化：visibleCount>0 时钳到[startIndex, startIndex+visibleCount)，否则取全部
	void ResolveVisibleRange(int total, int visibleCount, int& startIndex, int& endIndex)
	{
		endIndex = total;
		if (visibleCount > 0)
		{
			endIndex = (std::min)(total, startIndex + visibleCount);
			startIndex = (std::max)(0, (std::min)(startIndex, total - 1));
			if (endIndex <= startIndex)
				endIndex = startIndex + 1;
		}
		else
		{
			startIndex = 0;
		}
	}

	// 单根量柱按主动买卖拆分后的高度（手数比例），netBuy>=0 时红段在上
	struct VolumeSplit
	{
		int buy{ 0 };
		int sell{ 0 };
		int netBuy{ 0 };
	};

	// 从分时成交汇总中算出当前柱的红绿拆分高度
	// 返回false表示该视图模式/该时刻无汇总数据，调用方退回默认涨跌色
	bool CalcVolumeSplitHeights(const std::map<std::string, ::TickSummary>& vecTickSummary,
		UIViewMode curMode, const STOCK::TimelinePoint& item, int barHeight, VolumeSplit& out)
	{
		if (vecTickSummary.empty())
			return false;

		if (curMode == UI_VIEW_TIMELINE)
		{
			// 分时：取前一分钟的买卖占比
			std::string strTime = item.time.substr(0, 5);
			auto it = vecTickSummary.find(CCommon::subtractOneMinuteFast(strTime));
			if (it == vecTickSummary.end())
				return false;
			out.buy = static_cast<int>(barHeight * it->second.BuyRatio());
			out.sell = static_cast<int>(barHeight * it->second.SellRatio());
			out.netBuy = static_cast<int>(it->second.netBuy);
			return true;
		}

		if (curMode == UI_VIEW_MIN5_KLINE || curMode == UI_VIEW_MIN30_KLINE)
		{
			// 只统计与汇总同一天的K线
			if (item.fullTime.find(vecTickSummary.begin()->second.tradeDate) != 0)
				return false;
			int interval = (curMode == UI_VIEW_MIN5_KLINE) ? 5 : 30;
			auto itFind = vecTickSummary.find(item.time.substr(0, 5));
			if (itFind != vecTickSummary.end())
			{
				STOCK::TickSummary sumTick{};
				auto it = itFind;
				// 关键：向前跳一步，跳过K线结束时刻本身(09:35)
				if (it != vecTickSummary.begin())
				{
					--it;
					for (int i = 0; i < interval; ++i)
					{
						sumTick.buy += it->second.buy;
						sumTick.sell += it->second.sell;
						if (it == vecTickSummary.begin())
							break;
						--it;
					}
				}
				out.netBuy = static_cast<int>(sumTick.buy - sumTick.sell);
				out.buy = static_cast<int>(barHeight * sumTick.BuyRatio());
				out.sell = static_cast<int>(barHeight * sumTick.SellRatio());
			}
			// 日期匹配即按拆分柱绘制；无该时刻明细时高度为0（柱体全绿）
			return true;
		}
		return false;
	}

	// 绘制红绿分层柱：netBuy>=0 红段在上，否则绿段在上
	void DrawSplitVolumeBar(CDC& memDC, const VolumeSplit& split,
		int barX, int barY, int barWidth, int chartBottom)
	{
		if (split.netBuy >= 0)
		{
			CBrush brush(COLOR_RED_UP);
			memDC.FillRect(CRect(barX, barY, barX + barWidth, chartBottom - split.sell), &brush);
			CBrush brush1(COLOR_GREEN_DOWN);
			memDC.FillRect(CRect(barX, barY + split.buy, barX + barWidth, chartBottom), &brush1);
		}
		else
		{
			CBrush brush(COLOR_GREEN_DOWN);
			memDC.FillRect(CRect(barX, barY, barX + barWidth, chartBottom - split.buy), &brush);
			CBrush brush1(COLOR_RED_UP);
			memDC.FillRect(CRect(barX, barY + split.sell, barX + barWidth, chartBottom), &brush1);
		}
	}

	// 默认柱色：K线模式用收盘价vs开盘价（与K线柱颜色一致），分时模式用vs前一分钟价格
	COLORREF CalcDefaultVolumeColor(const std::vector<STOCK::TimelinePoint>& timelinePoint, int i)
	{
		const auto& item = timelinePoint[i];
		if (item.openPrice > 0)
			return (item.price >= item.openPrice) ? COLOR_RED_UP : COLOR_GREEN_DOWN;
		if (i > 0 && item.price >= timelinePoint[i - 1].price)
			return COLOR_RED_UP;
		return COLOR_GREEN_DOWN;
	}
}

// ========== DrawVolumeChart ==========
// 直接按原始成交量线性缩放绘制（最大成交量对应图表满高），不做任何非线性变换
// 拆分柱高度/默认涨跌色的计算已下沉到匿名命名空间

void CIndicatorChart::DrawVolumeChart(CDC& memDC, int x, int y, int width, int height,
	const std::vector<STOCK::TimelinePoint>& timelinePoint,
	const STOCK::StockInfo* stockInfo,
	int startIndex /* = 0 */, int visibleCount /* = -1 */, int xAxisPoints /* = 0 */)
{
	if (timelinePoint.empty())
		return;

	int endIdx = 0;
	ResolveVisibleRange(static_cast<int>(timelinePoint.size()), visibleCount, startIndex, endIdx);

	STOCK::Volume maxVolume = 0;
	for (int i = startIndex; i < endIdx; i++)
	{
		if (timelinePoint[i].volume > maxVolume)
			maxVolume = timelinePoint[i].volume;
	}
	if (maxVolume == 0)
		return;

	// 分时成交量汇总（主动买入/主动卖出明细），按当前视图模式选择拆分算法
	auto curMode = g_data.GetCurrentViewMode();
	std::map<std::string, ::TickSummary> vecTickSummary;
	if (stockInfo != nullptr)
	{
		auto strDate = CCommon::get_stock_trade_date();
		vecTickSummary = g_data.GetDbManager().LoadTickSummary(stockInfo->code, strDate);
	}

	const int xSlots = (xAxisPoints > 0) ? xAxisPoints : static_cast<int>(timelinePoint.size());
	const int fixedGap = 1;
	int slotWidth = xSlots > 0 ? width / xSlots : 1;
	int barWidth = max(2, slotWidth - fixedGap);
	int halfSlot = slotWidth / 2;

	for (int i = startIndex; i < endIdx; i++)
	{
		const auto& item = timelinePoint[i];
		int barX = x + static_cast<int>(width / static_cast<float>(xSlots) * i) + halfSlot - barWidth / 2;
		float ratio = static_cast<float>(item.volume) / maxVolume;
		int barHeight = max(static_cast<int>(ratio * height), 1);
		int barY = y + height - barHeight;

		// TODO : 这里的涨跌颜色判断逻辑可能需要根据实际需求调整，
		// 分时模式下柱子的颜色分成两部分，从vecTickSummary取当前时间下主动买入和主动卖出的成交量，主动买入为红色，主动卖出为绿色，
		// 可以直接将高度按比例分配给红色和绿色两部分，比例大的绘制在上方，比例小的绘制在下方，形成一个红绿分层的柱子。
		// K线模式下使用开盘价和收盘价判断涨跌
		VolumeSplit split;
		if (CalcVolumeSplitHeights(vecTickSummary, curMode, item, barHeight, split))
		{
			DrawSplitVolumeBar(memDC, split, barX, barY, barWidth, y + height);
		}
		else
		{
			CBrush brush(CalcDefaultVolumeColor(timelinePoint, i));
			memDC.FillRect(CRect(barX, barY, barX + barWidth, y + height), &brush);
		}
	}
}

// ========== DrawTimelineKDJChart ==========