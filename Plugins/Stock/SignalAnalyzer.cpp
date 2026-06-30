#include "pch.h"
#include "SignalAnalyzer.h"
#include "StockDef.h"
#include <cmath>

namespace
{
	constexpr double MACD_DIF_PRICE_RATIO = 0.001;
	constexpr double MIN_SIGNAL_PRICE_RATIO = 0.003;
	constexpr double MIN_SIGNAL_ATR_RATIO = 0.2;
	constexpr double NARROW_BOLL_RATIO = 0.005;
	constexpr double NARROW_BOLL_LOW_PRICE_ABS_THRESHOLD = 0.05;
	constexpr double NARROW_BOLL_ABS_THRESHOLD = 0.03;
	constexpr double NARROW_BOLL_LOW_PRICE_LIMIT = 1.5;
	constexpr size_t NARROW_BOLL_AVG_PERIOD = 10;
	constexpr double BOLL_EXPAND_RATIO = 1.4;
	constexpr int BOLL_HIGH_PERCENTILE = 90;
	constexpr double SELL_KDJ_D_OVERBUY = 70.0;
	constexpr double SELL_WR_OVERBUY = 30.0;
	constexpr double KDJ_OVERBUY = 80.0;
	constexpr double KDJ_OVERSELL = 20.0;
	constexpr int MIN_SIGNAL_BAR_GAP = 5;

	// 双周期共振趋势判定参数
	constexpr size_t TREND_30M_LOOKBACK = 25;      // 30分钟K线回看根数
	constexpr size_t TREND_5M_LOOKBACK = 20;       // 5分钟K线回看根数
	constexpr double MA20_SLOPE_THRESHOLD = 0.001;  // MA20斜率阈值，低于此视为走平
	constexpr size_t SWING_MIN_BARS = 3;            // 波段极值最小间隔K线数
	constexpr double VOL_RATIO_THRESHOLD = 1.3;     // 量比阈值（上涨放量/回调缩量）

	double GetMacdDifThreshold(double price)
	{
		return price > 0 ? price * MACD_DIF_PRICE_RATIO : 0.0;
	}

	double CalcATR(const std::vector<STOCK::Bar>& bars, size_t endIndex, int N = 14)
	{
		if (bars.empty() || endIndex >= bars.size() || N <= 0)
			return 0.0;

		size_t start = endIndex + 1 > static_cast<size_t>(N) ? endIndex + 1 - static_cast<size_t>(N) : 0;
		double sum = 0.0;
		size_t count = 0;
		for (size_t i = start; i <= endIndex; i++)
		{
			double prevClose = i > 0 ? bars[i - 1].close : bars[i].close;
			double tr = bars[i].high - bars[i].low;
			double highGap = fabs(bars[i].high - prevClose);
			double lowGap = fabs(bars[i].low - prevClose);
			if (highGap > tr) tr = highGap;
			if (lowGap > tr) tr = lowGap;
			sum += tr;
			count++;
		}
		return count > 0 ? sum / count : 0.0;
	}

	double GetSignalPriceDiffThreshold(const std::vector<STOCK::Bar>& bars, size_t endIndex)
	{
		if (bars.empty() || endIndex >= bars.size())
			return 0.0;

		double priceThreshold = bars[endIndex].close > 0 ? bars[endIndex].close * MIN_SIGNAL_PRICE_RATIO : 0.0;
		double atrThreshold = CalcATR(bars, endIndex) * MIN_SIGNAL_ATR_RATIO;
		return atrThreshold > priceThreshold ? atrThreshold : priceThreshold;
	}

	double GetNarrowBollThreshold(double mid)
	{
		double absThreshold = mid > 0 && mid < NARROW_BOLL_LOW_PRICE_LIMIT ? NARROW_BOLL_LOW_PRICE_ABS_THRESHOLD : NARROW_BOLL_ABS_THRESHOLD;
		double relativeThreshold = mid > 0 ? mid * NARROW_BOLL_RATIO : 0.0;
		return relativeThreshold > absThreshold ? relativeThreshold : absThreshold;
	}

	double CalcAverageBollBandwidth(const std::vector<double>& bollBand, size_t index, size_t period = NARROW_BOLL_AVG_PERIOD)
	{
		if (index >= bollBand.size() || period == 0)
			return 0.0;

		size_t start = index + 1 > period ? index + 1 - period : 0;
		double sum = 0.0;
		size_t count = 0;
		for (size_t i = start; i <= index; i++)
		{
			if (bollBand[i] <= 0)
				continue;
			sum += bollBand[i];
			count++;
		}
		return count > 0 ? sum / count : 0.0;
	}

	bool IsNarrowBoll(double mid, double avgBandwidth)
	{
		if (avgBandwidth <= 0)
			return false;

		return avgBandwidth < GetNarrowBollThreshold(mid);
	}

	bool IsBollBandwidthHighPercentile(const std::vector<double>& bollBand, size_t index, size_t lookback = 60)
	{
		if (index >= bollBand.size() || bollBand[index] <= 0)
			return false;

		size_t start = index > lookback ? index - lookback : 0;
		int validCount = 0;
		int lowerOrEqualCount = 0;
		for (size_t i = start; i < index; i++)
		{
			if (bollBand[i] <= 0)
				continue;
			validCount++;
			if (bollBand[index] >= bollBand[i])
				lowerOrEqualCount++;
		}

		return validCount >= 20 && lowerOrEqualCount * 100 >= validCount * BOLL_HIGH_PERCENTILE;
	}

	bool IsBollExpand(const std::vector<double>& bollBand, size_t index)
	{
		if (index == 0 || index >= bollBand.size() || bollBand[index] <= 0 || bollBand[index - 1] <= 0)
			return false;

		return bollBand[index] > bollBand[index - 1] * BOLL_EXPAND_RATIO || IsBollBandwidthHighPercentile(bollBand, index);
	}

	void CalcKDJSeries(const std::vector<STOCK::Bar>& bars, int N, std::vector<double>& kSeq, std::vector<double>& dSeq, std::vector<double>& jSeq)
	{
		size_t n = bars.size();
		kSeq.assign(n, 50.0);
		dSeq.assign(n, 50.0);
		jSeq.assign(n, 50.0);
		if (N <= 0 || n < static_cast<size_t>(N))
			return;

		double k = 50.0;
		double d = 50.0;
		for (size_t i = static_cast<size_t>(N) - 1; i < n; i++)
		{
			size_t start = i + 1 - static_cast<size_t>(N);
			double lowN = bars[start].low;
			double highN = bars[start].high;
			for (size_t j = start + 1; j <= i; j++)
			{
				if (bars[j].low < lowN) lowN = bars[j].low;
				if (bars[j].high > highN) highN = bars[j].high;
			}

			double rsv = highN != lowN ? (bars[i].close - lowN) / (highN - lowN) * 100.0 : 50.0;
			k = (2.0 * k + rsv) / 3.0;
			d = (2.0 * d + k) / 3.0;
			kSeq[i] = k;
			dSeq[i] = d;
			jSeq[i] = 3.0 * k - 2.0 * d;
		}
	}
}

// ========== 智能分析模块：基础工具函数 ==========

double CSignalAnalyzer::CalcMA(const std::vector<double>& values, int N)
{
	if (static_cast<int>(values.size()) < N || N <= 0)
		return 0.0;
	double sum = 0.0;
	for (int i = static_cast<int>(values.size()) - N; i < static_cast<int>(values.size()); i++)
		sum += values[i];
	return sum / N;
}

