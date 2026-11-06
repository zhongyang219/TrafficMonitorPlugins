#include "pch.h"
#include "StockIndicator.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <deque>

// ============================================================================
// CStockIndicator 实现：股票指标计算类
// ----------------------------------------------------------------------------
// 指标计算逻辑已统一迁移到 CSignalAnalyzer，本类仅保留：
//   - 滚动均价（MA5/MA10/MA20，依赖TimelinePoint的amount/volume，非标准指标）
//   - Y轴整齐刻度（Nice Number算法，纯UI辅助）
//   - K线周期高低点统计（UI标记用）
//   - 兼容旧调用的委托函数（内部调用CSignalAnalyzer统一实现）
// ============================================================================

// ========== 滚动均价计算 ==========

// 为每个分时数据点计算MA5/MA10/MA20滚动均价（滑动窗口）
void CStockIndicator::CalcAllRollingAvgPrices(std::vector<STOCK::TimelinePoint>& timelinePoint)
{
	int n = static_cast<int>(timelinePoint.size());
	if (n == 0)
		return;

	// 计算每个窗口的滚动均价，使用滑动窗口避免重复求和
	auto calcWindow = [&](int windowSize, int fieldOffset) {
		STOCK::Amount sumAmount = 0;
		STOCK::Volume sumVolume = 0;
		for (int i = 0; i < n; i++)
		{
			sumAmount += timelinePoint[i].amount;
			sumVolume += timelinePoint[i].volume;
			if (i >= windowSize)
			{
				sumAmount -= timelinePoint[i - windowSize].amount;
				sumVolume -= timelinePoint[i - windowSize].volume;
			}
			if (sumVolume > 0)
			{
				STOCK::Price maVal = sumAmount / sumVolume;
				switch (fieldOffset)
				{
				case 5: timelinePoint[i].ma5 = maVal; break;
				case 10: timelinePoint[i].ma10 = maVal; break;
				case 20: timelinePoint[i].ma20 = maVal; break;
				}
			}
		}
	};

	calcWindow(5, 5);
	calcWindow(10, 10);
	calcWindow(20, 20);
}

// ========== Y轴整齐刻度计算（Nice Number算法） ==========

double CStockIndicator::CalcNiceStep(double range, double divCount, double minStep)
{
	if (range <= 0) range = minStep;
	double rawStep = range / divCount;
	if (rawStep <= 0) rawStep = minStep;
	double mag = pow(10.0, floor(log10(rawStep)));
	double norm = rawStep / mag;
	// norm ∈ [1,10)，映射到1~10的整数步长，平滑过渡
	static const double thresholds[] = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0 };
	double niceNorm = 10.0;
	for (int i = 0; i < _countof(thresholds); i++)
	{
		if (norm <= thresholds[i] + 1e-9)
		{
			niceNorm = thresholds[i];
			break;
		}
	}
	return (std::max)(niceNorm * mag, minStep);
}

void CStockIndicator::CalcNiceAxisRange(double visMin, double visMax, double divCount, double minStep, double& outAxisMin, double& outAxisMax, double& outNiceStep)
{
	double range = visMax - visMin;
	double niceStep = CalcNiceStep(range, divCount, minStep);

	// 当niceStep被minStep强制提升后，divCount * niceStep可能远大于数据范围，
	// 导致Y轴空间浪费、辅助线（如布林带）被挤到可视区外。
	// 动态缩减divCount，使总范围更紧凑地包裹数据。
	double actualDivCount = divCount;
	if (niceStep > range / divCount + 1e-12)
	{
		double neededDivs = ceil((range + niceStep * 1e-9) / niceStep);
		// 至少保留2个等分（3根刻度线），且不超过原始divCount
		actualDivCount = (std::max)(2.0, (std::min)(neededDivs, divCount));
	}

	// 对齐axisMin到niceStep的整数倍
	double axisMin = floor((visMin + niceStep * 1e-9) / niceStep) * niceStep;
	// 如果axisMin刚好贴着visMin（数据贴边），向下多留一个niceStep边距，
	// 避免辅助线（如布林带下轨）超出可视区
	if (fabs(axisMin - visMin) < niceStep * 1e-6)
		axisMin -= niceStep;
	double axisMax = axisMin + actualDivCount * niceStep;

	// 如果axisRange不够包含所有数据，增加等分数
	while (axisMax < visMax - 1e-9)
	{
		axisMax += niceStep;
		actualDivCount += 1.0;
	}
	// 如果axisMax刚好贴着visMax（数据贴边），向上多留一个niceStep边距，
	// 避免辅助线（如布林带上轨）超出可视区
	if (fabs(axisMax - visMax) < niceStep * 1e-6)
	{
		axisMax += niceStep;
		actualDivCount += 1.0;
	}

	// 三位小数精度截断（与显示格式一致）
	axisMin = round(axisMin * 1000.0) / 1000.0;
	axisMax = round(axisMax * 1000.0) / 1000.0;

	// 股价非负约束
	if (axisMin < 0)
	{
		axisMin = 0;
		axisMax = round((axisMin + ceil((visMax + niceStep * 1e-9) / niceStep) * niceStep) * 1000.0) / 1000.0;
	}

	outAxisMin = axisMin;
	outAxisMax = axisMax;
	outNiceStep = niceStep;
}

