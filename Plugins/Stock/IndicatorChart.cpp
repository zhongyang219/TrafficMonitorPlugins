#include "pch.h"
#include "IndicatorChart.h"
#include "TimelineChart.h"
#include "ChartColors.h"
#include "Common.h"
#include "DataManager.h"
#include "TechnicalIndicatorModel.h"
#include <algorithm>

// ========== 隔天交替上色公共逻辑（时间标签+各区域竖向虚线共用） ==========

// "yyyy-mm-dd" → 天序号（相对固定纪元的天数，用于日期差奇偶判断）；解析失败返回-1
// 非static：TimelineChart.cpp的走势图竖线也复用该逻辑
int DayNumberFromYMD(const std::string& ymd)
{
	if (ymd.length() < 10 || ymd[4] != '-' || ymd[7] != '-')
		return -1;
	int y = atoi(ymd.substr(0, 4).c_str());
	int m = atoi(ymd.substr(5, 2).c_str());
	int d = atoi(ymd.substr(8, 2).c_str());
	if (y <= 0 || m < 1 || m > 12 || d < 1 || d > 31)
		return -1;
	static const int daysBeforeMonth[] = { 0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
	int dayNo = y * 365 + (y - 1) / 4 - (y - 1) / 100 + (y - 1) / 400
		+ daysBeforeMonth[m] + d;
	// 闰年且已过2月，补1天
	if (m > 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0))
		dayNo += 1;
	return dayNo;
}

// DayColorCtx 定义在 IndicatorChart.h

// 由可视区间末根（最新交易日）日期标定蓝色归属的半天
DayColorCtx CalcDayColorCtx(const std::vector<STOCK::TimelinePoint>& timelinePoint)
{
	DayColorCtx ctx;
	const STOCK::TimelinePoint& firstPt = timelinePoint[0];
	if (firstPt.fullTime.length() >= 11 && firstPt.fullTime[10] == ' ')
		ctx.klineMode = true;
	int n = static_cast<int>(timelinePoint.size());
	const STOCK::TimelinePoint& lastPt = timelinePoint[n - 1];
	if (lastPt.fullTime.length() >= 10)
	{
		// 最新交易日的前一天开始标色：奇偶翻转（最新的那天不标色）
		int lastDayNo = DayNumberFromYMD(lastPt.fullTime.substr(0, 10));
		if (lastDayNo >= 0)
			ctx.blueIfOdd = (lastDayNo % 2 == 0);
	}
	return ctx;
}

// 第idx个刻度点（标签/竖线）所在日期是否为标色日（最新交易日为基准前一天起交替）
bool IsBlueDayAt(const std::vector<STOCK::TimelinePoint>& timelinePoint, int idx, const DayColorCtx& ctx)
{
	if (!ctx.klineMode || idx < 0 || idx >= static_cast<int>(timelinePoint.size()))
		return false;
	const STOCK::TimelinePoint& pt = timelinePoint[idx];
	if (pt.fullTime.length() < 10)
		return false;
	int curDayNo = DayNumberFromYMD(pt.fullTime.substr(0, 10));
	if (curDayNo < 0)
		return false;
	// 按绝对天数奇偶上色（与可视窗口无关，滚动时同一天颜色稳定）
	return (curDayNo % 2 != 0) == ctx.blueIfOdd;
}

// ========== DrawMACDChart（分时数据版本） ==========

