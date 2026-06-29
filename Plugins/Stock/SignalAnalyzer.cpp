#include "pch.h"
#include "SignalAnalyzer.h"
#include "StockDef.h"
#include <cmath>

namespace
{
	constexpr double MACD_DIF_THRESHOLD = 0.001;
	constexpr double MIN_SIGNAL_PRICE_DIFF = 0.003;
	constexpr double NARROW_BOLL_RATIO = 0.008;
	constexpr double BOLL_EXPAND_RATIO = 1.1;
	constexpr double KDJ_OVERBUY = 80.0;
	constexpr double KDJ_OVERSELL = 20.0;
	constexpr int MIN_SIGNAL_BAR_GAP = 5;
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
	if (static_cast<int>(bars.size()) < N)
		return result;

	// 对RSV序列做SMA得到K，对K序列做SMA得到D
	// 需要历史RSV序列才能正确递推
	std::vector<double> rsvSeq;
	for (int start = static_cast<int>(bars.size()) - N - N + 1; start <= static_cast<int>(bars.size()) - N; start++)
	{
		if (start < 0) continue;
		double lowN = bars[start].low;
		double highN = bars[start].high;
		for (int j = start + 1; j < start + N && j < static_cast<int>(bars.size()); j++)
		{
			if (bars[j].low < lowN) lowN = bars[j].low;
			if (bars[j].high > highN) highN = bars[j].high;
		}
		int endIdx = min(start + N - 1, static_cast<int>(bars.size()) - 1);
		double c = bars[endIdx].close;
		double r = (highN != lowN) ? (c - lowN) / (highN - lowN) * 100.0 : 50.0;
		rsvSeq.push_back(r);
	}

	// 用SMA(3)递推K和D
	double k = 50.0, d = 50.0;  // K、D初始值50
	for (double rsv_val : rsvSeq)
	{
		k = (2.0 / 3.0) * k + (1.0 / 3.0) * rsv_val;   // SMA(RSV,3,1)
		d = (2.0 / 3.0) * d + (1.0 / 3.0) * k;          // SMA(K,3,1)
	}

	result.k = k;
	result.d = d;
	result.j = 3.0 * k - 2.0 * d;
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
	// 至少需要21根30min K线才能计算当前和前一根的BOLL
	if (bars30.size() < 22)
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

	// 判定行情
	bool condWeak = bollDown && rsiWeak && macBelow;
	bool condStrong = bollUp && rsiStrong && macAbove;

	if (condWeak)
		return STOCK::TrendState30m::STATE_WEAK;    // 弱势：只做反T，禁止加仓正T
	else if (condStrong)
		return STOCK::TrendState30m::STATE_STRONG;  // 强势：回踩可低吸正T
	else
		return STOCK::TrendState30m::STATE_SHAKE;   // 震荡：正反T均可
}

// ========== 智能分析模块：5分钟共振买卖判定 ==========
// 输入：完整5分钟K线 bars5
// 输出：SIG_SELL/SIG_BUY/SIG_NONE
// 规则：必要条件组+辅助条件组（解决指标共线性）

