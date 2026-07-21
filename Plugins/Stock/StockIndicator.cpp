#include "pch.h"
#include "StockIndicator.h"
#include <cmath>
#include <algorithm>
#include <limits>

// ============================================================================
// CStockIndicator 实现：所有股票指标计算逻辑（从CFloatingWnd抽取）
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

// 计算滚动均价：最近nMinutes分钟的成交额/成交量（单次查询）
STOCK::Price CStockIndicator::CalcRollingAvgPrice(const std::vector<STOCK::TimelinePoint>& timelinePoint, int nMinutes)
{
	int n = static_cast<int>(timelinePoint.size());
	if (n == 0 || nMinutes <= 0)
		return 0;

	int startIdx = max(0, n - nMinutes);
	STOCK::Amount sumAmount = 0;
	STOCK::Volume sumVolume = 0;
	for (int i = startIdx; i < n; i++)
	{
		sumAmount += timelinePoint[i].amount;
		sumVolume += timelinePoint[i].volume;
	}
	if (sumVolume == 0)
		return 0;
	return sumAmount / sumVolume;
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

	// 先对齐axisMin到niceStep的整数倍，确保数据完全包含在轴范围内
	double axisMin = floor((visMin + niceStep * 1e-9) / niceStep) * niceStep;
	double axisMax = axisMin + divCount * niceStep;

	// 如果axisRange不够包含所有数据，增加等分数
	while (axisMax < visMax - 1e-9)
	{
		axisMax += niceStep;
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

	double centerPrice = (visMin + visMax) / 2.0;
	double halfAxisRange = (divCount / 2.0) * niceStep;
	outMin = centerPrice - halfAxisRange;
	outMax = centerPrice + halfAxisRange;
	outNiceStep = niceStep;
}

// ========== MACD指标计算 ==========

std::vector<CStockIndicator::MACDData> CStockIndicator::CalculateTimelineMACD(const std::vector<STOCK::TimelinePoint>& timelinePoint)
{
	std::vector<MACDData> result;
	int n = static_cast<int>(timelinePoint.size());
	if (n == 0)
		return result;

	result.resize(n);

	// 提取收盘价（分时价格）
	std::vector<double> closes(n);
	for (int i = 0; i < n; i++)
		closes[i] = timelinePoint[i].price;

	// 通达信初始化方式：第一根收盘价作为EMA初始值，提前生成指标
	// 随着数据量增加，数值逐步收敛到标准值

	// 计算EMA12：第一根用收盘价初始化，后续迭代
	std::vector<double> ema12(n);
	ema12[0] = closes[0];
	for (int i = 1; i < n; i++)
		ema12[i] = closes[i] * 2.0 / 13.0 + ema12[i - 1] * 11.0 / 13.0;

	// 计算EMA26：第一根用收盘价初始化，后续迭代
	std::vector<double> ema26(n);
	ema26[0] = closes[0];
	for (int i = 1; i < n; i++)
		ema26[i] = closes[i] * 2.0 / 27.0 + ema26[i - 1] * 25.0 / 27.0;

	// 计算DIF = EMA12 - EMA26
	std::vector<double> dif(n);
	for (int i = 0; i < n; i++)
		dif[i] = ema12[i] - ema26[i];

	// 计算DEA（DIF的9周期EMA）：第一根DIF作为初始值，后续迭代
	std::vector<double> dea(n);
	dea[0] = dif[0];
	for (int i = 1; i < n; i++)
		dea[i] = dif[i] * 2.0 / 11.0 + dea[i - 1] * 9.0 / 11.0;

	// 计算MACD柱 = (DIF - DEA) * 2，所有数据点均有效
	for (int i = 0; i < n; i++)
	{
		result[i].dif = dif[i];
		result[i].dea = dea[i];
		result[i].bar = (dif[i] - dea[i]) * 2.0;
		result[i].valid = true;
	}

	return result;
}

std::vector<CStockIndicator::MACDData> CStockIndicator::CalculateKLineMACD(const std::vector<STOCK::KLinePoint>& klineData)
{
	std::vector<MACDData> result;
	int n = static_cast<int>(klineData.size());
	if (n == 0)
		return result;

	result.resize(n);

	// 提取收盘价
	std::vector<double> closes(n);
	for (int i = 0; i < n; i++)
		closes[i] = klineData[i].close;

	// 计算EMA12
	std::vector<double> ema12(n);
	ema12[0] = closes[0];
	for (int i = 1; i < n; i++)
		ema12[i] = closes[i] * 2.0 / 13.0 + ema12[i - 1] * 11.0 / 13.0;

	// 计算EMA26
	std::vector<double> ema26(n);
	ema26[0] = closes[0];
	for (int i = 1; i < n; i++)
		ema26[i] = closes[i] * 2.0 / 27.0 + ema26[i - 1] * 25.0 / 27.0;

	// 计算DIF = EMA12 - EMA26
	std::vector<double> dif(n);
	for (int i = 0; i < n; i++)
		dif[i] = ema12[i] - ema26[i];

	// 计算DEA（DIF的9周期EMA）
	std::vector<double> dea(n);
	dea[0] = dif[0];
	for (int i = 1; i < n; i++)
		dea[i] = dif[i] * 2.0 / 11.0 + dea[i - 1] * 9.0 / 11.0;

	// 计算MACD柱 = (DIF - DEA) * 2
	for (int i = 0; i < n; i++)
	{
		result[i].dif = dif[i];
		result[i].dea = dea[i];
		result[i].bar = (dif[i] - dea[i]) * 2.0;
		result[i].valid = true;
	}

	return result;
}

std::vector<CStockIndicator::MACDCrossSignal> CStockIndicator::DetectMACDCross(const std::vector<MACDData>& macdData)
{
	std::vector<MACDCrossSignal> signals(macdData.size(), MACDCrossSignal::None);
	if (macdData.size() < 2)
		return signals;

	const double epsilon = 1e-6; // 粘合阈值：差值小于此值视为未交叉
	const int MIN_CROSS_GAP = 10; // 两次交叉之间最少间隔10根K线，防止0轴附近频繁缠绕
	int lastCrossIdx = -MIN_CROSS_GAP; // 上次交叉位置

	for (size_t i = 1; i < macdData.size(); i++)
	{
		if (!macdData[i].valid || !macdData[i - 1].valid)
			continue;

		double difPrev = macdData[i - 1].dif;
		double deaPrev = macdData[i - 1].dea;
		double difNow = macdData[i].dif;
		double deaNow = macdData[i].dea;

		// 粘合状态：当前DIF与DEA差值过小，不触发新叉
		if (std::abs(difNow - deaNow) < epsilon)
			continue;

		// 金叉：上一时刻DIF < DEA，当前时刻DIF > DEA
		if (difPrev < deaPrev && difNow > deaNow)
		{
			if (static_cast<int>(i) - lastCrossIdx >= MIN_CROSS_GAP)
			{
				signals[i] = MACDCrossSignal::GoldenCross;
				lastCrossIdx = static_cast<int>(i);
			}
		}
		// 死叉：上一时刻DIF > DEA，当前时刻DIF < DEA
		else if (difPrev > deaPrev && difNow < deaNow)
		{
			if (static_cast<int>(i) - lastCrossIdx >= MIN_CROSS_GAP)
			{
				signals[i] = MACDCrossSignal::DeathCross;
				lastCrossIdx = static_cast<int>(i);
			}
		}
	}

	return signals;
}

CStockIndicator::MACDCrossSignal CStockIndicator::GetLatestMACDCross(const std::vector<MACDData>& macdData)
{
	auto signals = DetectMACDCross(macdData);
	for (int i = static_cast<int>(signals.size()) - 1; i >= 0; i--)
	{
		if (signals[i] != MACDCrossSignal::None)
			return signals[i];
	}
	return MACDCrossSignal::None;
}

// ========== T+0日内买卖点判定 ==========

CSignalAnalyzer::T0Signal CStockIndicator::DetectBuySignal(const std::vector<STOCK::TimelinePoint>& timelinePoint, const std::vector<MACDData>& macdData)
{
	CSignalAnalyzer::T0Signal signal;
	signal.valid = false;
	signal.isBuy = true;
	signal.strength = 0;
	signal.price = 0;

	int n = static_cast<int>(timelinePoint.size());
	int m = static_cast<int>(macdData.size());
	if (n < 10 || m < 10)
		return signal;

	int last = n - 1;
	signal.price = timelinePoint[last].price;
	signal.time = CString(timelinePoint[last].time.c_str());

	// ========== 量能基准：前三根成交量均值 ==========
	long long volAvg3 = 0;
	{
		int volCount = min(3, last);
		for (int i = last - volCount; i < last; i++)
			volAvg3 += timelinePoint[i].volume;
		if (volCount > 0) volAvg3 /= volCount;
	}
	bool isShrinking = (volAvg3 > 0 && timelinePoint[last].volume < volAvg3);           // 缩量：当前量 < 前三根均值
	bool isExpanding = (volAvg3 > 0 && timelinePoint[last].volume > volAvg3 * 3 / 2);    // 放量：当前量 > 前三根均值的1.5倍

	int strength = 0;
	CString reasons;

	// ========== 避坑规则：放量大跌不抄底 ==========
	{
		int lookback = min(5, n - 1);
		bool heavyDecline = true;
		for (int i = last - lookback + 1; i <= last; i++)
		{
			if (timelinePoint[i].price >= timelinePoint[i - 1].price)
			{
				heavyDecline = false;
				break;
			}
		}
		// 持续下跌且量能递增 = 放量杀跌
		if (heavyDecline)
		{
			bool volIncreasing = true;
			for (int i = last - lookback + 2; i <= last; i++)
			{
				if (timelinePoint[i].volume < timelinePoint[i - 1].volume)
				{
					volIncreasing = false;
					break;
				}
			}
			if (volIncreasing)
				return signal; // 放量杀跌，坚决不抄底
		}
	}

	// ========== 避坑规则：无量阴跌不急于接盘 ==========
	{
		int lookback = min(10, n - 1);
		bool steadyDecline = true;
		for (int i = last - lookback + 1; i <= last; i++)
		{
			if (timelinePoint[i].price >= timelinePoint[i - 1].price)
			{
				steadyDecline = false;
				break;
			}
		}
		bool lowVolume = isShrinking;
		bool macdStillDown = macdData[last].valid && macdData[last].dif < 0 && macdData[last].bar < 0;
		if (steadyDecline && lowVolume && macdStillDown)
			return signal; // 无量阴跌，MACD仍下行
	}

	// ========== 1. 量柱信号：缩量下跌 + 首根红量柱（需量能确认） ==========
	{
		int lookback = min(10, n - 1);
		int greenCount = 0;
		bool shrinking = true;
		long long prevGreenVol = 0;

		for (int i = last - lookback + 1; i <= last; i++)
		{
			bool isGreen = (timelinePoint[i].price < timelinePoint[i - 1].price);
			if (isGreen)
			{
				greenCount++;
				if (prevGreenVol > 0 && timelinePoint[i].volume > prevGreenVol)
					shrinking = false;
				prevGreenVol = timelinePoint[i].volume;
			}
		}

		// 最新一根是否为红柱（上涨），且之前有连续下跌
		bool firstRed = (timelinePoint[last].price > timelinePoint[last - 1].price) && greenCount >= 3;

		if (shrinking && greenCount >= 3 && isShrinking)
		{
			strength++;
			reasons += _T("缩量下跌抛压耗尽 ");
		}
		// 首根红量柱需要放量确认，否则可能是假反弹
		if (firstRed && isExpanding)
		{
			strength++;
			reasons += _T("放量红柱资金承接 ");
		}
	}

	// ========== 2. MACD信号 ==========
	if (macdData[last].valid)
	{
		// 绿柱缩短
		if (macdData[last].bar < 0)
		{
			int shrkCount = 0;
			for (int i = last; i > max(0, last - 5); i--)
			{
				if (macdData[i].valid && macdData[i].bar < 0 &&
					std::abs(macdData[i].bar) <= std::abs(macdData[i - 1].bar))
					shrkCount++;
			}
			if (shrkCount >= 3)
			{
				strength++;
				reasons += _T("MACD绿柱缩短 ");
			}
		}

		// 底背离检测：价格创新低，MACD未创新低
		int lookback = min(30, n - 1);
		int low1Idx = -1, low2Idx = -1;
		for (int i = last - lookback + 1; i < last; i++)
		{
			if (i > 0 && i < n - 1 &&
				timelinePoint[i].price < timelinePoint[i - 1].price &&
				timelinePoint[i].price < timelinePoint[i + 1].price)
			{
				if (low1Idx < 0)
					low1Idx = i;
				else if (i > low1Idx + 3 && low2Idx < 0)
					low2Idx = i;
			}
		}
		if (low1Idx >= 0 && low2Idx >= 0 &&
			timelinePoint[low2Idx].price < timelinePoint[low1Idx].price &&
			macdData[low2Idx].valid && macdData[low1Idx].valid &&
			macdData[low2Idx].dif > macdData[low1Idx].dif)
		{
			strength += 2;
			reasons += _T("MACD底背离 ");
		}
	}

	// ========== 3. 股价支撑（需量能确认） ==========
	{
		// 回踩均价支撑：需要缩量回踩才有效
		double avgPrice = timelinePoint[last].averagePrice;
		double price = timelinePoint[last].price;
		if (avgPrice > 0 && std::abs(price - avgPrice) / avgPrice < 0.003 && isShrinking)
		{
			strength++;
			reasons += _T("缩量回踩均价支撑 ");
		}
		// 分时底部企稳：需要放量止跌才可靠
		if (last >= 2 &&
			timelinePoint[last].price >= timelinePoint[last - 1].price &&
			timelinePoint[last - 1].price < timelinePoint[last - 2].price &&
			isExpanding)
		{
			strength++;
			reasons += _T("放量底部企稳 ");
		}
	}

	signal.strength = min(3, strength);
	signal.reason = reasons;
	signal.valid = (strength >= 2);
	return signal;
}

CSignalAnalyzer::T0Signal CStockIndicator::DetectSellSignal(const std::vector<STOCK::TimelinePoint>& timelinePoint, const std::vector<MACDData>& macdData)
{
	CSignalAnalyzer::T0Signal signal;
	signal.valid = false;
	signal.isBuy = false;
	signal.strength = 0;
	signal.price = 0;

	int n = static_cast<int>(timelinePoint.size());
	int m = static_cast<int>(macdData.size());
	if (n < 10 || m < 10)
		return signal;

	int last = n - 1;
	signal.price = timelinePoint[last].price;
	signal.time = CString(timelinePoint[last].time.c_str());

	// ========== 量能基准：前三根成交量均值 ==========
	long long volAvg3 = 0;
	{
		int volCount = min(3, last);
		for (int i = last - volCount; i < last; i++)
			volAvg3 += timelinePoint[i].volume;
		if (volCount > 0) volAvg3 /= volCount;
	}
	bool isShrinking = (volAvg3 > 0 && timelinePoint[last].volume < volAvg3);           // 缩量：当前量 < 前三根均值
	bool isExpanding = (volAvg3 > 0 && timelinePoint[last].volume > volAvg3 * 3 / 2);    // 放量：当前量 > 前三根均值的1.5倍

	int strength = 0;
	CString reasons;

	// ========== 避坑规则：放量大涨不卖出 ==========
	{
		int lookback = min(5, n - 1);
		bool strongRise = true;
		for (int i = last - lookback + 1; i <= last; i++)
		{
			if (timelinePoint[i].price <= timelinePoint[i - 1].price)
			{
				strongRise = false;
				break;
			}
		}
		if (strongRise)
		{
			bool volIncreasing = true;
			for (int i = last - lookback + 2; i <= last; i++)
			{
				if (timelinePoint[i].volume < timelinePoint[i - 1].volume)
				{
					volIncreasing = false;
					break;
				}
			}
			if (volIncreasing)
				return signal; // 放量大涨，不卖出
		}
	}

	// ========== 1. 量柱信号：放量冲高 + 量能萎缩（需量能确认） ==========
	{
		// 检查近期是否有放量冲高（使用前三根均值判定放量）
		int lookback = min(10, n - 1);
		bool hadSpike = false;
		int spikeIdx = -1;
		for (int i = last - lookback + 1; i <= last; i++)
		{
			// 计算该点的前三根均值
			long long localAvg3 = 0;
			int vc = min(3, i);
			for (int j = i - vc; j < i; j++)
				localAvg3 += timelinePoint[j].volume;
			if (vc > 0) localAvg3 /= vc;
			if (localAvg3 > 0 && timelinePoint[i].volume > localAvg3 * 3 / 2)
			{
				hadSpike = true;
				spikeIdx = i;
				break;
			}
		}
		// 放量后量能萎缩，且当前价格未明显回落（仍在高位）
		bool priceStillHigh = (hadSpike && spikeIdx >= 0 &&
			timelinePoint[last].price >= timelinePoint[spikeIdx].price * 0.995);
		bool shrinkingNow = false;
		if (hadSpike && last > spikeIdx + 1 &&
			timelinePoint[last].volume < timelinePoint[last - 1].volume &&
			isShrinking && priceStillHigh)
			shrinkingNow = true;

		if (hadSpike && shrinkingNow)
		{
			strength++;
			reasons += _T("放量冲高后缩量回落 ");
		}
	}

	// ========== 2. MACD信号 ==========
	if (macdData[last].valid)
	{
		// 红柱缩短
		if (macdData[last].bar > 0)
		{
			int shrkCount = 0;
			for (int i = last; i > max(0, last - 5); i--)
			{
				if (macdData[i].valid && macdData[i].bar > 0 &&
					macdData[i].bar <= macdData[i - 1].bar)
					shrkCount++;
			}
			if (shrkCount >= 3)
			{
				strength++;
				reasons += _T("MACD红柱缩短 ");
			}
		}

		// 死叉检测
		auto crossSignals = DetectMACDCross(macdData);
		if (last < static_cast<int>(crossSignals.size()) && crossSignals[last] == MACDCrossSignal::DeathCross)
		{
			strength++;
			reasons += _T("MACD死叉 ");
		}

		// 顶背离检测：价格创新高，MACD未创新高
		int lookback = min(30, n - 1);
		int high1Idx = -1, high2Idx = -1;
		for (int i = last - lookback + 1; i < last; i++)
		{
			if (i > 0 && i < n - 1 &&
				timelinePoint[i].price > timelinePoint[i - 1].price &&
				timelinePoint[i].price > timelinePoint[i + 1].price)
			{
				if (high1Idx < 0)
					high1Idx = i;
				else if (i > high1Idx + 3 && high2Idx < 0)
					high2Idx = i;
			}
		}
		if (high1Idx >= 0 && high2Idx >= 0 &&
			timelinePoint[high2Idx].price > timelinePoint[high1Idx].price &&
			macdData[high2Idx].valid && macdData[high1Idx].valid &&
			macdData[high2Idx].dif < macdData[high1Idx].dif)
		{
			strength += 2;
			reasons += _T("MACD顶背离 ");
		}
	}

	// ========== 3. 股价压力（需量能确认） ==========
	{
		// 遇均价压力：需要放量冲高遇阻才有效
		double avgPrice = timelinePoint[last].averagePrice;
		double price = timelinePoint[last].price;
		if (avgPrice > 0 && price > avgPrice && std::abs(price - avgPrice) / avgPrice < 0.005 && isExpanding)
		{
			strength++;
			reasons += _T("放量遇均价压力 ");
		}
		// 冲高滞涨：放量冲高后回落
		if (last >= 2 &&
			timelinePoint[last].price <= timelinePoint[last - 1].price &&
			timelinePoint[last - 1].price > timelinePoint[last - 2].price &&
			isShrinking)
		{
			strength++;
			reasons += _T("冲高缩量滞涨 ");
		}
	}

	signal.strength = min(3, strength);
	signal.reason = reasons;
	signal.valid = (strength >= 2);
	return signal;
}

// ========== KDJ指标计算 ==========

std::vector<CStockIndicator::KDJData> CStockIndicator::CalculateKDJ(const std::vector<STOCK::KLinePoint>& klineData, int period /* = 9 */)
{
	std::vector<KDJData> result;
	result.reserve(klineData.size());
	if (klineData.empty() || period <= 0)
		return result;

	// 初始 K = D = 50
	double prevK = 50.0;
	double prevD = 50.0;

	for (int i = 0; i < static_cast<int>(klineData.size()); i++)
	{
		KDJData kd;
		kd.k = 0;
		kd.d = 0;
		kd.j = 0;
		kd.valid = false;

		// 需要至少 period 个数据点才能计算 RSV
		if (i >= period - 1)
		{
			STOCK::Price highest = 0;
			STOCK::Price lowest = (std::numeric_limits<STOCK::Price>::max)();
			bool hasValid = false;
			for (int j = i - period + 1; j <= i; j++)
			{
				if (klineData[j].high > 0)
				{
					highest = (std::max)(highest, klineData[j].high);
					hasValid = true;
				}
				if (klineData[j].low > 0)
				{
					lowest = (std::min)(lowest, klineData[j].low);
					hasValid = true;
				}
			}

			if (hasValid && highest > lowest && klineData[i].close > 0)
			{
				double rsv = (static_cast<double>(klineData[i].close) - lowest) / (highest - lowest) * 100.0;
				// 限制 RSV 在 [0, 100] 范围
				if (rsv < 0) rsv = 0;
				if (rsv > 100) rsv = 100;

				// K(t) = 2/3 * K(t-1) + 1/3 * RSV(t)
				double curK = 2.0 / 3.0 * prevK + 1.0 / 3.0 * rsv;
				// D(t) = 2/3 * D(t-1) + 1/3 * K(t)
				double curD = 2.0 / 3.0 * prevD + 1.0 / 3.0 * curK;
				// J = 3K - 2D
				double curJ = 3.0 * curK - 2.0 * curD;

				kd.k = curK;
				kd.d = curD;
				kd.j = curJ;
				kd.valid = true;

				prevK = curK;
				prevD = curD;
			}
			else
			{
				// 数据无效，保持上一状态
				kd.k = prevK;
				kd.d = prevD;
				kd.j = 3.0 * prevK - 2.0 * prevD;
				kd.valid = true;
			}
		}
		else
		{
			// 前 period-1 个数据点无法计算 RSV，使用初始值 50
			kd.k = prevK;
			kd.d = prevD;
			kd.j = 3.0 * prevK - 2.0 * prevD;
			kd.valid = true;
		}

		result.push_back(kd);
	}

	return result;
}

std::vector<CStockIndicator::KDJData> CStockIndicator::CalculateTimelineKDJ(const std::vector<STOCK::TimelinePoint>& timelinePoint, int period /* = 9 */)
{
	std::vector<KDJData> result;
	result.reserve(timelinePoint.size());
	if (timelinePoint.empty() || period <= 0)
		return result;

	double prevK = 50.0;
	double prevD = 50.0;

	for (int i = 0; i < static_cast<int>(timelinePoint.size()); i++)
	{
		KDJData kd;
		kd.k = 0;
		kd.d = 0;
		kd.j = 0;
		kd.valid = false;

		if (i >= period - 1)
		{
			STOCK::Price highest = 0;
			STOCK::Price lowest = (std::numeric_limits<STOCK::Price>::max)();
			bool hasValid = false;
			for (int j = i - period + 1; j <= i; j++)
			{
				STOCK::Price price = timelinePoint[j].price;
				if (price > 0)
				{
					highest = (std::max)(highest, price);
					lowest = (std::min)(lowest, price);
					hasValid = true;
				}
			}

			if (hasValid && highest > lowest && timelinePoint[i].price > 0)
			{
				double rsv = (static_cast<double>(timelinePoint[i].price) - lowest) / (highest - lowest) * 100.0;
				if (rsv < 0) rsv = 0;
				if (rsv > 100) rsv = 100;

				double curK = 2.0 / 3.0 * prevK + 1.0 / 3.0 * rsv;
				double curD = 2.0 / 3.0 * prevD + 1.0 / 3.0 * curK;
				double curJ = 3.0 * curK - 2.0 * curD;

				kd.k = curK;
				kd.d = curD;
				kd.j = curJ;
				kd.valid = true;

				prevK = curK;
				prevD = curD;
			}
			else
			{
				kd.k = prevK;
				kd.d = prevD;
				kd.j = 3.0 * prevK - 2.0 * prevD;
				kd.valid = true;
			}
		}
		else
		{
			kd.k = prevK;
			kd.d = prevD;
			kd.j = 3.0 * prevK - 2.0 * prevD;
			kd.valid = true;
		}

		result.push_back(kd);
	}

	return result;
}

// ========== W&R威廉指标计算 ==========

std::vector<CStockIndicator::WRData> CStockIndicator::CalculateTimelineWR(const std::vector<STOCK::TimelinePoint>& timelinePoint, int period1 /* = 6 */, int period2 /* = 14 */)
{
	std::vector<WRData> result;
	result.reserve(timelinePoint.size());
	if (timelinePoint.empty() || period1 <= 0 || period2 <= 0)
		return result;

	int maxPeriod = (std::max)(period1, period2);

	for (int i = 0; i < static_cast<int>(timelinePoint.size()); i++)
	{
		WRData wr;
		wr.wr1 = 0;
		wr.wr2 = 0;
		wr.valid = false;

		// 计算WR1（短期，如N=6）
		if (i >= period1 - 1)
		{
			STOCK::Price highest = 0;
			STOCK::Price lowest = (std::numeric_limits<STOCK::Price>::max)();
			bool hasValid = false;
			for (int j = i - period1 + 1; j <= i; j++)
			{
				STOCK::Price price = timelinePoint[j].price;
				if (price > 0)
				{
					highest = (std::max)(highest, price);
					lowest = (std::min)(lowest, price);
					hasValid = true;
				}
			}
			if (hasValid && highest > lowest && timelinePoint[i].price > 0)
			{
				wr.wr1 = (static_cast<double>(highest) - timelinePoint[i].price) / (highest - lowest) * 100.0;
				if (wr.wr1 < 0) wr.wr1 = 0;
				if (wr.wr1 > 100) wr.wr1 = 100;
			}
			else if (hasValid && highest == lowest && timelinePoint[i].price > 0)
			{
				wr.wr1 = 0;  // 最高=最低时WR=0
			}
		}

		// 计算WR2（长期，如N=14）
		if (i >= period2 - 1)
		{
			STOCK::Price highest = 0;
			STOCK::Price lowest = (std::numeric_limits<STOCK::Price>::max)();
			bool hasValid = false;
			for (int j = i - period2 + 1; j <= i; j++)
			{
				STOCK::Price price = timelinePoint[j].price;
				if (price > 0)
				{
					highest = (std::max)(highest, price);
					lowest = (std::min)(lowest, price);
					hasValid = true;
				}
			}
			if (hasValid && highest > lowest && timelinePoint[i].price > 0)
			{
				wr.wr2 = (static_cast<double>(highest) - timelinePoint[i].price) / (highest - lowest) * 100.0;
				if (wr.wr2 < 0) wr.wr2 = 0;
				if (wr.wr2 > 100) wr.wr2 = 100;
			}
			else if (hasValid && highest == lowest && timelinePoint[i].price > 0)
			{
				wr.wr2 = 0;
			}
		}

		wr.valid = (i >= maxPeriod - 1);
		result.push_back(wr);
	}

	return result;
}

std::vector<CStockIndicator::WRData> CStockIndicator::CalculateKLineWR(const std::vector<STOCK::KLinePoint>& klineData, int period1 /* = 6 */, int period2 /* = 14 */)
{
	std::vector<WRData> result;
	result.reserve(klineData.size());
	if (klineData.empty() || period1 <= 0 || period2 <= 0)
		return result;

	int maxPeriod = (std::max)(period1, period2);

	for (int i = 0; i < static_cast<int>(klineData.size()); i++)
	{
		WRData wr;
		wr.wr1 = 0;
		wr.wr2 = 0;
		wr.valid = false;

		// 计算WR1（短期，如N=6）—— 使用K线的high/low
		if (i >= period1 - 1)
		{
			STOCK::Price highest = 0;
			STOCK::Price lowest = (std::numeric_limits<STOCK::Price>::max)();
			bool hasValid = false;
			for (int j = i - period1 + 1; j <= i; j++)
			{
				if (klineData[j].high > 0 && klineData[j].low > 0)
				{
					highest = (std::max)(highest, klineData[j].high);
					lowest = (std::min)(lowest, klineData[j].low);
					hasValid = true;
				}
			}
			if (hasValid && highest > lowest && klineData[i].close > 0)
			{
				wr.wr1 = (static_cast<double>(highest) - klineData[i].close) / (highest - lowest) * 100.0;
				if (wr.wr1 < 0) wr.wr1 = 0;
				if (wr.wr1 > 100) wr.wr1 = 100;
			}
			else if (hasValid && highest == lowest && klineData[i].close > 0)
			{
				wr.wr1 = 0;
			}
		}

		// 计算WR2（长期，如N=14）—— 使用K线的high/low
		if (i >= period2 - 1)
		{
			STOCK::Price highest = 0;
			STOCK::Price lowest = (std::numeric_limits<STOCK::Price>::max)();
			bool hasValid = false;
			for (int j = i - period2 + 1; j <= i; j++)
			{
				if (klineData[j].high > 0 && klineData[j].low > 0)
				{
					highest = (std::max)(highest, klineData[j].high);
					lowest = (std::min)(lowest, klineData[j].low);
					hasValid = true;
				}
			}
			if (hasValid && highest > lowest && klineData[i].close > 0)
			{
				wr.wr2 = (static_cast<double>(highest) - klineData[i].close) / (highest - lowest) * 100.0;
				if (wr.wr2 < 0) wr.wr2 = 0;
				if (wr.wr2 > 100) wr.wr2 = 100;
			}
			else if (hasValid && highest == lowest && klineData[i].close > 0)
			{
				wr.wr2 = 0;
			}
		}

		wr.valid = (i >= maxPeriod - 1);
		result.push_back(wr);
	}

	return result;
}

// ========== RSI相对强弱指标计算 ==========

std::vector<CStockIndicator::RSIData> CStockIndicator::CalculateTimelineRSI(const std::vector<STOCK::TimelinePoint>& timelinePoint, int period1 /* = 6 */, int period2 /* = 14 */)
{
	std::vector<RSIData> result;
	result.reserve(timelinePoint.size());
	if (timelinePoint.size() < 2 || period1 <= 1 || period2 <= 1)
		return result;

	int maxPeriod = (std::max)(period1, period2);

	// 辅助lambda：计算指定周期的RSI序列
	auto calcRSI = [&](int period) -> std::vector<double> {
		std::vector<double> rsiValues(timelinePoint.size(), 0);
		// 初始AU/AD：前period根K线的平均涨跌幅
		double au = 0, ad = 0;
		int count = 0;
		for (int i = 1; i <= period && i < static_cast<int>(timelinePoint.size()); i++)
		{
			double delta = timelinePoint[i].price - timelinePoint[i - 1].price;
			if (delta > 0)
				au += delta;
			else
				ad += (-delta);
			count++;
		}
		if (count > 0)
		{
			au /= count;
			ad /= count;
		}

		// 第period根的RSI
		if (period < static_cast<int>(timelinePoint.size()))
		{
			if (au == 0 && ad == 0)
				rsiValues[period] = 50.0;
			else
				rsiValues[period] = 100.0 - 100.0 / (1.0 + au / (ad > 0 ? ad : 1e-10));
		}

		// 从第period+1根开始，使用SMA平滑
		for (int i = period + 1; i < static_cast<int>(timelinePoint.size()); i++)
		{
			double delta = timelinePoint[i].price - timelinePoint[i - 1].price;
			double up = (delta > 0) ? delta : 0.0;
			double dn = (delta < 0) ? (-delta) : 0.0;
			au = (au * (period - 1) + up) / period;
			ad = (ad * (period - 1) + dn) / period;
			if (au == 0 && ad == 0)
				rsiValues[i] = 50.0;
			else
				rsiValues[i] = 100.0 - 100.0 / (1.0 + au / (ad > 0 ? ad : 1e-10));
		}
		return rsiValues;
	};

	auto rsi1Values = calcRSI(period1);
	auto rsi2Values = calcRSI(period2);

	for (int i = 0; i < static_cast<int>(timelinePoint.size()); i++)
	{
		RSIData rd;
		rd.rsi1 = rsi1Values[i];
		rd.rsi2 = rsi2Values[i];
		rd.valid = (i >= maxPeriod);
		result.push_back(rd);
	}

	return result;
}

std::vector<CStockIndicator::RSIData> CStockIndicator::CalculateKLineRSI(const std::vector<STOCK::KLinePoint>& klineData, int period1 /* = 6 */, int period2 /* = 14 */)
{
	std::vector<RSIData> result;
	result.reserve(klineData.size());
	if (klineData.size() < 2 || period1 <= 1 || period2 <= 1)
		return result;

	int maxPeriod = (std::max)(period1, period2);

	// 辅助lambda：计算指定周期的RSI序列
	auto calcRSI = [&](int period) -> std::vector<double> {
		std::vector<double> rsiValues(klineData.size(), 0);
		double au = 0, ad = 0;
		int count = 0;
		for (int i = 1; i <= period && i < static_cast<int>(klineData.size()); i++)
		{
			double delta = klineData[i].close - klineData[i - 1].close;
			if (delta > 0)
				au += delta;
			else
				ad += (-delta);
			count++;
		}
		if (count > 0)
		{
			au /= count;
			ad /= count;
		}

		if (period < static_cast<int>(klineData.size()))
		{
			if (au == 0 && ad == 0)
				rsiValues[period] = 50.0;
			else
				rsiValues[period] = 100.0 - 100.0 / (1.0 + au / (ad > 0 ? ad : 1e-10));
		}

		for (int i = period + 1; i < static_cast<int>(klineData.size()); i++)
		{
			double delta = klineData[i].close - klineData[i - 1].close;
			double up = (delta > 0) ? delta : 0.0;
			double dn = (delta < 0) ? (-delta) : 0.0;
			au = (au * (period - 1) + up) / period;
			ad = (ad * (period - 1) + dn) / period;
			if (au == 0 && ad == 0)
				rsiValues[i] = 50.0;
			else
				rsiValues[i] = 100.0 - 100.0 / (1.0 + au / (ad > 0 ? ad : 1e-10));
		}
		return rsiValues;
	};

	auto rsi1Values = calcRSI(period1);
	auto rsi2Values = calcRSI(period2);

	for (int i = 0; i < static_cast<int>(klineData.size()); i++)
	{
		RSIData rd;
		rd.rsi1 = rsi1Values[i];
		rd.rsi2 = rsi2Values[i];
		rd.valid = (i >= maxPeriod);
		result.push_back(rd);
	}

	return result;
}

// ========== K线周期高低点统计 ==========

void CStockIndicator::CalculatePeriodHighsLows(const std::vector<STOCK::KLinePoint>& klineData, int startIndex,
	PeriodPoint periodHighs[3], PeriodPoint periodLows[3], bool useClose /* = false */)
{
	const int DAYS_PER_YEAR = 250;

	for (int p = 1; p <= 3; p++)
	{
		int rangeEnd = static_cast<int>(klineData.size()) - (p - 1) * DAYS_PER_YEAR;
		int rangeStart = max(startIndex, static_cast<int>(klineData.size()) - p * DAYS_PER_YEAR);
		if (rangeStart >= rangeEnd) continue;

		STOCK::Price hh = 0, ll = (std::numeric_limits<STOCK::Price>::max)();
		int hIdx = -1, lIdx = -1;
		for (int i = rangeStart; i < rangeEnd; i++)
		{
			STOCK::Price price = useClose ? klineData[i].close : klineData[i].high;
			STOCK::Price lowPrice = useClose ? klineData[i].close : klineData[i].low;
			if (price > 0 && price > hh) { hh = price; hIdx = i; }
			if (lowPrice > 0 && lowPrice < ll) { ll = lowPrice; lIdx = i; }
		}
		periodHighs[p - 1] = { hIdx, hh, hIdx >= 0 ? klineData[hIdx].day : "" };
		periodLows[p - 1] = { lIdx, ll, lIdx >= 0 ? klineData[lIdx].day : "" };
	}
}