void CStockIndicator::CalcNiceAxisRangeSymmetric(double visMin, double visMax, double divCount, double minStep, double& outMin, double& outMax, double& outNiceStep)
{
	double range = visMax - visMin;
	double niceStep = CalcNiceStep(range, divCount, minStep);

	// 当niceStep被minStep强制提升后，动态缩减divCount，避免Y轴范围过度扩展
	double actualDivCount = divCount;
	if (niceStep > range / divCount + 1e-12)
	{
		double neededDivs = ceil((range + niceStep * 1e-9) / niceStep);
		// 对称模式需要偶数等分，至少2个
		if (fmod(neededDivs, 2.0) > 1e-9)
			neededDivs += 1.0;
		actualDivCount = (std::max)(2.0, (std::min)(neededDivs, divCount));
	}

	double centerPrice = (visMin + visMax) / 2.0;
	double halfAxisRange = (actualDivCount / 2.0) * niceStep;
	outMin = centerPrice - halfAxisRange;
	outMax = centerPrice + halfAxisRange;
	outNiceStep = niceStep;
}

// ========== MACD指标计算（委托CSignalAnalyzer） ==========

std::vector<CStockIndicator::MACDData> CStockIndicator::CalculateTimelineMACD(const std::vector<STOCK::TimelinePoint>& timelinePoint,
	int shortPeriod, int longPeriod, int signalPeriod)
{
	std::vector<MACDData> result;
	int n = static_cast<int>(timelinePoint.size());
	if (n == 0)
		return result;

	// 提取收盘价
	std::vector<double> closes(n);
	for (int i = 0; i < n; i++)
		closes[i] = timelinePoint[i].price;

	// 委托统一计算
	std::vector<double> difSeq, deaSeq, barSeq;
	CSignalAnalyzer::CalcMACDSeriesFromCloses(closes, difSeq, deaSeq, barSeq, shortPeriod, longPeriod, signalPeriod);

	result.resize(n);
	for (int i = 0; i < n; i++)
	{
		result[i].dif = difSeq[i];
		result[i].dea = deaSeq[i];
		result[i].bar = barSeq[i];
		result[i].valid = true;
	}
	return result;
}

std::vector<CStockIndicator::MACDCrossSignal> CStockIndicator::DetectMACDCross(const std::vector<MACDData>& macdData)
{
	// 提取序列并委托统一交叉检测
	std::vector<double> difSeq, deaSeq;
	std::vector<bool> validFlags;
	difSeq.reserve(macdData.size());
	deaSeq.reserve(macdData.size());
	validFlags.reserve(macdData.size());
	for (const auto& d : macdData)
	{
		difSeq.push_back(d.dif);
		deaSeq.push_back(d.dea);
		validFlags.push_back(d.valid);
	}

	auto crossSignals = CSignalAnalyzer::DetectMACDCross(difSeq, deaSeq, validFlags);

	// 转换枚举类型
	std::vector<MACDCrossSignal> result(macdData.size(), MACDCrossSignal::None);
	for (size_t i = 0; i < crossSignals.size() && i < result.size(); i++)
	{
		switch (crossSignals[i])
		{
		case CSignalAnalyzer::CrossSignal::GoldenCross: result[i] = MACDCrossSignal::GoldenCross; break;
		case CSignalAnalyzer::CrossSignal::DeathCross: result[i] = MACDCrossSignal::DeathCross; break;
		case CSignalAnalyzer::CrossSignal::RepeatedGoldenCross: result[i] = MACDCrossSignal::RepeatedGoldenCross; break;
		case CSignalAnalyzer::CrossSignal::RepeatedDeathCross: result[i] = MACDCrossSignal::RepeatedDeathCross; break;
		default: break;
		}
	}
	return result;
}

RegResult CStockIndicator::CalcDIFTrend(const std::vector<MACDData>& macdData, int lookback)
{
	// 提取DIF值并委托统一线性回归
	std::vector<double> difValues;
	difValues.reserve(macdData.size());
	for (const auto& d : macdData)
	{
		if (d.valid)
			difValues.push_back(d.dif);
	}
	// DIF值可能很小，需要autoScale
	return CSignalAnalyzer::CalcLinearReg(difValues, lookback, true);
}