STOCK::Signal5m CSignalAnalyzer::Get5mSignal(const std::vector<STOCK::Bar>& bars5, STOCK::TrendState30m trendState)
{
	if (bars5.size() < 120)
		return STOCK::Signal5m::SIG_NONE;

	const int barCount = static_cast<int>(bars5.size());
	const STOCK::Bar& last = bars5.back();

	// 1. 指标批量计算
	STOCK::BollResult boll = CalcBoll(bars5, 20);
	STOCK::MACDResult macd = CalcMACD(bars5);
	STOCK::KDJResult kdj = CalcKDJ(bars5, 9);
	double rsi6 = CalcRSI(bars5, 6);
	double wr6 = CalcWR(bars5, 6);

	// 计算前一根bars5的MACD和KDJ（用于对比，无未来函数）
	std::vector<STOCK::Bar> prevBars(bars5.begin(), bars5.end() - 1);
	STOCK::MACDResult prevMacd = CalcMACD(prevBars);
	STOCK::KDJResult prevKdj = CalcKDJ(prevBars, 9);

	// ========== 卖出判定（必要条件组+辅助条件组） ==========
	// 必要条件组：价格触及轨道(S1) + MACD信号(S2) 至少满足1个
	bool sellS1 = (last.high >= boll.up);
	bool sellS2_redShrink = (macd.macd_bar > 0) && (macd.macd_bar < prevMacd.macd_bar) && fabs(macd.dif) > MACD_DIF_THRESHOLD;
	bool sellS2_topDiv = (last.high > bars5[barCount - 2].high) && (macd.dif < prevMacd.dif) && fabs(macd.dif) > MACD_DIF_THRESHOLD;
	bool sellS2 = sellS2_redShrink || sellS2_topDiv;
	bool sellNecessary = (sellS1 || sellS2);

	// 辅助条件组：KDJ/RSI/WR 三个超买指标至少满足2个
	bool sellA1 = (kdj.k > KDJ_OVERBUY && kdj.d > KDJ_OVERBUY && kdj.j < prevKdj.j);
	bool sellA2 = (rsi6 > 70);
	bool sellA3 = (wr6 < 20);
	int sellAuxCount = (sellA1 ? 1 : 0) + (sellA2 ? 1 : 0) + (sellA3 ? 1 : 0);

	bool hasSell = sellNecessary && (sellAuxCount >= 2);

	// ========== 买入判定（必要条件组+辅助条件组） ==========
	// 必要条件组：价格触及轨道(B1) + MACD信号(B2) 至少满足1个
	bool buyB1 = (last.low <= boll.dn);
	bool buyB2_greenShrink = (macd.macd_bar < 0) && (macd.macd_bar > prevMacd.macd_bar) && fabs(macd.dif) > MACD_DIF_THRESHOLD;
	bool buyB2_bottomDiv = (last.low < bars5[barCount - 2].low) && (macd.dif > prevMacd.dif) && fabs(macd.dif) > MACD_DIF_THRESHOLD;
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
		// 强势优先买、弱势优先卖
		if (trendState == STOCK::TrendState30m::STATE_STRONG)
			return STOCK::Signal5m::SIG_BUY;
		if (trendState == STOCK::TrendState30m::STATE_WEAK)
			return STOCK::Signal5m::SIG_SELL;

		// 震荡行情：数量相等 → 优先顺势；不等则选辅助条件更多一方
		if (buyAuxCount > sellAuxCount)
			return STOCK::Signal5m::SIG_BUY;
		else if (sellAuxCount > buyAuxCount)
			return STOCK::Signal5m::SIG_SELL;
		else
		{
			// 辅助指标数量相同，依然跟随大周期趋势兜底
			if (trendState == STOCK::TrendState30m::STATE_STRONG)
				return STOCK::Signal5m::SIG_BUY;
			else
				return STOCK::Signal5m::SIG_SELL;
		}
	}
	if (hasSell) return STOCK::Signal5m::SIG_SELL;
	if (hasBuy) return STOCK::Signal5m::SIG_BUY;
	return STOCK::Signal5m::SIG_NONE;
}

// ========== 智能分析模块：单边钝化风控过滤器 ==========
// 只要触发禁止操作，直接忽略上面买卖信号