double CSignalAnalyzer::CalcEMA(const std::vector<double>& values, int N)
{
	if (values.empty() || N <= 0)
		return 0.0;
	double k = 2.0 / (N + 1.0);
	double ema = values[0];
	for (size_t i = 1; i < values.size(); i++)
		ema = values[i] * k + ema * (1.0 - k);
	return ema;
}

double CSignalAnalyzer::CalcSMA(const std::vector<double>& values, int N, double M /* = 1.0 */)
{
	if (values.empty() || N <= 0)
		return 0.0;
	// SMA(X,N,M) = (M*X + (N-M)*Y') / N，Y'为上一周期SMA值
	// 初始值取第一个值
	double sma = values[0];
	for (size_t i = 1; i < values.size(); i++)
		sma = (M * values[i] + (N - M) * sma) / N;
	return sma;
}

double CSignalAnalyzer::CalcStdDev(const std::vector<double>& values)
{
	if (values.empty())
		return 0.0;
	double mean = 0.0;
	for (double v : values)
		mean += v;
	mean /= values.size();
	double sumSq = 0.0;
	for (double v : values)
		sumSq += (v - mean) * (v - mean);
	return sqrt(sumSq / values.size());
}

// ========== 智能分析模块：5个核心指标计算函数 ==========

STOCK::BollResult CSignalAnalyzer::CalcBoll(const std::vector<STOCK::Bar>& bars, int N /* = 20 */)
{
	STOCK::BollResult result;
	if (static_cast<int>(bars.size()) < N)
		return result;

	std::vector<double> closes;
	closes.reserve(N);
	for (int i = static_cast<int>(bars.size()) - N; i < static_cast<int>(bars.size()); i++)
		closes.push_back(bars[i].close);

	double mid = CalcMA(closes, N);
	double stdDev = CalcStdDev(closes);
	result.mid = mid;
	result.up = mid + 2.0 * stdDev;
	result.dn = mid - 2.0 * stdDev;
	result.bandwidth = result.up - result.dn;
	return result;
}

STOCK::MACDResult CSignalAnalyzer::CalcMACD(const std::vector<STOCK::Bar>& bars)
{
	STOCK::MACDResult result;
	if (bars.size() < 26)
		return result;

	// 提取收盘价序列
	std::vector<double> closes;
	closes.reserve(bars.size());
	for (const auto& bar : bars)
		closes.push_back(bar.close);

	// 计算EMA12和EMA26完整序列
	double k12 = 2.0 / 13.0;
	double k26 = 2.0 / 27.0;
	std::vector<double> ema12(closes.size());
	std::vector<double> ema26(closes.size());
	ema12[0] = closes[0];
	ema26[0] = closes[0];
	for (size_t i = 1; i < closes.size(); i++)
	{
		ema12[i] = closes[i] * k12 + ema12[i - 1] * (1.0 - k12);
		ema26[i] = closes[i] * k26 + ema26[i - 1] * (1.0 - k26);
	}

	// DIF序列
	std::vector<double> difSeq(closes.size());
	for (size_t i = 0; i < closes.size(); i++)
		difSeq[i] = ema12[i] - ema26[i];

	// DEA：对DIF序列求EMA9
	double k9 = 2.0 / 10.0;
	std::vector<double> deaSeq(difSeq.size());
	deaSeq[0] = difSeq[0];
	for (size_t i = 1; i < difSeq.size(); i++)
		deaSeq[i] = difSeq[i] * k9 + deaSeq[i - 1] * (1.0 - k9);

	// 取最新值
	size_t last = closes.size() - 1;
	result.dif = difSeq[last];
	result.dea = deaSeq[last];
	result.macd_bar = 2.0 * (difSeq[last] - deaSeq[last]);
	return result;
}

STOCK::KDJResult CSignalAnalyzer::CalcKDJ(const std::vector<STOCK::Bar>& bars, int N /* = 9 */)
{
	STOCK::KDJResult result;
	std::vector<double> kSeq, dSeq, jSeq;
	CalcKDJSeries(bars, N, kSeq, dSeq, jSeq);
	if (kSeq.empty() || static_cast<int>(bars.size()) < N)
		return result;

	result.k = kSeq.back();
	result.d = dSeq.back();
	result.j = jSeq.back();
	return result;
}

double CSignalAnalyzer::CalcRSI(const std::vector<STOCK::Bar>& bars, int N)
{
	if (static_cast<int>(bars.size()) < 2)
		return 50.0;

	// 计算涨跌序列
	std::vector<double> upList, dnList;
	for (size_t i = 1; i < bars.size(); i++)
	{
		double delta = bars[i].close - bars[i - 1].close;
		upList.push_back(max(delta, 0.0));
		dnList.push_back(max(-delta, 0.0));
	}

	if (static_cast<int>(upList.size()) < N)
		return 50.0;

	// 用SMA平滑AU、AD
	double au = CalcSMA(upList, N);
	double ad = CalcSMA(dnList, N);

	if (ad == 0.0)
		return 99.99;

	double rs = au / ad;
	return 100.0 - 100.0 / (1.0 + rs);
}

double CSignalAnalyzer::CalcWR(const std::vector<STOCK::Bar>& bars, int N)
{
	if (static_cast<int>(bars.size()) < N || N <= 0)
		return 50.0;

	double hhv = bars[bars.size() - N].high;
	double llv = bars[bars.size() - N].low;
	for (int i = bars.size() - N + 1; i < static_cast<int>(bars.size()); i++)
	{
		if (bars[i].high > hhv) hhv = bars[i].high;
		if (bars[i].low < llv) llv = bars[i].low;
	}

	double c = bars.back().close;
	if (hhv == llv)
		return 50.0;

	return 100.0 * (hhv - c) / (hhv - llv);
}

// ========== 智能分析模块：30分钟趋势判定 ==========
// 输入：完整历史30分钟K线数组 bars30
// 输出：STATE_WEAK(仅反T) / STATE_STRONG(可正T) / STATE_SHAKE(震荡双向)