void CIndicatorChart::DrawMACDChart(CDC& memDC, int x, int y, int width, int height,
	const std::vector<STOCK::TimelinePoint>& timelinePoint,
	const std::vector<MACDData>& macdData, int startIndex /* = 0 */, int visibleCount /* = -1 */, int xAxisPoints /* = 0 */,
	const TimelineDrawContext* modelData /* = nullptr */)
{
	if (timelinePoint.empty() || macdData.empty())
		return;

	int totalPts = static_cast<int>(timelinePoint.size());

	// ===== Y轴自适应缩放（仅在绘图层做，指标数据保持原值不动）=====
	// 双轴方案：DIF/DEA 共用一套Y轴，MACD柱单独一套独立Y轴。
	// 柱值(DIF-DEA的2倍差)通常远小于DIF/DEA本身，共用一套轴时柱子会被压扁；
	// 柱单独按自己可视区间的极值缩放后可以撑满子图（行情软件通用做法）
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

	int zeroY = y + height / 2;
	// 自适应Y轴：单位 = 可用半高 / 可视区间最大绝对值（不再固定量程）
	// 极值为0时（区间内全为0）回退固定量程1，避免除0
	float unitY = (height / 2.0f - g_data.RDPI(2)) / static_cast<float>(lineMaxAbs > 0 ? lineMaxAbs : 1.0);
	// MACD柱独立轴
	float barUnitY = (height / 2.0f - g_data.RDPI(2)) / static_cast<float>(barMaxAbs > 0 ? barMaxAbs : 1.0);

	CPen zeroPen(PS_SOLID, 1, COLOR_GRAY_MIDDLE);
	CPen* pOldPen = memDC.SelectObject(&zeroPen);

	const int xSlots = (xAxisPoints > 0) ? xAxisPoints : totalPts;
	const int fixedGap = 1;
	int slotWidth = xSlots > 0 ? width / xSlots : 1;
	int barWidth = max(2, static_cast<int>((slotWidth - fixedGap)) * 0.6);
	int halfSlot = slotWidth / 2;

	// ===== DIF上行区间浅红背景（读模型缓存区间序列，渲染层不做计算）=====
	// MVC：区间提取在 TechnicalIndicatorModel::UpdateAll 完成并缓存，
	// 此处只把索引对换算成x坐标绘制背景（先画背景，柱/线后画不被覆盖）
	// DIF在0轴上方时背景占满全高；DIF在0轴下方时只画零轴以下半段，
	// 背景高度随DIF水下/水上分段变化
	{
		static const std::vector<std::pair<int, int>> emptyIntervals;
		const auto& upIntervals = (modelData && modelData->indicatorModel)
			? modelData->indicatorModel->GetDIFUpIntervals() : emptyIntervals;
		if (!upIntervals.empty())
		{
			CBrush bgBrush(COLOR_LIGHT_RED_BG);
			const float pxPerSlot = width / static_cast<float>(xSlots);
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
	}

	memDC.MoveTo(x, zeroY);
	memDC.LineTo(x + width, zeroY);

	// 水上/水下最长柱索引（可视区间内正/负极值柱，分别标深红/墨绿）
	int maxPosIdx = -1, maxNegIdx = -1;
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

	// 绘制 MACD 柱状图（独立Y轴缩放；水上最长柱深红、水下最长柱墨绿）
	for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(macdData.size()); i++)
	{
		int idx = startIndex + i;
		if (!macdData[idx].valid)
			continue;

		int barX = x + static_cast<int>(width / static_cast<float>(xSlots) * i) + halfSlot - barWidth / 2;
		double barVal = macdData[idx].bar;
		int barHeight = static_cast<int>(std::abs(barVal) * barUnitY);
		// 最小高度保护：数值极小时柱子至少2px，避免肉眼不可见
		barHeight = max(2, barHeight);

		COLORREF color;
		if (maxPosIdx == idx)			// 水上最长柱：紫色
			color = RGB(128, 0, 128);
		else if (maxNegIdx == idx)		// 水下最长柱：灰色
			color = RGB(0, 0, 0);
		else
			color = (barVal >= 0) ? COLOR_RED_UP : COLOR_GREEN_DOWN;
		CBrush brush(color);
		int barY = (barVal >= 0) ? zeroY - barHeight : zeroY;
		memDC.FillRect(CRect(barX, barY, barX + barWidth, zeroY + (barVal >= 0 ? 0 : barHeight)), &brush);
	}

	// 绘制 DIF/DEA 双线（分段动态画笔：多头DIF粗橙实线+DEA浅灰细虚线，空头DEA粗蓝实线+DIF浅灰细虚线）
	{
		// 收集有效点的索引和坐标
		struct MacdPoint { int x; int yDif; int yDea; double dif; double dea; };
		std::vector<MacdPoint> pts;
		pts.reserve(totalPts);
		for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(macdData.size()); i++)
		{
			int idx = startIndex + i;
			if (!macdData[idx].valid) continue;
			int px = x + static_cast<int>(width / static_cast<float>(xSlots) * i) + halfSlot;
			int pyDif = zeroY - static_cast<int>(macdData[idx].dif * unitY);
			int pyDea = zeroY - static_cast<int>(macdData[idx].dea * unitY);
			pts.push_back({ px, pyDif, pyDea, macdData[idx].dif, macdData[idx].dea });
		}
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

		// ===== 叠加KDJ的J线（绿色）：点击KDJ按钮后开启（ctx.overlayKDJInMacd）=====
		// J值范围(通常-10~110)与DIF量级无关，独立线性映射到图表全高：
		// J按其可视区间极值自适应缩放，高低极值各留10%边距
		if (modelData && modelData->overlayKDJInMacd && modelData->indicatorModel)
		{
			static const std::vector<KDJData> emptyKdjJ;
			const auto& kdjData = modelData->indicatorModel->GetKDJ();
			if (!kdjData.empty())
			{
				double jMin = 1e308, jMax = -1e308;
				for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(kdjData.size()); i++)
				{
					const auto& kd = kdjData[startIndex + i];
					if (!kd.valid) continue;
					jMin = (std::min)(jMin, kd.j);
					jMax = (std::max)(jMax, kd.j);
				}
				if (jMax > jMin)
				{
					const int jPadding = g_data.RDPI(3);
					const float jUnit = (height - jPadding * 2) / static_cast<float>(jMax - jMin);
					auto jToY = [&](double jval) {
						return y + jPadding + static_cast<int>((jMax - jval) * jUnit);
						};
					// J线量程刻度：0/100两条灰色虚线（量程含0~100时才画）
					CPen jRefPen(PS_DOT, 1, COLOR_GRAY_MIDDLE);
					CPen* pOldJRefPen = memDC.SelectObject(&jRefPen);
					const double jRefValues[] = { 0.0, 100.0 };
					for (double refVal : jRefValues)
					{
						if (refVal >= jMin && refVal <= jMax)
						{
							int refY = jToY(refVal);
							memDC.MoveTo(x, refY);
							memDC.LineTo(x + width, refY);
						}
					}
					memDC.SelectObject(pOldJRefPen);

					CPen jPen(PS_SOLID, 1, COLOR_GREEN_DOWN);
					CPen* pOldJPen = memDC.SelectObject(&jPen);
					bool jFirst = true;
					for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(kdjData.size()); i++)
					{
						const auto& kd = kdjData[startIndex + i];
						if (!kd.valid) { jFirst = true; continue; }
						int px = x + static_cast<int>(width / static_cast<float>(xSlots) * i) + halfSlot;
						int py = jToY(kd.j);
						if (jFirst) { memDC.MoveTo(px, py); jFirst = false; }
						else memDC.LineTo(px, py);
					}
					memDC.SelectObject(pOldJPen);
				}
			}
		}

		// DIF 最高/最低点标记：最高点红色圆圈H标记，最低点绿色圆圈L标记
		// 5分钟/30分钟模式优先展示最后一个交易日（可视序列末尾连续同fullTime日期）的极值，
		// 可视序列无法界定日期（分时模式）时退回整个可视区间
		if (pts.size() >= 2)
		{
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
				DrawDifExtremeMark(memDC, pts[difHiIdx].x, pts[difHiIdx].yDif, true);
			if (difLoIdx >= 0 && difLoIdx != difHiIdx)
				DrawDifExtremeMark(memDC, pts[difLoIdx].x, pts[difLoIdx].yDif, false);
		}
	}

	memDC.SelectObject(pOldPen);

	// 绘制金叉死叉标记：金叉红色↑，死叉绿色↓，箭头紧贴圆点
	// 金叉死叉信号：直接读模型缓存序列，不再每帧重算
	static const std::vector<MACDCrossSignal> emptyCross;
	const auto& crossSignals = (modelData && modelData->indicatorModel)
		? modelData->indicatorModel->GetMACDCross() : emptyCross;
	const int dotRadius = g_data.RDPI(3);
	const int smallDotRadius = g_data.RDPI(2);
	int oldBkMode = memDC.SetBkMode(TRANSPARENT);
	int lastDrawnDir = 0; // 0=无, 1=金叉, -1=死叉

	for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(crossSignals.size()); i++)
	{
		int idx = startIndex + i;
		if (crossSignals[idx] == MACDCrossSignal::None)
			continue;

		int markX = x + static_cast<int>(width / static_cast<float>(xSlots) * i) + halfSlot;
		int markY = zeroY - static_cast<int>(macdData[idx].dif * unitY);

		bool isGolden = (crossSignals[idx] == MACDCrossSignal::GoldenCross || crossSignals[idx] == MACDCrossSignal::RepeatedGoldenCross);
		bool isRepeated = (crossSignals[idx] == MACDCrossSignal::RepeatedGoldenCross || crossSignals[idx] == MACDCrossSignal::RepeatedDeathCross);
		int curDir = isGolden ? 1 : -1;

		// 方向反转时始终绘制箭头；同方向重复则只画圆点
		// 箭头语义：金叉恒为红↑、死叉恒为绿↓（箭头头朝交叉方向）；
		// 位置按交叉点DIF所在0轴上/下半区：0轴上方贴图表顶边，0轴下方贴底边，
		// 箭头数组画在图表内、头朝外侧 lifts
		bool drawArrow = !isRepeated || (curDir != lastDrawnDir);
		if (drawArrow)
		{
			bool aboveZero = (macdData[idx].dif >= 0);
			// 0轴上方: 基线=顶边(y)，箭头从该边缘向下延伸后再从边缘反向指外——即金叉在
			// 顶边仍↑(头朝上但受边缘限制改在顶边下方靠近边缘处向上指)；0轴下方同理镜像
			DrawCrossArrow2(memDC, markX, aboveZero ? y : (y + height), dotRadius, isGolden, aboveZero);
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

// ========== DrawVolumeChartArea ==========

void CIndicatorChart::DrawVolumeChartArea(CDC& memDC, const TimelineDrawContext& ctx, int areaTop, int areaHeight, bool drawTimeLabels, const HoverState& hover)
{
	const auto& timelinePoint = *ctx.timelinePoint;
	const auto& fullData = ctx.fullTimeline ? *ctx.fullTimeline : timelinePoint;
	int titleH = g_data.RDPI(16);
	int oldBkMode = memDC.SetBkMode(TRANSPARENT);

	CRect volumeTitleRect(0, areaTop, ctx.chartWidth, areaTop + titleH);
	memDC.FillSolidRect(volumeTitleRect, RGB(245, 245, 245));

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

	memDC.SetBkMode(oldBkMode);

	TimelineDrawContext tmpCtx = ctx;
	tmpCtx.volumeChartTop = areaTop + titleH;
	tmpCtx.volumeChartHeight = areaHeight - titleH;

	// 先绘制背景高亮，确保数据不被覆盖
	//CTimelineChart::DrawTimelineBackgroundHighlightsForArea(memDC, tmpCtx, tmpCtx.volumeChartTop, tmpCtx.volumeChartHeight);

	DrawVolumeChart(memDC, 0, tmpCtx.volumeChartTop, ctx.chartWidth, tmpCtx.volumeChartHeight, *ctx.timelinePoint, &ctx.realtimeData, 0, -1, ctx.xAxisPoints);

	if (hover.viewMode == UI_VIEW_MIN5_KLINE && ctx.fullTimeline && !ctx.fullTimeline->empty() && ctx.timelinePoint && !ctx.timelinePoint->empty())
	{
		const auto& fullData = *ctx.fullTimeline;
		const auto& visibleData = *ctx.timelinePoint;
		const int totalPoints = static_cast<int>(visibleData.size());
		const int fullCount = static_cast<int>(fullData.size());
		const int startIndex = ctx.startIndex;
		int endIndex = min(fullCount, startIndex + totalPoints);

		STOCK::Volume maxVolume = 0;
		for (int i = startIndex; i < endIndex; i++)
			maxVolume = max(maxVolume, fullData[i].volume);

		if (maxVolume > 0)
		{
			// 成交量均线直接读模型缓存序列（fullTimeline等长），不再逐点重算
			static const std::vector<double> emptySeq;
			const auto& ma5 = ctx.indicatorModel ? ctx.indicatorModel->GetVolumeMA5() : emptySeq;
			const auto& ma10 = ctx.indicatorModel ? ctx.indicatorModel->GetVolumeMA10() : emptySeq;

			auto volumeToY = [&](double volume) -> int {
				int py = tmpCtx.volumeChartTop + tmpCtx.volumeChartHeight - static_cast<int>(volume / static_cast<double>(maxVolume) * tmpCtx.volumeChartHeight);
				return max(tmpCtx.volumeChartTop, min(py, tmpCtx.volumeChartTop + tmpCtx.volumeChartHeight));
				};

			auto drawVolumeMALine = [&](const std::vector<double>& values, COLORREF color) {
				CPen pen(PS_SOLID, 1, color);
				CPen* pOldPen = memDC.SelectObject(&pen);
				bool first = true;
				for (int i = 0; i < totalPoints; i++)
				{
					int globalIndex = startIndex + i;
					if (globalIndex >= static_cast<int>(values.size()) || values[globalIndex] <= 0) { first = true; continue; }
					int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) / 2);
					int pointY = volumeToY(values[globalIndex]);
					if (first) { memDC.MoveTo(pointX, pointY); first = false; }
					else memDC.LineTo(pointX, pointY);
				}
				memDC.SelectObject(pOldPen);
				};

			drawVolumeMALine(ma5, RGB(0, 0, 230));
			drawVolumeMALine(ma10, RGB(0, 166, 235));
		}
	}

	CPen pGrid(PS_DOT, 1, COLOR_GRAY_GRID);
	CPen* pOldVolPen = memDC.SelectObject(&pGrid);
	int volumeY = tmpCtx.volumeChartTop;
	memDC.MoveTo(0, volumeY);
	memDC.LineTo(ctx.chartWidth, volumeY);

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
				int yPos = volumeY + tmpCtx.volumeChartHeight * i / 3;
				memDC.MoveTo(0, yPos);
				memDC.LineTo(ctx.chartWidth, yPos);
				STOCK::Volume volAtLine = maxVolume * (3 - i) / 3;
				STOCK::Volume volInLots = volAtLine / 100;
				CString volLabel = CCommon::FormatVolumeInt(volInLots);
				CSize labelSize = memDC.GetTextExtent(volLabel);
				memDC.TextOut(-labelSize.cx - g_data.RDPI(3), yPos - labelSize.cy / 2, volLabel);
			}
			memDC.SelectObject(&pGrid);
		}
	}

	if (ctx.timelinePoint && !ctx.timelinePoint->empty())
	{
		// 竖向虚线所在日期与时间标签同规则交替变色（仅5/30分钟K线模式生效）
		// X坐标对齐柱中心（与K线/鼠标十字线一致）
		const auto& tp = *ctx.timelinePoint;
		const int totalPts = static_cast<int>(tp.size());
		const int numVLines = 6;
		const int slots = (ctx.xAxisPoints > 0) ? ctx.xAxisPoints : totalPts;
		const DayColorCtx dayCtx = CalcDayColorCtx(tp);
		CPen blueGridPen(PS_DOT, 1, COLOR_BLACK);
		for (int i = 0; i <= numVLines; i++)
		{
			int idx = totalPts * i / numVLines;
			if (idx >= totalPts) idx = totalPts - 1;
			int xPos2 = static_cast<int>(ctx.chartWidth / static_cast<float>(slots) * (static_cast<float>(idx) + 0.5f));
			memDC.SelectObject(IsBlueDayAt(tp, idx, dayCtx) ? &blueGridPen : &pGrid);
			memDC.MoveTo(max(0, xPos2), volumeY);
			memDC.LineTo(max(0, xPos2), volumeY + tmpCtx.volumeChartHeight);
		}
	}
	memDC.SelectObject(pOldVolPen);

	if (drawTimeLabels && !timelinePoint.empty())
	{
		// 标签X坐标对齐柱中心
		const DayColorCtx dayCtx = CalcDayColorCtx(timelinePoint);
		const int totalPts = static_cast<int>(timelinePoint.size());
		const int numVLines = 6;
		const int slots = (ctx.xAxisPoints > 0) ? ctx.xAxisPoints : totalPts;
		for (int i = 0; i <= numVLines; i++)
		{
			int idx = totalPts * i / numVLines;
			if (idx >= totalPts) idx = totalPts - 1;
			int xPos = max(0, static_cast<int>(ctx.chartWidth / static_cast<float>(slots) * (static_cast<float>(idx) + 0.5f)));
			CString timeLabel(timelinePoint[idx].time.c_str());
			if (timeLabel.GetLength() >= 5) timeLabel = timeLabel.Left(5);
			CSize labelSize = memDC.GetTextExtent(timeLabel);
			int labelX = max(0, min(xPos - labelSize.cx / 2, ctx.chartWidth - labelSize.cx));
			memDC.SetTextColor(IsBlueDayAt(timelinePoint, idx, dayCtx) ? COLOR_BLACK : COLOR_GRAY_TEXT);
			memDC.TextOut(labelX, tmpCtx.volumeChartTop + tmpCtx.volumeChartHeight + g_data.RDPI(2), timeLabel);
		}
	}
}