bool CSignalAnalyzer::IsForbidTrade(const std::vector<STOCK::Bar>& bars5, STOCK::TrendState30m trendState)
{
	if (bars5.size() < 120)
		return false;

	// 布林带宽放大：连续3根K线带宽持续扩大（捕获单边趋势中段）
	STOCK::BollResult boll = CalcBoll(bars5, 20);
	std::vector<STOCK::Bar> prevBars(bars5.begin(), bars5.end() - 1);
	STOCK::BollResult prevBoll = CalcBoll(prevBars, 20);
	std::vector<STOCK::Bar> prevPrevBars(bars5.begin(), bars5.end() - 2);
	STOCK::BollResult prevPrevBoll = CalcBoll(prevPrevBars, 20);
	bool bandExpand = prevBoll.bandwidth > 0 && boll.bandwidth > prevBoll.bandwidth * BOLL_EXPAND_RATIO
		&& prevPrevBoll.bandwidth > 0 && prevBoll.bandwidth > prevPrevBoll.bandwidth * BOLL_EXPAND_RATIO;
	double bandRatio = boll.mid > 0 ? boll.bandwidth / boll.mid : 0;
	bool narrowBand = bandRatio > 0 && bandRatio < NARROW_BOLL_RATIO;

	// KDJ钝化
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

	// 风控判定：按趋势动态处理
	if (bandExpand || narrowBand)
		return true;
	if (trendState == STOCK::TrendState30m::STATE_STRONG)
		return kdjBottomPassive || wrTopPassive || wrBottomPassive;
	if (trendState == STOCK::TrendState30m::STATE_WEAK)
		return kdjTopPassive || wrTopPassive || wrBottomPassive;
	return kdjTopPassive || kdjBottomPassive || wrTopPassive || wrBottomPassive;
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
	if (IsForbidTrade(bars5, trendState))
	{
		signal.isForbid = true;
		signal.valid = false;
		signal.reason = _T("单边钝化，停止滚动做T");
		return signal;
	}

	// ===== 第3步：获取5分钟共振买卖信号 =====
	STOCK::Signal5m sig = Get5mSignal(bars5, trendState);

	if (sig == STOCK::Signal5m::SIG_NONE)
	{
		signal.valid = false;
		return signal;
	}

	// ===== 第4步：分趋势分支处理信号 =====
	if (lastSignalPrice > 0 && fabs(bars5.back().close - lastSignalPrice) < MIN_SIGNAL_PRICE_DIFF)
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
	// EMA稳定性：至少120根K线用于计算指标（EMA26需足够历史）
	if (n < 120) return result;

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

	// KDJ序列（修正：基于RSV序列用SMA平滑得到K/D/J，符合标准定义）
	std::vector<double> rsvSeq(n, 50);
	for (size_t i = 8; i < n; i++)
	{
		double llv = bars5[i - 8].low, hhv = bars5[i - 8].high;
		for (size_t j = i - 7; j <= i; j++) { if (bars5[j].low < llv) llv = bars5[j].low; if (bars5[j].high > hhv) hhv = bars5[j].high; }
		rsvSeq[i] = (hhv != llv) ? (bars5[i].close - llv) / (hhv - llv) * 100.0 : 50.0;
	}
	std::vector<double> kSeq(n, 50), dSeq(n, 50), jSeq(n, 50);
	for (size_t i = 1; i < n; i++)
	{
		// SMA(RSV, 3) = 2/3 * prevK + 1/3 * RSV
		kSeq[i] = 2.0 / 3.0 * kSeq[i - 1] + 1.0 / 3.0 * rsvSeq[i];
		// SMA(K, 3) = 2/3 * prevD + 1/3 * K
		dSeq[i] = 2.0 / 3.0 * dSeq[i - 1] + 1.0 / 3.0 * kSeq[i];
		jSeq[i] = 3.0 * kSeq[i] - 2.0 * dSeq[i];
	}

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
	for (size_t i = 21; i < n; i++)
	{
		// ===== 风控过滤（优化版） =====
		// 布林带宽放大：连续3根K线带宽持续扩大（捕获单边趋势中段，避免突破起点误杀）
		bool bandExpand = false;
		if (i >= 3 && bollBand[i] > 0 && bollBand[i - 1] > 0 && bollBand[i - 2] > 0)
		{
			bandExpand = bollBand[i - 1] > 0 && bollBand[i] > bollBand[i - 1] * BOLL_EXPAND_RATIO
				&& bollBand[i - 2] > 0 && bollBand[i - 1] > bollBand[i - 2] * BOLL_EXPAND_RATIO;
		}

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

		// 风控判定：布林带宽连续扩大或窄幅箱体直接禁止；KDJ/WR钝化按趋势动态处理
		bool forbid = false;
		double bandMid = bollMid[i];
		double bandRatio = bandMid > 0 ? bollBand[i] / bandMid : 0;
		bool narrowBand = bandRatio > 0 && bandRatio < NARROW_BOLL_RATIO;
		if (bandExpand || narrowBand)
		{
			forbid = true;
		}
		else if (trendState == STOCK::TrendState30m::STATE_STRONG)
		{
			// 强势：允许KDJ高位钝化（趋势延续正常），只禁止低位钝化和WR钝化
			forbid = kdjBottomPassive || wrTopPassive || wrBottomPassive;
		}
		else if (trendState == STOCK::TrendState30m::STATE_WEAK)
		{
			// 弱势：允许KDJ低位钝化（下跌延续正常），只禁止高位钝化和WR钝化
			forbid = kdjTopPassive || wrTopPassive || wrBottomPassive;
		}
		else
		{
			// 震荡：KDJ/WR钝化都禁止
			forbid = kdjTopPassive || kdjBottomPassive || wrTopPassive || wrBottomPassive;
		}

		if (forbid)
		{
			result.push_back({ static_cast<int>(i), false, true, trendState, _T("禁止做T") });
			continue;
		}

		// ===== 信号判定（优化版：必要条件组+辅助条件组，解决指标共线性） =====

		// --- 卖出判定 ---
		// 必要条件组：价格触及轨道(S1) + MACD信号(S2) 至少满足1个
		bool sellS1 = (bars5[i].high >= bollUp[i]);
		// MACD：红柱缩短 or 顶背离（DIF远离0轴才认定有效动量信号）
		bool sellS2_redShrink = (macdBarSeq[i] > 0) && (macdBarSeq[i] < macdBarSeq[i - 1]) && fabs(difSeq[i]) > MACD_DIF_THRESHOLD;
		bool sellS2_topDiv = (bars5[i].high > bars5[i - 1].high) && (difSeq[i] < difSeq[i - 1]) && fabs(difSeq[i]) > MACD_DIF_THRESHOLD;
		bool sellS2 = sellS2_redShrink || sellS2_topDiv;
		bool sellNecessary = (sellS1 || sellS2);

		// 辅助条件组：KDJ/RSI/WR 三个超买指标至少满足2个
		bool sellA1 = (kSeq[i] > KDJ_OVERBUY && dSeq[i] > KDJ_OVERBUY && jSeq[i] < jSeq[i - 1]);
		bool sellA2 = (rsi6Seq[i] > 70);
		bool sellA3 = (wr6Seq[i] < 20);
		int sellAuxCount = (sellA1 ? 1 : 0) + (sellA2 ? 1 : 0) + (sellA3 ? 1 : 0);

		bool hasSell = sellNecessary && (sellAuxCount >= 2);

		// --- 买入判定 ---
		// 必要条件组：价格触及轨道(B1) + MACD信号(B2) 至少满足1个
		bool buyB1 = (bars5[i].low <= bollDn[i]);
		// MACD：绿柱缩短 or 底背离（DIF远离0轴才认定有效动量信号）
		bool buyB2_greenShrink = (macdBarSeq[i] < 0) && (macdBarSeq[i] > macdBarSeq[i - 1]) && fabs(difSeq[i]) > MACD_DIF_THRESHOLD;
		bool buyB2_bottomDiv = (bars5[i].low < bars5[i - 1].low) && (difSeq[i] > difSeq[i - 1]) && fabs(difSeq[i]) > MACD_DIF_THRESHOLD;
		bool buyB2 = buyB2_greenShrink || buyB2_bottomDiv;
		bool buyNecessary = (buyB1 || buyB2);

		// 辅助条件组：KDJ/RSI/WR 三个超卖指标至少满足2个
		bool buyA1 = (kSeq[i] < KDJ_OVERSELL && dSeq[i] < KDJ_OVERSELL && jSeq[i] > jSeq[i - 1]);
		bool buyA2 = (rsi6Seq[i] < 30);
		bool buyA3 = (wr6Seq[i] > 80);
		int buyAuxCount = (buyA1 ? 1 : 0) + (buyA2 ? 1 : 0) + (buyA3 ? 1 : 0);

		bool hasBuy = buyNecessary && (buyAuxCount >= 2);

		if (!hasSell && !hasBuy) continue;
		if (lastSignalPrice > 0 && fabs(bars5[i].close - lastSignalPrice) < MIN_SIGNAL_PRICE_DIFF)
			continue;

		// 买卖同时满足时，优先取与趋势一致的方向
		bool isBuy;
		if (hasSell && hasBuy)
		{
			if (trendState == STOCK::TrendState30m::STATE_STRONG) isBuy = true;
			else if (trendState == STOCK::TrendState30m::STATE_WEAK) isBuy = false;
			else if (buyAuxCount > sellAuxCount) isBuy = true;
			else isBuy = false;
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