STOCK::TrendState30m CSignalAnalyzer::Get30mTrendState(const std::vector<STOCK::Bar>& bars30)
{
	// 至少需要30根30min K线才能计算当前/前一根BOLL；EMA60 在数据不足时按现有序列递推
	if (bars30.size() < 30)
		return STOCK::TrendState30m::STATE_SHAKE;

	// 1. 计算30min全部指标
	STOCK::BollResult boll = CalcBoll(bars30, 20);
	double mid = boll.mid;
	double up = boll.up;
	double dn = boll.dn;
	(void)up; (void)dn;  // 当前模块未直接使用上下轨

	const STOCK::Bar& lastBar = bars30.back();

	// 计算前一根BOLL的中轨（取bars30[-21:-1]）
	std::vector<STOCK::Bar> prevBars(bars30.end() - 21, bars30.end() - 1);
	STOCK::BollResult prevBoll = CalcBoll(prevBars, 20);
	double prevMid = prevBoll.mid;

	// BOLL条件
	bool bollDown = (mid < prevMid) && (lastBar.close < mid);
	bool bollUp = (mid > prevMid) && (lastBar.close > mid);

	// RSI14
	double rsi14 = CalcRSI(bars30, 14);
	bool rsiWeak = rsi14 < 50;
	bool rsiStrong = rsi14 > 50;

	// MACD
	STOCK::MACDResult macd = CalcMACD(bars30);
	bool macBelow = macd.dif < 0;
	bool macAbove = macd.dif > 0;

	// EMA20/60辅助判断趋势方向和强度
	std::vector<double> closes;
	closes.reserve(bars30.size());
	for (const auto& bar : bars30)
		closes.push_back(bar.close);
	double ema20 = CalcEMA(closes, 20);
	double ema60 = CalcEMA(closes, 60);
	std::vector<double> prevCloses(closes.begin(), closes.end() - 1);
	double prevEma20 = CalcEMA(prevCloses, 20);
	bool maStrong = ema20 > ema60 && ema20 > prevEma20 && lastBar.close > ema20;
	bool maWeak = ema20 < ema60 && ema20 < prevEma20 && lastBar.close < ema20;

	// 判定行情：强势允许BOLL/RSI/MACD/均线中满足3项即可，避免强趋势被误判为震荡
	int weakScore = (bollDown ? 1 : 0) + (rsiWeak ? 1 : 0) + (macBelow ? 1 : 0) + (maWeak ? 1 : 0);
	int strongScore = (bollUp ? 1 : 0) + (rsiStrong ? 1 : 0) + (macAbove ? 1 : 0) + (maStrong ? 1 : 0);
	bool condWeak = weakScore >= 3 && weakScore > strongScore;
	bool condStrong = strongScore >= 3 && strongScore >= weakScore;

	if (condWeak)
		return STOCK::TrendState30m::STATE_WEAK;    // 弱势：只做反T，禁止加仓正T
	else if (condStrong)
		return STOCK::TrendState30m::STATE_STRONG;  // 强势：回踩可低吸正T
	else
		return STOCK::TrendState30m::STATE_SHAKE;   // 震荡：正反T均可
}

// ========== 双周期共振趋势判定模块 ==========

namespace
{
	// 波段极值点结构
	struct SwingPoint {
		size_t index;   // K线索引
		double value;   // 极值价格
		bool isHigh;    // true=高点, false=低点
	};

	// 提取波段高低点序列：遍历K线，局部极大值为高点，局部极小值为低点
	// minBars: 两侧至少minBars根K线才能确认极值
	std::vector<SwingPoint> ExtractSwingPoints(const std::vector<STOCK::Bar>& bars, size_t minBars = SWING_MIN_BARS)
	{
		std::vector<SwingPoint> points;
		if (bars.size() < 2 * minBars + 1)
			return points;

		for (size_t i = minBars; i < bars.size() - minBars; i++)
		{
			// 检查是否为局部高点
			bool isHigh = true;
			for (size_t j = i - minBars; j < i; j++)
			{
				if (bars[j].high >= bars[i].high) { isHigh = false; break; }
			}
			if (isHigh)
			{
				for (size_t j = i + 1; j <= i + minBars; j++)
				{
					if (bars[j].high >= bars[i].high) { isHigh = false; break; }
				}
			}
			if (isHigh)
			{
				// 避免连续同类型极值点，只保留更高的
				if (!points.empty() && points.back().isHigh && points.back().value >= bars[i].high)
					continue;
				if (!points.empty() && points.back().isHigh && points.back().value < bars[i].high)
					points.back() = { i, bars[i].high, true };
				else
					points.push_back({ i, bars[i].high, true });
				continue;
			}

			// 检查是否为局部低点
			bool isLow = true;
			for (size_t j = i - minBars; j < i; j++)
			{
				if (bars[j].low <= bars[i].low) { isLow = false; break; }
			}
			if (isLow)
			{
				for (size_t j = i + 1; j <= i + minBars; j++)
				{
					if (bars[j].low <= bars[i].low) { isLow = false; break; }
				}
			}
			if (isLow)
			{
				// 避免连续同类型极值点，只保留更低的
				if (!points.empty() && !points.back().isHigh && points.back().value <= bars[i].low)
					continue;
				if (!points.empty() && !points.back().isHigh && points.back().value > bars[i].low)
					points.back() = { i, bars[i].low, false };
				else
					points.push_back({ i, bars[i].low, false });
			}
		}
		return points;
	}

	// 从波段点中分别提取高点序列和低点序列
	void SeparateSwingPoints(const std::vector<SwingPoint>& swings,
		std::vector<double>& highs, std::vector<double>& lows)
	{
		highs.clear();
		lows.clear();
		for (const auto& p : swings)
		{
			if (p.isHigh)
				highs.push_back(p.value);
			else
				lows.push_back(p.value);
		}
	}

	// 判断序列是否逐次抬高（至少3个点，每个点 > 前一个点）
	bool IsAscending(const std::vector<double>& vals)
	{
		if (vals.size() < 3) return false;
		size_t n = vals.size();
		return vals[n - 1] > vals[n - 2] && vals[n - 2] > vals[n - 3];
	}

	// 判断序列是否逐次降低（至少3个点，每个点 < 前一个点）
	bool IsDescending(const std::vector<double>& vals)
	{
		if (vals.size() < 3) return false;
		size_t n = vals.size();
		return vals[n - 1] < vals[n - 2] && vals[n - 2] < vals[n - 3];
	}

	// 计算MA20斜率：用最近两根MA20的差值除以价格归一化
	double CalcMA20Slope(const std::vector<STOCK::Bar>& bars)
	{
		if (bars.size() < 22) return 0.0;  // 至少需要21根算MA20，22根算斜率

		// 当前MA20
		std::vector<double> closes1;
		closes1.reserve(20);
		for (auto it = bars.end() - 20; it != bars.end(); ++it)
			closes1.push_back(it->close);
		double ma20_current = CSignalAnalyzer::CalcMA(closes1, 20);

		// 前一根MA20
		std::vector<double> closes2;
		closes2.reserve(20);
		for (auto it = bars.end() - 21; it != bars.end() - 1; ++it)
			closes2.push_back(it->close);
		double ma20_prev = CSignalAnalyzer::CalcMA(closes2, 20);

		// 归一化斜率
		double mid = (ma20_current + ma20_prev) / 2.0;
		return mid > 0 ? (ma20_current - ma20_prev) / mid : 0.0;
	}

	// 判断高低点是否来回交叉（无持续抬高/降低）
	bool IsSwingCrossing(const std::vector<double>& highs, const std::vector<double>& lows)
	{
		if (highs.size() < 3 || lows.size() < 3) return true;  // 数据不足视为交叉

		// 高点既非逐次抬高也非逐次降低
		bool highNoTrend = !IsAscending(highs) && !IsDescending(highs);
		// 低点既非逐次抬高也非逐次降低
		bool lowNoTrend = !IsAscending(lows) && !IsDescending(lows);

		return highNoTrend && lowNoTrend;
	}