// ========== DrawMacdChartArea ==========

void CIndicatorChart::DrawMacdChartArea(CDC& memDC, const TimelineDrawContext& ctx, int areaTop, int areaHeight, const CString& macdTitleTip, const HoverState& hover)
{
	const auto& timelinePoint = *ctx.timelinePoint;
	int titleH = g_data.RDPI(16);
	int oldBkMode = memDC.SetBkMode(TRANSPARENT);

	CRect macdTitleRect(0, areaTop, ctx.chartWidth, areaTop + titleH);
	memDC.FillSolidRect(macdTitleRect, RGB(245, 245, 245));

	TimelineDrawContext tmpCtx = ctx;
	tmpCtx.macdChartTop = areaTop + titleH;
	tmpCtx.macdChartHeight = areaHeight - titleH;

	// 指标序列由 TechnicalIndicatorModel 在序列变化时计算并缓存，此处只读
	static const std::vector<MACDData> emptyMacd;
	const std::vector<MACDData>& macdData =
		ctx.indicatorModel ? ctx.indicatorModel->GetMACD() : emptyMacd;

	// 标题栏：DIF标签红色/DEA标签蓝色，数值正数红色/负数绿色
	{
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
		{
			// 从 "DIF:xxx DEA:xxx" 解析数值
			CString tip = hover.timelineMacdTitleTip;
			int difPos = tip.Find(_T("DIF:"));
			int deaPos = tip.Find(_T("DEA:"));
			if (difPos >= 0 && deaPos >= 0)
			{
				CString difStr = tip.Mid(difPos + 4, deaPos - difPos - 4);
				difStr.TrimRight();
				CString deaStr = tip.Mid(deaPos + 4);
				deaStr.TrimRight();
				difVal = _ttof(difStr);
				deaVal = _ttof(deaStr);
			}
		}
		else if (ctx.indicatorModel)
		{
			ctx.indicatorModel->GetLastMACD(difVal, deaVal);
		}

		drawLabel(_T("DIF:"), COLOR_DARK_ORANGE);
		drawValue(formatMACDValue(difVal), difVal);
		drawLabel(_T("DEA:"), COLOR_BLUE_AVG1);
		drawValue(formatMACDValue(deaVal), deaVal);

		// DIF趋势箭头：随悬停位置实时截断计算（lookback≤10，开销可忽略，保留每帧计算
		// 以保证悬停时箭头跟随鼠标位置变化；非悬停时基于最新数据）
		{
			// 悬停时取鼠标位置的数据计算趋势，非悬停时取最新数据
			int trendEndIdx = -1;
			if (!hover.timelineMacdTitleTip.IsEmpty() && hover.hoveredBarIndex >= 0)
			{
				// 悬停位置对应的全局索引
				trendEndIdx = ctx.startIndex + hover.hoveredBarIndex;
				if (trendEndIdx >= static_cast<int>(macdData.size()))
					trendEndIdx = static_cast<int>(macdData.size()) - 1;
			}
			else
			{
				// 取最后一个有效数据
				for (int i = static_cast<int>(macdData.size()) - 1; i >= 0; i--)
				{
					if (macdData[i].valid) { trendEndIdx = i; break; }
				}
			}

			if (trendEndIdx >= 0)
			{
				// 截取到悬停位置为止的子序列计算趋势
				std::vector<MACDData> subMacd(macdData.begin(), macdData.begin() + trendEndIdx + 1);
				// 1分钟用6根，5分钟用8根，30分钟和日K用10根
				int lookback = 10;
				if (hover.viewMode == UI_VIEW_MIN5_KLINE) lookback = 8;
				else if (hover.viewMode == UI_VIEW_TIMELINE) lookback = 6;
				auto trend = CStockIndicator::CalcDIFTrend(subMacd, lookback);
				if (trend.valid && trend.slope != 0.0)
				{
					bool isUp = trend.slope > 0;
					double refDif = std::abs(macdData[trendEndIdx].dif);
					double base = (refDif > 0.001) ? refDif : 0.001;
					double absSlope = std::abs(trend.slope);
					int arrowCount = 1;
					if (absSlope >= base * 0.3) arrowCount = 3;
					else if (absSlope >= base * 0.15) arrowCount = 2;

					COLORREF arrowColor = (macdData[trendEndIdx].dea >= 0) ? COLOR_RED_UP : COLOR_GREEN_DOWN;
					CString arrows;
					for (int a = 0; a < arrowCount; a++)
						arrows += isUp ? _T("↑") : _T("↓");
					memDC.SetTextColor(arrowColor);
					CSize arrowSize = memDC.GetTextExtent(arrows);
					memDC.TextOut(xPos, centerY - arrowSize.cy / 2, arrows);
				}
			}
		}
	}

	memDC.SetBkMode(oldBkMode);

	// 先绘制背景高亮，确保数据不被覆盖
	//CTimelineChart::DrawTimelineBackgroundHighlightsForArea(memDC, tmpCtx, tmpCtx.macdChartTop, tmpCtx.macdChartHeight);

	DrawMACDChart(memDC, 0, tmpCtx.macdChartTop, ctx.chartWidth, tmpCtx.macdChartHeight, timelinePoint, macdData, ctx.startIndex, -1, ctx.xAxisPoints, &ctx);

	CPen pGrid(PS_DOT, 1, COLOR_GRAY_GRID);
	CPen* pOldPen = memDC.SelectObject(&pGrid);
	int macdY = tmpCtx.macdChartTop;
	memDC.MoveTo(0, macdY);
	memDC.LineTo(ctx.chartWidth, macdY);
	memDC.MoveTo(0, macdY + tmpCtx.macdChartHeight);
	memDC.LineTo(ctx.chartWidth, macdY + tmpCtx.macdChartHeight);

	const int totalPts = static_cast<int>(timelinePoint.size());

	const int numVLines = 6;
	if (totalPts > 0)
	{
		// 竖向虚线所在日期与时间标签同规则交替变色（仅5/30分钟K线模式生效）
		// X坐标对齐柱中心（与K线/鼠标十字线一致）
		const int slots = (ctx.xAxisPoints > 0) ? ctx.xAxisPoints : totalPts;
		const DayColorCtx dayCtx = CalcDayColorCtx(timelinePoint);
		CPen blueGridPen(PS_DOT, 1, COLOR_BLACK);
		for (int i = 0; i <= numVLines; i++)
		{
			int idx = totalPts * i / numVLines;
			if (idx >= totalPts) idx = totalPts - 1;
			int xPos = max(0, static_cast<int>(ctx.chartWidth / static_cast<float>(slots) * (static_cast<float>(idx) + 0.5f)));
			memDC.SelectObject(IsBlueDayAt(timelinePoint, idx, dayCtx) ? &blueGridPen : &pGrid);
			memDC.MoveTo(xPos, macdY);
			memDC.LineTo(xPos, macdY + tmpCtx.macdChartHeight);
		}
	}
	memDC.SelectObject(pOldPen);
}