std::vector<CStockIndicator::MACDCrossSignal> CStockIndicator::DetectKDJCross(const std::vector<KDJData>& kdjData)
{
	// 提取序列并委托统一交叉检测
	std::vector<double> kSeq, dSeq;
	std::vector<bool> validFlags;
	kSeq.reserve(kdjData.size());
	dSeq.reserve(kdjData.size());
	validFlags.reserve(kdjData.size());
	for (const auto& d : kdjData)
	{
		kSeq.push_back(d.k);
		dSeq.push_back(d.d);
		validFlags.push_back(d.valid);
	}

	auto crossSignals = CSignalAnalyzer::DetectKDJCross(kSeq, dSeq, validFlags);

	// 转换枚举类型
	std::vector<MACDCrossSignal> result(kdjData.size(), MACDCrossSignal::None);
	for (size_t i = 0; i < crossSignals.size() && i < result.size(); i++)
	{
		switch (crossSignals[i])
		{
		case CSignalAnalyzer::CrossSignal::GoldenCross: result[i] = MACDCrossSignal::GoldenCross; break;
		case CSignalAnalyzer::CrossSignal::DeathCross: result[i] = MACDCrossSignal::DeathCross; break;
		case CSignalAnalyzer::CrossSignal::RepeatedGoldenCross: result[i] = MACDCrossSignal::RepeatedGoldenCross; break;
		case CSignalAnalyzer::CrossSignal::RepeatedDeathCross: result[i] = MACDCrossSignal::RepeatedDeathCross; break;
		default: break;
		}
	}
	return result;
}

// ========== KDJ指标计算（委托CSignalAnalyzer） ==========

std::vector<CStockIndicator::KDJData> CStockIndicator::CalculateKDJ(const std::vector<STOCK::KLinePoint>& klineData, int n, int m1, int m2)
{
	std::vector<KDJData> result;
	int size = static_cast<int>(klineData.size());
	if (size == 0 || n <= 0 || m1 <= 0 || m2 <= 0)
		return result;

	// 提取HLC序列
	std::vector<double> highs(size), lows(size), closes(size);
	for (int i = 0; i < size; i++)
	{
		highs[i] = klineData[i].high;
		lows[i] = klineData[i].low;
		closes[i] = klineData[i].close;
	}

	// 委托统一计算
	std::vector<double> kSeq, dSeq, jSeq;
	CSignalAnalyzer::CalcKDJSeriesFromHLC(highs, lows, closes, n, kSeq, dSeq, jSeq, m1, m2);

	result.resize(size);
	for (int i = 0; i < size; i++)
	{
		result[i].k = kSeq[i];
		result[i].d = dSeq[i];
		result[i].j = jSeq[i];
		result[i].valid = (i >= n - 1);
	}
	return result;
}

std::vector<CStockIndicator::KDJData> CStockIndicator::CalculateTimelineKDJ(const std::vector<STOCK::TimelinePoint>& timelinePoint, int n, int m1, int m2)
{
	std::vector<KDJData> result;
	int size = static_cast<int>(timelinePoint.size());
	if (size == 0 || n <= 0 || m1 <= 0 || m2 <= 0)
		return result;

	// 分时数据：high=low=close=price
	std::vector<double> highs(size), lows(size), closes(size);
	for (int i = 0; i < size; i++)
	{
		highs[i] = timelinePoint[i].price;
		lows[i] = timelinePoint[i].price;
		closes[i] = timelinePoint[i].price;
	}

	// 委托统一计算
	std::vector<double> kSeq, dSeq, jSeq;
	CSignalAnalyzer::CalcKDJSeriesFromHLC(highs, lows, closes, n, kSeq, dSeq, jSeq, m1, m2);

	result.resize(size);
	for (int i = 0; i < size; i++)
	{
		result[i].k = kSeq[i];
		result[i].d = dSeq[i];
		result[i].j = jSeq[i];
		result[i].valid = (i >= n - 1);
	}
	return result;
}

RegResult CStockIndicator::CalcKDJTrend(const std::vector<KDJData>& kdjData, int lookback)
{
	// 提取K值并委托统一线性回归
	std::vector<double> kValues;
	kValues.reserve(kdjData.size());
	for (const auto& d : kdjData)
	{
		if (d.valid)
			kValues.push_back(d.k);
	}
	// KDJ值范围0~100，不需要autoScale
	return CSignalAnalyzer::CalcLinearReg(kValues, lookback, false);
}