	// 计算价格穿越MA20的频率：在最近N根K线中，收盘价穿越MA20的次数
	int CountMACrossings(const std::vector<STOCK::Bar>& bars, int period = 20)
	{
		if (static_cast<int>(bars.size()) < period + 1) return 0;

		int crossings = 0;
		for (int i = static_cast<int>(bars.size()) - period; i < static_cast<int>(bars.size()); i++)
		{
			if (i < 1) continue;
			// 计算i和i-1位置的MA20
			int start1 = (i - 19 > 0) ? i - 19 : 0;
			int start0 = (i - 20 > 0) ? i - 20 : 0;
			if (i - start1 + 1 < 20 || i - 1 - start0 + 1 < 20) continue;

			std::vector<double> c1;
			c1.reserve(20);
			for (auto it = bars.begin() + start1; it != bars.begin() + i + 1; ++it)
				c1.push_back(it->close);
			std::vector<double> c0;
			c0.reserve(20);
			for (auto it = bars.begin() + start0; it != bars.begin() + i; ++it)
				c0.push_back(it->close);
			double ma1 = CSignalAnalyzer::CalcMA(c1, 20);
			double ma0 = CSignalAnalyzer::CalcMA(c0, 20);
			if (ma1 <= 0 || ma0 <= 0) continue;

			// 收盘价从一侧穿越到另一侧
			bool prevAbove = bars[i - 1].close > ma0;
			bool currAbove = bars[i].close > ma1;
			if (prevAbove != currAbove)
				crossings++;
		}
		return crossings;
	}

	// 计算5分钟K线中上涨K线放量/回调缩量的特征
	// 返回：上涨放量占比（0~1，越高越符合多头量能特征）
	double CalcUpVolumeRatio(const std::vector<STOCK::Bar>& bars5)
	{
		if (bars5.size() < 6) return 0.5;

		double upVol = 0, downVol = 0;
		int upCount = 0, downCount = 0;

		for (size_t i = bars5.size() - 6; i < bars5.size(); i++)
		{
			if (i < 1) continue;
			bool isUp = bars5[i].close > bars5[i - 1].close;
			if (isUp) { upVol += bars5[i].volume; upCount++; }
			else { downVol += bars5[i].volume; downCount++; }
		}

		if (upCount == 0 || downCount == 0) return 0.5;
		double avgUpVol = upVol / upCount;
		double avgDownVol = downVol / downCount;
		// 上涨平均量 > 回调平均量时，返回值 > 0.5
		double total = avgUpVol + avgDownVol;
		return total > 0 ? avgUpVol / total : 0.5;
	}
}

// 30分钟上升结构判定
bool CSignalAnalyzer::Calc30UpStruct(const std::vector<STOCK::Bar>& bars30)
{
	if (bars30.size() < TREND_30M_LOOKBACK)
		return false;

	// 取最近25根30分钟K线
	std::vector<STOCK::Bar> recent(bars30.end() - TREND_30M_LOOKBACK, bars30.end());

	// 提取波段高低点
	auto swings = ExtractSwingPoints(recent);
	std::vector<double> swingHighs, swingLows;
	SeparateSwingPoints(swings, swingHighs, swingLows);

	// 条件1：波段低点逐次抬高
	bool lowAscending = IsAscending(swingLows);
	// 条件2：波段高点逐次抬高
	bool highAscending = IsAscending(swingHighs);
	// 条件3：最新收盘价 > MA20，且MA20向上倾斜
	double slope = CalcMA20Slope(recent);
	std::vector<double> closes;
	closes.reserve(20);
	for (auto it = recent.end() - 20; it != recent.end(); ++it)
		closes.push_back(it->close);
	double ma20 = CalcMA(closes, 20);
	bool priceAboveMA20 = recent.back().close > ma20;
	bool ma20Up = slope > MA20_SLOPE_THRESHOLD;

	return lowAscending && highAscending && priceAboveMA20 && ma20Up;
}

// 30分钟下跌结构判定
bool CSignalAnalyzer::Calc30DownStruct(const std::vector<STOCK::Bar>& bars30)
{
	if (bars30.size() < TREND_30M_LOOKBACK)
		return false;

	std::vector<STOCK::Bar> recent(bars30.end() - TREND_30M_LOOKBACK, bars30.end());

	auto swings = ExtractSwingPoints(recent);
	std::vector<double> swingHighs, swingLows;
	SeparateSwingPoints(swings, swingHighs, swingLows);

	// 条件1：波段高点逐次降低
	bool highDescending = IsDescending(swingHighs);
	// 条件2：波段低点逐次降低
	bool lowDescending = IsDescending(swingLows);
	// 条件3：最新收盘价 < MA20，且MA20向下倾斜
	double slope = CalcMA20Slope(recent);
	std::vector<double> closes;
	closes.reserve(20);
	for (auto it = recent.end() - 20; it != recent.end(); ++it)
		closes.push_back(it->close);
	double ma20 = CalcMA(closes, 20);
	bool priceBelowMA20 = recent.back().close < ma20;
	bool ma20Down = slope < -MA20_SLOPE_THRESHOLD;

	return highDescending && lowDescending && priceBelowMA20 && ma20Down;
}

// 30分钟震荡结构判定（满足任意两条即判定为震荡）
bool CSignalAnalyzer::Calc30SideStruct(const std::vector<STOCK::Bar>& bars30)
{
	if (bars30.size() < TREND_30M_LOOKBACK)
		return true;  // 数据不足默认震荡

	std::vector<STOCK::Bar> recent(bars30.end() - TREND_30M_LOOKBACK, bars30.end());

	// 条件1：MA20走平（斜率接近0）
	double slope = CalcMA20Slope(recent);
	bool ma20Flat = fabs(slope) <= MA20_SLOPE_THRESHOLD;

	// 条件2：高低点来回交叉，无持续抬高/降低
	auto swings = ExtractSwingPoints(recent);
	std::vector<double> swingHighs, swingLows;
	SeparateSwingPoints(swings, swingHighs, swingLows);
	bool crossing = IsSwingCrossing(swingHighs, swingLows);

	// 条件3：价格频繁穿越MA20均线
	int crossings = CountMACrossings(recent, 20);
	bool frequentCross = crossings >= 3;

	int sideScore = (ma20Flat ? 1 : 0) + (crossing ? 1 : 0) + (frequentCross ? 1 : 0);
	return sideScore >= 2;
}

// 5分钟短线多头判定
bool CSignalAnalyzer::Calc5MinUp(const std::vector<STOCK::Bar>& bars5)
{
	if (bars5.size() < TREND_5M_LOOKBACK)
		return false;

	std::vector<STOCK::Bar> recent(bars5.end() - TREND_5M_LOOKBACK, bars5.end());

	// 条件1：5分钟短期低点抬高
	auto swings = ExtractSwingPoints(recent, 2);  // 5分钟用更短的极值间隔
	std::vector<double> swingHighs, swingLows;
	SeparateSwingPoints(swings, swingHighs, swingLows);
	bool lowAscending = IsAscending(swingLows);

	// 条件2：价格站稳5分钟MA5
	std::vector<double> closes5;
	closes5.reserve(5);
	for (auto it = recent.end() - 5; it != recent.end(); ++it)
		closes5.push_back(it->close);
	double ma5 = CalcMA(closes5, 5);
	bool aboveMA5 = recent.back().close > ma5;
	// 近3根K线收盘价都在MA5上方
	bool stableAboveMA5 = true;
	for (int i = static_cast<int>(recent.size()) - 3; i < static_cast<int>(recent.size()); i++)
	{
		if (i < 0) continue;
		std::vector<double> c;
		c.reserve(5);
		for (auto it = recent.begin() + (i - 4 > 0 ? i - 4 : 0); it != recent.begin() + i + 1; ++it)
			c.push_back(it->close);
		if (static_cast<int>(c.size()) >= 5)
		{
			double m = CalcMA(c, 5);
			if (recent[i].close <= m) { stableAboveMA5 = false; break; }
		}
	}

	// 条件3：上涨K线放量、回调缩量
	double volRatio = CalcUpVolumeRatio(recent);
	bool upVolDownShrink = volRatio > (1.0 / (1.0 + VOL_RATIO_THRESHOLD));

	// 至少满足2条
	int score = (lowAscending ? 1 : 0) + (aboveMA5 && stableAboveMA5 ? 1 : 0) + (upVolDownShrink ? 1 : 0);
	return score >= 2;
}