// ========== DrawIndicatorChartArea ==========

void CIndicatorChart::DrawIndicatorChartArea(CDC& memDC, const TimelineDrawContext& ctx, int areaTop, int areaHeight, bool drawTimeLabels, TimelineIndicator indicator, const HoverState& hover)
{
	const auto& timelinePoint = *ctx.timelinePoint;
	int titleH = g_data.RDPI(16);
	int oldBkMode = memDC.SetBkMode(TRANSPARENT);

	CRect indicatorTitleRect(0, areaTop, ctx.chartWidth, areaTop + titleH);
	memDC.FillSolidRect(indicatorTitleRect, RGB(245, 245, 245));

	if (indicator == TimelineIndicator::KDJ)
	{
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
		static const std::vector<KDJData> emptyKdjTitle;
		const auto& kdjData = ctx.indicatorModel ? ctx.indicatorModel->GetKDJ() : emptyKdjTitle;

		if (!hover.timelineKdjTitleTip.IsEmpty())
		{
			// 从 "K:xx.x D:xx.x J:xx.x" 解析数值
			CString tip = hover.timelineKdjTitleTip;
			int kPos = tip.Find(_T("K:"));
			int dPos = tip.Find(_T("D:"));
			int jPos = tip.Find(_T("J:"));
			if (kPos >= 0 && dPos >= 0 && jPos >= 0)
			{
				CString kStr = tip.Mid(kPos + 2, dPos - kPos - 2); kStr.TrimRight();
				CString dStr = tip.Mid(dPos + 2, jPos - dPos - 2); dStr.TrimRight();
				CString jStr = tip.Mid(jPos + 2); jStr.TrimRight();
				kVal = _ttof(kStr);
				dVal = _ttof(dStr);
				jVal = _ttof(jStr);
			}
		}
		else
		{
			ctx.indicatorModel->GetLastKDJ(kVal, dVal, jVal);
		}

		drawKDJLabel(_T("K:"), COLOR_RED_UP);
		CString kStr; kStr.Format(_T("%.1f"), kVal);
		drawKDJValue(kStr, kVal);
		drawKDJLabel(_T("D:"), RGB(0, 68, 204));
		CString dStr; dStr.Format(_T("%.1f"), dVal);
		drawKDJValue(dStr, dVal);
		drawKDJLabel(_T("J:"), RGB(0, 136, 34));
		CString jStr; jStr.Format(_T("%.1f"), jVal);
		drawKDJValue(jStr, jVal);

		// J值趋势箭头
		{
			int trendEndIdx = -1;
			if (!hover.timelineKdjTitleTip.IsEmpty() && hover.hoveredBarIndex >= 0)
			{
				trendEndIdx = ctx.startIndex + hover.hoveredBarIndex;
				if (trendEndIdx >= static_cast<int>(kdjData.size()))
					trendEndIdx = static_cast<int>(kdjData.size()) - 1;
			}
			else
			{
				for (int i = static_cast<int>(kdjData.size()) - 1; i >= 0; i--)
				{
					if (kdjData[i].valid) { trendEndIdx = i; break; }
				}
			}

			if (trendEndIdx >= 0)
			{
				std::vector<KDJData> subKdj(kdjData.begin(), kdjData.begin() + trendEndIdx + 1);
				// 1分钟用6根，5分钟用8根，30分钟和日K用10根
				int lookback = 10;
				if (hover.viewMode == UI_VIEW_MIN5_KLINE) lookback = 8;
				else if (hover.viewMode == UI_VIEW_TIMELINE) lookback = 6;
				auto trend = CStockIndicator::CalcKDJTrend(subKdj, lookback);
				if (trend.valid && trend.slope != 0.0)
				{
					bool isUp = trend.slope > 0;
					// K-D差值范围通常-20~20，阈值：1个箭头=slope>=0.5, 2个>=1.5, 3个>=3
					double absSlope = std::abs(trend.slope);
					int arrowCount = 1;
					if (absSlope >= 3) arrowCount = 3;
					else if (absSlope >= 1.5) arrowCount = 2;

					// K>=D用红色，K<D用绿色
					COLORREF arrowColor = (kdjData[trendEndIdx].k >= kdjData[trendEndIdx].d) ? COLOR_RED_UP : COLOR_GREEN_DOWN;
					CString arrows;
					for (int a = 0; a < arrowCount; a++)
						arrows += isUp ? _T("\u2191") : _T("\u2193");
					memDC.SetTextColor(arrowColor);
					CSize arrowSize = memDC.GetTextExtent(arrows);
					memDC.TextOut(xPos, centerY - arrowSize.cy / 2, arrows);
				}
			}
		}
	}
	else if (indicator == TimelineIndicator::WR)
	{
		int xPos = g_data.RDPI(4);
		int centerY = areaTop + titleH / 2;
		auto drawWRLabel = [&](const CString& label, const CString& value, COLORREF color) {
			memDC.SetTextColor(color);
			CString text = label + value;
			CSize ts = memDC.GetTextExtent(text);
			memDC.TextOut(xPos, centerY - ts.cy / 2, text);
			xPos += ts.cx + g_data.RDPI(4);
			};
		if (!hover.timelineWrTitleTip.IsEmpty())
		{
			CString tip = hover.timelineWrTitleTip;
			int pos = 0;
			CString token;
			COLORREF colors[] = { RGB(0, 68, 204), RGB(204, 34, 34) };
			int colorIdx = 0;
			while ((token = tip.Tokenize(_T(" "), pos)) != _T(""))
			{
				drawWRLabel(_T(""), token, colors[min(colorIdx, 1)]);
				colorIdx++;
			}
		}
		else
		{
			drawWRLabel(_T("WR6:"), _T(""), RGB(0, 68, 204));
			drawWRLabel(_T("WR14:"), _T(""), RGB(204, 34, 34));
		}
	}
	else if (indicator == TimelineIndicator::RSI)
	{
		int xPos = g_data.RDPI(4);
		int centerY = areaTop + titleH / 2;
		auto drawRSILabel = [&](const CString& label, const CString& value, COLORREF color) {
			memDC.SetTextColor(color);
			CString text = label + value;
			CSize ts = memDC.GetTextExtent(text);
			memDC.TextOut(xPos, centerY - ts.cy / 2, text);
			xPos += ts.cx + g_data.RDPI(4);
			};
		if (!hover.timelineRsiTitleTip.IsEmpty())
		{
			CString tip = hover.timelineRsiTitleTip;
			int pos = 0;
			CString token;
			COLORREF colors[] = { RGB(0, 68, 204), RGB(204, 34, 34) };
			int colorIdx = 0;
			while ((token = tip.Tokenize(_T(" "), pos)) != _T(""))
			{
				drawRSILabel(_T(""), token, colors[min(colorIdx, 1)]);
				colorIdx++;
			}
		}
		else
		{
			drawRSILabel(_T("RSI6:"), _T(""), RGB(0, 68, 204));
			drawRSILabel(_T("RSI14:"), _T(""), RGB(204, 34, 34));
		}
	}
	else
	{
		// 成交量标题栏（CJL模式）
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

	memDC.SetBkMode(oldBkMode);

	TimelineDrawContext tmpCtx = ctx;

	if (indicator == TimelineIndicator::KDJ)
	{
		tmpCtx.macdChartTop = areaTop + titleH;
		tmpCtx.macdChartHeight = areaHeight - titleH;
		//CTimelineChart::DrawTimelineBackgroundHighlightsForArea(memDC, tmpCtx, tmpCtx.macdChartTop, tmpCtx.macdChartHeight);
		DrawTimelineKDJSection(memDC, tmpCtx, hover);
	}
	else if (indicator == TimelineIndicator::WR)
	{
		tmpCtx.macdChartTop = areaTop + titleH;
		tmpCtx.macdChartHeight = areaHeight - titleH;
		//CTimelineChart::DrawTimelineBackgroundHighlightsForArea(memDC, tmpCtx, tmpCtx.macdChartTop, tmpCtx.macdChartHeight);
		DrawTimelineWRSection(memDC, tmpCtx, hover);
	}
	else if (indicator == TimelineIndicator::RSI)
	{
		tmpCtx.macdChartTop = areaTop + titleH;
		tmpCtx.macdChartHeight = areaHeight - titleH;
		//CTimelineChart::DrawTimelineBackgroundHighlightsForArea(memDC, tmpCtx, tmpCtx.macdChartTop, tmpCtx.macdChartHeight);
		DrawTimelineRSISection(memDC, tmpCtx, hover);
	}
	else
	{
		// 成交量模式（MACD枚举值现在表示成交量）
		tmpCtx.volumeChartTop = areaTop + titleH;
		tmpCtx.volumeChartHeight = areaHeight - titleH;
		//CTimelineChart::DrawTimelineBackgroundHighlightsForArea(memDC, tmpCtx, tmpCtx.volumeChartTop, tmpCtx.volumeChartHeight);
		DrawVolumeChart(memDC, 0, tmpCtx.volumeChartTop, ctx.chartWidth, tmpCtx.volumeChartHeight, *ctx.timelinePoint, &ctx.realtimeData, 0, -1, ctx.xAxisPoints);

		if (hover.viewMode == UI_VIEW_MIN5_KLINE && ctx.fullTimeline && !ctx.fullTimeline->empty() && ctx.timelinePoint && !ctx.timelinePoint->empty())
		{
			const auto& fullData = *ctx.fullTimeline;
			const auto& visibleData = *ctx.timelinePoint;
			const int totalPoints = static_cast<int>(visibleData.size());
			const int fullCount = static_cast<int>(fullData.size());
			const int startIndex = ctx.startIndex;
			int endIndex = min(fullCount, startIndex + totalPoints);

			STOCK::Volume maxVolume = 0;
			for (int i = startIndex; i < endIndex; i++)
				maxVolume = max(maxVolume, fullData[i].volume);

			if (maxVolume > 0)
			{
				// 成交量均线直接读模型缓存序列（fullTimeline等长），不再逐点重算
				static const std::vector<double> emptySeq;
				const auto& ma5 = ctx.indicatorModel ? ctx.indicatorModel->GetVolumeMA5() : emptySeq;
				const auto& ma10 = ctx.indicatorModel ? ctx.indicatorModel->GetVolumeMA10() : emptySeq;

				auto volumeToY = [&](double volume) -> int {
					int py = tmpCtx.volumeChartTop + tmpCtx.volumeChartHeight - static_cast<int>(volume / static_cast<double>(maxVolume) * tmpCtx.volumeChartHeight);
					return max(tmpCtx.volumeChartTop, min(py, tmpCtx.volumeChartTop + tmpCtx.volumeChartHeight));
					};

				auto drawVolumeMALine = [&](const std::vector<double>& values, COLORREF color) {
					CPen pen(PS_SOLID, 1, color);
					CPen* pOldPen = memDC.SelectObject(&pen);
					bool first = true;
					for (int i = 0; i < totalPoints; i++)
					{
						int globalIndex = startIndex + i;
						if (globalIndex >= static_cast<int>(values.size()) || values[globalIndex] <= 0) { first = true; continue; }
						int pointX = static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) * i) + static_cast<int>(ctx.chartWidth / static_cast<float>(totalPoints) / 2);
						int pointY = volumeToY(values[globalIndex]);
						if (first) { memDC.MoveTo(pointX, pointY); first = false; }
						else memDC.LineTo(pointX, pointY);
					}
					memDC.SelectObject(pOldPen);
					};

				drawVolumeMALine(ma5, RGB(0, 0, 230));
				drawVolumeMALine(ma10, RGB(0, 166, 235));
			}
		}

		// 成交量网格线
		CPen pGrid(PS_DOT, 1, COLOR_GRAY_GRID);
		CPen* pOldVolPen = memDC.SelectObject(&pGrid);
		int volumeY = tmpCtx.volumeChartTop;
		memDC.MoveTo(0, volumeY);
		memDC.LineTo(ctx.chartWidth, volumeY);

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
					int yPos = volumeY + tmpCtx.volumeChartHeight * i / 3;
					memDC.MoveTo(0, yPos);
					memDC.LineTo(ctx.chartWidth, yPos);
					STOCK::Volume volAtLine = maxVolume * (3 - i) / 3;
					STOCK::Volume volInLots = volAtLine / 100;
					CString volLabel = CCommon::FormatVolumeInt(volInLots);
					CSize labelSize = memDC.GetTextExtent(volLabel);
					memDC.TextOut(-labelSize.cx - g_data.RDPI(3), yPos - labelSize.cy / 2, volLabel);
				}
				memDC.SelectObject(&pGrid);
			}
		}

		// X轴时间竖线
		if (ctx.timelinePoint && !ctx.timelinePoint->empty())
		{
			// 竖向虚线所在日期与时间标签同规则交替变色（仅5/30分钟K线模式生效）
			const auto& tp = *ctx.timelinePoint;
			const int totalPts = static_cast<int>(tp.size());
			const int numVLines = 6;
			const int slots = (ctx.xAxisPoints > 0) ? ctx.xAxisPoints : totalPts;
			const DayColorCtx dayCtx = CalcDayColorCtx(tp);
			CPen blueGridPen(PS_DOT, 1, COLOR_BLACK);
			for (int i = 0; i <= numVLines; i++)
			{
				int idx = totalPts * i / numVLines;
				if (idx >= totalPts) idx = totalPts - 1;
				int xPos = max(0, static_cast<int>(ctx.chartWidth / static_cast<float>(slots) * (static_cast<float>(idx) + 0.5f)));
				memDC.SelectObject(IsBlueDayAt(tp, idx, dayCtx) ? &blueGridPen : &pGrid);
				memDC.MoveTo(xPos, volumeY);
				memDC.LineTo(xPos, volumeY + tmpCtx.volumeChartHeight);
			}
		}
		memDC.SelectObject(pOldVolPen);
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
	const int numVLines = 6;
	if (totalPts > 0)
	{
		// 竖向虚线所在日期与时间标签同规则交替变色（仅5/30分钟K线模式生效）
		// X坐标对齐柱中心（与K线/鼠标十字线一致）
		const int slots = (ctx.xAxisPoints > 0) ? ctx.xAxisPoints : totalPts;
		const DayColorCtx dayCtx = CalcDayColorCtx(timelinePoint);
		CPen blueGridPen(PS_DOT, 1, COLOR_BLACK);
		for (int i = 0; i <= numVLines; i++)
		{
			int idx = totalPts * i / numVLines;
			if (idx >= totalPts) idx = totalPts - 1;
			int xPos = max(0, static_cast<int>(ctx.chartWidth / static_cast<float>(slots) * (static_cast<float>(idx) + 0.5f)));
			bool isBlue = IsBlueDayAt(timelinePoint, idx, dayCtx);
			memDC.SelectObject(isBlue ? &blueGridPen : &pGrid);
			memDC.MoveTo(xPos, chartTop);
			memDC.LineTo(xPos, chartTop + chartHeight);
		}
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
	memDC.TextOut(markX - memDC.GetTextExtent(label).cx / 2, markY - memDC.GetTextExtent(label).cy / 2, label);
	memDC.SetBkMode(oldBkMode);
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

	auto valueToY = [&](double val) {
		double ratio = (val - minVal) / (maxVal - minVal);
		return y + padding + static_cast<int>((1.0 - ratio) * drawHeight);
		};

	CPen borderPen(PS_DASH, 1, COLOR_GRAY_GRID);
	CPen* pOldPen = memDC.SelectObject(&borderPen);
	double borderValues[] = { 0.0, 100.0 };
	for (double v : borderValues)
	{
		int gy = valueToY(v);
		memDC.MoveTo(x, gy);
		memDC.LineTo(x + width, gy);
	}
	CPen refPen(PS_DOT, 1, COLOR_GRAY_GRID);
	memDC.SelectObject(&refPen);
	double refValues[] = { 20.0, 50.0, 80.0 };
	for (double v : refValues)
	{
		int gy = valueToY(v);
		memDC.MoveTo(x, gy);
		memDC.LineTo(x + width, gy);
	}
	memDC.SelectObject(pOldPen);

	const int xSlots = (xAxisPoints > 0) ? xAxisPoints : static_cast<int>(timelinePoint.size());
	const int totalPts = static_cast<int>(timelinePoint.size());

	CPen kPen(PS_SOLID, 1, COLOR_RED_UP);
	memDC.SelectObject(&kPen);
	bool kFirst = true;
	for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(kdjData.size()); i++)
	{
		int idx = startIndex + i;
		if (!kdjData[idx].valid) continue;
		int pointX = x + static_cast<int>(width / static_cast<float>(xSlots) * i) + static_cast<int>(width / static_cast<float>(xSlots) / 2);
		int pointY = valueToY(kdjData[idx].k);
		if (kFirst) { memDC.MoveTo(pointX, pointY); kFirst = false; }
		else memDC.LineTo(pointX, pointY);
	}

	CPen dPen(PS_SOLID, 1, RGB(0, 68, 204));
	memDC.SelectObject(&dPen);
	bool dFirst = true;
	for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(kdjData.size()); i++)
	{
		int idx = startIndex + i;
		if (!kdjData[idx].valid) continue;
		int pointX = x + static_cast<int>(width / static_cast<float>(xSlots) * i) + static_cast<int>(width / static_cast<float>(xSlots) / 2);
		int pointY = valueToY(kdjData[idx].d);
		if (dFirst) { memDC.MoveTo(pointX, pointY); dFirst = false; }
		else memDC.LineTo(pointX, pointY);
	}

	CPen jPen(PS_SOLID, 1, RGB(0, 136, 34));
	memDC.SelectObject(&jPen);
	bool jFirst = true;
	for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(kdjData.size()); i++)
	{
		int idx = startIndex + i;
		if (!kdjData[idx].valid) continue;
		int pointX = x + static_cast<int>(width / static_cast<float>(xSlots) * i) + static_cast<int>(width / static_cast<float>(xSlots) / 2);
		int pointY = valueToY(kdjData[idx].j);
		if (jFirst) { memDC.MoveTo(pointX, pointY); jFirst = false; }
		else memDC.LineTo(pointX, pointY);
	}

	memDC.SelectObject(pOldPen);

	// KDJ金叉死叉信号：直接读模型缓存序列
	static const std::vector<MACDCrossSignal> emptyCross2;
	const auto& crossSignals = (modelCtx && modelCtx->indicatorModel)
		? modelCtx->indicatorModel->GetKDJCross() : emptyCross2;
	const int dotRadius = g_data.RDPI(3);
	const int smallDotRadius = g_data.RDPI(2);
	int oldBkMode = memDC.SetBkMode(TRANSPARENT);
	int lastDrawnDir = 0; // 0=无, 1=金叉, -1=死叉

	for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(crossSignals.size()); i++)
	{
		int idx = startIndex + i;
		if (crossSignals[idx] == MACDCrossSignal::None)
			continue;

		int markX = x + static_cast<int>(width / static_cast<float>(xSlots) * i) + static_cast<int>(width / static_cast<float>(xSlots) / 2);
		int markY = valueToY(kdjData[idx].k);

		bool isGolden = (crossSignals[idx] == MACDCrossSignal::GoldenCross || crossSignals[idx] == MACDCrossSignal::RepeatedGoldenCross);
		bool isRepeated = (crossSignals[idx] == MACDCrossSignal::RepeatedGoldenCross || crossSignals[idx] == MACDCrossSignal::RepeatedDeathCross);
		int curDir = isGolden ? 1 : -1;

		// 方向反转时始终绘制箭头；同方向重复则只画圆点
		bool drawArrow = !isRepeated || (curDir != lastDrawnDir);
		if (drawArrow)
		{
			DrawCrossArrow(memDC, markX, markY, dotRadius, isGolden);
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

// ========== DrawTimelineWRChart ==========

void CIndicatorChart::DrawTimelineWRChart(CDC& memDC, int x, int y, int width, int height, const std::vector<STOCK::TimelinePoint>& timelinePoint, const std::vector<WRData>& wrData, int startIndex /* = 0 */, int xAxisPoints /* = 0 */)
{
	if (timelinePoint.empty() || wrData.empty())
		return;

	double minVal = 0;
	double maxVal = 100;

	const int padding = g_data.RDPI(4);
	int drawHeight = height - padding * 2;
	if (drawHeight <= 0) return;

	auto valueToY = [&](double val) {
		double ratio = (val - minVal) / (maxVal - minVal);
		return y + padding + static_cast<int>((1.0 - ratio) * drawHeight);
		};

	CPen refPen(PS_DOT, 1, COLOR_GRAY_GRID);
	CPen* pOldPen = memDC.SelectObject(&refPen);
	double refValues[] = { 20.0, 50.0, 80.0 };
	for (double v : refValues)
	{
		int gy = valueToY(v);
		memDC.MoveTo(x, gy);
		memDC.LineTo(x + width, gy);
	}
	memDC.SelectObject(pOldPen);

	const int xSlots = (xAxisPoints > 0) ? xAxisPoints : static_cast<int>(timelinePoint.size());
	const int totalPts = static_cast<int>(timelinePoint.size());

	CPen wr1Pen(PS_SOLID, 1, RGB(0, 68, 204));
	memDC.SelectObject(&wr1Pen);
	bool wr1First = true;
	for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(wrData.size()); i++)
	{
		int idx = startIndex + i;
		if (!wrData[idx].valid) continue;
		int pointX = x + static_cast<int>(width / static_cast<float>(xSlots) * i) + static_cast<int>(width / static_cast<float>(xSlots) / 2);
		int pointY = valueToY(wrData[idx].wr1);
		if (wr1First) { memDC.MoveTo(pointX, pointY); wr1First = false; }
		else memDC.LineTo(pointX, pointY);
	}

	CPen wr2Pen(PS_SOLID, 1, RGB(204, 34, 34));
	memDC.SelectObject(&wr2Pen);
	bool wr2First = true;
	for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(wrData.size()); i++)
	{
		int idx = startIndex + i;
		if (!wrData[idx].valid) continue;
		int pointX = x + static_cast<int>(width / static_cast<float>(xSlots) * i) + static_cast<int>(width / static_cast<float>(xSlots) / 2);
		int pointY = valueToY(wrData[idx].wr2);
		if (wr2First) { memDC.MoveTo(pointX, pointY); wr2First = false; }
		else memDC.LineTo(pointX, pointY);
	}

	memDC.SelectObject(pOldPen);
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

	double minVal = 0;
	double maxVal = 100;

	const int padding = g_data.RDPI(4);
	int drawHeight = height - padding * 2;
	if (drawHeight <= 0) return;

	auto valueToY = [&](double val) {
		double ratio = (val - minVal) / (maxVal - minVal);
		return y + padding + static_cast<int>((1.0 - ratio) * drawHeight);
		};

	CPen refPen(PS_DOT, 1, COLOR_GRAY_GRID);
	CPen* pOldPen = memDC.SelectObject(&refPen);
	double refValues[] = { 30.0, 50.0, 70.0 };
	for (double v : refValues)
	{
		int gy = valueToY(v);
		memDC.MoveTo(x, gy);
		memDC.LineTo(x + width, gy);
	}
	memDC.SelectObject(pOldPen);

	const int xSlots = (xAxisPoints > 0) ? xAxisPoints : static_cast<int>(timelinePoint.size());
	const int totalPts = static_cast<int>(timelinePoint.size());

	CPen rsi1Pen(PS_SOLID, 1, RGB(0, 68, 204));
	memDC.SelectObject(&rsi1Pen);
	bool rsi1First = true;
	for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(rsiData.size()); i++)
	{
		int idx = startIndex + i;
		if (!rsiData[idx].valid) continue;
		int pointX = x + static_cast<int>(width / static_cast<float>(xSlots) * i) + static_cast<int>(width / static_cast<float>(xSlots) / 2);
		int pointY = valueToY(rsiData[idx].rsi1);
		if (rsi1First) { memDC.MoveTo(pointX, pointY); rsi1First = false; }
		else memDC.LineTo(pointX, pointY);
	}

	CPen rsi2Pen(PS_SOLID, 1, RGB(204, 34, 34));
	memDC.SelectObject(&rsi2Pen);
	bool rsi2First = true;
	for (int i = 0; i < totalPts && (startIndex + i) < static_cast<int>(rsiData.size()); i++)
	{
		int idx = startIndex + i;
		if (!rsiData[idx].valid) continue;
		int pointX = x + static_cast<int>(width / static_cast<float>(xSlots) * i) + static_cast<int>(width / static_cast<float>(xSlots) / 2);
		int pointY = valueToY(rsiData[idx].rsi2);
		if (rsi2First) { memDC.MoveTo(pointX, pointY); rsi2First = false; }
		else memDC.LineTo(pointX, pointY);
	}

	memDC.SelectObject(pOldPen);
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