// ========== W&R威廉指标计算（委托CSignalAnalyzer） ==========

std::vector<CStockIndicator::WRData> CStockIndicator::CalculateTimelineWR(const std::vector<STOCK::TimelinePoint>& timelinePoint, int period1, int period2)
{
	std::vector<WRData> result;
	int size = static_cast<int>(timelinePoint.size());
	if (size == 0 || period1 <= 0 || period2 <= 0)
		return result;

	// 分时数据：high=low=close=price
	std::vector<double> highs(size), lows(size), closes(size);
	for (int i = 0; i < size; i++)
	{
		highs[i] = timelinePoint[i].price;
		lows[i] = timelinePoint[i].price;
		closes[i] = timelinePoint[i].price;
	}

	// 委托统一计算
	auto wrPoints = CSignalAnalyzer::CalcWRSeriesFromHLC(highs, lows, closes, period1, period2);

	result.resize(wrPoints.size());
	for (size_t i = 0; i < wrPoints.size(); i++)
	{
		result[i].wr1 = wrPoints[i].wr1;
		result[i].wr2 = wrPoints[i].wr2;
		result[i].valid = wrPoints[i].valid;
	}
	return result;
}

std::vector<CStockIndicator::WRData> CStockIndicator::CalculateKLineWR(const std::vector<STOCK::KLinePoint>& klineData, int period1, int period2)
{
	std::vector<WRData> result;
	int size = static_cast<int>(klineData.size());
	if (size == 0 || period1 <= 0 || period2 <= 0)
		return result;

	// 提取HLC序列
	std::vector<double> highs(size), lows(size), closes(size);
	for (int i = 0; i < size; i++)
	{
		highs[i] = klineData[i].high;
		lows[i] = klineData[i].low;
		closes[i] = klineData[i].close;
	}

	// 委托统一计算
	auto wrPoints = CSignalAnalyzer::CalcWRSeriesFromHLC(highs, lows, closes, period1, period2);

	result.resize(wrPoints.size());
	for (size_t i = 0; i < wrPoints.size(); i++)
	{
		result[i].wr1 = wrPoints[i].wr1;
		result[i].wr2 = wrPoints[i].wr2;
		result[i].valid = wrPoints[i].valid;
	}
	return result;
}

// ========== RSI相对强弱指标计算（委托CSignalAnalyzer） ==========

std::vector<CStockIndicator::RSIData> CStockIndicator::CalculateTimelineRSI(const std::vector<STOCK::TimelinePoint>& timelinePoint, int period1, int period2)
{
	std::vector<RSIData> result;
	int size = static_cast<int>(timelinePoint.size());
	if (size < 2 || period1 <= 1 || period2 <= 1)
		return result;

	// 提取收盘价
	std::vector<double> closes(size);
	for (int i = 0; i < size; i++)
		closes[i] = timelinePoint[i].price;

	// 委托统一计算（使用SMA模式，与原实现一致）
	auto rsi1Values = CSignalAnalyzer::CalcRSISeriesFromCloses(closes, period1, false);
	auto rsi2Values = CSignalAnalyzer::CalcRSISeriesFromCloses(closes, period2, false);

	int maxPeriod = (std::max)(period1, period2);
	result.reserve(size);
	for (int i = 0; i < size; i++)
	{
		RSIData rd;
		rd.rsi1 = rsi1Values[i];
		rd.rsi2 = rsi2Values[i];
		rd.valid = (i >= maxPeriod);
		result.push_back(rd);
	}
	return result;
}

std::vector<CStockIndicator::RSIData> CStockIndicator::CalculateKLineRSI(const std::vector<STOCK::KLinePoint>& klineData, int period1, int period2)
{
	std::vector<RSIData> result;
	int size = static_cast<int>(klineData.size());
	if (size < 2 || period1 <= 1 || period2 <= 1)
		return result;

	// 提取收盘价
	std::vector<double> closes(size);
	for (int i = 0; i < size; i++)
		closes[i] = klineData[i].close;

	// 委托统一计算（使用SMA模式，与原实现一致）
	auto rsi1Values = CSignalAnalyzer::CalcRSISeriesFromCloses(closes, period1, false);
	auto rsi2Values = CSignalAnalyzer::CalcRSISeriesFromCloses(closes, period2, false);

	int maxPeriod = (std::max)(period1, period2);
	result.reserve(size);
	for (int i = 0; i < size; i++)
	{
		RSIData rd;
		rd.rsi1 = rsi1Values[i];
		rd.rsi2 = rsi2Values[i];
		rd.valid = (i >= maxPeriod);
		result.push_back(rd);
	}
	return result;
}