// 5分钟短线空头判定
bool CSignalAnalyzer::Calc5MinDown(const std::vector<STOCK::Bar>& bars5)
{
	if (bars5.size() < TREND_5M_LOOKBACK)
		return false;

	std::vector<STOCK::Bar> recent(bars5.end() - TREND_5M_LOOKBACK, bars5.end());

	// 条件1：5分钟短期高点降低
	auto swings = ExtractSwingPoints(recent, 2);
	std::vector<double> swingHighs, swingLows;
	SeparateSwingPoints(swings, swingHighs, swingLows);
	bool highDescending = IsDescending(swingHighs);

	// 条件2：价格持续在5分钟MA5下方
	std::vector<double> closes5;
	closes5.reserve(5);
	for (auto it = recent.end() - 5; it != recent.end(); ++it)
		closes5.push_back(it->close);
	double ma5 = CalcMA(closes5, 5);
	bool belowMA5 = recent.back().close < ma5;
	// 近3根K线收盘价都在MA5下方
	bool stableBelowMA5 = true;
	for (int i = static_cast<int>(recent.size()) - 3; i < static_cast<int>(recent.size()); i++)
	{
		if (i < 0) continue;
		std::vector<double> c;
		c.reserve(5);
		for (auto it = recent.begin() + (i - 4 > 0 ? i - 4 : 0); it != recent.begin() + i + 1; ++it)
			c.push_back(it->close);
		if (static_cast<int>(c.size()) >= 5)
		{
			double m = CalcMA(c, 5);
			if (recent[i].close >= m) { stableBelowMA5 = false; break; }
		}
	}

	// 条件3：下跌放量、反弹缩量（即上涨量 < 下跌量）
	double volRatio = CalcUpVolumeRatio(recent);
	bool downVolUpShrink = volRatio < (1.0 / (1.0 + VOL_RATIO_THRESHOLD));

	int score = (highDescending ? 1 : 0) + (belowMA5 && stableBelowMA5 ? 1 : 0) + (downVolUpShrink ? 1 : 0);
	return score >= 2;
}

// 内外盘净比计算
double CSignalAnalyzer::CalcOuterInnerRatio(STOCK::Volume outerVol, STOCK::Volume innerVol)
{
	STOCK::Volume total = outerVol + innerVol;
	if (total <= 0) return 0.0;
	return static_cast<double>(outerVol - innerVol) / total;
}

// 完整趋势判定主函数：5分钟+30分钟双周期共振
STOCK::TrendResult CSignalAnalyzer::CalcTrend(
	const std::vector<STOCK::Bar>& bars5,
	const std::vector<STOCK::Bar>& bars30,
	STOCK::Volume outerVol,
	STOCK::Volume innerVol)
{
	STOCK::TrendResult result;

	// ===== 步骤1：30分钟波段结构判定 =====
	bool b30Up = Calc30UpStruct(bars30);
	bool b30Down = Calc30DownStruct(bars30);
	bool b30Side = Calc30SideStruct(bars30);
	(void)b30Side;  // 震荡标记已通过 baseDir=DIR_SIDE 间接使用

	// ===== 步骤2：确定底层大方向 =====
	STOCK::TrendDir baseDir;
	if (b30Up)
		baseDir = STOCK::TrendDir::DIR_UP;
	else if (b30Down)
		baseDir = STOCK::TrendDir::DIR_DOWN;
	else
		baseDir = STOCK::TrendDir::DIR_SIDE;

	result.BaseDir = baseDir;

	// ===== 步骤3：5分钟短线多空判定 =====
	bool b5Up = Calc5MinUp(bars5);
	bool b5Down = Calc5MinDown(bars5);
	result.Is5Up = b5Up;
	result.Is5Down = b5Down;

	// ===== 步骤4：双周期共振合并 =====
	if (baseDir == STOCK::TrendDir::DIR_UP)
	{
		result.FinalTrend = STOCK::TrendDir::DIR_UP;
		result.IsShortPullback = b5Down;  // 5分钟空头标记短线回调
	}
	else if (baseDir == STOCK::TrendDir::DIR_DOWN)
	{
		result.FinalTrend = STOCK::TrendDir::DIR_DOWN;
		result.IsShortRebound = b5Up;     // 5分钟多头标记短线反弹
	}
	else  // DIR_SIDE
	{
		result.FinalTrend = STOCK::TrendDir::DIR_SIDE;
		if (b5Up)
			result.SideTagValue = STOCK::SideTag::SIDE_LONG_POINT;   // 震荡下轨低吸点
		else if (b5Down)
			result.SideTagValue = STOCK::SideTag::SIDE_SHORT_POINT;  // 震荡上轨高抛点
		else
			result.SideTagValue = STOCK::SideTag::SIDE_MID;          // 震荡中间，观望
	}

	// ===== 步骤5：内外盘净比辅助校验 =====
	result.OuterInnerRatio = CalcOuterInnerRatio(outerVol, innerVol);

	return result;
}

// ========== 智能分析模块：5分钟共振买卖判定 ==========
// 输入：完整5分钟K线 bars5
// 输出：SIG_SELL/SIG_BUY/SIG_NONE
// 规则：必要条件组+辅助条件组（解决指标共线性）