// 绘制成交量图表
// 70% 以内的数据用开方缩放，70% 以上的数据用线性缩放，避免单根巨量柱子导致其他柱子过小而看不清

void CIndicatorChart::DrawVolumeChart(CDC& memDC, int x, int y, int width, int height,
	const std::vector<STOCK::TimelinePoint>& timelinePoint,
	const STOCK::StockInfo* stockInfo,
	int startIndex /* = 0 */, int visibleCount /* = -1 */, int xAxisPoints /* = 0 */)
{
	if (timelinePoint.empty())
		return;

	int total = static_cast<int>(timelinePoint.size());
	int endIdx = total;
	if (visibleCount > 0)
	{
		endIdx = (std::min)(total, startIndex + visibleCount);
		startIndex = (std::max)(0, (std::min)(startIndex, total - 1));
		if (endIdx <= startIndex) endIdx = startIndex + 1;
	}
	else
	{
		startIndex = 0;
	}

	std::vector<STOCK::Volume> sortVolumes;
	STOCK::Volume maxVolume = 0;
	for (int i = startIndex; i < endIdx; i++)
	{
		sortVolumes.push_back(timelinePoint[i].volume);
		if (timelinePoint[i].volume > maxVolume)
			maxVolume = timelinePoint[i].volume;
	}

	if (maxVolume == 0)
		return;
	std::sort(sortVolumes.begin(), sortVolumes.end());
	STOCK::Volume percentile_9 = sortVolumes[sortVolumes.size() * 0.7]; //取70%位置的成交量作为开方缩放的分界点

	// 获取分时成交量汇总数据
	auto curMode = g_data.GetCurrentViewMode();
	std::map<std::string, ::TickSummary>vecTickSummary;
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
	int normalHight = height * 0.7;

	for (int i = startIndex; i < endIdx; i++)
	{
		const auto& item = timelinePoint[i];
		auto& itemVol = item.volume;
		int barX = x + static_cast<int>(width / static_cast<float>(xSlots) * i) + halfSlot - barWidth / 2;
		int barHeight;
		if (itemVol < percentile_9)
		{
			double sqrtRatio = sqrt(static_cast<double>(itemVol) / percentile_9);
			barHeight = normalHight * sqrtRatio;
		}
		else
		{
			float ratio = static_cast<float>(item.volume) / maxVolume;
			barHeight = max(static_cast<int>(ratio * height), 1);
		}

		int barY = y + height - barHeight;

		// TODO : 这里的涨跌颜色判断逻辑可能需要根据实际需求调整，
		// 分时模式下柱子的颜色分成两部分，从vecTickSummary取当前时间下主动买入和主动卖出的成交量，主动买入为红色，主动卖出为绿色，
		// 可以直接将高度按比例分配给红色和绿色两部分，比例大的绘制在上方，比例小的绘制在下方，形成一个红绿分层的柱子。
		// K线模式下使用开盘价和收盘价判断涨跌
		bool bDrawDefault = true;
		if (!vecTickSummary.empty())
		{
			int buyHeght, sellHeght, netBuy;
			if (curMode == UI_VIEW_TIMELINE)
			{
				std::string strTime = item.time.substr(0, 5);
				std::string preTime = CCommon::subtractOneMinuteFast(strTime);
				if (vecTickSummary.find(preTime) != vecTickSummary.end())
				{
					auto& curTick = vecTickSummary[preTime];
					buyHeght = barHeight * curTick.BuyRatio();
					sellHeght = barHeight * curTick.SellRatio();
					netBuy = curTick.netBuy;
					bDrawDefault = false;
				}
			}
			else if (curMode == UI_VIEW_MIN5_KLINE || curMode == UI_VIEW_MIN30_KLINE)
			{
				if (item.fullTime.find(vecTickSummary.begin()->second.tradeDate) == 0)
				{
					int interval = (curMode == UI_VIEW_MIN5_KLINE) ? 5 : 30;
					std::string strTime = item.time.substr(0, 5);
					STOCK::TickSummary sumTick;
					auto itFind = vecTickSummary.find(strTime);
					if (itFind != vecTickSummary.end())
					{
						sumTick = {};
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
						netBuy = sumTick.buy - sumTick.sell;
					}

					buyHeght = barHeight * sumTick.BuyRatio();
					sellHeght = barHeight * sumTick.SellRatio();
					bDrawDefault = false;
				}
			}

			if (!bDrawDefault)
			{
				if (netBuy >= 0)
				{
					CRect buyRect(barX, barY, barX + barWidth, y + height - sellHeght);
					CBrush brush(COLOR_RED_UP);
					memDC.FillRect(buyRect, &brush);
					CRect sellRect(barX, barY + buyHeght, barX + barWidth, y + height);
					CBrush brush1(COLOR_GREEN_DOWN);
					memDC.FillRect(sellRect, &brush1);
				}
				else
				{
					CRect sellRect(barX, barY, barX + barWidth, y + height - buyHeght);
					CBrush brush(COLOR_GREEN_DOWN);
					memDC.FillRect(sellRect, &brush);

					CRect buyRect(barX, barY + sellHeght, barX + barWidth, y + height);
					CBrush brush1(COLOR_RED_UP);
					memDC.FillRect(buyRect, &brush1);
				}
			}
		}

		if (bDrawDefault)
		{
			COLORREF color = COLOR_GREEN_DOWN;
			// K线模式下openPrice>0，使用收盘价vs开盘价判断涨跌，与K线柱颜色一致
			if (item.openPrice > 0)
			{
				if (item.price >= item.openPrice)
					color = COLOR_RED_UP;
			}
			else if (i > 0)
			{
				if (item.price >= timelinePoint[i - 1].price)
					color = COLOR_RED_UP;
			}

			CBrush brush(color);
			memDC.FillRect(CRect(barX, barY, barX + barWidth, y + height), &brush);
		}
	}
}

// ========== DrawTimelineKDJChart ==========