STOCK::Signal5m CSignalAnalyzer::Get5mSignal(const std::vector<STOCK::Bar>& bars5, STOCK::TrendState30m trendState)
{
	if (bars5.size() < 60)
		return STOCK::Signal5m::SIG_NONE;

	const int barCount = static_cast<int>(bars5.size());
	const STOCK::Bar& last = bars5.back();

	// 1. 指标批量计算
	STOCK::BollResult boll = CalcBoll(bars5, 20);
	std::vector<double> difSeq, deaSeq, macdBarSeq;
	CalcMACDSeries(bars5, difSeq, deaSeq, macdBarSeq);
	std::vector<double> kSeq, dSeq, jSeq;
	CalcKDJSeries(bars5, 9, kSeq, dSeq, jSeq);
	if (difSeq.size() != bars5.size() || deaSeq.size() != bars5.size() || macdBarSeq.size() != bars5.size()
		|| kSeq.size() != bars5.size() || dSeq.size() != bars5.size() || jSeq.size() != bars5.size())
		return STOCK::Signal5m::SIG_NONE;

	size_t lastIndex = bars5.size() - 1;
	size_t prevIndex = lastIndex - 1;
	STOCK::MACDResult macd;
	macd.dif = difSeq[lastIndex];
	macd.dea = deaSeq[lastIndex];
	macd.macd_bar = macdBarSeq[lastIndex];
	STOCK::MACDResult prevMacd;
	prevMacd.dif = difSeq[prevIndex];
	prevMacd.dea = deaSeq[prevIndex];
	prevMacd.macd_bar = macdBarSeq[prevIndex];
	STOCK::KDJResult kdj;
	kdj.k = kSeq[lastIndex];
	kdj.d = dSeq[lastIndex];
	kdj.j = jSeq[lastIndex];
	STOCK::KDJResult prevKdj;
	prevKdj.k = kSeq[prevIndex];
	prevKdj.d = dSeq[prevIndex];
	prevKdj.j = jSeq[prevIndex];
	double rsi6 = CalcRSI(bars5, 6);
	double wr6 = CalcWR(bars5, 6);

	// ========== 卖出判定（必要条件组+辅助条件组） ==========
	// 必要条件组：价格突破轨道(S1) + MACD动量减弱(S2) 至少满足1个
	bool sellS1 = (last.high >= boll.up) || (last.close > boll.up);
	bool sellS2_redShrink = (macd.macd_bar > 0) && (macd.macd_bar < prevMacd.macd_bar);
	bool sellS2_topDiv = (last.high > bars5[barCount - 2].high) && (macd.dif < prevMacd.dif);
	bool sellS2 = sellS2_redShrink || sellS2_topDiv;
	bool sellNecessary = (sellS1 || sellS2);

	// 辅助条件组：KDJ/RSI/WR 三个超买指标满足1个即可触发卖出辅助
	bool sellA1 = ((kdj.k > KDJ_OVERBUY && kdj.d > SELL_KDJ_D_OVERBUY && kdj.j < prevKdj.j) || kdj.j > 100.0);
	bool sellA2 = (rsi6 > 70);
	bool sellA3 = (wr6 < SELL_WR_OVERBUY);
	int sellAuxCount = (sellA1 ? 1 : 0) + (sellA2 ? 1 : 0) + (sellA3 ? 1 : 0);

	bool hasSell = sellNecessary && (sellAuxCount >= 1);

	// ========== 买入判定（必要条件组+辅助条件组） ==========
	// 必要条件组：价格触及轨道(B1) + MACD信号(B2) 至少满足1个
	bool buyB1 = (last.low <= boll.dn);
	bool buyB2_greenShrink = (macd.macd_bar < 0) && (macd.macd_bar > prevMacd.macd_bar) && fabs(macd.dif) > GetMacdDifThreshold(last.close);
	bool buyB2_bottomDiv = (last.low < bars5[barCount - 2].low) && (macd.dif > prevMacd.dif) && fabs(macd.dif) > GetMacdDifThreshold(last.close);
	bool buyB2 = buyB2_greenShrink || buyB2_bottomDiv;
	bool buyNecessary = (buyB1 || buyB2);

	// 辅助条件组：KDJ/RSI/WR 三个超卖指标至少满足2个
	bool buyA1 = (kdj.k < KDJ_OVERSELL && kdj.d < KDJ_OVERSELL && kdj.j > prevKdj.j);
	bool buyA2 = (rsi6 < 30);
	bool buyA3 = (wr6 > 80);
	int buyAuxCount = (buyA1 ? 1 : 0) + (buyA2 ? 1 : 0) + (buyA3 ? 1 : 0);

	bool hasBuy = buyNecessary && (buyAuxCount >= 2);

	// 信号输出
	if (hasSell && hasBuy)
	{
		if (sellAuxCount >= buyAuxCount)
			return STOCK::Signal5m::SIG_SELL;
		if (trendState == STOCK::TrendState30m::STATE_STRONG)
			return STOCK::Signal5m::SIG_BUY;
		if (trendState == STOCK::TrendState30m::STATE_WEAK)
			return STOCK::Signal5m::SIG_SELL;

		return STOCK::Signal5m::SIG_BUY;
	}
	if (hasSell) return STOCK::Signal5m::SIG_SELL;
	if (hasBuy) return STOCK::Signal5m::SIG_BUY;
	return STOCK::Signal5m::SIG_NONE;
}

// ========== 智能分析模块：单边钝化风控过滤器 ==========
// 只要触发禁止操作，直接忽略上面买卖信号

bool CSignalAnalyzer::IsForbidTrade(const std::vector<STOCK::Bar>& bars5, STOCK::TrendState30m trendState)
{
	if (bars5.size() < 60)
		return false;

	// 布林带宽放大：使用当前带宽增长率或历史高分位，避免强制连续三根导致风控滞后
	STOCK::BollResult boll = CalcBoll(bars5, 20);
	std::vector<double> bollBandSeq(bars5.size(), 0.0);
	for (size_t i = 19; i < bars5.size(); i++)
	{
		std::vector<double> closes;
		closes.reserve(20);
		for (size_t j = i - 19; j <= i; j++)
			closes.push_back(bars5[j].close);
		bollBandSeq[i] = 4.0 * CalcStdDev(closes);
	}
	bool bandExpand = IsBollExpand(bollBandSeq, bars5.size() - 1);
	double avgBollBandwidth = CalcAverageBollBandwidth(bollBandSeq, bars5.size() - 1);
	bool narrowBand = IsNarrowBoll(boll.mid, avgBollBandwidth);

	// KDJ钝化
	std::vector<STOCK::Bar> prevBars(bars5.begin(), bars5.end() - 1);
	std::vector<STOCK::Bar> prevPrevBars(bars5.begin(), bars5.end() - 2);
	STOCK::KDJResult kdj = CalcKDJ(bars5, 9);
	STOCK::KDJResult prevKdj = CalcKDJ(prevBars, 9);
	STOCK::KDJResult prevPrevKdj = CalcKDJ(prevPrevBars, 9);
	bool kdjTopPassive = (kdj.k > 85 && prevKdj.k > 85 && prevPrevKdj.k > 85
		&& kdj.k < prevKdj.k && prevKdj.k < prevPrevKdj.k);
	bool kdjBottomPassive = (kdj.k < 15 && prevKdj.k < 15 && prevPrevKdj.k < 15
		&& kdj.k > prevKdj.k && prevKdj.k > prevPrevKdj.k);

	// WR钝化
	double wr6 = CalcWR(bars5, 6);
	double wrPrev = CalcWR(prevBars, 6);
	double wrPrevPrev = CalcWR(prevPrevBars, 6);
	bool wrTopPassive = (wr6 < 18 && wrPrev < 18 && wrPrevPrev < 18
		&& wr6 > wrPrev && wrPrev > wrPrevPrev);
	bool wrBottomPassive = (wr6 > 82 && wrPrev > 82 && wrPrevPrev > 82
		&& wr6 < wrPrev && wrPrev < wrPrevPrev);

	// 风控判定：只限制低吸/买入风险，不屏蔽顺势高抛卖出
	if (trendState == STOCK::TrendState30m::STATE_WEAK)
		return false;
	if (trendState == STOCK::TrendState30m::STATE_STRONG)
		return kdjBottomPassive || wrBottomPassive;
	return bandExpand || narrowBand || kdjBottomPassive || wrBottomPassive;
}

// ========== 智能分析模块：完整买卖点判定函数 ==========
// 严格按执行顺序：30min趋势 → 风控过滤 → 5min共振信号 → 趋势分支处理

CSignalAnalyzer::T0Signal CSignalAnalyzer::DetectSmartSignal(const std::vector<STOCK::Bar>& bars5, const std::vector<STOCK::Bar>& bars30, double lastSignalPrice)
{
	T0Signal signal;

	// ===== 第1步：获取30分钟全局趋势 =====
	STOCK::TrendState30m trendState = Get30mTrendState(bars30);
	signal.trendState = trendState;

	// ===== 第2步：先判断是否禁止交易（风控过滤） =====
	bool forbidTrade = IsForbidTrade(bars5, trendState);

	// ===== 第3步：获取5分钟共振买卖信号 =====
	STOCK::Signal5m sig = Get5mSignal(bars5, trendState);

	if (sig == STOCK::Signal5m::SIG_NONE)
	{
		signal.valid = false;
		return signal;
	}

	if (forbidTrade && sig != STOCK::Signal5m::SIG_SELL)
	{
		signal.isForbid = true;
		signal.valid = false;
		signal.reason = _T("风控限制买入，停止低吸做T");
		return signal;
	}

	// ===== 第4步：分趋势分支处理信号 =====
	if (lastSignalPrice > 0 && fabs(bars5.back().close - lastSignalPrice) < GetSignalPriceDiffThreshold(bars5, bars5.size() - 1))
	{
		signal.valid = false;
		return signal;
	}

	signal.valid = true;
	signal.price = bars5.back().close;
	signal.strength = 3;  // 共振信号强度固定为3（5项≥4项共振，高置信度）

	if (trendState == STOCK::TrendState30m::STATE_WEAK)
	{
		// 弱势：只允许卖出（反T高抛），买入仅用来等量接回，禁止加仓
		if (sig == STOCK::Signal5m::SIG_SELL)
		{
			signal.isBuy = false;
			signal.reason = _T("反卖");
		}
		else if (sig == STOCK::Signal5m::SIG_BUY)
		{
			signal.isBuy = true;
			signal.reason = _T("反买");
		}
	}
	else if (trendState == STOCK::TrendState30m::STATE_STRONG)
	{
		// 强势：买入可加仓做正T，卖出止盈
		if (sig == STOCK::Signal5m::SIG_BUY)
		{
			signal.isBuy = true;
			signal.reason = _T("正买");
		}
		else if (sig == STOCK::Signal5m::SIG_SELL)
		{
			signal.isBuy = false;
			signal.reason = _T("正卖");
		}
	}
	else  // STATE_SHAKE
	{
		// 震荡：买卖信号自由执行
		if (sig == STOCK::Signal5m::SIG_BUY)
		{
			signal.isBuy = true;
			signal.reason = _T("正买");
		}
		else if (sig == STOCK::Signal5m::SIG_SELL)
		{
			signal.isBuy = false;
			signal.reason = _T("正卖");
		}
	}

	return signal;
}

// ========== 智能分析模块：MACD批量序列计算 ==========
void CSignalAnalyzer::CalcMACDSeries(const std::vector<STOCK::Bar>& bars, std::vector<double>& difSeq, std::vector<double>& deaSeq, std::vector<double>& barSeq)
{
	difSeq.clear(); deaSeq.clear(); barSeq.clear();
	size_t n = bars.size();
	if (n < 2) { difSeq.resize(n, 0); deaSeq.resize(n, 0); barSeq.resize(n, 0); return; }

	std::vector<double> ema12(n), ema26(n);
	ema12[0] = bars[0].close;
	ema26[0] = bars[0].close;
	double k12 = 2.0 / 13.0;
	double k26 = 2.0 / 27.0;
	for (size_t i = 1; i < n; i++)
	{
		ema12[i] = bars[i].close * k12 + ema12[i - 1] * (1.0 - k12);
		ema26[i] = bars[i].close * k26 + ema26[i - 1] * (1.0 - k26);
	}

	difSeq.resize(n); deaSeq.resize(n); barSeq.resize(n);
	deaSeq[0] = difSeq[0];
	for (size_t i = 0; i < n; i++)
		difSeq[i] = ema12[i] - ema26[i];

	double k9 = 2.0 / 10.0;
	deaSeq[0] = difSeq[0];
	for (size_t i = 1; i < n; i++)
		deaSeq[i] = difSeq[i] * k9 + deaSeq[i - 1] * (1.0 - k9);

	for (size_t i = 0; i < n; i++)
		barSeq[i] = 2.0 * (difSeq[i] - deaSeq[i]);
}

// ========== 智能分析模块：批量信号检测 ==========
// 一次性计算全部指标序列，然后逐根判定信号

std::vector<CSignalAnalyzer::SmartSignalPoint> CSignalAnalyzer::BatchDetectSignals(const std::vector<STOCK::Bar>& bars5, const std::vector<STOCK::Bar>& bars30)
{
	std::vector<SmartSignalPoint> result;
	size_t n = bars5.size();
	// EMA稳定性：至少60根K线用于盘中短线检测
	if (n < 60) return result;

	// 1. 30分钟趋势（一次性）
	STOCK::TrendState30m trendState = Get30mTrendState(bars30);

	// 2. 批量计算5分钟指标序列
	// BOLL序列
	std::vector<double> bollMid(n, 0), bollUp(n, 0), bollDn(n, 0), bollBand(n, 0);
	for (size_t i = 19; i < n; i++)
	{
		std::vector<double> closes;
		for (size_t j = i - 19; j <= i; j++) closes.push_back(bars5[j].close);
		double mid = CalcMA(closes, 20);
		double sd = CalcStdDev(closes);
		bollMid[i] = mid;
		bollUp[i] = mid + 2.0 * sd;
		bollDn[i] = mid - 2.0 * sd;
		bollBand[i] = 4.0 * sd;
	}

	// MACD序列
	std::vector<double> difSeq, deaSeq, macdBarSeq;
	CalcMACDSeries(bars5, difSeq, deaSeq, macdBarSeq);

	// KDJ序列：每根K线先按过去N日最高/最低计算当前RSV，再从第一根有效K线开始递推K/D
	std::vector<double> kSeq, dSeq, jSeq;
	CalcKDJSeries(bars5, 9, kSeq, dSeq, jSeq);

	// RSI6序列
	std::vector<double> rsi6Seq(n, 50);
	{
		if (n >= 7)
		{
			std::vector<double> upList, dnList;
			for (size_t i = 1; i < n; i++) { double d = bars5[i].close - bars5[i - 1].close; upList.push_back(max(d, 0.0)); dnList.push_back(max(-d, 0.0)); }
			double au = 0, ad = 0;
			for (size_t i = 0; i < 6 && i < upList.size(); i++) { au += upList[i]; ad += dnList[i]; }
			au /= 6.0; ad /= 6.0;
			if (ad == 0) rsi6Seq[6] = 99.99; else rsi6Seq[6] = 100.0 - 100.0 / (1.0 + au / ad);
			for (size_t i = 6; i < upList.size(); i++)
			{
				au = (upList[i] + 5.0 * au) / 6.0;
				ad = (dnList[i] + 5.0 * ad) / 6.0;
				if (ad == 0) rsi6Seq[i + 1] = 99.99; else rsi6Seq[i + 1] = 100.0 - 100.0 / (1.0 + au / ad);
			}
		}
	}

	// WR6序列
	std::vector<double> wr6Seq(n, 50);
	for (size_t i = 5; i < n; i++)
	{
		double hhv = bars5[i - 5].high, llv = bars5[i - 5].low;
		for (size_t j = i - 4; j <= i; j++) { if (bars5[j].high > hhv) hhv = bars5[j].high; if (bars5[j].low < llv) llv = bars5[j].low; }
		wr6Seq[i] = (hhv != llv) ? 100.0 * (hhv - bars5[i].close) / (hhv - llv) : 50.0;
	}

	// 3. 逐根K线检测信号（使用预计算序列，缓存上一根指标结果）
	double lastSignalPrice = 0;
	int lastForbidBarIndex = -MIN_SIGNAL_BAR_GAP;
	for (size_t i = 21; i < n; i++)
	{
		// ===== 风控过滤（优化版） =====
		// 布林带宽放大：使用当前带宽增长率或历史高分位，避免强制连续三根导致风控滞后
		bool bandExpand = IsBollExpand(bollBand, i);

		// KDJ钝化：不再一刀切禁止，根据30分钟趋势动态处理
		// 强势行情允许KDJ高位钝化（趋势延续），弱势行情收紧超买阈值
		bool kdjTopPassive = (i >= 2 && kSeq[i] > 85 && kSeq[i - 1] > 85 && kSeq[i - 2] > 85
			&& kSeq[i] < kSeq[i - 1] && kSeq[i - 1] < kSeq[i - 2]);
		bool kdjBottomPassive = (i >= 2 && kSeq[i] < 15 && kSeq[i - 1] < 15 && kSeq[i - 2] < 15
			&& kSeq[i] > kSeq[i - 1] && kSeq[i - 1] > kSeq[i - 2]);

		// WR钝化：连续3根在极端区域且停滞
		bool wrTopPassive = (i >= 2 && wr6Seq[i] < 18 && wr6Seq[i - 1] < 18 && wr6Seq[i - 2] < 18
			&& wr6Seq[i] > wr6Seq[i - 1] && wr6Seq[i - 1] > wr6Seq[i - 2]);
		bool wrBottomPassive = (i >= 2 && wr6Seq[i] > 82 && wr6Seq[i - 1] > 82 && wr6Seq[i - 2] > 82
			&& wr6Seq[i] < wr6Seq[i - 1] && wr6Seq[i - 1] < wr6Seq[i - 2]);

		// 风控判定：只限制低吸/买入风险，不屏蔽顺势高抛卖出
		bool forbid = false;
		bool narrowBand = IsNarrowBoll(bollMid[i], CalcAverageBollBandwidth(bollBand, i));
		if (trendState == STOCK::TrendState30m::STATE_STRONG)
		{
			forbid = kdjBottomPassive || wrBottomPassive;
		}
		else if (trendState == STOCK::TrendState30m::STATE_SHAKE)
		{
			forbid = bandExpand || narrowBand || kdjBottomPassive || wrBottomPassive;
		}

		if (forbid && static_cast<int>(i) - lastForbidBarIndex >= MIN_SIGNAL_BAR_GAP)
		{
			result.push_back({ static_cast<int>(i), false, true, trendState, _T("风控限制买入") });
			lastForbidBarIndex = static_cast<int>(i);
		}

		// ===== 信号判定（优化版：必要条件组+辅助条件组，解决指标共线性） =====

		// --- 卖出判定 ---
		// 必要条件组：价格突破轨道(S1) + MACD动量减弱(S2) 至少满足1个
		bool sellS1 = (bars5[i].high >= bollUp[i]) || (bars5[i].close > bollUp[i]);
		bool sellS2_redShrink = (macdBarSeq[i] > 0) && (macdBarSeq[i] < macdBarSeq[i - 1]);
		bool sellS2_topDiv = (bars5[i].high > bars5[i - 1].high) && (difSeq[i] < difSeq[i - 1]);
		bool sellS2 = sellS2_redShrink || sellS2_topDiv;
		bool sellNecessary = (sellS1 || sellS2);

		// 辅助条件组：KDJ/RSI/WR 三个超买指标满足1个即可触发卖出辅助
		bool sellA1 = ((kSeq[i] > KDJ_OVERBUY && dSeq[i] > SELL_KDJ_D_OVERBUY && jSeq[i] < jSeq[i - 1]) || jSeq[i] > 100.0);
		bool sellA2 = (rsi6Seq[i] > 70);
		bool sellA3 = (wr6Seq[i] < SELL_WR_OVERBUY);
		int sellAuxCount = (sellA1 ? 1 : 0) + (sellA2 ? 1 : 0) + (sellA3 ? 1 : 0);

		bool hasSell = sellNecessary && (sellAuxCount >= 1);

		// --- 买入判定 ---
		// 必要条件组：价格触及轨道(B1) + MACD信号(B2) 至少满足1个
		bool buyB1 = (bars5[i].low <= bollDn[i]);
		// MACD：绿柱缩短 or 底背离（DIF远离0轴才认定有效动量信号）
		double macdDifThreshold = GetMacdDifThreshold(bars5[i].close);
		bool buyB2_greenShrink = (macdBarSeq[i] < 0) && (macdBarSeq[i] > macdBarSeq[i - 1]) && fabs(difSeq[i]) > macdDifThreshold;
		bool buyB2_bottomDiv = (bars5[i].low < bars5[i - 1].low) && (difSeq[i] > difSeq[i - 1]) && fabs(difSeq[i]) > macdDifThreshold;
		bool buyB2 = buyB2_greenShrink || buyB2_bottomDiv;
		bool buyNecessary = (buyB1 || buyB2);

		// 辅助条件组：KDJ/RSI/WR 三个超卖指标至少满足2个
		bool buyA1 = (kSeq[i] < KDJ_OVERSELL && dSeq[i] < KDJ_OVERSELL && jSeq[i] > jSeq[i - 1]);
		bool buyA2 = (rsi6Seq[i] < 30);
		bool buyA3 = (wr6Seq[i] > 80);
		int buyAuxCount = (buyA1 ? 1 : 0) + (buyA2 ? 1 : 0) + (buyA3 ? 1 : 0);

		bool hasBuy = buyNecessary && (buyAuxCount >= 2);

		if (!hasSell && !hasBuy) continue;
		if (forbid && hasBuy && !hasSell)
			continue;
		if (lastSignalPrice > 0 && fabs(bars5[i].close - lastSignalPrice) < GetSignalPriceDiffThreshold(bars5, i))
			continue;

		// 买卖同时满足时，卖出辅助不弱于买入则优先保留卖点
		bool isBuy;
		if (hasSell && hasBuy)
		{
			if (sellAuxCount >= buyAuxCount) isBuy = false;
			else if (trendState == STOCK::TrendState30m::STATE_STRONG) isBuy = true;
			else if (trendState == STOCK::TrendState30m::STATE_WEAK) isBuy = false;
			else isBuy = true;
		}
		else
		{
			isBuy = hasBuy;
		}

		CString reason;
		if (trendState == STOCK::TrendState30m::STATE_WEAK)
			reason = isBuy ? _T("反买") : _T("反卖");
		else if (trendState == STOCK::TrendState30m::STATE_STRONG)
			reason = isBuy ? _T("正买") : _T("正卖");
		else
			reason = isBuy ? _T("正买") : _T("正卖");

		result.push_back({ static_cast<int>(i), isBuy, false, trendState, reason });
		lastSignalPrice = bars5[i].close;
	}

	// 4. 信号去重
	std::vector<SmartSignalPoint> filtered;
	filtered.reserve(result.size());
	int lastSignalBarIndex = -MIN_SIGNAL_BAR_GAP;
	for (const auto& point : result)
	{
		if (point.isForbid)
		{
			filtered.push_back(point);
			continue;
		}
		if (point.barIndex - lastSignalBarIndex >= MIN_SIGNAL_BAR_GAP)
		{
			filtered.push_back(point);
			lastSignalBarIndex = point.barIndex;
		}
	}
	result.swap(filtered);

	return result;
}