#include "pch.h"
#include "SignalAnalyzer.h"
#include "StockDef.h"
#include <cmath>
#include <map>
#include <set>

namespace
{
	// ========== 日志分级 ==========
	enum class LogLevel { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR };
	// 日志输出：指标计算异常、风控拦截、信号触发分别打印不同级别
	// 生产环境可通过编译宏控制日志级别
#ifdef _DEBUG
	static LogLevel g_minLogLevel = LogLevel::LOG_DEBUG;
#else
	static LogLevel g_minLogLevel = LogLevel::LOG_WARN;
#endif

	static void SignalLog(LogLevel level, const CString& msg)
	{
		if (level < g_minLogLevel) return;
		// 使用VS OutputDebugString输出，支持线上监控
		CString prefix;
		switch (level)
		{
		case LogLevel::LOG_DEBUG: prefix = _T("[SIG-DBG] "); break;
		case LogLevel::LOG_INFO:  prefix = _T("[SIG-INF] "); break;
		case LogLevel::LOG_WARN:  prefix = _T("[SIG-WRN] "); break;
		case LogLevel::LOG_ERROR: prefix = _T("[SIG-ERR] "); break;
		}
		OutputDebugString(prefix + msg + _T("\n"));
	}

	// ========== 性能埋点 ==========
	struct PerfCounter {
		const char* name;
		DWORD startTick;
		PerfCounter(const char* n) : name(n), startTick(GetTickCount()) {}
		~PerfCounter() {
			DWORD elapsed = GetTickCount() - startTick;
			if (elapsed > 10) {  // >10ms才记录，避免噪音
				CString msg;
				msg.Format(_T("[PERF] %s: %ums"), CString(name), elapsed);
				OutputDebugString(msg + _T("\n"));
			}
		}
	};

	// ========== 回测模式控制 ==========
	static bool g_backtestMode = false;

	// ========== 跨周期时间对齐 ==========
	// 5min Bar的time戳是秒级时间戳，30min Bar的time戳也是秒级
	// 一根30min Bar覆盖6根5min Bar
	// 对齐规则：5min Bar的时间戳落在30min Bar的[start, end)区间内
	size_t Find30mBarFor5m(long timestamp5m, const std::vector<STOCK::Bar>& bars30)
	{
		// 30min K线时间戳就是该周期的起始时间
		// 使用二分查找找到最大的 <= timestamp5m 的30min Bar
		if (bars30.empty()) return SIZE_MAX;

		size_t lo = 0, hi = bars30.size() - 1;
		size_t result = SIZE_MAX;
		while (lo <= hi && hi < bars30.size())
		{
			size_t mid = lo + (hi - lo) / 2;
			if (bars30[mid].time <= timestamp5m)
			{
				result = mid;
				lo = mid + 1;
			}
			else
			{
				if (mid == 0) break;
				hi = mid - 1;
			}
		}
		// 验证5min Bar是否在该30min Bar的区间内（30min = 1800秒）
		if (result != SIZE_MAX && bars30[result].time <= timestamp5m &&
			timestamp5m < bars30[result].time + 1800)
			return result;
		return SIZE_MAX;
	}

	constexpr double MACD_DIF_PRICE_RATIO = 0.001;
	constexpr double MIN_SIGNAL_PRICE_RATIO = 0.003;
	constexpr double MIN_SIGNAL_ATR_RATIO = 0.2;
	constexpr double NARROW_BOLL_ATR_RATIO = 1.5;       // 窄布林ATR动态阈值：带宽 < ATR×此比例视为窄布林
	constexpr size_t NARROW_BOLL_AVG_PERIOD = 10;
	constexpr double BOLL_EXPAND_RATIO = 1.4;
	constexpr size_t BOLL_EXPAND_CONSECUTIVE = 2;  // 连续N根带宽放大判定扩张（替代60根分位，消除未来函数）
	constexpr double SELL_KDJ_D_OVERBUY = 70.0;
	constexpr double SELL_WR_OVERBUY = 30.0;
	constexpr double KDJ_OVERBUY = 80.0;
	constexpr double KDJ_OVERSELL = 20.0;
	constexpr int MIN_SIGNAL_BAR_GAP = 5;

	// 双周期共振趋势判定参数
	constexpr size_t TREND_30M_LOOKBACK = 25;      // 30分钟K线回看根数
	constexpr size_t TREND_5M_LOOKBACK = 20;       // 5分钟K线回看根数
	constexpr double MA20_SLOPE_THRESHOLD = 0.001;  // MA20斜率阈值，低于此视为走平
	constexpr size_t SWING_MIN_BARS = 3;            // 波段极值最小间隔K线数（回测模式）
	constexpr size_t SWING_MIN_BARS_REALTIME = 0;   // 实时模式：仅左侧确认，无需右侧未来K线
	constexpr double SWING_MIN_ATR_RATIO = 0.3;     // 极值振幅过滤：小于ATR×此比例的微型高低点丢弃
	constexpr size_t CONSECUTIVE_BAR_TREND = 5;     // 连续同向K线判定趋势兜底
	constexpr double DIV_ATR_RATIO = 0.3;           // 背离幅度阈值：价格/DIF差值须大于ATR×此比例
	constexpr size_t DIV_LOOKBACK = 30;             // 背离检测回看K线数
	constexpr double VOL_RATIO_THRESHOLD = 1.3;     // 量比阈值（上涨放量/回调缩量）

	// 安全除法：分母为0、NaN、Inf时返回0.0
	double SafeDiv(double numerator, double denominator)
	{
		if (!std::isfinite(denominator) || std::isnan(denominator) || denominator == 0.0)
			return 0.0;
		return numerator / denominator;
	}

	// 安全浮点数：所有指标写入序列前统一过滤，非法值强制置0并打印ERROR日志
	double SafeDouble(double val, const char* context = nullptr)
	{
		if (std::isfinite(val) && !std::isnan(val))
			return val;
		if (context)
		{
			CStringA ctxA(context);
			CString msg;
			msg.Format(_T("非法浮点数: %s"), CString(ctxA));
			SignalLog(LogLevel::LOG_ERROR, msg);
		}
		return 0.0;
	}

	// 检查价格值是否有限（非NaN、非Inf、非0）
	bool IsFinitePrice(double v)
	{
		return std::isfinite(v) && !std::isnan(v) && v != 0.0;
	}

	// CalcMA安全包装：结果非有限时返回0.0
	double SafeCalcMA(const std::vector<double>& values, int N)
	{
		double result = CSignalAnalyzer::CalcMA(values, N);
		return std::isfinite(result) ? result : 0.0;
	}

	// CalcStdDev安全包装：结果非有限时返回0.0
	double SafeCalcStdDev(const std::vector<double>& values)
	{
		double result = CSignalAnalyzer::CalcStdDev(values);
		return std::isfinite(result) ? result : 0.0;
	}

	// MACD DIF有效阈值：兼顾股价比例与真实波动率
	// 公式：max(price*0.001, ATR*0.2)
	// 高价/低价/高波动标的自动适配
	double GetMacdDifThreshold(double price, double atr = 0.0)
	{
		double priceThreshold = price > 0 ? price * MACD_DIF_PRICE_RATIO : 0.0;
		double atrThreshold = atr > 0 ? atr * 0.2 : 0.0;
		double result = priceThreshold > atrThreshold ? priceThreshold : atrThreshold;
		return result > 0 ? result : 0.0;
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

	// 信号价差阈值（多空拆分版）
	// 下跌行情（30min弱势）放大买入价差阈值，过滤小幅回调假买点
	// 上涨行情（30min强势）缩小卖出价差阈值，捕捉精准止盈点
	// 震荡统一阈值
	double GetLongThreshold(const std::vector<STOCK::Bar>& bars, size_t endIndex, STOCK::TrendState30m trendState)
	{
		if (bars.empty() || endIndex >= bars.size())
			return 0.0;

		double priceThreshold = bars[endIndex].close > 0 ? bars[endIndex].close * MIN_SIGNAL_PRICE_RATIO : 0.0;
		double atrThreshold = CalcATR(bars, endIndex) * MIN_SIGNAL_ATR_RATIO;
		double base = atrThreshold > priceThreshold ? atrThreshold : priceThreshold;

		// 弱势放大买入价差：1.5倍，过滤小幅回调假买点
		if (trendState == STOCK::TrendState30m::STATE_WEAK || trendState == STOCK::TrendState30m::STATE_WEAK_SHAKE)
			return base * 1.5;
		return base;
	}

	double GetShortThreshold(const std::vector<STOCK::Bar>& bars, size_t endIndex, STOCK::TrendState30m trendState)
	{
		if (bars.empty() || endIndex >= bars.size())
			return 0.0;

		double priceThreshold = bars[endIndex].close > 0 ? bars[endIndex].close * MIN_SIGNAL_PRICE_RATIO : 0.0;
		double atrThreshold = CalcATR(bars, endIndex) * MIN_SIGNAL_ATR_RATIO;
		double base = atrThreshold > priceThreshold ? atrThreshold : priceThreshold;

		// 强势缩小卖出价差：0.7倍，捕捉精准止盈点
		if (trendState == STOCK::TrendState30m::STATE_STRONG)
			return base * 0.7;
		return base;
	}

	// 窄布林ATR动态阈值：带宽 < ATR × NARROW_BOLL_ATR_RATIO 视为窄布林
	// 自动适配高价股（ATR大→阈值大）和低价股（ATR小→阈值小），消除固定数值适配缺陷
	double GetNarrowBollThresholdATR(double atr)
	{
		return atr > 0 ? atr * NARROW_BOLL_ATR_RATIO : 0.0;
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

	// 窄布林判定（ATR动态阈值版）
	bool IsNarrowBoll(double avgBandwidth, double atr)
	{
		if (avgBandwidth <= 0)
			return false;

		return avgBandwidth < GetNarrowBollThresholdATR(atr);
	}

	// 布林扩张判定：连续N根带宽放大（替代60根历史分位，消除未来函数）
	// 实盘实时：仅使用左侧历史数据，不包含未来K线
	// 回测批量：同样仅用左侧数据，保证一致性
	bool IsBollExpand(const std::vector<double>& bollBand, size_t index)
	{
		if (index == 0 || index >= bollBand.size() || bollBand[index] <= 0 || bollBand[index - 1] <= 0)
			return false;

		// 条件1：当前带宽 > 前一根 × 扩张倍数
		if (bollBand[index] > bollBand[index - 1] * BOLL_EXPAND_RATIO)
			return true;

		// 条件2：连续N根带宽逐根放大（环比扩张）
		size_t consecCount = 0;
		for (size_t i = index; i > 0; i--)
		{
			if (bollBand[i] > bollBand[i - 1])
				consecCount++;
			else
				break;
			if (consecCount >= BOLL_EXPAND_CONSECUTIVE)
				return true;
		}

		return false;
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
		double rsvSmooth = 50.0;  // RSV平滑缓存

		// 滑动窗口极值缓存：维护窗口内的最高价和最低价计数
		// 避免移出极值时全量重扫N根K线
		std::multiset<double> highWindow;  // 允许重复值的有序集合
		std::multiset<double> lowWindow;

		for (size_t i = static_cast<size_t>(N) - 1; i < n; i++)
		{
			if (i == static_cast<size_t>(N) - 1)
			{
				// 首根：初始化窗口
				for (size_t j = 0; j <= i; j++)
				{
					highWindow.insert(bars[j].high);
					lowWindow.insert(bars[j].low);
				}
			}
			else
			{
				// 滑动窗口：移除i-N，加入i
				size_t outIdx = i - static_cast<size_t>(N);
				double outHigh = bars[outIdx].high;
				double outLow = bars[outIdx].low;
				// 安全移除：find可能返回end()（NaN等异常值），需检查
				auto itH = highWindow.find(outHigh);
				if (itH != highWindow.end()) highWindow.erase(itH);
				auto itL = lowWindow.find(outLow);
				if (itL != lowWindow.end()) lowWindow.erase(itL);
				highWindow.insert(bars[i].high);
				lowWindow.insert(bars[i].low);
			}

			// O(1)取极值：multiset的最后一个/第一个元素
			double highN = *highWindow.rbegin();  // 最大值
			double lowN = *lowWindow.begin();      // 最小值

			double rsv = SafeDouble(highN != lowN ? (bars[i].close - lowN) / (highN - lowN) * 100.0 : 50.0, "KDJ RSV");

			// RSV短期MA平滑（3周期），降低盘中价格毛刺导致KDJ剧烈抖动
			constexpr int RSV_SMOOTH_N = 3;
			if (i == static_cast<size_t>(N) - 1)
				rsvSmooth = rsv;  // 首根初始化
			else
				rsvSmooth = SafeDouble((rsv + (RSV_SMOOTH_N - 1) * rsvSmooth) / RSV_SMOOTH_N, "KDJ rsvSmooth");

			k = SafeDouble((2.0 * k + rsvSmooth) / 3.0, "KDJ K");
			d = SafeDouble((2.0 * d + k) / 3.0, "KDJ D");
			kSeq[i] = k;
			dSeq[i] = d;
			jSeq[i] = SafeDouble(3.0 * k - 2.0 * d, "KDJ J");
		}
	}
}

// ========== 智能分析模块：基础工具函数 ==========

double CSignalAnalyzer::CalcMA(const std::vector<double>& values, int N)
{
	if (values.empty() || N <= 0)
		return 0.0;
	if (static_cast<int>(values.size()) < N)
		return 0.0;
	double sum = 0.0;
	for (int i = static_cast<int>(values.size()) - N; i < static_cast<int>(values.size()); i++)
		sum += values[i];
	double result = sum / N;
	return std::isfinite(result) ? result : 0.0;
}

double CSignalAnalyzer::CalcEMA(const std::vector<double>& values, int N)
{
	if (values.empty() || N <= 0)
		return 0.0;
	double k = 2.0 / (N + 1.0);
	double ema = values[0];
	for (size_t i = 1; i < values.size(); i++)
		ema = values[i] * k + ema * (1.0 - k);
	return std::isfinite(ema) ? ema : 0.0;
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

// ========== 全局指标参数配置 ==========
CSignalAnalyzer::SignalParam& CSignalAnalyzer::GetParam()
{
	static SignalParam g_param;
	return g_param;
}

void CSignalAnalyzer::SetParam(const SignalParam& param)
{
	GetParam() = param;
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
	double result = sqrt(sumSq / values.size());
	return std::isfinite(result) ? result : 0.0;
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

	double au, ad;
	if (GetParam().rsiUseEma)
	{
		// EMA平滑：与市面行情软件对齐，标准RSI计算方式
		double k = 2.0 / (N + 1.0);
		au = upList[0];
		ad = dnList[0];
		for (size_t i = 1; i < upList.size(); i++)
		{
			au = upList[i] * k + au * (1.0 - k);
			ad = dnList[i] * k + ad * (1.0 - k);
		}
	}
	else
	{
		// SMA平滑：保留原有计算方式，适配不同回测需求
		au = CalcSMA(upList, N);
		ad = CalcSMA(dnList, N);
	}

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
	for (int i = static_cast<int>(bars.size()) - N + 1; i < static_cast<int>(bars.size()); i++)
	{
		if (bars[i].high > hhv) hhv = bars[i].high;
		if (bars[i].low < llv) llv = bars[i].low;
	}

	double c = bars.back().close;
	if (hhv == llv)
		return 50.0;

	return 100.0 * (hhv - c) / (hhv - llv);
}

// 成交量加权WR：放量高低点权重更大，过滤长上影假极值
// 计算方式：对窗口内每根K线的高低点按成交量加权，取加权最高价和加权最低价
double CSignalAnalyzer::CalcWR_VolumeWeighted(const std::vector<STOCK::Bar>& bars, int N)
{
	if (static_cast<int>(bars.size()) < N || N <= 0)
		return 50.0;

	double volSum = 0;
	double wHigh = 0, wLow = 0;
	for (int i = static_cast<int>(bars.size()) - N; i < static_cast<int>(bars.size()); i++)
	{
		double v = bars[i].volume;
		if (v <= 0) v = 1.0;  // 防止零成交量
		volSum += v;
		wHigh += bars[i].high * v;
		wLow += bars[i].low * v;
	}
	if (volSum <= 0)
		return 50.0;

	wHigh /= volSum;
	wLow /= volSum;

	double c = bars.back().close;
	if (wHigh == wLow)
		return 50.0;

	return 100.0 * (wHigh - c) / (wHigh - wLow);
}

// ========== 智能分析模块：30分钟趋势判定 ==========
// 输入：完整历史30分钟K线数组 bars30
// 输出：TrendStateResult（含趋势状态、置信度、加权得分）
// 改进：加权评分 + 中性缓冲区间 + 置信度输出

STOCK::TrendStateResult CSignalAnalyzer::Get30mTrendState(const std::vector<STOCK::Bar>& bars30)
{
	STOCK::TrendStateResult result;

	// 至少需要30根30min K线才能计算当前/前一根BOLL；EMA60 在数据不足时按现有序列递推
	if (bars30.size() < 30)
	{
		result.state = STOCK::TrendState30m::STATE_SHAKE;
		result.confidence = 0;
		return result;
	}

	// 1. 计算30min全部指标
	STOCK::BollResult boll = CalcBoll(bars30, GetParam().bollPeriod);
	double mid = boll.mid;

	const STOCK::Bar& lastBar = bars30.back();

	// 计算前一根BOLL的中轨（取bars30[-21:-1]）
	std::vector<STOCK::Bar> prevBars(bars30.end() - 21, bars30.end() - 1);
	STOCK::BollResult prevBoll = CalcBoll(prevBars, GetParam().bollPeriod);
	double prevMid = prevBoll.mid;

	// ===== 维度1：价格轨道(BOLL) - 权重1.5 =====
	// BOLL中轨方向 + 价格与中轨关系，反映价格轨道趋势
	bool bollDown = (mid < prevMid) && (lastBar.close < mid);
	bool bollUp = (mid > prevMid) && (lastBar.close > mid);
	double bollWeight = 1.5;
	double bollWeakScore = bollDown ? bollWeight : 0.0;
	double bollStrongScore = bollUp ? bollWeight : 0.0;

	// ===== 维度2：动量(MACD) - 权重1.5 =====
	// DIF方向反映中期动量，比简单正负更可靠
	STOCK::MACDResult macd = CalcMACD(bars30);
	double macdWeight = 1.5;
	double macdWeakScore = (macd.dif < 0) ? macdWeight : 0.0;
	double macdStrongScore = (macd.dif > 0) ? macdWeight : 0.0;

	// ===== 维度3：超买超卖(RSI) - 权重1.0 =====
	// RSI作为辅助确认，权重较低（与BOLL/MACD有共线性）
	double rsi14 = CalcRSI(bars30, GetParam().rsiPeriod30m);
	double rsiWeight = 1.0;
	double rsiWeakScore = (rsi14 < 50) ? rsiWeight : 0.0;
	double rsiStrongScore = (rsi14 > 50) ? rsiWeight : 0.0;

	// ===== 维度4：均线结构(EMA20/60) - 权重2.0 =====
	// 均线排列是最稳定的趋势确认信号，权重最高
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
	double maWeight = 2.0;
	double maWeakScore = maWeak ? maWeight : 0.0;
	double maStrongScore = maStrong ? maWeight : 0.0;

	// ===== 加权汇总 =====
	double weakScore = bollWeakScore + macdWeakScore + rsiWeakScore + maWeakScore;
	double strongScore = bollStrongScore + macdStrongScore + rsiStrongScore + maStrongScore;
	double totalWeight = bollWeight + macdWeight + rsiWeight + maWeight;  // = 6.0

	result.weakScore = weakScore;
	result.strongScore = strongScore;

	// ===== 趋势判定（含中性缓冲区间） =====
	// 强判定阈值：≥4.0（约67%权重方向一致），弱判定阈值：≥2.5（约42%权重方向一致）
	// 2.5~4.0 之间为弱震荡缓冲区，不轻易切强/弱
	double strongThreshold = 4.0;
	double weakThreshold = 2.5;

	if (strongScore >= strongThreshold && strongScore > weakScore)
	{
		result.state = STOCK::TrendState30m::STATE_STRONG;
	}
	else if (weakScore >= strongThreshold && weakScore > strongScore)
	{
		result.state = STOCK::TrendState30m::STATE_WEAK;
	}
	else if (weakScore >= weakThreshold && weakScore > strongScore)
	{
		// 弱震荡缓冲：偏弱但未达强判定阈值
		result.state = STOCK::TrendState30m::STATE_WEAK_SHAKE;
	}
	else if (strongScore >= weakThreshold && strongScore > weakScore)
	{
		// 偏强但未达强判定阈值，仍归为震荡（强势需要更高确认）
		result.state = STOCK::TrendState30m::STATE_SHAKE;
	}
	else
	{
		result.state = STOCK::TrendState30m::STATE_SHAKE;
	}

	// ===== 置信度计算 =====
	// 基于优势方向的得分占比和领先幅度
	double dominantScore = (weakScore > strongScore) ? weakScore : strongScore;
	double scoreDiff = fabs(weakScore - strongScore);
	// 基础置信度 = 优势方向得分占比 * 60 + 领先幅度加成 * 40
	double baseConf = (dominantScore / totalWeight) * 60.0 + (scoreDiff / totalWeight) * 40.0;
	// 映射到0-100
	result.confidence = static_cast<int>(baseConf > 100.0 ? 100 : (baseConf < 0.0 ? 0 : baseConf));

	return result;
}

// ========== 趋势防抖 ==========
// 连续3根K线同状态才确认切换，期间保持旧趋势（锁定冷却周期）
// 解决连续交替震荡/强势仍会抖动的问题
STOCK::TrendState30m CSignalAnalyzer::DebounceTrendState(
	const std::vector<STOCK::TrendStateResult>& recentResults,
	STOCK::TrendState30m currentState)
{
	if (recentResults.empty())
		return currentState;

	// 取最近1根K线的趋势状态
	STOCK::TrendState30m latest = recentResults.back().state;

	// 如果最新状态与当前锁定状态一致，直接返回
	if (latest == currentState)
		return currentState;

	// 新状态与当前不同，检查是否连续3根都是同一新状态
	size_t confirmCount = 3;
	if (recentResults.size() < confirmCount)
		return currentState;  // 数据不足，保持旧状态

	// 从最新往前数，检查连续confirmCount根是否都是同一新状态
	STOCK::TrendState30m candidate = latest;
	bool allSame = true;
	for (size_t k = 0; k < confirmCount; k++)
	{
		size_t idx = recentResults.size() - 1 - k;
		if (recentResults[idx].state != candidate)
		{
			allSame = false;
			break;
		}
	}

	if (allSame)
		return candidate;  // 连续3根同状态，确认切换

	// 未达确认条件，保持旧状态（锁定冷却）
	return currentState;
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

	// 提取波段高低点序列
	// realtime: true=实时盘中模式（仅左侧确认，消除滞后），false=回测全量模式（左右双侧确认，事后精准）
	// minBars: 两侧至少minBars根K线才能确认极值（实时模式下仅左侧使用）
	std::vector<SwingPoint> ExtractSwingPoints(const std::vector<STOCK::Bar>& bars,
		size_t minBars = SWING_MIN_BARS, bool realtime = false)
	{
		std::vector<SwingPoint> points;
		size_t n = bars.size();
		size_t leftBars = minBars;
		size_t rightBars = realtime ? 0 : minBars;  // 实时模式无需右侧确认

		if (n < leftBars + 1)
			return points;

		// 计算ATR用于极值振幅过滤
		double atr = 0.0;
		if (n >= 15)
		{
			std::vector<STOCK::Bar> barsForATR(bars.end() - 15, bars.end());
			atr = CalcATR(barsForATR, barsForATR.size() - 1);
		}
		double minSwingSize = atr * SWING_MIN_ATR_RATIO;

		// 遍历K线检测局部极值
		size_t endIdx = realtime ? n : (n - rightBars);  // 实时模式可检测到最后一根
		for (size_t i = leftBars; i < endIdx; i++)
		{
			// 检查是否为局部高点
			bool isHigh = true;
			for (size_t j = i - leftBars; j < i; j++)  // 左侧确认
			{
				if (bars[j].high >= bars[i].high) { isHigh = false; break; }
			}
			if (isHigh && rightBars > 0)  // 右侧确认（仅回测模式）
			{
				for (size_t j = i + 1; j <= i + rightBars && j < n; j++)
				{
					if (bars[j].high >= bars[i].high) { isHigh = false; break; }
				}
			}

			if (isHigh)
			{
				// 极值振幅过滤：高点振幅过小则丢弃杂波
				if (minSwingSize > 0 && i > 0)
				{
					double swingSize = bars[i].high - bars[i].low;
					if (swingSize < minSwingSize)
						continue;
				}
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
			for (size_t j = i - leftBars; j < i; j++)  // 左侧确认
			{
				if (bars[j].low <= bars[i].low) { isLow = false; break; }
			}
			if (isLow && rightBars > 0)  // 右侧确认（仅回测模式）
			{
				for (size_t j = i + 1; j <= i + rightBars && j < n; j++)
				{
					if (bars[j].low <= bars[i].low) { isLow = false; break; }
				}
			}

			if (isLow)
			{
				// 极值振幅过滤：低点振幅过小则丢弃杂波
				if (minSwingSize > 0 && i > 0)
				{
					double swingSize = bars[i].high - bars[i].low;
					if (swingSize < minSwingSize)
						continue;
				}
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

	// 连续同向K线判定：若连续N根阳线/阴线，直接判定上升/下跌结构，不依赖波段点
	// 返回: 1=连续阳线(上升趋势), -1=连续阴线(下跌趋势), 0=无明确方向
	int DetectConsecutiveTrend(const std::vector<STOCK::Bar>& bars, size_t count = CONSECUTIVE_BAR_TREND)
	{
		if (bars.size() < count)
			return 0;

		// 检查最近count根K线是否全为阳线或全为阴线
		bool allUp = true;
		bool allDown = true;
		for (size_t i = bars.size() - count; i < bars.size(); i++)
		{
			if (bars[i].close <= bars[i].open) allUp = false;
			if (bars[i].close >= bars[i].open) allDown = false;
		}
		if (allUp) return 1;
		if (allDown) return -1;
		return 0;
	}

	// ========== MACD背离检测 ==========
	// 背离等级
	enum class DivLevel { NONE = 0, WEAK = 1, STANDARD = 2, STRONG = 3 };

	// 背离检测结果
	struct DivergenceInfo {
		bool hasTopDiv;         // 顶背离
		bool hasBottomDiv;      // 底背离
		DivLevel topDivLevel;   // 顶背离等级
		DivLevel bottomDivLevel; // 底背离等级
		CString topDivDesc;     // 顶背离描述（用于reason）
		CString bottomDivDesc;  // 底背离描述（用于reason）

		DivergenceInfo() : hasTopDiv(false), hasBottomDiv(false),
			topDivLevel(DivLevel::NONE), bottomDivLevel(DivLevel::NONE) {
		}
	};

	// MACD背离检测：对比近3个极值点，而非仅相邻1根K线
	// 顶背离：价格高点逐次抬高，DIF高点逐次降低
	// 底背离：价格低点逐次降低，DIF低点逐次抬高
	// 过滤条件：0轴区分 + ATR幅度阈值 + 多根极值对比
	DivergenceInfo CalcDivergence(const std::vector<STOCK::Bar>& bars,
		const std::vector<double>& difSeq, size_t curIdx)
	{
		DivergenceInfo info;
		size_t n = curIdx + 1;
		if (n < DIV_LOOKBACK)
			return info;

		// 计算ATR用于幅度过滤
		double atr = 0.0;
		if (n >= 15)
		{
			std::vector<STOCK::Bar> barsForATR(bars.begin() + (n - 15), bars.begin() + n);
			atr = CalcATR(barsForATR, barsForATR.size() - 1);
		}
		double minPriceDiff = atr * DIV_ATR_RATIO;
		double minDifDiff = atr * DIV_ATR_RATIO * 0.5;  // DIF差值阈值略低

		size_t startIdx = (n > DIV_LOOKBACK) ? (n - DIV_LOOKBACK) : 0;

		// 提取回看区间内的波段极值点（实时模式）
		std::vector<STOCK::Bar> lookbackBars(bars.begin() + startIdx, bars.begin() + n);
		auto swings = ExtractSwingPoints(lookbackBars, 2, !g_backtestMode);

		// 分离高点和低点，同时记录原始索引
		struct Extremum { size_t origIdx; double priceVal; double difVal; double volume; };
		std::vector<Extremum> highExts, lowExts;
		for (const auto& sw : swings)
		{
			size_t oi = sw.index + startIdx;  // 映射回原始索引
			if (oi > curIdx) continue;
			if (sw.isHigh)
				highExts.push_back({ oi, sw.value, difSeq[oi], bars[oi].volume });
			else
				lowExts.push_back({ oi, sw.value, difSeq[oi], bars[oi].volume });
		}

		// 计算回看区间均量（用于量能校验）
		double avgVol = 0.0;
		{
			double volSum = 0.0;
			size_t volCount = 0;
			size_t volStart = (n > 20) ? (n - 20) : 0;
			for (size_t vi = volStart; vi < n; vi++)
			{
				volSum += bars[vi].volume;
				volCount++;
			}
			avgVol = volCount > 0 ? volSum / volCount : 0.0;
		}

		// ===== 顶背离检测 =====
		// 需要至少2个高点（当前+前一个），优先对比最近3个
		if (highExts.size() >= 2)
		{
			size_t checkCount = highExts.size() >= 3 ? 3 : highExts.size();
			// 取最近checkCount个高点
			auto itEnd = highExts.end();
			auto itStart = itEnd - checkCount;

			// 价格高点是否逐次抬高
			bool priceAsc = true;
			for (auto it = itStart + 1; it != itEnd; ++it)
			{
				if (it->priceVal <= (it - 1)->priceVal) { priceAsc = false; break; }
			}
			// DIF高点是否逐次降低
			bool difDesc = true;
			for (auto it = itStart + 1; it != itEnd; ++it)
			{
				if (it->difVal >= (it - 1)->difVal) { difDesc = false; break; }
			}

			if (priceAsc && difDesc)
			{
				// 幅度过滤：最近两个高点的价格差和DIF差须大于阈值
				double priceDiff = highExts.back().priceVal - (highExts.rbegin() + 1)->priceVal;
				double difDiff = (highExts.rbegin() + 1)->difVal - highExts.back().difVal;

				if (priceDiff >= minPriceDiff && difDiff >= minDifDiff)
				{
					info.hasTopDiv = true;

					// 量能校验：冲高放量才认定有效顶背离，无量冲高降级为弱背离
					bool volSurge = (avgVol > 0 && highExts.back().volume > avgVol * VOL_RATIO_THRESHOLD);

					// 0轴过滤：DIF在0轴上方时顶背离做空权重降低
					bool difAboveZero = highExts.back().difVal > 0;

					// 量能不足时强制降级为弱背离
					bool forceWeak = !volSurge;

					// 等级判定
					if (forceWeak || difAboveZero)
					{
						info.topDivLevel = DivLevel::WEAK;
						info.topDivDesc = forceWeak ? _T("弱顶背(无量)") : _T("弱顶背");
					}
					else if (checkCount >= 3)
					{
						info.topDivLevel = DivLevel::STRONG; // 0轴下方+3极值+放量=强背离
						info.topDivDesc = _T("强顶背");
					}
					else
					{
						info.topDivLevel = DivLevel::STANDARD; // 0轴下方+2极值+放量=标准背离
						info.topDivDesc = _T("顶背离");
					}
				}
			}
		}

		// ===== 底背离检测 =====
		if (lowExts.size() >= 2)
		{
			size_t checkCount = lowExts.size() >= 3 ? 3 : lowExts.size();
			auto itEnd = lowExts.end();
			auto itStart = itEnd - checkCount;

			// 价格低点是否逐次降低
			bool priceDesc = true;
			for (auto it = itStart + 1; it != itEnd; ++it)
			{
				if (it->priceVal >= (it - 1)->priceVal) { priceDesc = false; break; }
			}
			// DIF低点是否逐次抬高
			bool difAsc = true;
			for (auto it = itStart + 1; it != itEnd; ++it)
			{
				if (it->difVal <= (it - 1)->difVal) { difAsc = false; break; }
			}

			if (priceDesc && difAsc)
			{
				double priceDiff = (lowExts.rbegin() + 1)->priceVal - lowExts.back().priceVal;
				double difDiff = lowExts.back().difVal - (lowExts.rbegin() + 1)->difVal;

				if (priceDiff >= minPriceDiff && difDiff >= minDifDiff)
				{
					info.hasBottomDiv = true;

					// 量能校验：回调缩量才认定有效底背离，放量急跌降级为弱背离
					bool volShrink = (avgVol > 0 && lowExts.back().volume < avgVol * 0.7);

					// 0轴过滤：DIF在0轴下方时底背离仅标记弱反弹
					bool difBelowZero = lowExts.back().difVal < 0;

					// 放量急跌或DIF在0轴下方时降级为弱背离
					bool forceWeak = !volShrink || difBelowZero;

					if (forceWeak)
					{
						info.bottomDivLevel = DivLevel::WEAK;
						info.bottomDivDesc = (!volShrink && !difBelowZero) ? _T("弱底背(放量)") :
							(!volShrink && difBelowZero) ? _T("弱底背(放量)") : _T("弱底背");
					}
					else if (checkCount >= 3)
					{
						info.bottomDivLevel = DivLevel::STRONG; // 0轴上方+3极值+缩量=强背离
						info.bottomDivDesc = _T("强底背");
					}
					else
					{
						info.bottomDivLevel = DivLevel::STANDARD; // 0轴上方+2极值+缩量=标准背离
						info.bottomDivDesc = _T("底背离");
					}
				}
			}
		}

		return info;
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

	// 连续同向K线兜底：若连续5根阳线，直接判定上升结构
	int consecTrend = DetectConsecutiveTrend(recent);
	if (consecTrend == 1)
		return true;

	// 提取波段高低点（实时模式：仅左侧确认）
	auto swings = ExtractSwingPoints(recent, SWING_MIN_BARS, !g_backtestMode);
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

	// 连续同向K线兜底：若连续5根阴线，直接判定下跌结构
	int consecTrend = DetectConsecutiveTrend(recent);
	if (consecTrend == -1)
		return true;

	auto swings = ExtractSwingPoints(recent, SWING_MIN_BARS, !g_backtestMode);
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
	auto swings = ExtractSwingPoints(recent, SWING_MIN_BARS, !g_backtestMode);
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

	// 连续同向K线兜底：若连续5根阳线，直接判定短线多头
	int consecTrend = DetectConsecutiveTrend(recent);
	if (consecTrend == 1)
		return true;

	// 条件1：5分钟短期低点抬高
	auto swings = ExtractSwingPoints(recent, 2, !g_backtestMode);  // 5分钟用更短的极值间隔
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

	// 连续同向K线兜底：若连续5根阴线，直接判定短线空头
	int consecTrend = DetectConsecutiveTrend(recent);
	if (consecTrend == -1)
		return true;

	// 条件1：5分钟短期高点降低
	auto swings = ExtractSwingPoints(recent, 2, !g_backtestMode);
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

// ========== 回测模式控制 ==========
void CSignalAnalyzer::SetBacktestMode(bool enabled)
{
	g_backtestMode = enabled;
	CString msg;
	msg.Format(_T("回测模式：%s"), enabled ? _T("开启") : _T("关闭"));
	SignalLog(LogLevel::LOG_INFO, msg);
}

bool CSignalAnalyzer::IsBacktestMode()
{
	return g_backtestMode;
}

// ========== 跨周期时间对齐 ==========
std::vector<size_t> CSignalAnalyzer::Align5mTo30m(const std::vector<STOCK::Bar>& bars5, const std::vector<STOCK::Bar>& bars30)
{
	std::vector<size_t> alignMap(bars5.size(), SIZE_MAX);
	for (size_t i = 0; i < bars5.size(); i++)
	{
		alignMap[i] = Find30mBarFor5m(bars5[i].time, bars30);
	}
	return alignMap;
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
	PerfCounter _perf("Get5mSignal");
	if (bars5.size() < 60)
		return STOCK::Signal5m::SIG_NONE;

	const int barCount = static_cast<int>(bars5.size());
	const STOCK::Bar& last = bars5.back();

	// 1. 指标批量计算
	STOCK::BollResult boll = CalcBoll(bars5, GetParam().bollPeriod);
	std::vector<double> difSeq, deaSeq, macdBarSeq;
	CalcMACDSeries(bars5, difSeq, deaSeq, macdBarSeq);
	std::vector<double> kSeq, dSeq, jSeq;
	CalcKDJSeries(bars5, GetParam().kdjPeriod, kSeq, dSeq, jSeq);
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
	double rsi6 = CalcRSI(bars5, GetParam().rsiPeriod);
	double wr6 = CalcWR(bars5, GetParam().wrPeriod);

	// ========== 趋势强度权重：30min强趋势下弱化短线超买卖出信号 ==========
	// DIF持续0轴上方 + BOLL中轨持续上行 → 强趋势延续，压制卖出信号
	bool strongTrendUp = false;
	if (trendState == STOCK::TrendState30m::STATE_STRONG && lastIndex >= 1)
	{
		bool difAboveZero = (macd.dif > 0);
		bool bollMidUp = (boll.mid > 0) && (lastIndex >= 20);
		if (bollMidUp)
		{
			// 计算前一根BOLL中轨
			std::vector<STOCK::Bar> prevBars20(bars5.end() - 21, bars5.end() - 1);
			STOCK::BollResult prevBoll = CalcBoll(prevBars20, 20);
			bollMidUp = (boll.mid > prevBoll.mid);
		}
		strongTrendUp = difAboveZero && bollMidUp;
	}

	// ========== 连续超买钝化豁免机制 ==========
	// 连续3根K线KDJ高位钝化（K>85且逐根下降）且30min强势 → 屏蔽卖出信号，避免主升浪频繁止盈
	bool kdjTopPassiveExempt = false;
	if (trendState == STOCK::TrendState30m::STATE_STRONG && lastIndex >= 2)
	{
		bool kdjHigh3 = (kSeq[lastIndex] > 85 && kSeq[prevIndex] > 85 && kSeq[prevIndex - 1] > 85);
		bool kdjDeclining3 = (kSeq[lastIndex] < kSeq[prevIndex] && kSeq[prevIndex] < kSeq[prevIndex - 1]);
		kdjTopPassiveExempt = kdjHigh3 && kdjDeclining3;
	}

	// ========== MACD背离检测（多根极值对比+0轴过滤+ATR幅度+等级） ==========
	DivergenceInfo divInfo = CalcDivergence(bars5, difSeq, lastIndex);

	// ========== 卖出判定（必要条件组+辅助条件组） ==========
	// 必要条件组：价格突破轨道(S1) + MACD动量减弱(S2) 至少满足1个
	bool sellS1 = (last.high >= boll.up) || (last.close > boll.up);
	bool sellS2_redShrink = (macd.macd_bar > 0) && (macd.macd_bar < prevMacd.macd_bar);
	// 顶背离：弱背离不单独触发卖出，标准/强背离可触发
	bool sellS2_topDiv = divInfo.hasTopDiv && (divInfo.topDivLevel >= DivLevel::STANDARD);
	bool sellS2 = sellS2_redShrink || sellS2_topDiv;
	bool sellNecessary = (sellS1 || sellS2);

	// 辅助条件组：KDJ/RSI/WR 三个超买指标
	bool sellA1 = ((kdj.k > KDJ_OVERBUY && kdj.d > SELL_KDJ_D_OVERBUY && kdj.j < prevKdj.j) || kdj.j > 100.0);
	bool sellA2 = (rsi6 > 70);
	bool sellA3 = (wr6 < SELL_WR_OVERBUY);
	// 量能辅助：冲高放量（当前成交量 > 前5根均量×1.3 且当前为上涨K线）
	double avgVol5 = 0;
	if (lastIndex >= 5)
	{
		double volSum = 0;
		for (size_t j = lastIndex - 5; j < lastIndex; j++) volSum += bars5[j].volume;
		avgVol5 = volSum / 5.0;
	}
	bool sellA4_volSurge = (avgVol5 > 0 && last.volume > avgVol5 * VOL_RATIO_THRESHOLD && last.close > last.open);
	int sellAuxCount = (sellA1 ? 1 : 0) + (sellA2 ? 1 : 0) + (sellA3 ? 1 : 0) + (sellA4_volSurge ? 1 : 0);

	// 分趋势差异化配置辅助指标达标数量
	int sellAuxThreshold = 2;  // 默认：卖出辅助≥2
	if (trendState == STOCK::TrendState30m::STATE_STRONG)
		sellAuxThreshold = 2;   // 强势：卖出辅助≥2（收紧做空，防卖飞）
	else if (trendState == STOCK::TrendState30m::STATE_SHAKE || trendState == STOCK::TrendState30m::STATE_WEAK_SHAKE)
		sellAuxThreshold = 2;   // 震荡/弱震荡：卖出辅助≥2（平衡多空频率）
	// 弱势也要求≥2，防止高位回落后仅剩1个超买指标就触发低位追卖

	bool hasSell = sellNecessary && (sellAuxCount >= sellAuxThreshold);

	// 趋势强度权重：强趋势延续时，仅单指标超买不足以触发卖出
	if (hasSell && strongTrendUp && sellAuxCount < 2)
		hasSell = false;

	// 连续超买钝化豁免：主升浪中KDJ高位钝化屏蔽卖出
	if (hasSell && kdjTopPassiveExempt)
		hasSell = false;

	// 无量冲高不过滤卖出信号（冲高抛、回落接做T思路）

	// ========== 买入判定（必要条件组+辅助条件组） ==========
	// 必要条件组：价格触及轨道(B1) + MACD信号(B2) 至少满足1个
	bool buyB1 = (last.low <= boll.dn) || (last.close < boll.dn);
	// MACD：绿柱缩短 or 底背离（DIF远离0轴才认定有效动量信号）
	double macdDifThreshold = GetMacdDifThreshold(last.close, CalcATR(bars5, lastIndex));
	bool buyB2_greenShrink = (macd.macd_bar < 0) && (macd.macd_bar > prevMacd.macd_bar) && fabs(macd.dif) > macdDifThreshold;
	// 底背离：弱背离不单独触发买入，标准/强背离可触发
	bool buyB2_bottomDiv = divInfo.hasBottomDiv && (divInfo.bottomDivLevel >= DivLevel::STANDARD);
	bool buyB2 = buyB2_greenShrink || buyB2_bottomDiv;
	bool buyNecessary = (buyB1 || buyB2);

	// 辅助条件组：KDJ/RSI/WR 三个超卖指标
	bool buyA1 = (kdj.k < KDJ_OVERSELL && kdj.d < KDJ_OVERSELL && kdj.j > prevKdj.j);
	bool buyA2 = (rsi6 < 30);
	bool buyA3 = (wr6 > 80);
	// 量能辅助：回调缩量（当前成交量 < 前5根均量×0.7）
	bool buyA4_volShrink = (avgVol5 > 0 && last.volume < avgVol5 * 0.7);
	int buyAuxCount = (buyA1 ? 1 : 0) + (buyA2 ? 1 : 0) + (buyA3 ? 1 : 0) + (buyA4_volShrink ? 1 : 0);

	// 分趋势差异化配置辅助指标达标数量
	int buyAuxThreshold = 2;  // 默认：买入辅助≥2
	if (trendState == STOCK::TrendState30m::STATE_STRONG)
		buyAuxThreshold = 1;   // 强势：买入辅助≥1即可（顺势放宽做多）
	else if (trendState == STOCK::TrendState30m::STATE_SHAKE || trendState == STOCK::TrendState30m::STATE_WEAK_SHAKE)
		buyAuxThreshold = 2;   // 震荡/弱震荡：买入辅助≥2（平衡多空频率）

	bool hasBuy = buyNecessary && (buyAuxCount >= buyAuxThreshold);

	// 放量大跌不过滤买入信号（冲高抛、回落接做T思路）

	// ========== 共振总分计算 ==========
	// 每个指标对买入/卖出方向贡献1分强度（必要+辅助共6项：BOLL/MACD/RSI/KDJ/WR/VOL）
	// 弱背离额外贡献0.5分（降低权重，不单独触发买卖）
	int sellStrBoll = (sellS1 ? 1 : 0);
	int sellStrMacd = (sellS2 ? 1 : 0) + (divInfo.hasTopDiv && divInfo.topDivLevel == DivLevel::WEAK ? 1 : 0);
	int sellStrRsi = (sellA2 ? 1 : 0);
	int sellStrKdj = (sellA1 ? 1 : 0);
	int sellStrWr = (sellA3 ? 1 : 0);
	int sellStrVol = (sellA4_volSurge ? 1 : 0);
	int sellTotalStr = sellStrBoll + sellStrMacd + sellStrRsi + sellStrKdj + sellStrWr + sellStrVol;

	int buyStrBoll = (buyB1 ? 1 : 0);
	int buyStrMacd = (buyB2 ? 1 : 0) + (divInfo.hasBottomDiv && divInfo.bottomDivLevel == DivLevel::WEAK ? 1 : 0);
	int buyStrRsi = (buyA2 ? 1 : 0);
	int buyStrKdj = (buyA1 ? 1 : 0);
	int buyStrWr = (buyA3 ? 1 : 0);
	int buyStrVol = (buyA4_volShrink ? 1 : 0);
	int buyTotalStr = buyStrBoll + buyStrMacd + buyStrRsi + buyStrKdj + buyStrWr + buyStrVol;

	// ========== MA20位置判定（冲突仲裁辅助） ==========
	// 强势趋势下，价格在MA20上方禁止优先卖出；弱势趋势下，价格在MA20下方禁止优先买入
	double ma20 = 0;
	if (bars5.size() >= 20)
	{
		std::vector<double> closes20;
		closes20.reserve(20);
		for (auto it = bars5.end() - 20; it != bars5.end(); ++it)
			closes20.push_back(it->close);
		ma20 = CalcMA(closes20, 20);
	}
	bool priceAboveMA20 = (ma20 > 0 && last.close > ma20);
	bool priceBelowMA20 = (ma20 > 0 && last.close < ma20);

	// 信号输出
	if (hasSell && hasBuy)
	{
		// 第1优先级：共振总分高的方向优先
		if (sellTotalStr > buyTotalStr)
		{
			// 卖出总分高，但强势+价格在MA20上方时禁止优先卖出
			if (trendState == STOCK::TrendState30m::STATE_STRONG && priceAboveMA20)
				return STOCK::Signal5m::SIG_BUY;
			return STOCK::Signal5m::SIG_SELL;
		}
		if (buyTotalStr > sellTotalStr)
		{
			// 买入总分高，但弱势+价格在MA20下方时禁止优先买入
			if (trendState == STOCK::TrendState30m::STATE_WEAK && priceBelowMA20)
				return STOCK::Signal5m::SIG_SELL;
			return STOCK::Signal5m::SIG_BUY;
		}

		// 第2优先级：总分相同时看30min趋势方向
		if (trendState == STOCK::TrendState30m::STATE_STRONG)
		{
			// 强势优先买，但价格在MA20下方时倾向卖出（破位风险）
			if (priceBelowMA20) return STOCK::Signal5m::SIG_SELL;
			return STOCK::Signal5m::SIG_BUY;
		}
		if (trendState == STOCK::TrendState30m::STATE_WEAK)
		{
			// 弱势优先卖，但价格在MA20上方时倾向买入（反弹信号）
			if (priceAboveMA20) return STOCK::Signal5m::SIG_BUY;
			return STOCK::Signal5m::SIG_SELL;
		}
		if (trendState == STOCK::TrendState30m::STATE_WEAK_SHAKE)
		{
			// 弱震荡：略偏空，价格在MA20下方时优先卖出
			if (priceBelowMA20) return STOCK::Signal5m::SIG_SELL;
			return STOCK::Signal5m::SIG_BUY;
		}

		// 震荡：取当前大周期方向（默认买入，震荡中回调低吸优先）
		return STOCK::Signal5m::SIG_BUY;
	}
	if (hasSell) return STOCK::Signal5m::SIG_SELL;
	if (hasBuy) return STOCK::Signal5m::SIG_BUY;
	return STOCK::Signal5m::SIG_NONE;
}

// ========== 双通道风控过滤器 ==========
// 区分forbidBuy/forbidSell，不同趋势状态下差异化风控

CSignalAnalyzer::ForbidResult CSignalAnalyzer::CalcForbidResult(const std::vector<STOCK::Bar>& bars5, STOCK::TrendState30m trendState)
{
	ForbidResult result;
	if (bars5.size() < 60)
		return result;

	// 布林带宽放大：使用当前带宽增长率或历史高分位，避免强制连续三根导致风控滞后
	STOCK::BollResult boll = CalcBoll(bars5, GetParam().bollPeriod);
	int bollP = GetParam().bollPeriod;
	std::vector<double> bollBandSeq(bars5.size(), 0.0);
	for (size_t i = bollP - 1; i < bars5.size(); i++)
	{
		std::vector<double> closes;
		closes.reserve(bollP);
		for (size_t j = i - bollP + 1; j <= i; j++)
			closes.push_back(bars5[j].close);
		bollBandSeq[i] = 4.0 * CalcStdDev(closes);
	}
	bool bandExpand = IsBollExpand(bollBandSeq, bars5.size() - 1);
	double avgBollBandwidth = CalcAverageBollBandwidth(bollBandSeq, bars5.size() - 1);
	double atr5 = CalcATR(bars5, bars5.size() - 1);
	bool narrowBand = IsNarrowBoll(avgBollBandwidth, atr5);

	// BOLL中轨方向：上涨时扩张禁止卖出，下跌时扩张禁止买入
	bool bollMidUp = false;
	if (bars5.size() >= 21)
	{
		std::vector<STOCK::Bar> prevBars20(bars5.end() - 21, bars5.end() - 1);
		STOCK::BollResult prevBoll = CalcBoll(prevBars20, GetParam().bollPeriod);
		bollMidUp = (boll.mid > prevBoll.mid);
	}

	// KDJ钝化
	std::vector<STOCK::Bar> prevBars(bars5.begin(), bars5.end() - 1);
	std::vector<STOCK::Bar> prevPrevBars(bars5.begin(), bars5.end() - 2);
	STOCK::KDJResult kdj = CalcKDJ(bars5, GetParam().kdjPeriod);
	STOCK::KDJResult prevKdj = CalcKDJ(prevBars, GetParam().kdjPeriod);
	STOCK::KDJResult prevPrevKdj = CalcKDJ(prevPrevBars, GetParam().kdjPeriod);
	bool kdjTopPassive = (kdj.k > 85 && prevKdj.k > 85 && prevPrevKdj.k > 85
		&& kdj.k < prevKdj.k && prevKdj.k < prevPrevKdj.k);
	bool kdjBottomPassive = (kdj.k < 15 && prevKdj.k < 15 && prevPrevKdj.k < 15
		&& kdj.k > prevKdj.k && prevKdj.k > prevPrevKdj.k);

	// WR钝化
	double wr6 = CalcWR(bars5, GetParam().wrPeriod);
	double wrPrev = CalcWR(prevBars, GetParam().wrPeriod);
	double wrPrevPrev = CalcWR(prevPrevBars, GetParam().wrPeriod);
	bool wrTopPassive = (wr6 < 18 && wrPrev < 18 && wrPrevPrev < 18
		&& wr6 > wrPrev && wrPrev > wrPrevPrev);
	bool wrBottomPassive = (wr6 > 82 && wrPrev > 82 && wrPrevPrev > 82
		&& wr6 < wrPrev && wrPrev < wrPrevPrev);

	// 按趋势状态差异化配置双通道风控
	// 布林风控规则：
	// 1. 窄布林统一禁止新开仓（变盘不确定性高，无论强弱/震荡）
	// 2. 布林扩张区分多空：强势多头扩张禁止做空，弱势空头扩张禁止做多
	if (trendState == STOCK::TrendState30m::STATE_STRONG)
	{
		// 强势：底部钝化禁止买入，顶部钝化禁止卖出
		if (kdjBottomPassive) { result.forbidBuy = true; result.forbidBuyReason = _T("KDJ钝化"); }
		if (wrBottomPassive) { result.forbidBuy = true; if (result.forbidBuyReason.IsEmpty()) result.forbidBuyReason = _T("WR钝化"); }
		if (kdjTopPassive) { result.forbidSell = true; result.forbidSellReason = _T("KDJ钝化"); }
		if (wrTopPassive) { result.forbidSell = true; if (result.forbidSellReason.IsEmpty()) result.forbidSellReason = _T("WR钝化"); }
		// 窄布林统一禁止新开仓
		if (narrowBand) { result.forbidBuy = true; if (result.forbidBuyReason.IsEmpty()) result.forbidBuyReason = _T("BOLL过窄"); }
		// 布林扩张：中轨上行禁止卖出（防卖飞），中轨下行禁止买入（防追跌）
		if (bandExpand && bollMidUp) { result.forbidSell = true; if (result.forbidSellReason.IsEmpty()) result.forbidSellReason = _T("BOLL扩张"); }
		if (bandExpand && !bollMidUp) { result.forbidBuy = true; if (result.forbidBuyReason.IsEmpty()) result.forbidBuyReason = _T("BOLL扩张"); }
	}
	else if (trendState == STOCK::TrendState30m::STATE_WEAK)
	{
		// 弱势：底部钝化禁止逆势卖出（防弱势抄底失败后割肉）
		if (kdjBottomPassive) { result.forbidSell = true; result.forbidSellReason = _T("KDJ钝化"); }
		if (wrBottomPassive) { result.forbidSell = true; if (result.forbidSellReason.IsEmpty()) result.forbidSellReason = _T("WR钝化"); }
		// 窄布林统一禁止新开仓
		if (narrowBand) { result.forbidBuy = true; result.forbidBuyReason = _T("BOLL过窄"); }
		// 布林扩张：中轨上行禁止卖出，中轨下行禁止买入
		if (bandExpand && bollMidUp) { result.forbidSell = true; if (result.forbidSellReason.IsEmpty()) result.forbidSellReason = _T("BOLL扩张"); }
		if (bandExpand && !bollMidUp) { result.forbidBuy = true; if (result.forbidBuyReason.IsEmpty()) result.forbidBuyReason = _T("BOLL扩张"); }
	}
	else if (trendState == STOCK::TrendState30m::STATE_WEAK_SHAKE)
	{
		// 弱震荡：偏弱缓冲，底部钝化限制买入
		if (kdjBottomPassive) { result.forbidBuy = true; result.forbidBuyReason = _T("KDJ钝化"); }
		if (wrBottomPassive) { result.forbidBuy = true; if (result.forbidBuyReason.IsEmpty()) result.forbidBuyReason = _T("WR钝化"); }
		// 窄布林统一禁止新开仓
		if (narrowBand) { result.forbidBuy = true; if (result.forbidBuyReason.IsEmpty()) result.forbidBuyReason = _T("BOLL过窄"); }
		// 布林扩张：中轨上行禁止卖出，中轨下行禁止买入
		if (bandExpand && bollMidUp) { result.forbidSell = true; if (result.forbidSellReason.IsEmpty()) result.forbidSellReason = _T("BOLL扩张"); }
		if (bandExpand && !bollMidUp) { result.forbidBuy = true; if (result.forbidBuyReason.IsEmpty()) result.forbidBuyReason = _T("BOLL扩张"); }
	}
	else  // STATE_SHAKE
	{
		// 震荡：底部钝化限制买入，不限制卖出
		if (kdjBottomPassive) { result.forbidBuy = true; result.forbidBuyReason = _T("KDJ钝化"); }
		if (wrBottomPassive) { result.forbidBuy = true; if (result.forbidBuyReason.IsEmpty()) result.forbidBuyReason = _T("WR钝化"); }
		// 窄布林统一禁止新开仓
		if (narrowBand) { result.forbidBuy = true; if (result.forbidBuyReason.IsEmpty()) result.forbidBuyReason = _T("BOLL过窄"); }
		// 布林扩张：中轨上行禁止卖出，中轨下行禁止买入
		if (bandExpand && bollMidUp) { result.forbidSell = true; if (result.forbidSellReason.IsEmpty()) result.forbidSellReason = _T("BOLL扩张"); }
		if (bandExpand && !bollMidUp) { result.forbidBuy = true; if (result.forbidBuyReason.IsEmpty()) result.forbidBuyReason = _T("BOLL扩张"); }
	}

	if (result.forbidBuy)
		SignalLog(LogLevel::LOG_WARN, _T("风控拦截：禁止买入 - ") + result.forbidBuyReason);
	if (result.forbidSell)
		SignalLog(LogLevel::LOG_WARN, _T("风控拦截：禁止卖出 - ") + result.forbidSellReason);

	return result;
}

// ========== 智能分析模块：完整买卖点判定函数 ==========
// 严格按执行顺序：30min趋势 → 风控过滤 → 5min共振信号 → 趋势分支处理

CSignalAnalyzer::T0Signal CSignalAnalyzer::DetectSmartSignal(const std::vector<STOCK::Bar>& bars5, const std::vector<STOCK::Bar>& bars30, double lastSignalPrice)
{
	T0Signal signal;

	// ===== 第1步：获取30分钟全局趋势 =====
	STOCK::TrendStateResult trendResult = Get30mTrendState(bars30);
	STOCK::TrendState30m trendState = trendResult.state;
	signal.trendState = trendState;

	// ===== 第2步：双通道风控过滤 =====
	ForbidResult forbid = CalcForbidResult(bars5, trendState);

	// ===== 第3步：获取5分钟共振买卖信号 =====
	STOCK::Signal5m sig = Get5mSignal(bars5, trendState);

	// 计算背离信息（用于reason记录，方便复盘识别假背离）
	std::vector<double> difSeqForDiv, deaSeqForDiv, macdBarSeqForDiv;
	CalcMACDSeries(bars5, difSeqForDiv, deaSeqForDiv, macdBarSeqForDiv);
	DivergenceInfo divInfoForReason;
	if (difSeqForDiv.size() == bars5.size())
		divInfoForReason = CalcDivergence(bars5, difSeqForDiv, bars5.size() - 1);

	if (sig == STOCK::Signal5m::SIG_NONE)
	{
		signal.valid = false;
		return signal;
	}

	// 风控分别拦截买卖信号
	if (sig == STOCK::Signal5m::SIG_BUY && forbid.forbidBuy)
	{
		signal.isForbid = true;
		signal.valid = false;
		signal.reason = _T("风控限制买入：") + forbid.forbidBuyReason;
		signal.forbidReason = forbid.forbidBuyReason;
		return signal;
	}
	if (sig == STOCK::Signal5m::SIG_SELL && forbid.forbidSell)
	{
		signal.isForbid = true;
		signal.valid = false;
		signal.reason = _T("风控限制卖出：") + forbid.forbidSellReason;
		signal.forbidReason = forbid.forbidSellReason;
		return signal;
	}

	// ===== 第4步：分趋势分支处理信号 =====
	// 信号价差过滤：根据信号方向使用多空差异化阈值
	if (lastSignalPrice > 0)
	{
		double diffThreshold = (sig == STOCK::Signal5m::SIG_BUY)
			? GetLongThreshold(bars5, bars5.size() - 1, trendState)
			: GetShortThreshold(bars5, bars5.size() - 1, trendState);
		if (fabs(bars5.back().close - lastSignalPrice) < diffThreshold)
		{
			signal.valid = false;
			return signal;
		}
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
			if (divInfoForReason.hasTopDiv) signal.reason += _T("+") + divInfoForReason.topDivDesc;
		}
		else if (sig == STOCK::Signal5m::SIG_BUY)
		{
			signal.isBuy = true;
			signal.reason = _T("反买");
			if (divInfoForReason.hasBottomDiv) signal.reason += _T("+") + divInfoForReason.bottomDivDesc;
		}
	}
	else if (trendState == STOCK::TrendState30m::STATE_STRONG)
	{
		// 强势：买入可加仓做正T，卖出止盈
		if (sig == STOCK::Signal5m::SIG_BUY)
		{
			signal.isBuy = true;
			signal.reason = _T("正买");
			if (divInfoForReason.hasBottomDiv) signal.reason += _T("+") + divInfoForReason.bottomDivDesc;
		}
		else if (sig == STOCK::Signal5m::SIG_SELL)
		{
			signal.isBuy = false;
			signal.reason = _T("正卖");
			if (divInfoForReason.hasTopDiv) signal.reason += _T("+") + divInfoForReason.topDivDesc;
		}
	}
	else  // STATE_SHAKE / STATE_WEAK_SHAKE
	{
		// 震荡/弱震荡：买卖信号自由执行
		if (sig == STOCK::Signal5m::SIG_BUY)
		{
			signal.isBuy = true;
			signal.reason = (trendState == STOCK::TrendState30m::STATE_WEAK_SHAKE) ? _T("弱震买") : _T("正买");
			if (divInfoForReason.hasBottomDiv) signal.reason += _T("+") + divInfoForReason.bottomDivDesc;
		}
		else if (sig == STOCK::Signal5m::SIG_SELL)
		{
			signal.isBuy = false;
			signal.reason = (trendState == STOCK::TrendState30m::STATE_WEAK_SHAKE) ? _T("弱震卖") : _T("正卖");
			if (divInfoForReason.hasTopDiv) signal.reason += _T("+") + divInfoForReason.topDivDesc;
		}
	}

	if (signal.valid)
		SignalLog(LogLevel::LOG_INFO, _T("信号触发：") + signal.reason);

	return signal;
}

// ========== 智能分析模块：MACD批量序列计算 ==========
void CSignalAnalyzer::CalcMACDSeries(const std::vector<STOCK::Bar>& bars, std::vector<double>& difSeq, std::vector<double>& deaSeq, std::vector<double>& barSeq)
{
	difSeq.clear(); deaSeq.clear(); barSeq.clear();
	size_t n = bars.size();
	if (n < 2) { difSeq.resize(n, 0); deaSeq.resize(n, 0); barSeq.resize(n, 0); return; }

	std::vector<double> ema12(n), ema26(n);
		ema12[0] = SafeDouble(bars[0].close, "MACD ema12[0]");
		ema26[0] = SafeDouble(bars[0].close, "MACD ema26[0]");
		double k12 = 2.0 / 13.0;
		double k26 = 2.0 / 27.0;
		for (size_t i = 1; i < n; i++)
		{
			ema12[i] = SafeDouble(bars[i].close * k12 + ema12[i - 1] * (1.0 - k12), "MACD ema12");
			ema26[i] = SafeDouble(bars[i].close * k26 + ema26[i - 1] * (1.0 - k26), "MACD ema26");
		}

		difSeq.resize(n); deaSeq.resize(n); barSeq.resize(n);
		deaSeq[0] = difSeq[0];
		for (size_t i = 0; i < n; i++)
			difSeq[i] = SafeDouble(ema12[i] - ema26[i], "MACD dif");

		double k9 = 2.0 / 10.0;
		deaSeq[0] = difSeq[0];
		for (size_t i = 1; i < n; i++)
			deaSeq[i] = SafeDouble(difSeq[i] * k9 + deaSeq[i - 1] * (1.0 - k9), "MACD dea");

		for (size_t i = 0; i < n; i++)
			barSeq[i] = SafeDouble(2.0 * (difSeq[i] - deaSeq[i]), "MACD bar");
	}

// ========== 智能分析模块：批量信号检测 ==========
// 一次性计算全部指标序列，然后逐根判定信号

std::vector<CSignalAnalyzer::SmartSignalPoint> CSignalAnalyzer::BatchDetectSignals(const std::vector<STOCK::Bar>& bars5, const std::vector<STOCK::Bar>& bars30)
{
	PerfCounter _perf("BatchDetectSignals");
	std::vector<SmartSignalPoint> result;
	size_t n = bars5.size();
	// EMA稳定性：至少60根K线用于盘中短线检测
	if (n < 60) return result;

	// 1. 跨周期时间对齐：每根5min Bar匹配对应30min Bar
	std::vector<size_t> alignMap = Align5mTo30m(bars5, bars30);

	// 2. 预计算30min趋势序列：逐根5min Bar动态获取当期30min趋势
	// 不再整段共用同一个trendState，消除跨周期对齐偏差
	std::vector<STOCK::TrendState30m> trendStateSeq(n, STOCK::TrendState30m::STATE_SHAKE);
	{
		// 对每个30min Bar索引，计算其趋势状态（缓存避免重复计算）
		std::map<size_t, STOCK::TrendState30m> trendCache;
		for (size_t i = 0; i < n; i++)
		{
			size_t idx30 = alignMap[i];
			if (idx30 == SIZE_MAX || idx30 < 1)
			{
				trendStateSeq[i] = STOCK::TrendState30m::STATE_SHAKE;
				continue;
			}
			auto it = trendCache.find(idx30);
			if (it != trendCache.end())
			{
				trendStateSeq[i] = it->second;
			}
			else
			{
				// 用该30min Bar及之前的K线计算趋势
				std::vector<STOCK::Bar> bars30sub(bars30.begin(), bars30.begin() + idx30 + 1);
				STOCK::TrendStateResult tr = Get30mTrendState(bars30sub);
				trendCache[idx30] = tr.state;
				trendStateSeq[i] = tr.state;
			}
		}
	}
	// 当前趋势（取最后一根5min Bar对应的趋势，用于循环外逻辑）
	STOCK::TrendState30m trendState = trendStateSeq.back();

	// 2. 批量计算5分钟指标序列
	// BOLL序列
	int bollP = GetParam().bollPeriod;
	std::vector<double> bollMid(n, 0), bollUp(n, 0), bollDn(n, 0), bollBand(n, 0);
	for (size_t i = bollP - 1; i < n; i++)
	{
		std::vector<double> closes;
		for (size_t j = i - bollP + 1; j <= i; j++) closes.push_back(bars5[j].close);
		double mid = CalcMA(closes, bollP);
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
	CalcKDJSeries(bars5, GetParam().kdjPeriod, kSeq, dSeq, jSeq);

	// RSI序列
	int rsiP = GetParam().rsiPeriod;
	std::vector<double> rsi6Seq(n, 50);
	{
		if (n >= static_cast<size_t>(rsiP + 1))
		{
			std::vector<double> upList, dnList;
			for (size_t i = 1; i < n; i++) { double d = bars5[i].close - bars5[i - 1].close; upList.push_back(max(d, 0.0)); dnList.push_back(max(-d, 0.0)); }
			double au = 0, ad = 0;
			if (GetParam().rsiUseEma)
			{
				// EMA模式
				double k = 2.0 / (rsiP + 1.0);
				au = upList[0]; ad = dnList[0];
				for (size_t i = 1; i < upList.size(); i++)
				{
					au = upList[i] * k + au * (1.0 - k);
					ad = dnList[i] * k + ad * (1.0 - k);
					if (ad == 0) rsi6Seq[i + 1] = 99.99; else rsi6Seq[i + 1] = 100.0 - 100.0 / (1.0 + au / ad);
				}
			}
			else
			{
				// SMA模式
				for (int i = 0; i < rsiP && i < static_cast<int>(upList.size()); i++) { au += upList[i]; ad += dnList[i]; }
				au /= rsiP; ad /= rsiP;
				if (ad == 0) rsi6Seq[rsiP] = 99.99; else rsi6Seq[rsiP] = 100.0 - 100.0 / (1.0 + au / ad);
				for (size_t i = rsiP; i < upList.size(); i++)
				{
					au = (upList[i] + (rsiP - 1.0) * au) / rsiP;
					ad = (dnList[i] + (rsiP - 1.0) * ad) / rsiP;
					if (ad == 0) rsi6Seq[i + 1] = 99.99; else rsi6Seq[i + 1] = 100.0 - 100.0 / (1.0 + au / ad);
				}
			}
		}
	}

	// WR序列
	int wrP = GetParam().wrPeriod;
	std::vector<double> wr6Seq(n, 50);
	for (size_t i = wrP - 1; i < n; i++)
	{
		double hhv = bars5[i - wrP + 1].high, llv = bars5[i - wrP + 1].low;
		for (size_t j = i - wrP + 2; j <= i; j++) { if (bars5[j].high > hhv) hhv = bars5[j].high; if (bars5[j].low < llv) llv = bars5[j].low; }
		wr6Seq[i] = (hhv != llv) ? 100.0 * (hhv - bars5[i].close) / (hhv - llv) : 50.0;
	}

	// 量能均量序列（前5根均量，用于量能辅助条件）
	std::vector<double> avgVol5Seq(n, 0);
	for (size_t i = 5; i < n; i++)
	{
		double volSum = 0;
		for (size_t j = i - 5; j < i; j++) volSum += bars5[j].volume;
		avgVol5Seq[i] = volSum / 5.0;
	}

	// 3. 逐根K线检测信号（使用预计算序列，缓存上一根指标结果）
	// 预计算前一BOLL中轨序列（用于趋势强度权重判定）
	std::vector<double> prevBollMidSeq(n, 0);
	for (size_t i = 20; i < n; i++)
	{
		std::vector<double> closes;
		for (size_t j = i - 20; j < i; j++) closes.push_back(bars5[j].close);
		prevBollMidSeq[i] = CalcMA(closes, 20);
	}

	// ATR序列预计算（用于窄布林ATR动态阈值）
	int atrPeriod = GetParam().atrPeriod;
	std::vector<double> atrSeq(n, 0);
	for (size_t i = atrPeriod; i < n; i++)
	{
		std::vector<STOCK::Bar> barsForATR(bars5.begin() + i - atrPeriod, bars5.begin() + i + 1);
		atrSeq[i] = CalcATR(barsForATR, barsForATR.size() - 1);
	}

	// 分趋势差异化辅助指标达标数量（动态计算，每根K线根据当期trendState确定）
	// 不再循环外固定，因为跨周期对齐后trendState逐根变化

	int lastForbidBarIndex = -MIN_SIGNAL_BAR_GAP;
	for (size_t i = 21; i < n; i++)
	{
		// 当期30min趋势（跨周期对齐，逐根动态获取）
		STOCK::TrendState30m curTrendState = trendStateSeq[i];

		// 分趋势差异化辅助指标达标数量
		int sellAuxThreshold = 2;  // 默认：卖出辅助≥2（弱势也要求≥2，防低位追卖）
		if (curTrendState == STOCK::TrendState30m::STATE_STRONG)
			sellAuxThreshold = 2;   // 强势：卖出辅助≥2（收紧做空，防卖飞）
		else if (curTrendState == STOCK::TrendState30m::STATE_SHAKE || curTrendState == STOCK::TrendState30m::STATE_WEAK_SHAKE)
			sellAuxThreshold = 2;   // 震荡/弱震荡：卖出辅助≥2

		int buyAuxThreshold = 2;  // 默认：买入辅助≥2
		if (curTrendState == STOCK::TrendState30m::STATE_STRONG)
			buyAuxThreshold = 1;   // 强势：买入辅助≥1即可（顺势放宽做多）
		else if (curTrendState == STOCK::TrendState30m::STATE_SHAKE || curTrendState == STOCK::TrendState30m::STATE_WEAK_SHAKE)
			buyAuxThreshold = 2;   // 震荡/弱震荡：买入辅助≥2

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

		// 风控判定：双通道分别判定禁止买入/禁止卖出
		// 布林风控规则：
		// 1. 窄布林统一禁止新开仓（变盘不确定性高，无论强弱/震荡）
		// 2. 布林扩张：中轨上行禁止卖出，中轨下行禁止买入
		bool forbidBuy = false;
		bool forbidSell = false;
		CString forbidBuyReason;
		CString forbidSellReason;
		bool narrowBand = IsNarrowBoll(CalcAverageBollBandwidth(bollBand, i), atrSeq[i]);
		bool bollMidUp = (bollMid[i] > prevBollMidSeq[i]);
		if (curTrendState == STOCK::TrendState30m::STATE_STRONG)
		{
			// 强势：底部钝化禁止买入，顶部钝化禁止卖出
			if (kdjBottomPassive) { forbidBuy = true; forbidBuyReason = _T("KDJ钝化"); }
			if (wrBottomPassive) { forbidBuy = true; if (forbidBuyReason.IsEmpty()) forbidBuyReason = _T("WR钝化"); }
			if (kdjTopPassive) { forbidSell = true; forbidSellReason = _T("KDJ钝化"); }
			if (wrTopPassive) { forbidSell = true; if (forbidSellReason.IsEmpty()) forbidSellReason = _T("WR钝化"); }
			// 窄布林统一禁止新开仓
			if (narrowBand) { forbidBuy = true; if (forbidBuyReason.IsEmpty()) forbidBuyReason = _T("BOLL过窄"); }
			// 布林扩张：中轨上行禁止卖出，中轨下行禁止买入
			if (bandExpand && bollMidUp) { forbidSell = true; if (forbidSellReason.IsEmpty()) forbidSellReason = _T("BOLL扩张"); }
			if (bandExpand && !bollMidUp) { forbidBuy = true; if (forbidBuyReason.IsEmpty()) forbidBuyReason = _T("BOLL扩张"); }
		}
		else if (curTrendState == STOCK::TrendState30m::STATE_WEAK)
		{
			// 弱势：底部钝化禁止逆势卖出
			if (kdjBottomPassive) { forbidSell = true; forbidSellReason = _T("KDJ钝化"); }
			if (wrBottomPassive) { forbidSell = true; if (forbidSellReason.IsEmpty()) forbidSellReason = _T("WR钝化"); }
			// 窄布林统一禁止新开仓
			if (narrowBand) { forbidBuy = true; forbidBuyReason = _T("BOLL过窄"); }
			// 布林扩张：中轨上行禁止卖出，中轨下行禁止买入
			if (bandExpand && bollMidUp) { forbidSell = true; if (forbidSellReason.IsEmpty()) forbidSellReason = _T("BOLL扩张"); }
			if (bandExpand && !bollMidUp) { forbidBuy = true; if (forbidBuyReason.IsEmpty()) forbidBuyReason = _T("BOLL扩张"); }
		}
		else if (curTrendState == STOCK::TrendState30m::STATE_WEAK_SHAKE)
		{
			// 弱震荡：偏弱缓冲，底部钝化限制买入
			if (kdjBottomPassive) { forbidBuy = true; forbidBuyReason = _T("KDJ钝化"); }
			if (wrBottomPassive) { forbidBuy = true; if (forbidBuyReason.IsEmpty()) forbidBuyReason = _T("WR钝化"); }
			// 窄布林统一禁止新开仓
			if (narrowBand) { forbidBuy = true; if (forbidBuyReason.IsEmpty()) forbidBuyReason = _T("BOLL过窄"); }
			// 布林扩张：中轨上行禁止卖出，中轨下行禁止买入
			if (bandExpand && bollMidUp) { forbidSell = true; if (forbidSellReason.IsEmpty()) forbidSellReason = _T("BOLL扩张"); }
			if (bandExpand && !bollMidUp) { forbidBuy = true; if (forbidBuyReason.IsEmpty()) forbidBuyReason = _T("BOLL扩张"); }
		}
		else  // STATE_SHAKE
		{
			// 震荡：底部钝化限制买入，不限制卖出
			if (kdjBottomPassive) { forbidBuy = true; forbidBuyReason = _T("KDJ钝化"); }
			if (wrBottomPassive) { forbidBuy = true; if (forbidBuyReason.IsEmpty()) forbidBuyReason = _T("WR钝化"); }
			// 窄布林统一禁止新开仓
			if (narrowBand) { forbidBuy = true; if (forbidBuyReason.IsEmpty()) forbidBuyReason = _T("BOLL过窄"); }
			// 布林扩张：中轨上行禁止卖出，中轨下行禁止买入
			if (bandExpand && bollMidUp) { forbidSell = true; if (forbidSellReason.IsEmpty()) forbidSellReason = _T("BOLL扩张"); }
			if (bandExpand && !bollMidUp) { forbidBuy = true; if (forbidBuyReason.IsEmpty()) forbidBuyReason = _T("BOLL扩张"); }
		}

		if (forbidBuy && static_cast<int>(i) - lastForbidBarIndex >= MIN_SIGNAL_BAR_GAP)
		{
			result.push_back({ static_cast<int>(i), true, true, curTrendState, _T("风控限制买入：") + forbidBuyReason });
			lastForbidBarIndex = static_cast<int>(i);
		}
		if (forbidSell && static_cast<int>(i) - lastForbidBarIndex >= MIN_SIGNAL_BAR_GAP)
		{
			result.push_back({ static_cast<int>(i), false, true, curTrendState, _T("风控限制卖出：") + forbidSellReason });
			lastForbidBarIndex = static_cast<int>(i);
		}

		// ===== 信号判定（优化版：必要条件组+辅助条件组，解决指标共线性） =====

		// --- 趋势强度权重：30min强趋势下弱化短线超买卖出信号 ---
		bool strongTrendUp = false;
		if (curTrendState == STOCK::TrendState30m::STATE_STRONG)
		{
			bool difAboveZero = (difSeq[i] > 0);
			bool bollMidUp = (bollMid[i] > 0 && prevBollMidSeq[i] > 0 && bollMid[i] > prevBollMidSeq[i]);
			strongTrendUp = difAboveZero && bollMidUp;
		}

		// --- 连续超买钝化豁免机制 ---
		// 连续3根K线KDJ高位钝化（K>85且逐根下降）且30min强势 → 屏蔽卖出信号
		bool kdjTopPassiveExempt = false;
		if (curTrendState == STOCK::TrendState30m::STATE_STRONG && i >= 2)
		{
			bool kdjHigh3 = (kSeq[i] > 85 && kSeq[i - 1] > 85 && kSeq[i - 2] > 85);
			bool kdjDeclining3 = (kSeq[i] < kSeq[i - 1] && kSeq[i - 1] < kSeq[i - 2]);
			kdjTopPassiveExempt = kdjHigh3 && kdjDeclining3;
		}

		// --- MACD背离检测（多根极值对比+0轴过滤+ATR幅度+等级） ---
		DivergenceInfo divInfo = CalcDivergence(bars5, difSeq, i);

		// --- 卖出判定 ---
		// 必要条件组：价格突破轨道(S1) + MACD动量减弱(S2) 至少满足1个
		bool sellS1 = (bars5[i].high >= bollUp[i]) || (bars5[i].close > bollUp[i]);
		bool sellS2_redShrink = (macdBarSeq[i] > 0) && (macdBarSeq[i] < macdBarSeq[i - 1]);
		// 顶背离：弱背离不单独触发卖出，标准/强背离可触发
		bool sellS2_topDiv = divInfo.hasTopDiv && (divInfo.topDivLevel >= DivLevel::STANDARD);
		bool sellS2 = sellS2_redShrink || sellS2_topDiv;
		bool sellNecessary = (sellS1 || sellS2);

		// 辅助条件组：KDJ/RSI/WR 三个超买指标
		bool sellA1 = ((kSeq[i] > KDJ_OVERBUY && dSeq[i] > SELL_KDJ_D_OVERBUY && jSeq[i] < jSeq[i - 1]) || jSeq[i] > 100.0);
		bool sellA2 = (rsi6Seq[i] > 70);
		bool sellA3 = (wr6Seq[i] < SELL_WR_OVERBUY);
		// 量能辅助：冲高放量
		double avgVol5_i = avgVol5Seq[i];
		bool sellA4_volSurge = (avgVol5_i > 0 && bars5[i].volume > avgVol5_i * VOL_RATIO_THRESHOLD && bars5[i].close > bars5[i].open);
		int sellAuxCount = (sellA1 ? 1 : 0) + (sellA2 ? 1 : 0) + (sellA3 ? 1 : 0) + (sellA4_volSurge ? 1 : 0);

		bool hasSell = sellNecessary && (sellAuxCount >= sellAuxThreshold);

		// 趋势强度权重：强趋势延续时，仅单指标超买不足以触发卖出
		if (hasSell && strongTrendUp && sellAuxCount < 2)
			hasSell = false;

		// 连续超买钝化豁免：主升浪中KDJ高位钝化屏蔽卖出
		if (hasSell && kdjTopPassiveExempt)
			hasSell = false;

		// 无量冲高不过滤卖出信号（冲高抛、回落接做T思路）

		// --- 买入判定 ---
		// 必要条件组：价格触及轨道(B1) + MACD信号(B2) 至少满足1个
		bool buyB1 = (bars5[i].low <= bollDn[i]) || (bars5[i].close < bollDn[i]);
		// MACD：绿柱缩短 or 底背离
		double macdDifThreshold = GetMacdDifThreshold(bars5[i].close, atrSeq[i]);
		bool buyB2_greenShrink = (macdBarSeq[i] < 0) && (macdBarSeq[i] > macdBarSeq[i - 1]) && fabs(difSeq[i]) > macdDifThreshold;
		// 底背离：弱背离不单独触发买入，标准/强背离可触发
		bool buyB2_bottomDiv = divInfo.hasBottomDiv && (divInfo.bottomDivLevel >= DivLevel::STANDARD);
		bool buyB2 = buyB2_greenShrink || buyB2_bottomDiv;
		bool buyNecessary = (buyB1 || buyB2);

		// 辅助条件组：KDJ/RSI/WR 三个超卖指标
		bool buyA1 = (kSeq[i] < KDJ_OVERSELL && dSeq[i] < KDJ_OVERSELL && jSeq[i] > jSeq[i - 1]);
		bool buyA2 = (rsi6Seq[i] < 30);
		bool buyA3 = (wr6Seq[i] > 80);
		// 量能辅助：回调缩量
		bool buyA4_volShrink = (avgVol5_i > 0 && bars5[i].volume < avgVol5_i * 0.7);
		int buyAuxCount = (buyA1 ? 1 : 0) + (buyA2 ? 1 : 0) + (buyA3 ? 1 : 0) + (buyA4_volShrink ? 1 : 0);

		bool hasBuy = buyNecessary && (buyAuxCount >= buyAuxThreshold);

		// 放量大跌不过滤买入信号（冲高抛、回落接做T思路）

		if (!hasSell && !hasBuy) continue;
		// 风控分别拦截买卖信号
		if (forbidBuy && hasBuy && !hasSell)
			continue;
		if (forbidSell && hasSell && !hasBuy)
			continue;

		// ========== 共振总分计算 ==========
		// 弱背离额外贡献1分（降低权重，不单独触发买卖）
		int sellStrBoll = (sellS1 ? 1 : 0);
		int sellStrMacd = (sellS2 ? 1 : 0) + (divInfo.hasTopDiv && divInfo.topDivLevel == DivLevel::WEAK ? 1 : 0);
		int sellStrRsi = (sellA2 ? 1 : 0);
		int sellStrKdj = (sellA1 ? 1 : 0);
		int sellStrWr = (sellA3 ? 1 : 0);
		int sellStrVol = (sellA4_volSurge ? 1 : 0);
		int sellTotalStr = sellStrBoll + sellStrMacd + sellStrRsi + sellStrKdj + sellStrWr + sellStrVol;

		int buyStrBoll = (buyB1 ? 1 : 0);
		int buyStrMacd = (buyB2 ? 1 : 0) + (divInfo.hasBottomDiv && divInfo.bottomDivLevel == DivLevel::WEAK ? 1 : 0);
		int buyStrRsi = (buyA2 ? 1 : 0);
		int buyStrKdj = (buyA1 ? 1 : 0);
		int buyStrWr = (buyA3 ? 1 : 0);
		int buyStrVol = (buyA4_volShrink ? 1 : 0);
		int buyTotalStr = buyStrBoll + buyStrMacd + buyStrRsi + buyStrKdj + buyStrWr + buyStrVol;

		// MA20位置判定（冲突仲裁辅助）
		bool priceAboveMA20 = (bollMid[i] > 0 && bars5[i].close > bollMid[i]);
		bool priceBelowMA20 = (bollMid[i] > 0 && bars5[i].close < bollMid[i]);

		// 买卖同时满足时，按共振总分+趋势方向+MA20位置仲裁
		bool isBuy;
		if (hasSell && hasBuy)
		{
			if (sellTotalStr > buyTotalStr)
			{
				// 卖出总分高，但强势+价格在MA20上方时禁止优先卖出
				if (curTrendState == STOCK::TrendState30m::STATE_STRONG && priceAboveMA20)
					isBuy = true;
				else
					isBuy = false;
			}
			else if (buyTotalStr > sellTotalStr)
			{
				// 买入总分高，但弱势+价格在MA20下方时禁止优先买入
				if (curTrendState == STOCK::TrendState30m::STATE_WEAK && priceBelowMA20)
					isBuy = false;
				else
					isBuy = true;
			}
			else
			{
				// 总分相同，看30min趋势方向
				if (curTrendState == STOCK::TrendState30m::STATE_STRONG)
					isBuy = !priceBelowMA20;   // 强势优先买，破MA20则卖
				else if (curTrendState == STOCK::TrendState30m::STATE_WEAK)
					isBuy = priceAboveMA20;     // 弱势优先卖，站上MA20则买
				else if (curTrendState == STOCK::TrendState30m::STATE_WEAK_SHAKE)
					isBuy = !priceBelowMA20;    // 弱震荡略偏空，破MA20则卖
				else
					isBuy = true;               // 震荡默认买入
			}
		}
		else
		{
			isBuy = hasBuy;
		}

		// 风控禁止时，分别拦截对应方向的信号
		if (forbidBuy && isBuy)
		{
			if (hasSell && !forbidSell)
				isBuy = false;  // 有卖出条件且卖出未被风控时转为卖出
			else
				continue;  // 无可用卖出条件则跳过
		}
		if (forbidSell && !isBuy)
		{
			if (hasBuy && !forbidBuy)
				isBuy = true;  // 有买入条件且买入未被风控时转为买入
			else
				continue;  // 无可用买入条件则跳过
		}

		CString reason;
		if (curTrendState == STOCK::TrendState30m::STATE_WEAK)
			reason = isBuy ? _T("反买") : _T("反卖");
		else if (curTrendState == STOCK::TrendState30m::STATE_STRONG)
			reason = isBuy ? _T("正买") : _T("正卖");
		else if (curTrendState == STOCK::TrendState30m::STATE_WEAK_SHAKE)
			reason = isBuy ? _T("弱震买") : _T("弱震卖");
		else
			reason = isBuy ? _T("正买") : _T("正卖");

		result.push_back({ static_cast<int>(i), isBuy, false, curTrendState, reason });
	}

	// 4. 不去重，5分钟间隔已足够，同方向可连续触发（做T需要分批止盈/分批建仓）

	return result;
}

// ========== 分时图信号检测（1分钟粒度） ==========

std::vector<STOCK::Bar> CSignalAnalyzer::ConvertTimelineToBars(const std::vector<STOCK::TimelinePoint>& timeline)
{
	std::vector<STOCK::Bar> bars;
	bars.reserve(timeline.size());
	for (size_t i = 0; i < timeline.size(); i++)
	{
		double p = timeline[i].price;
		double v = static_cast<double>(timeline[i].volume);
		long t = static_cast<long>(i);  // 用索引作为时间戳
		bars.push_back(STOCK::Bar(p, p, p, p, v, t));  // open=high=low=close=price
	}
	return bars;
}

std::vector<CSignalAnalyzer::SmartSignalPoint> CSignalAnalyzer::BatchDetectSignalsFromTimeline(
	const std::vector<STOCK::Bar>& bars1m, const std::vector<STOCK::Bar>& bars30)
{
	PerfCounter _perf("BatchDetectSignalsFromTimeline");
	std::vector<SmartSignalPoint> result;
	size_t n = bars1m.size();
	// 1分钟粒度需要更多数据点（至少120个点≈2小时）
	if (n < 120) return result;

	// 1. 跨周期时间对齐：每根1min Bar匹配对应30min Bar
	// 1分钟K线每30根对应1根30分钟K线
	std::vector<size_t> alignMap(n, SIZE_MAX);
	{
		// 简化对齐：按索引比例映射
		// 30分钟K线的时间戳是5分钟K线的6倍，1分钟K线的30倍
		// 直接按 bars30 的时间范围和 bars1m 的时间范围对齐
		// 由于1分钟Bar没有真实时间戳，使用bars30的索引按比例映射
		size_t n30 = bars30.size();
		if (n30 >= 2)
		{
			for (size_t i = 0; i < n; i++)
			{
				// 按比例：1分钟Bar索引 / 总1分钟Bar数 * 总30分钟Bar数
				size_t idx30 = static_cast<size_t>(static_cast<double>(i) / n * n30);
				if (idx30 >= n30) idx30 = n30 - 1;
				if (idx30 < 1) idx30 = 1;
				alignMap[i] = idx30;
			}
		}
	}

	// 2. 预计算30min趋势序列
	std::vector<STOCK::TrendState30m> trendStateSeq(n, STOCK::TrendState30m::STATE_SHAKE);
	{
		std::map<size_t, STOCK::TrendState30m> trendCache;
		for (size_t i = 0; i < n; i++)
		{
			size_t idx30 = alignMap[i];
			if (idx30 == SIZE_MAX || idx30 < 1)
			{
				trendStateSeq[i] = STOCK::TrendState30m::STATE_SHAKE;
				continue;
			}
			auto it = trendCache.find(idx30);
			if (it != trendCache.end())
			{
				trendStateSeq[i] = it->second;
			}
			else
			{
				std::vector<STOCK::Bar> bars30sub(bars30.begin(), bars30.begin() + idx30 + 1);
				STOCK::TrendStateResult tr = Get30mTrendState(bars30sub);
				trendCache[idx30] = tr.state;
				trendStateSeq[i] = tr.state;
			}
		}
	}
	STOCK::TrendState30m trendState = trendStateSeq.back();

	// 3. 批量计算1分钟指标序列
	// BOLL序列（1分钟粒度，周期参数不变）
	int bollP = GetParam().bollPeriod;
	std::vector<double> bollMid(n, 0), bollUp(n, 0), bollDn(n, 0), bollBand(n, 0);
	for (size_t i = bollP - 1; i < n; i++)
	{
		std::vector<double> closes;
		for (size_t j = i - bollP + 1; j <= i; j++) closes.push_back(bars1m[j].close);
		double mid = CalcMA(closes, bollP);
		double sd = CalcStdDev(closes);
		bollMid[i] = mid;
		bollUp[i] = mid + 2.0 * sd;
		bollDn[i] = mid - 2.0 * sd;
		bollBand[i] = 4.0 * sd;
	}

	// MACD序列
	std::vector<double> difSeq, deaSeq, macdBarSeq;
	CalcMACDSeries(bars1m, difSeq, deaSeq, macdBarSeq);

	// KDJ序列
	std::vector<double> kSeq, dSeq, jSeq;
	CalcKDJSeries(bars1m, GetParam().kdjPeriod, kSeq, dSeq, jSeq);

	// RSI序列
	int rsiP = GetParam().rsiPeriod;
	std::vector<double> rsi6Seq(n, 50);
	{
		if (n >= static_cast<size_t>(rsiP + 1))
		{
			std::vector<double> upList, dnList;
			for (size_t i = 1; i < n; i++) { double d = bars1m[i].close - bars1m[i - 1].close; upList.push_back(max(d, 0.0)); dnList.push_back(max(-d, 0.0)); }
			double au = 0, ad = 0;
			if (GetParam().rsiUseEma)
			{
				double k = 2.0 / (rsiP + 1.0);
				au = upList[0]; ad = dnList[0];
				for (size_t i = 1; i < upList.size(); i++)
				{
					au = upList[i] * k + au * (1.0 - k);
					ad = dnList[i] * k + ad * (1.0 - k);
					if (ad == 0) rsi6Seq[i + 1] = 99.99; else rsi6Seq[i + 1] = 100.0 - 100.0 / (1.0 + au / ad);
				}
			}
			else
			{
				for (int i = 0; i < rsiP && i < static_cast<int>(upList.size()); i++) { au += upList[i]; ad += dnList[i]; }
				au /= rsiP; ad /= rsiP;
				if (ad == 0) rsi6Seq[rsiP] = 99.99; else rsi6Seq[rsiP] = 100.0 - 100.0 / (1.0 + au / ad);
				for (size_t i = rsiP; i < upList.size(); i++)
				{
					au = (upList[i] + (rsiP - 1.0) * au) / rsiP;
					ad = (dnList[i] + (rsiP - 1.0) * ad) / rsiP;
					if (ad == 0) rsi6Seq[i + 1] = 99.99; else rsi6Seq[i + 1] = 100.0 - 100.0 / (1.0 + au / ad);
				}
			}
		}
	}

	// WR序列
	int wrP = GetParam().wrPeriod;
	std::vector<double> wr6Seq(n, 50);
	for (size_t i = wrP - 1; i < n; i++)
	{
		double hhv = bars1m[i - wrP + 1].high, llv = bars1m[i - wrP + 1].low;
		for (size_t j = i - wrP + 2; j <= i; j++) { if (bars1m[j].high > hhv) hhv = bars1m[j].high; if (bars1m[j].low < llv) llv = bars1m[j].low; }
		wr6Seq[i] = (hhv != llv) ? 100.0 * (hhv - bars1m[i].close) / (hhv - llv) : 50.0;
	}

	// 量能均量序列
	std::vector<double> avgVol5Seq(n, 0);
	for (size_t i = 5; i < n; i++)
	{
		double volSum = 0;
		for (size_t j = i - 5; j < i; j++) volSum += bars1m[j].volume;
		avgVol5Seq[i] = volSum / 5.0;
	}

	// 前一BOLL中轨序列
	std::vector<double> prevBollMidSeq(n, 0);
	for (size_t i = 20; i < n; i++)
	{
		std::vector<double> closes;
		for (size_t j = i - 20; j < i; j++) closes.push_back(bars1m[j].close);
		prevBollMidSeq[i] = CalcMA(closes, 20);
	}

	// ATR序列
	int atrPeriod = GetParam().atrPeriod;
	std::vector<double> atrSeq(n, 0);
	for (size_t i = atrPeriod; i < n; i++)
	{
		std::vector<STOCK::Bar> barsForATR(bars1m.begin() + i - atrPeriod, bars1m.begin() + i + 1);
		atrSeq[i] = CalcATR(barsForATR, barsForATR.size() - 1);
	}

	// 4. 逐根1分钟Bar检测信号
	int lastForbidBarIndex = -MIN_SIGNAL_BAR_GAP;
	for (size_t i = 21; i < n; i++)
	{
		STOCK::TrendState30m curTrendState = trendStateSeq[i];

		// 分趋势差异化辅助指标达标数量
		int sellAuxThreshold = 2;
		if (curTrendState == STOCK::TrendState30m::STATE_STRONG)
			sellAuxThreshold = 2;
		else if (curTrendState == STOCK::TrendState30m::STATE_SHAKE || curTrendState == STOCK::TrendState30m::STATE_WEAK_SHAKE)
			sellAuxThreshold = 2;

		int buyAuxThreshold = 2;
		if (curTrendState == STOCK::TrendState30m::STATE_STRONG)
			buyAuxThreshold = 1;
		else if (curTrendState == STOCK::TrendState30m::STATE_SHAKE || curTrendState == STOCK::TrendState30m::STATE_WEAK_SHAKE)
			buyAuxThreshold = 2;

		// ===== 风控过滤 =====
		bool bandExpand = IsBollExpand(bollBand, i);
		bool kdjTopPassive = (i >= 2 && kSeq[i] > 85 && kSeq[i - 1] > 85 && kSeq[i - 2] > 85
			&& kSeq[i] < kSeq[i - 1] && kSeq[i - 1] < kSeq[i - 2]);
		bool kdjBottomPassive = (i >= 2 && kSeq[i] < 15 && kSeq[i - 1] < 15 && kSeq[i - 2] < 15
			&& kSeq[i] > kSeq[i - 1] && kSeq[i - 1] > kSeq[i - 2]);
		bool wrTopPassive = (i >= 2 && wr6Seq[i] < 18 && wr6Seq[i - 1] < 18 && wr6Seq[i - 2] < 18
			&& wr6Seq[i] > wr6Seq[i - 1] && wr6Seq[i - 1] > wr6Seq[i - 2]);
		bool wrBottomPassive = (i >= 2 && wr6Seq[i] > 82 && wr6Seq[i - 1] > 82 && wr6Seq[i - 2] > 82
			&& wr6Seq[i] < wr6Seq[i - 1] && wr6Seq[i - 1] < wr6Seq[i - 2]);

		bool forbidBuy = false;
		bool forbidSell = false;
		CString forbidBuyReason;
		CString forbidSellReason;
		bool narrowBand = IsNarrowBoll(CalcAverageBollBandwidth(bollBand, i), atrSeq[i]);
		bool bollMidUp = (bollMid[i] > prevBollMidSeq[i]);
		if (curTrendState == STOCK::TrendState30m::STATE_STRONG)
		{
			if (kdjBottomPassive) { forbidBuy = true; forbidBuyReason = _T("KDJ钝化"); }
			if (wrBottomPassive) { forbidBuy = true; if (forbidBuyReason.IsEmpty()) forbidBuyReason = _T("WR钝化"); }
			if (kdjTopPassive) { forbidSell = true; forbidSellReason = _T("KDJ钝化"); }
			if (wrTopPassive) { forbidSell = true; if (forbidSellReason.IsEmpty()) forbidSellReason = _T("WR钝化"); }
			if (narrowBand) { forbidBuy = true; if (forbidBuyReason.IsEmpty()) forbidBuyReason = _T("BOLL过窄"); }
			// 布林扩张：中轨上行禁止卖出，中轨下行禁止买入
			if (bandExpand && bollMidUp) { forbidSell = true; if (forbidSellReason.IsEmpty()) forbidSellReason = _T("BOLL扩张"); }
			if (bandExpand && !bollMidUp) { forbidBuy = true; if (forbidBuyReason.IsEmpty()) forbidBuyReason = _T("BOLL扩张"); }
		}
		else if (curTrendState == STOCK::TrendState30m::STATE_WEAK)
		{
			if (kdjBottomPassive) { forbidSell = true; forbidSellReason = _T("KDJ钝化"); }
			if (wrBottomPassive) { forbidSell = true; if (forbidSellReason.IsEmpty()) forbidSellReason = _T("WR钝化"); }
			if (narrowBand) { forbidBuy = true; forbidBuyReason = _T("BOLL过窄"); }
			// 布林扩张：中轨上行禁止卖出，中轨下行禁止买入
			if (bandExpand && bollMidUp) { forbidSell = true; if (forbidSellReason.IsEmpty()) forbidSellReason = _T("BOLL扩张"); }
			if (bandExpand && !bollMidUp) { forbidBuy = true; if (forbidBuyReason.IsEmpty()) forbidBuyReason = _T("BOLL扩张"); }
		}
		else if (curTrendState == STOCK::TrendState30m::STATE_WEAK_SHAKE)
		{
			if (kdjBottomPassive) { forbidBuy = true; forbidBuyReason = _T("KDJ钝化"); }
			if (wrBottomPassive) { forbidBuy = true; if (forbidBuyReason.IsEmpty()) forbidBuyReason = _T("WR钝化"); }
			if (narrowBand) { forbidBuy = true; if (forbidBuyReason.IsEmpty()) forbidBuyReason = _T("BOLL过窄"); }
			// 布林扩张：中轨上行禁止卖出，中轨下行禁止买入
			if (bandExpand && bollMidUp) { forbidSell = true; if (forbidSellReason.IsEmpty()) forbidSellReason = _T("BOLL扩张"); }
			if (bandExpand && !bollMidUp) { forbidBuy = true; if (forbidBuyReason.IsEmpty()) forbidBuyReason = _T("BOLL扩张"); }
		}
		else  // STATE_SHAKE
		{
			if (kdjBottomPassive) { forbidBuy = true; forbidBuyReason = _T("KDJ钝化"); }
			if (wrBottomPassive) { forbidBuy = true; if (forbidBuyReason.IsEmpty()) forbidBuyReason = _T("WR钝化"); }
			if (narrowBand) { forbidBuy = true; if (forbidBuyReason.IsEmpty()) forbidBuyReason = _T("BOLL过窄"); }
			// 布林扩张：中轨上行禁止卖出，中轨下行禁止买入
			if (bandExpand && bollMidUp) { forbidSell = true; if (forbidSellReason.IsEmpty()) forbidSellReason = _T("BOLL扩张"); }
			if (bandExpand && !bollMidUp) { forbidBuy = true; if (forbidBuyReason.IsEmpty()) forbidBuyReason = _T("BOLL扩张"); }
		}

		if (forbidBuy && static_cast<int>(i) - lastForbidBarIndex >= MIN_SIGNAL_BAR_GAP)
		{
			result.push_back({ static_cast<int>(i), true, true, curTrendState, _T("风控限制买入：") + forbidBuyReason });
			lastForbidBarIndex = static_cast<int>(i);
		}
		if (forbidSell && static_cast<int>(i) - lastForbidBarIndex >= MIN_SIGNAL_BAR_GAP)
		{
			result.push_back({ static_cast<int>(i), false, true, curTrendState, _T("风控限制卖出：") + forbidSellReason });
			lastForbidBarIndex = static_cast<int>(i);
		}

		// ===== 信号判定 =====
		bool strongTrendUp = false;
		if (curTrendState == STOCK::TrendState30m::STATE_STRONG)
		{
			bool difAboveZero = (difSeq[i] > 0);
			bool bollMidUp = (bollMid[i] > 0 && prevBollMidSeq[i] > 0 && bollMid[i] > prevBollMidSeq[i]);
			strongTrendUp = difAboveZero && bollMidUp;
		}

		bool kdjTopPassiveExempt = false;
		if (curTrendState == STOCK::TrendState30m::STATE_STRONG && i >= 2)
		{
			bool kdjHigh3 = (kSeq[i] > 85 && kSeq[i - 1] > 85 && kSeq[i - 2] > 85);
			bool kdjDeclining3 = (kSeq[i] < kSeq[i - 1] && kSeq[i - 1] < kSeq[i - 2]);
			kdjTopPassiveExempt = kdjHigh3 && kdjDeclining3;
		}

		DivergenceInfo divInfo = CalcDivergence(bars1m, difSeq, i);

		// --- 卖出判定 ---
		bool sellS1 = (bars1m[i].high >= bollUp[i]) || (bars1m[i].close > bollUp[i]);
		bool sellS2_redShrink = (macdBarSeq[i] > 0) && (macdBarSeq[i] < macdBarSeq[i - 1]);
		bool sellS2_topDiv = divInfo.hasTopDiv && (divInfo.topDivLevel >= DivLevel::STANDARD);
		bool sellS2 = sellS2_redShrink || sellS2_topDiv;
		bool sellNecessary = (sellS1 || sellS2);

		bool sellA1 = ((kSeq[i] > KDJ_OVERBUY && dSeq[i] > SELL_KDJ_D_OVERBUY && jSeq[i] < jSeq[i - 1]) || jSeq[i] > 100.0);
		bool sellA2 = (rsi6Seq[i] > 70);
		bool sellA3 = (wr6Seq[i] < SELL_WR_OVERBUY);
		double avgVol5_i = avgVol5Seq[i];
		bool sellA4_volSurge = (avgVol5_i > 0 && bars1m[i].volume > avgVol5_i * VOL_RATIO_THRESHOLD && bars1m[i].close > bars1m[i].open);
		int sellAuxCount = (sellA1 ? 1 : 0) + (sellA2 ? 1 : 0) + (sellA3 ? 1 : 0) + (sellA4_volSurge ? 1 : 0);

		bool hasSell = sellNecessary && (sellAuxCount >= sellAuxThreshold);
		if (hasSell && strongTrendUp && sellAuxCount < 2)
			hasSell = false;
		if (hasSell && kdjTopPassiveExempt)
			hasSell = false;

		// --- 买入判定 ---
		bool buyB1 = (bars1m[i].low <= bollDn[i]) || (bars1m[i].close < bollDn[i]);
		double macdDifThreshold = GetMacdDifThreshold(bars1m[i].close, atrSeq[i]);
		bool buyB2_greenShrink = (macdBarSeq[i] < 0) && (macdBarSeq[i] > macdBarSeq[i - 1]) && fabs(difSeq[i]) > macdDifThreshold;
		bool buyB2_bottomDiv = divInfo.hasBottomDiv && (divInfo.bottomDivLevel >= DivLevel::STANDARD);
		bool buyB2 = buyB2_greenShrink || buyB2_bottomDiv;
		bool buyNecessary = (buyB1 || buyB2);

		bool buyA1 = (kSeq[i] < KDJ_OVERSELL && dSeq[i] < KDJ_OVERSELL && jSeq[i] > jSeq[i - 1]);
		bool buyA2 = (rsi6Seq[i] < 30);
		bool buyA3 = (wr6Seq[i] > 80);
		bool buyA4_volShrink = (avgVol5_i > 0 && bars1m[i].volume < avgVol5_i * 0.7);
		int buyAuxCount = (buyA1 ? 1 : 0) + (buyA2 ? 1 : 0) + (buyA3 ? 1 : 0) + (buyA4_volShrink ? 1 : 0);

		bool hasBuy = buyNecessary && (buyAuxCount >= buyAuxThreshold);

		if (!hasSell && !hasBuy) continue;
		if (forbidBuy && hasBuy && !hasSell)
			continue;
		if (forbidSell && hasSell && !hasBuy)
			continue;

		// ========== 共振总分计算 ==========
		int sellStrBoll = (sellS1 ? 1 : 0);
		int sellStrMacd = (sellS2 ? 1 : 0) + (divInfo.hasTopDiv && divInfo.topDivLevel == DivLevel::WEAK ? 1 : 0);
		int sellStrRsi = (sellA2 ? 1 : 0);
		int sellStrKdj = (sellA1 ? 1 : 0);
		int sellStrWr = (sellA3 ? 1 : 0);
		int sellStrVol = (sellA4_volSurge ? 1 : 0);
		int sellTotalStr = sellStrBoll + sellStrMacd + sellStrRsi + sellStrKdj + sellStrWr + sellStrVol;

		int buyStrBoll = (buyB1 ? 1 : 0);
		int buyStrMacd = (buyB2 ? 1 : 0) + (divInfo.hasBottomDiv && divInfo.bottomDivLevel == DivLevel::WEAK ? 1 : 0);
		int buyStrRsi = (buyA2 ? 1 : 0);
		int buyStrKdj = (buyA1 ? 1 : 0);
		int buyStrWr = (buyA3 ? 1 : 0);
		int buyStrVol = (buyA4_volShrink ? 1 : 0);
		int buyTotalStr = buyStrBoll + buyStrMacd + buyStrRsi + buyStrKdj + buyStrWr + buyStrVol;

		bool priceAboveMA20 = (bollMid[i] > 0 && bars1m[i].close > bollMid[i]);
		bool priceBelowMA20 = (bollMid[i] > 0 && bars1m[i].close < bollMid[i]);

		bool isBuy;
		if (hasSell && hasBuy)
		{
			if (sellTotalStr > buyTotalStr)
			{
				if (curTrendState == STOCK::TrendState30m::STATE_STRONG && priceAboveMA20)
					isBuy = true;
				else
					isBuy = false;
			}
			else if (buyTotalStr > sellTotalStr)
			{
				if (curTrendState == STOCK::TrendState30m::STATE_WEAK && priceBelowMA20)
					isBuy = false;
				else
					isBuy = true;
			}
			else
			{
				if (curTrendState == STOCK::TrendState30m::STATE_STRONG)
					isBuy = !priceBelowMA20;
				else if (curTrendState == STOCK::TrendState30m::STATE_WEAK)
					isBuy = priceAboveMA20;
				else if (curTrendState == STOCK::TrendState30m::STATE_WEAK_SHAKE)
					isBuy = !priceBelowMA20;
				else
					isBuy = true;
			}
		}
		else
		{
			isBuy = hasBuy;
		}

		if (forbidBuy && isBuy)
		{
			if (hasSell && !forbidSell)
				isBuy = false;
			else
				continue;
		}
		if (forbidSell && !isBuy)
		{
			if (hasBuy && !forbidBuy)
				isBuy = true;
			else
				continue;
		}

		CString reason;
		if (curTrendState == STOCK::TrendState30m::STATE_WEAK)
			reason = isBuy ? _T("反买") : _T("反卖");
		else if (curTrendState == STOCK::TrendState30m::STATE_STRONG)
			reason = isBuy ? _T("正买") : _T("正卖");
		else if (curTrendState == STOCK::TrendState30m::STATE_WEAK_SHAKE)
			reason = isBuy ? _T("弱震买") : _T("弱震卖");
		else
			reason = isBuy ? _T("正买") : _T("正卖");

		result.push_back({ static_cast<int>(i), isBuy, false, curTrendState, reason });
	}

	return result;
}

// ========== 统一信号分析（走势图绘制和弹窗共用） ==========
CSignalAnalyzer::SignalAnalysisResult CSignalAnalyzer::AnalyzeSignalAt(
	const std::vector<STOCK::Bar>& bars5, const std::vector<STOCK::Bar>& bars30, int barIndex)
{
	SignalAnalysisResult result;

	if (barIndex < 0 || barIndex >= static_cast<int>(bars5.size()))
		return result;

	size_t n = bars5.size();
	size_t idx = static_cast<size_t>(barIndex);

	// ===== 1. 计算指标序列（只算一次，与BatchDetectSignals完全相同） =====
	// 30分钟趋势（跨周期对齐）
	result.trendResult = Get30mTrendState(bars30);
	auto alignMap = Align5mTo30m(bars5, bars30);
	// 预计算30min趋势序列：逐根5min Bar动态获取当期30min趋势
	result.trendStateSeq.resize(n, STOCK::TrendState30m::STATE_SHAKE);
	{
		std::map<size_t, STOCK::TrendState30m> trendCache;
		for (size_t i = 0; i < n; i++)
		{
			size_t idx30 = alignMap[i];
			if (idx30 == SIZE_MAX || idx30 < 1)
			{
				result.trendStateSeq[i] = STOCK::TrendState30m::STATE_SHAKE;
				continue;
			}
			auto it = trendCache.find(idx30);
			if (it != trendCache.end())
			{
				result.trendStateSeq[i] = it->second;
			}
			else
			{
				std::vector<STOCK::Bar> bars30sub(bars30.begin(), bars30.begin() + idx30 + 1);
				STOCK::TrendStateResult tr = Get30mTrendState(bars30sub);
				trendCache[idx30] = tr.state;
				result.trendStateSeq[i] = tr.state;
			}
		}
	}
	STOCK::TrendState30m trendState = result.trendStateSeq[idx];
	// 更新trendResult为barIndex处的趋势
	if (idx < alignMap.size() && alignMap[idx] != SIZE_MAX && alignMap[idx] < bars30.size())
	{
		std::vector<STOCK::Bar> bars30sub(bars30.begin(), bars30.begin() + alignMap[idx] + 1);
		result.trendResult = Get30mTrendState(bars30sub);
	}

	// BOLL序列
	int bollP = GetParam().bollPeriod;
	result.bollUpSeq.resize(n, 0); result.bollMidSeq.resize(n, 0); result.bollDnSeq.resize(n, 0); result.bollBandSeq.resize(n, 0);
	for (size_t i = bollP - 1; i < n; i++)
	{
		std::vector<double> closes;
		for (size_t j = i - bollP + 1; j <= i; j++) closes.push_back(bars5[j].close);
		double mid = CalcMA(closes, bollP);
		double sd = CalcStdDev(closes);
		result.bollMidSeq[i] = mid;
		result.bollUpSeq[i] = mid + 2.0 * sd;
		result.bollDnSeq[i] = mid - 2.0 * sd;
		result.bollBandSeq[i] = 4.0 * sd;
	}

	// MACD序列
	CalcMACDSeries(bars5, result.difSeq, result.deaSeq, result.macdBarSeq);

	// KDJ序列
	CalcKDJSeries(bars5, GetParam().kdjPeriod, result.kSeq, result.dSeq, result.jSeq);

	// RSI序列（与BatchDetectSignals相同的EMA/SMA递推方式）
	int rsiP = GetParam().rsiPeriod;
	result.rsi6Seq.resize(n, 50);
	if (n >= static_cast<size_t>(rsiP + 1))
	{
		std::vector<double> upList, dnList;
		for (size_t i = 1; i < n; i++) { double d = bars5[i].close - bars5[i - 1].close; upList.push_back(max(d, 0.0)); dnList.push_back(max(-d, 0.0)); }
		double au = 0, ad = 0;
		if (GetParam().rsiUseEma)
		{
			double k = 2.0 / (rsiP + 1.0);
			au = upList[0]; ad = dnList[0];
			for (size_t i = 1; i < upList.size(); i++)
			{
				au = upList[i] * k + au * (1.0 - k);
				ad = dnList[i] * k + ad * (1.0 - k);
				if (ad == 0) result.rsi6Seq[i + 1] = 99.99; else result.rsi6Seq[i + 1] = 100.0 - 100.0 / (1.0 + au / ad);
			}
		}
		else
		{
			for (int i = 0; i < rsiP && i < static_cast<int>(upList.size()); i++) { au += upList[i]; ad += dnList[i]; }
			au /= rsiP; ad /= rsiP;
			if (ad == 0) result.rsi6Seq[rsiP] = 99.99; else result.rsi6Seq[rsiP] = 100.0 - 100.0 / (1.0 + au / ad);
			for (size_t i = rsiP; i < upList.size(); i++)
			{
				au = (upList[i] + (rsiP - 1.0) * au) / rsiP;
				ad = (dnList[i] + (rsiP - 1.0) * ad) / rsiP;
				if (ad == 0) result.rsi6Seq[i + 1] = 99.99; else result.rsi6Seq[i + 1] = 100.0 - 100.0 / (1.0 + au / ad);
			}
		}
	}

	// WR序列（与BatchDetectSignals相同的滑动窗口方式）
	int wrP = GetParam().wrPeriod;
	result.wr6Seq.resize(n, 50);
	for (size_t i = wrP - 1; i < n; i++)
	{
		double hhv = bars5[i - wrP + 1].high, llv = bars5[i - wrP + 1].low;
		for (size_t j = i - wrP + 2; j <= i; j++) { if (bars5[j].high > hhv) hhv = bars5[j].high; if (bars5[j].low < llv) llv = bars5[j].low; }
		result.wr6Seq[i] = (hhv != llv) ? 100.0 * (hhv - bars5[i].close) / (hhv - llv) : 50.0;
	}

	// ATR序列
	int atrPeriod = GetParam().atrPeriod;
	result.atrSeq.resize(n, 0);
	for (size_t i = atrPeriod; i < n; i++)
	{
		std::vector<STOCK::Bar> barsForATR(bars5.begin() + i - atrPeriod, bars5.begin() + i + 1);
		result.atrSeq[i] = CalcATR(barsForATR, barsForATR.size() - 1);
	}

	// 均量序列
	std::vector<double> avgVol5Seq(n, 0);
	for (size_t i = 5; i < n; i++)
	{
		double volSum = 0;
		for (size_t j = i - 5; j < i; j++) volSum += bars5[j].volume;
		avgVol5Seq[i] = volSum / 5.0;
	}

	// ===== 2. 从序列中提取barIndex处的指标值 =====
	result.boll.up = result.bollUpSeq[idx];
	result.boll.mid = result.bollMidSeq[idx];
	result.boll.dn = result.bollDnSeq[idx];
	result.boll.bandwidth = result.bollBandSeq[idx];
	result.macd.dif = result.difSeq[idx];
	result.macd.dea = result.deaSeq[idx];
	result.macd.macd_bar = result.macdBarSeq[idx];
	result.kdj.k = result.kSeq[idx];
	result.kdj.d = result.dSeq[idx];
	result.kdj.j = result.jSeq[idx];
	result.rsi = result.rsi6Seq[idx];
	result.wr = result.wr6Seq[idx];
	result.atr = result.atrSeq[idx];
	if (idx >= 1)
	{
		result.prevMacd.dif = result.difSeq[idx - 1];
		result.prevMacd.dea = result.deaSeq[idx - 1];
		result.prevMacd.macd_bar = result.macdBarSeq[idx - 1];
		result.prevKdj.k = result.kSeq[idx - 1];
		result.prevKdj.d = result.dSeq[idx - 1];
		result.prevKdj.j = result.jSeq[idx - 1];
	}

	// ===== 3. 条件诊断（使用序列值，与BatchDetectSignals逐根判定完全相同） =====
	// 卖出条件
	result.sellS1 = (bars5[idx].high >= result.bollUpSeq[idx]) || (bars5[idx].close > result.bollUpSeq[idx]);
	result.sellS2_redShrink = (result.macdBarSeq[idx] > 0) && (idx >= 1 && result.macdBarSeq[idx] < result.macdBarSeq[idx - 1]);
	result.sellNecessary = result.sellS1 || result.sellS2_redShrink;
	result.sellA1 = ((result.kSeq[idx] > KDJ_OVERBUY && result.dSeq[idx] > SELL_KDJ_D_OVERBUY && idx >= 1 && result.jSeq[idx] < result.jSeq[idx - 1]) || result.jSeq[idx] > 100.0);
	result.sellA2 = (result.rsi6Seq[idx] > 70);
	result.sellA3 = (result.wr6Seq[idx] < SELL_WR_OVERBUY);
	result.sellA4 = (avgVol5Seq[idx] > 0 && bars5[idx].volume > avgVol5Seq[idx] * VOL_RATIO_THRESHOLD && bars5[idx].close > bars5[idx].open);
	result.sellAuxCount = (result.sellA1 ? 1 : 0) + (result.sellA2 ? 1 : 0) + (result.sellA3 ? 1 : 0) + (result.sellA4 ? 1 : 0);

	// 买入条件
	result.buyB1 = (bars5[idx].low <= result.bollDnSeq[idx]) || (bars5[idx].close < result.bollDnSeq[idx]);
	double macdDifThreshold = GetMacdDifThreshold(bars5[idx].close, result.atrSeq[idx]);
	result.buyB2_greenShrink = (result.macdBarSeq[idx] < 0) && (idx >= 1 && result.macdBarSeq[idx] > result.macdBarSeq[idx - 1]) && fabs(result.difSeq[idx]) > macdDifThreshold;
	result.buyNecessary = result.buyB1 || result.buyB2_greenShrink;
	result.buyA1 = (result.kSeq[idx] < KDJ_OVERSELL && result.dSeq[idx] < KDJ_OVERSELL && idx >= 1 && result.jSeq[idx] > result.jSeq[idx - 1]);
	result.buyA2 = (result.rsi6Seq[idx] < 30);
	result.buyA3 = (result.wr6Seq[idx] > 80);
	result.buyA4 = (avgVol5Seq[idx] > 0 && bars5[idx].volume < avgVol5Seq[idx] * 0.7);
	result.buyAuxCount = (result.buyA1 ? 1 : 0) + (result.buyA2 ? 1 : 0) + (result.buyA3 ? 1 : 0) + (result.buyA4 ? 1 : 0);

	// ===== 4. 批量信号检测（使用相同的指标序列） =====
	result.batchSignals = BatchDetectSignals(bars5, bars30);
	result.clickedSignal = nullptr;
	for (const auto& item : result.batchSignals)
	{
		if (item.barIndex == barIndex)
		{
			result.clickedSignal = &item;
			break;
		}
	}

	// ===== 5. 批量判定链诊断（与BatchDetectSignals逐根判定完全相同） =====
	int sellAuxThreshold = 2;
	if (trendState == STOCK::TrendState30m::STATE_STRONG) sellAuxThreshold = 2;
	int buyAuxThreshold = 2;
	if (trendState == STOCK::TrendState30m::STATE_STRONG) buyAuxThreshold = 1;

	result.batchHasSell = result.sellNecessary && (result.sellAuxCount >= sellAuxThreshold);
	result.batchHasBuy = result.buyNecessary && (result.buyAuxCount >= buyAuxThreshold);

	// 趋势强度权重
	result.batchStrongTrendUp = false;
	if (trendState == STOCK::TrendState30m::STATE_STRONG && idx >= 1)
	{
		bool difAboveZero = (result.difSeq[idx] > 0);
		bool bollMidUp = (result.bollMidSeq[idx] > 0 && idx >= 1 && result.bollMidSeq[idx] > result.bollMidSeq[idx - 1]);
		result.batchStrongTrendUp = difAboveZero && bollMidUp;
	}
	if (result.batchHasSell && result.batchStrongTrendUp && result.sellAuxCount < 2)
		result.batchHasSell = false;

	// KDJ钝化豁免
	result.batchKdjTopPassiveExempt = false;
	if (trendState == STOCK::TrendState30m::STATE_STRONG && idx >= 2)
	{
		bool kdjHigh3 = (result.kSeq[idx] > 85 && result.kSeq[idx - 1] > 85 && result.kSeq[idx - 2] > 85);
		bool kdjDeclining3 = (result.kSeq[idx] < result.kSeq[idx - 1] && result.kSeq[idx - 1] < result.kSeq[idx - 2]);
		result.batchKdjTopPassiveExempt = kdjHigh3 && kdjDeclining3;
	}
	if (result.batchHasSell && result.batchKdjTopPassiveExempt)
		result.batchHasSell = false;

	// ===== 6. 风控判定（与BatchDetectSignals完全相同，修复SHAKE缺少bandExpand的Bug） =====
	bool narrowBand = IsNarrowBoll(CalcAverageBollBandwidth(result.bollBandSeq, idx), result.atrSeq[idx]);
	bool bandExpand = IsBollExpand(result.bollBandSeq, idx);
	bool bollMidUp = (idx >= 1 && result.bollMidSeq[idx] > result.bollMidSeq[idx - 1]);
	bool kdjTopPassive = (idx >= 2 && result.kSeq[idx] > 85 && result.kSeq[idx - 1] > 85 && result.kSeq[idx - 2] > 85
		&& result.kSeq[idx] < result.kSeq[idx - 1] && result.kSeq[idx - 1] < result.kSeq[idx - 2]);
	bool kdjBottomPassive = (idx >= 2 && result.kSeq[idx] < 15 && result.kSeq[idx - 1] < 15 && result.kSeq[idx - 2] < 15
		&& result.kSeq[idx] > result.kSeq[idx - 1] && result.kSeq[idx - 1] > result.kSeq[idx - 2]);
	bool wrTopPassive = (idx >= 2 && result.wr6Seq[idx] < 18 && result.wr6Seq[idx - 1] < 18 && result.wr6Seq[idx - 2] < 18
		&& result.wr6Seq[idx] > result.wr6Seq[idx - 1] && result.wr6Seq[idx - 1] > result.wr6Seq[idx - 2]);
	bool wrBottomPassive = (idx >= 2 && result.wr6Seq[idx] > 82 && result.wr6Seq[idx - 1] > 82 && result.wr6Seq[idx - 2] > 82
		&& result.wr6Seq[idx] < result.wr6Seq[idx - 1] && result.wr6Seq[idx - 1] < result.wr6Seq[idx - 2]);

	result.batchForbidBuy = false;
	result.batchForbidSell = false;
	if (trendState == STOCK::TrendState30m::STATE_STRONG)
	{
		if (kdjBottomPassive) { result.batchForbidBuy = true; result.batchForbidBuyReason = _T("KDJ钝化"); }
		if (wrBottomPassive) { result.batchForbidBuy = true; if (result.batchForbidBuyReason.IsEmpty()) result.batchForbidBuyReason = _T("WR钝化"); }
		if (kdjTopPassive) { result.batchForbidSell = true; result.batchForbidSellReason = _T("KDJ钝化"); }
		if (wrTopPassive) { result.batchForbidSell = true; if (result.batchForbidSellReason.IsEmpty()) result.batchForbidSellReason = _T("WR钝化"); }
		if (narrowBand) { result.batchForbidBuy = true; if (result.batchForbidBuyReason.IsEmpty()) result.batchForbidBuyReason = _T("BOLL过窄"); }
		// 布林扩张：中轨上行禁止卖出，中轨下行禁止买入
		if (bandExpand && bollMidUp) { result.batchForbidSell = true; if (result.batchForbidSellReason.IsEmpty()) result.batchForbidSellReason = _T("BOLL扩张"); }
		if (bandExpand && !bollMidUp) { result.batchForbidBuy = true; if (result.batchForbidBuyReason.IsEmpty()) result.batchForbidBuyReason = _T("BOLL扩张"); }
	}
	else if (trendState == STOCK::TrendState30m::STATE_WEAK)
	{
		if (kdjBottomPassive) { result.batchForbidSell = true; result.batchForbidSellReason = _T("KDJ钝化"); }
		if (wrBottomPassive) { result.batchForbidSell = true; if (result.batchForbidSellReason.IsEmpty()) result.batchForbidSellReason = _T("WR钝化"); }
		if (narrowBand) { result.batchForbidBuy = true; if (result.batchForbidBuyReason.IsEmpty()) result.batchForbidBuyReason = _T("BOLL过窄"); }
		// 布林扩张：中轨上行禁止卖出，中轨下行禁止买入
		if (bandExpand && bollMidUp) { result.batchForbidSell = true; if (result.batchForbidSellReason.IsEmpty()) result.batchForbidSellReason = _T("BOLL扩张"); }
		if (bandExpand && !bollMidUp) { result.batchForbidBuy = true; if (result.batchForbidBuyReason.IsEmpty()) result.batchForbidBuyReason = _T("BOLL扩张"); }
	}
	else if (trendState == STOCK::TrendState30m::STATE_WEAK_SHAKE)
	{
		if (kdjBottomPassive) { result.batchForbidBuy = true; result.batchForbidBuyReason = _T("KDJ钝化"); }
		if (wrBottomPassive) { result.batchForbidBuy = true; if (result.batchForbidBuyReason.IsEmpty()) result.batchForbidBuyReason = _T("WR钝化"); }
		if (narrowBand) { result.batchForbidBuy = true; if (result.batchForbidBuyReason.IsEmpty()) result.batchForbidBuyReason = _T("BOLL过窄"); }
		// 布林扩张：中轨上行禁止卖出，中轨下行禁止买入
		if (bandExpand && bollMidUp) { result.batchForbidSell = true; if (result.batchForbidSellReason.IsEmpty()) result.batchForbidSellReason = _T("BOLL扩张"); }
		if (bandExpand && !bollMidUp) { result.batchForbidBuy = true; if (result.batchForbidBuyReason.IsEmpty()) result.batchForbidBuyReason = _T("BOLL扩张"); }
	}
	else  // STATE_SHAKE - 修复：添加bandExpand风控
	{
		if (kdjBottomPassive) { result.batchForbidBuy = true; result.batchForbidBuyReason = _T("KDJ钝化"); }
		if (wrBottomPassive) { result.batchForbidBuy = true; if (result.batchForbidBuyReason.IsEmpty()) result.batchForbidBuyReason = _T("WR钝化"); }
		if (narrowBand) { result.batchForbidBuy = true; if (result.batchForbidBuyReason.IsEmpty()) result.batchForbidBuyReason = _T("BOLL过窄"); }
		// 布林扩张：中轨上行禁止卖出，中轨下行禁止买入
		if (bandExpand && bollMidUp) { result.batchForbidSell = true; if (result.batchForbidSellReason.IsEmpty()) result.batchForbidSellReason = _T("BOLL扩张"); }
		if (bandExpand && !bollMidUp) { result.batchForbidBuy = true; if (result.batchForbidBuyReason.IsEmpty()) result.batchForbidBuyReason = _T("BOLL扩张"); }
	}

	// 风控拦截
	result.batchSellFilteredByForbid = false;
	result.batchBuyFilteredByForbid = false;
	if (!result.batchHasSell && !result.batchHasBuy)
		result.batchFilterReason = _T("无买卖条件");
	else if (result.batchForbidBuy && result.batchHasBuy && !result.batchHasSell)
	{ result.batchBuyFilteredByForbid = true; result.batchFilterReason = _T("买入被风控拦截(") + result.batchForbidBuyReason + _T(")"); }
	else if (result.batchForbidSell && result.batchHasSell && !result.batchHasBuy)
	{ result.batchSellFilteredByForbid = true; result.batchFilterReason = _T("卖出被风控拦截(") + result.batchForbidSellReason + _T(")"); }
	else
		result.batchFilterReason = _T("通过");

	// 5分钟信号（从批量判定结果推断，不再独立计算）
	if (result.batchHasSell && !result.batchSellFilteredByForbid)
		result.sig5m = STOCK::Signal5m::SIG_SELL;
	else if (result.batchHasBuy && !result.batchBuyFilteredByForbid)
		result.sig5m = STOCK::Signal5m::SIG_BUY;
	else
		result.sig5m = STOCK::Signal5m::SIG_NONE;

	return result;
}

// ========== 分时图统一信号分析（1分钟粒度版AnalyzeSignalAt） ==========
CSignalAnalyzer::SignalAnalysisResult CSignalAnalyzer::AnalyzeSignalAtFromTimeline(
	const std::vector<STOCK::Bar>& bars1m, const std::vector<STOCK::Bar>& bars30, int barIndex)
{
	SignalAnalysisResult result;

	if (barIndex < 0 || barIndex >= static_cast<int>(bars1m.size()))
		return result;

	size_t n = bars1m.size();
	size_t idx = static_cast<size_t>(barIndex);

	// ===== 1. 计算指标序列（只算一次，与BatchDetectSignalsFromTimeline完全相同） =====
	// 30分钟趋势（跨周期对齐，1分钟粒度按比例映射）
	result.trendResult = Get30mTrendState(bars30);
	// 预计算30min趋势序列：逐根1min Bar动态获取当期30min趋势
	// 与BatchDetectSignalsFromTimeline相同的对齐方式
	std::vector<size_t> alignMap(n, SIZE_MAX);
	{
		size_t n30 = bars30.size();
		if (n30 >= 2)
		{
			for (size_t i = 0; i < n; i++)
			{
				size_t idx30 = static_cast<size_t>(static_cast<double>(i) / n * n30);
				if (idx30 >= n30) idx30 = n30 - 1;
				if (idx30 < 1) idx30 = 1;
				alignMap[i] = idx30;
			}
		}
	}
	result.trendStateSeq.resize(n, STOCK::TrendState30m::STATE_SHAKE);
	{
		std::map<size_t, STOCK::TrendState30m> trendCache;
		for (size_t i = 0; i < n; i++)
		{
			size_t idx30 = alignMap[i];
			if (idx30 == SIZE_MAX || idx30 < 1)
			{
				result.trendStateSeq[i] = STOCK::TrendState30m::STATE_SHAKE;
				continue;
			}
			auto it = trendCache.find(idx30);
			if (it != trendCache.end())
			{
				result.trendStateSeq[i] = it->second;
			}
			else
			{
				std::vector<STOCK::Bar> bars30sub(bars30.begin(), bars30.begin() + idx30 + 1);
				STOCK::TrendStateResult tr = Get30mTrendState(bars30sub);
				trendCache[idx30] = tr.state;
				result.trendStateSeq[i] = tr.state;
			}
		}
	}
	STOCK::TrendState30m trendState = result.trendStateSeq[idx];
	// 更新trendResult为barIndex处的趋势
	if (alignMap[idx] != SIZE_MAX && alignMap[idx] < bars30.size())
	{
		std::vector<STOCK::Bar> bars30sub(bars30.begin(), bars30.begin() + alignMap[idx] + 1);
		result.trendResult = Get30mTrendState(bars30sub);
	}

	// BOLL序列
	int bollP = GetParam().bollPeriod;
	result.bollUpSeq.resize(n, 0); result.bollMidSeq.resize(n, 0); result.bollDnSeq.resize(n, 0); result.bollBandSeq.resize(n, 0);
	for (size_t i = bollP - 1; i < n; i++)
	{
		std::vector<double> closes;
		for (size_t j = i - bollP + 1; j <= i; j++) closes.push_back(bars1m[j].close);
		double mid = CalcMA(closes, bollP);
		double sd = CalcStdDev(closes);
		result.bollMidSeq[i] = mid;
		result.bollUpSeq[i] = mid + 2.0 * sd;
		result.bollDnSeq[i] = mid - 2.0 * sd;
		result.bollBandSeq[i] = 4.0 * sd;
	}

	// MACD序列
	CalcMACDSeries(bars1m, result.difSeq, result.deaSeq, result.macdBarSeq);

	// KDJ序列
	CalcKDJSeries(bars1m, GetParam().kdjPeriod, result.kSeq, result.dSeq, result.jSeq);

	// RSI序列（与BatchDetectSignalsFromTimeline相同的EMA/SMA递推方式）
	int rsiP = GetParam().rsiPeriod;
	result.rsi6Seq.resize(n, 50);
	if (n >= static_cast<size_t>(rsiP + 1))
	{
		std::vector<double> upList, dnList;
		for (size_t i = 1; i < n; i++) { double d = bars1m[i].close - bars1m[i - 1].close; upList.push_back(max(d, 0.0)); dnList.push_back(max(-d, 0.0)); }
		double au = 0, ad = 0;
		if (GetParam().rsiUseEma)
		{
			double k = 2.0 / (rsiP + 1.0);
			au = upList[0]; ad = dnList[0];
			for (size_t i = 1; i < upList.size(); i++)
			{
				au = upList[i] * k + au * (1.0 - k);
				ad = dnList[i] * k + ad * (1.0 - k);
				if (ad == 0) result.rsi6Seq[i + 1] = 99.99; else result.rsi6Seq[i + 1] = 100.0 - 100.0 / (1.0 + au / ad);
			}
		}
		else
		{
			for (int i = 0; i < rsiP && i < static_cast<int>(upList.size()); i++) { au += upList[i]; ad += dnList[i]; }
			au /= rsiP; ad /= rsiP;
			if (ad == 0) result.rsi6Seq[rsiP] = 99.99; else result.rsi6Seq[rsiP] = 100.0 - 100.0 / (1.0 + au / ad);
			for (size_t i = rsiP; i < upList.size(); i++)
			{
				au = (upList[i] + (rsiP - 1.0) * au) / rsiP;
				ad = (dnList[i] + (rsiP - 1.0) * ad) / rsiP;
				if (ad == 0) result.rsi6Seq[i + 1] = 99.99; else result.rsi6Seq[i + 1] = 100.0 - 100.0 / (1.0 + au / ad);
			}
		}
	}

	// WR序列（与BatchDetectSignalsFromTimeline相同的滑动窗口方式）
	int wrP = GetParam().wrPeriod;
	result.wr6Seq.resize(n, 50);
	for (size_t i = wrP - 1; i < n; i++)
	{
		double hhv = bars1m[i - wrP + 1].high, llv = bars1m[i - wrP + 1].low;
		for (size_t j = i - wrP + 2; j <= i; j++) { if (bars1m[j].high > hhv) hhv = bars1m[j].high; if (bars1m[j].low < llv) llv = bars1m[j].low; }
		result.wr6Seq[i] = (hhv != llv) ? 100.0 * (hhv - bars1m[i].close) / (hhv - llv) : 50.0;
	}

	// ATR序列
	int atrPeriod = GetParam().atrPeriod;
	result.atrSeq.resize(n, 0);
	for (size_t i = atrPeriod; i < n; i++)
	{
		std::vector<STOCK::Bar> barsForATR(bars1m.begin() + i - atrPeriod, bars1m.begin() + i + 1);
		result.atrSeq[i] = CalcATR(barsForATR, barsForATR.size() - 1);
	}

	// 均量序列
	std::vector<double> avgVol5Seq(n, 0);
	for (size_t i = 5; i < n; i++)
	{
		double volSum = 0;
		for (size_t j = i - 5; j < i; j++) volSum += bars1m[j].volume;
		avgVol5Seq[i] = volSum / 5.0;
	}

	// ===== 2. 从序列中提取barIndex处的指标值 =====
	result.boll.up = result.bollUpSeq[idx];
	result.boll.mid = result.bollMidSeq[idx];
	result.boll.dn = result.bollDnSeq[idx];
	result.boll.bandwidth = result.bollBandSeq[idx];
	result.macd.dif = result.difSeq[idx];
	result.macd.dea = result.deaSeq[idx];
	result.macd.macd_bar = result.macdBarSeq[idx];
	result.kdj.k = result.kSeq[idx];
	result.kdj.d = result.dSeq[idx];
	result.kdj.j = result.jSeq[idx];
	result.rsi = result.rsi6Seq[idx];
	result.wr = result.wr6Seq[idx];
	result.atr = result.atrSeq[idx];
	if (idx >= 1)
	{
		result.prevMacd.dif = result.difSeq[idx - 1];
		result.prevMacd.dea = result.deaSeq[idx - 1];
		result.prevMacd.macd_bar = result.macdBarSeq[idx - 1];
		result.prevKdj.k = result.kSeq[idx - 1];
		result.prevKdj.d = result.dSeq[idx - 1];
		result.prevKdj.j = result.jSeq[idx - 1];
	}

	// ===== 3. 条件诊断（使用序列值，与BatchDetectSignalsFromTimeline逐根判定完全相同） =====
	// 卖出条件
	result.sellS1 = (bars1m[idx].high >= result.bollUpSeq[idx]) || (bars1m[idx].close > result.bollUpSeq[idx]);
	result.sellS2_redShrink = (result.macdBarSeq[idx] > 0) && (idx >= 1 && result.macdBarSeq[idx] < result.macdBarSeq[idx - 1]);
	result.sellNecessary = result.sellS1 || result.sellS2_redShrink;
	result.sellA1 = ((result.kSeq[idx] > KDJ_OVERBUY && result.dSeq[idx] > SELL_KDJ_D_OVERBUY && idx >= 1 && result.jSeq[idx] < result.jSeq[idx - 1]) || result.jSeq[idx] > 100.0);
	result.sellA2 = (result.rsi6Seq[idx] > 70);
	result.sellA3 = (result.wr6Seq[idx] < SELL_WR_OVERBUY);
	result.sellA4 = (avgVol5Seq[idx] > 0 && bars1m[idx].volume > avgVol5Seq[idx] * VOL_RATIO_THRESHOLD && bars1m[idx].close > bars1m[idx].open);
	result.sellAuxCount = (result.sellA1 ? 1 : 0) + (result.sellA2 ? 1 : 0) + (result.sellA3 ? 1 : 0) + (result.sellA4 ? 1 : 0);

	// 买入条件
	result.buyB1 = (bars1m[idx].low <= result.bollDnSeq[idx]) || (bars1m[idx].close < result.bollDnSeq[idx]);
	double macdDifThreshold = GetMacdDifThreshold(bars1m[idx].close, result.atrSeq[idx]);
	result.buyB2_greenShrink = (result.macdBarSeq[idx] < 0) && (idx >= 1 && result.macdBarSeq[idx] > result.macdBarSeq[idx - 1]) && fabs(result.difSeq[idx]) > macdDifThreshold;
	result.buyNecessary = result.buyB1 || result.buyB2_greenShrink;
	result.buyA1 = (result.kSeq[idx] < KDJ_OVERSELL && result.dSeq[idx] < KDJ_OVERSELL && idx >= 1 && result.jSeq[idx] > result.jSeq[idx - 1]);
	result.buyA2 = (result.rsi6Seq[idx] < 30);
	result.buyA3 = (result.wr6Seq[idx] > 80);
	result.buyA4 = (avgVol5Seq[idx] > 0 && bars1m[idx].volume < avgVol5Seq[idx] * 0.7);
	result.buyAuxCount = (result.buyA1 ? 1 : 0) + (result.buyA2 ? 1 : 0) + (result.buyA3 ? 1 : 0) + (result.buyA4 ? 1 : 0);

	// ===== 4. 批量信号检测（使用相同的指标序列） =====
	result.batchSignals = BatchDetectSignalsFromTimeline(bars1m, bars30);
	result.clickedSignal = nullptr;
	for (const auto& item : result.batchSignals)
	{
		if (item.barIndex == barIndex)
		{
			result.clickedSignal = &item;
			break;
		}
	}

	// ===== 5. 批量判定链诊断（与BatchDetectSignalsFromTimeline逐根判定完全相同） =====
	int sellAuxThreshold = 2;
	if (trendState == STOCK::TrendState30m::STATE_STRONG) sellAuxThreshold = 2;
	int buyAuxThreshold = 2;
	if (trendState == STOCK::TrendState30m::STATE_STRONG) buyAuxThreshold = 1;

	result.batchHasSell = result.sellNecessary && (result.sellAuxCount >= sellAuxThreshold);
	result.batchHasBuy = result.buyNecessary && (result.buyAuxCount >= buyAuxThreshold);

	// 趋势强度权重
	result.batchStrongTrendUp = false;
	if (trendState == STOCK::TrendState30m::STATE_STRONG && idx >= 1)
	{
		bool difAboveZero = (result.difSeq[idx] > 0);
		bool bollMidUp = (result.bollMidSeq[idx] > 0 && idx >= 1 && result.bollMidSeq[idx] > result.bollMidSeq[idx - 1]);
		result.batchStrongTrendUp = difAboveZero && bollMidUp;
	}
	if (result.batchHasSell && result.batchStrongTrendUp && result.sellAuxCount < 2)
		result.batchHasSell = false;

	// KDJ钝化豁免
	result.batchKdjTopPassiveExempt = false;
	if (trendState == STOCK::TrendState30m::STATE_STRONG && idx >= 2)
	{
		bool kdjHigh3 = (result.kSeq[idx] > 85 && result.kSeq[idx - 1] > 85 && result.kSeq[idx - 2] > 85);
		bool kdjDeclining3 = (result.kSeq[idx] < result.kSeq[idx - 1] && result.kSeq[idx - 1] < result.kSeq[idx - 2]);
		result.batchKdjTopPassiveExempt = kdjHigh3 && kdjDeclining3;
	}
	if (result.batchHasSell && result.batchKdjTopPassiveExempt)
		result.batchHasSell = false;

	// ===== 6. 风控判定（与BatchDetectSignalsFromTimeline完全相同） =====
	bool narrowBand = IsNarrowBoll(CalcAverageBollBandwidth(result.bollBandSeq, idx), result.atrSeq[idx]);
	bool bandExpand = IsBollExpand(result.bollBandSeq, idx);
	bool bollMidUp = (idx >= 1 && result.bollMidSeq[idx] > result.bollMidSeq[idx - 1]);
	bool kdjTopPassive = (idx >= 2 && result.kSeq[idx] > 85 && result.kSeq[idx - 1] > 85 && result.kSeq[idx - 2] > 85
		&& result.kSeq[idx] < result.kSeq[idx - 1] && result.kSeq[idx - 1] < result.kSeq[idx - 2]);
	bool kdjBottomPassive = (idx >= 2 && result.kSeq[idx] < 15 && result.kSeq[idx - 1] < 15 && result.kSeq[idx - 2] < 15
		&& result.kSeq[idx] > result.kSeq[idx - 1] && result.kSeq[idx - 1] > result.kSeq[idx - 2]);
	bool wrTopPassive = (idx >= 2 && result.wr6Seq[idx] < 18 && result.wr6Seq[idx - 1] < 18 && result.wr6Seq[idx - 2] < 18
		&& result.wr6Seq[idx] > result.wr6Seq[idx - 1] && result.wr6Seq[idx - 1] > result.wr6Seq[idx - 2]);
	bool wrBottomPassive = (idx >= 2 && result.wr6Seq[idx] > 82 && result.wr6Seq[idx - 1] > 82 && result.wr6Seq[idx - 2] > 82
		&& result.wr6Seq[idx] < result.wr6Seq[idx - 1] && result.wr6Seq[idx - 1] < result.wr6Seq[idx - 2]);

	result.batchForbidBuy = false;
	result.batchForbidSell = false;
	if (trendState == STOCK::TrendState30m::STATE_STRONG)
	{
		if (kdjBottomPassive) { result.batchForbidBuy = true; result.batchForbidBuyReason = _T("KDJ钝化"); }
		if (wrBottomPassive) { result.batchForbidBuy = true; if (result.batchForbidBuyReason.IsEmpty()) result.batchForbidBuyReason = _T("WR钝化"); }
		if (kdjTopPassive) { result.batchForbidSell = true; result.batchForbidSellReason = _T("KDJ钝化"); }
		if (wrTopPassive) { result.batchForbidSell = true; if (result.batchForbidSellReason.IsEmpty()) result.batchForbidSellReason = _T("WR钝化"); }
		if (narrowBand) { result.batchForbidBuy = true; if (result.batchForbidBuyReason.IsEmpty()) result.batchForbidBuyReason = _T("BOLL过窄"); }
		// 布林扩张：中轨上行禁止卖出，中轨下行禁止买入
		if (bandExpand && bollMidUp) { result.batchForbidSell = true; if (result.batchForbidSellReason.IsEmpty()) result.batchForbidSellReason = _T("BOLL扩张"); }
		if (bandExpand && !bollMidUp) { result.batchForbidBuy = true; if (result.batchForbidBuyReason.IsEmpty()) result.batchForbidBuyReason = _T("BOLL扩张"); }
	}
	else if (trendState == STOCK::TrendState30m::STATE_WEAK)
	{
		if (kdjBottomPassive) { result.batchForbidSell = true; result.batchForbidSellReason = _T("KDJ钝化"); }
		if (wrBottomPassive) { result.batchForbidSell = true; if (result.batchForbidSellReason.IsEmpty()) result.batchForbidSellReason = _T("WR钝化"); }
		if (narrowBand) { result.batchForbidBuy = true; if (result.batchForbidBuyReason.IsEmpty()) result.batchForbidBuyReason = _T("BOLL过窄"); }
		// 布林扩张：中轨上行禁止卖出，中轨下行禁止买入
		if (bandExpand && bollMidUp) { result.batchForbidSell = true; if (result.batchForbidSellReason.IsEmpty()) result.batchForbidSellReason = _T("BOLL扩张"); }
		if (bandExpand && !bollMidUp) { result.batchForbidBuy = true; if (result.batchForbidBuyReason.IsEmpty()) result.batchForbidBuyReason = _T("BOLL扩张"); }
	}
	else if (trendState == STOCK::TrendState30m::STATE_WEAK_SHAKE)
	{
		if (kdjBottomPassive) { result.batchForbidBuy = true; result.batchForbidBuyReason = _T("KDJ钝化"); }
		if (wrBottomPassive) { result.batchForbidBuy = true; if (result.batchForbidBuyReason.IsEmpty()) result.batchForbidBuyReason = _T("WR钝化"); }
		if (narrowBand) { result.batchForbidBuy = true; if (result.batchForbidBuyReason.IsEmpty()) result.batchForbidBuyReason = _T("BOLL过窄"); }
		// 布林扩张：中轨上行禁止卖出，中轨下行禁止买入
		if (bandExpand && bollMidUp) { result.batchForbidSell = true; if (result.batchForbidSellReason.IsEmpty()) result.batchForbidSellReason = _T("BOLL扩张"); }
		if (bandExpand && !bollMidUp) { result.batchForbidBuy = true; if (result.batchForbidBuyReason.IsEmpty()) result.batchForbidBuyReason = _T("BOLL扩张"); }
	}
	else  // STATE_SHAKE
	{
		if (kdjBottomPassive) { result.batchForbidBuy = true; result.batchForbidBuyReason = _T("KDJ钝化"); }
		if (wrBottomPassive) { result.batchForbidBuy = true; if (result.batchForbidBuyReason.IsEmpty()) result.batchForbidBuyReason = _T("WR钝化"); }
		if (narrowBand) { result.batchForbidBuy = true; if (result.batchForbidBuyReason.IsEmpty()) result.batchForbidBuyReason = _T("BOLL过窄"); }
		// 布林扩张：中轨上行禁止卖出，中轨下行禁止买入
		if (bandExpand && bollMidUp) { result.batchForbidSell = true; if (result.batchForbidSellReason.IsEmpty()) result.batchForbidSellReason = _T("BOLL扩张"); }
		if (bandExpand && !bollMidUp) { result.batchForbidBuy = true; if (result.batchForbidBuyReason.IsEmpty()) result.batchForbidBuyReason = _T("BOLL扩张"); }
	}

	// 风控拦截
	result.batchSellFilteredByForbid = false;
	result.batchBuyFilteredByForbid = false;
	if (!result.batchHasSell && !result.batchHasBuy)
		result.batchFilterReason = _T("无买卖条件");
	else if (result.batchForbidBuy && result.batchHasBuy && !result.batchHasSell)
	{ result.batchBuyFilteredByForbid = true; result.batchFilterReason = _T("买入被风控拦截(") + result.batchForbidBuyReason + _T(")"); }
	else if (result.batchForbidSell && result.batchHasSell && !result.batchHasBuy)
	{ result.batchSellFilteredByForbid = true; result.batchFilterReason = _T("卖出被风控拦截(") + result.batchForbidSellReason + _T(")"); }
	else
		result.batchFilterReason = _T("通过");

	// 5分钟信号（从批量判定结果推断，不再独立计算）
	if (result.batchHasSell && !result.batchSellFilteredByForbid)
		result.sig5m = STOCK::Signal5m::SIG_SELL;
	else if (result.batchHasBuy && !result.batchBuyFilteredByForbid)
		result.sig5m = STOCK::Signal5m::SIG_BUY;
	else
		result.sig5m = STOCK::Signal5m::SIG_NONE;

	return result;
}

// ========== 实时指标信号检测 ==========
// 基于当前实时价格判断5个核心指标的买卖状态
// endIndex: 计算到该索引位置为止（-1表示使用最后一根K线）
// 信号强度分档规则：
//   BOLL: 1档=触碰轨道，2档=越过轨道1/3带宽，3档=越过轨道2/3带宽
//   MACD: 1档=刚交叉，2档=DIF偏离DEA中等幅度，3档=DIF偏离DEA大幅度
//   RSI:  卖1档=70-80，卖2档=80-90，卖3档=90+；买1档=30-20，买2档=20-10，买3档=10-
//   KDJ:  卖1档=80-90(K)，卖2档=90-100(K)，卖3档=J>100；买1档=20-10(K)，买2档=10-0(K)，买3档=J<0
//   W&R:  卖1档=20-10，卖2档=10-5，卖3档=5-0；买1档=80-90，买2档=90-95，买3档=95-100
CSignalAnalyzer::RealtimeSignal CSignalAnalyzer::CalcRealtimeSignals(const std::vector<STOCK::Bar>& bars5, int endIndex /* = -1 */)
{
	RealtimeSignal sig;
	if (bars5.size() < 26)
		return sig;

	// 确定计算截止位置
	size_t lastIdx = (endIndex >= 0 && static_cast<size_t>(endIndex) < bars5.size()) ? static_cast<size_t>(endIndex) : bars5.size() - 1;
	if (lastIdx < 25)  // 至少需要26根K线才能计算MACD
		return sig;

	// 截取到endIndex位置的子序列用于指标计算
	std::vector<STOCK::Bar> subBars(bars5.begin(), bars5.begin() + lastIdx + 1);
	const STOCK::Bar& last = subBars.back();
	double currentPrice = last.close;

	// 1. BOLL：价格触碰上轨=卖出，触碰下轨=买入
	// 强度：越远离中轨越强
	STOCK::BollResult boll = CalcBoll(subBars, GetParam().bollPeriod);
	if (boll.up > 0 && boll.dn > 0 && boll.bandwidth > 0)
	{
		double halfBand = boll.bandwidth / 2.0;  // 中轨到上/下轨的距离
		if (currentPrice >= boll.up || last.high >= boll.up)
		{
			sig.boll = 1;
			double exceed = (currentPrice - boll.up) / halfBand;
			if (exceed >= 0.66) sig.bollStr = 3;
			else if (exceed >= 0.33) sig.bollStr = 2;
			else sig.bollStr = 1;
		}
		else if (currentPrice <= boll.dn || last.low <= boll.dn)
		{
			sig.boll = -1;
			double exceed = (boll.dn - currentPrice) / halfBand;
			if (exceed >= 0.66) sig.bollStr = 3;
			else if (exceed >= 0.33) sig.bollStr = 2;
			else sig.bollStr = 1;
		}
	}

	// 2. MACD：金叉=买入，死叉=卖出
	// 强度：DIF偏离DEA的幅度
	{
		std::vector<double> difSeq, deaSeq, macdBarSeq;
		CalcMACDSeries(subBars, difSeq, deaSeq, macdBarSeq);
		if (difSeq.size() >= 2 && deaSeq.size() >= 2)
		{
			size_t n = difSeq.size();
			double difDeaDiff = fabs(difSeq[n - 1] - deaSeq[n - 1]);
			double difThreshold = GetMacdDifThreshold(currentPrice, CalcATR(bars5, bars5.size() - 1));
			// 金叉：前一根DIF < DEA，当前DIF >= DEA
			if (difSeq[n - 2] < deaSeq[n - 2] && difSeq[n - 1] >= deaSeq[n - 1])
			{
				sig.macd = -1;
				if (difDeaDiff >= difThreshold * 2) sig.macdStr = 3;
				else if (difDeaDiff >= difThreshold) sig.macdStr = 2;
				else sig.macdStr = 1;
			}
			// 死叉：前一根DIF > DEA，当前DIF <= DEA
			else if (difSeq[n - 2] > deaSeq[n - 2] && difSeq[n - 1] <= deaSeq[n - 1])
			{
				sig.macd = 1;
				if (difDeaDiff >= difThreshold * 2) sig.macdStr = 3;
				else if (difDeaDiff >= difThreshold) sig.macdStr = 2;
				else sig.macdStr = 1;
			}
		}
	}

	// 3. RSI：超买(>70)=卖出，超卖(<30)=买入
	double rsi6 = CalcRSI(subBars, GetParam().rsiPeriod);
	if (rsi6 > 70)
	{
		sig.rsi = 1;
		if (rsi6 >= 90) sig.rsiStr = 3;
		else if (rsi6 >= 80) sig.rsiStr = 2;
		else sig.rsiStr = 1;
	}
	else if (rsi6 < 30)
	{
		sig.rsi = -1;
		if (rsi6 <= 10) sig.rsiStr = 3;
		else if (rsi6 <= 20) sig.rsiStr = 2;
		else sig.rsiStr = 1;
	}

	// 4. KDJ：超买(K>80)=卖出，超卖(K<20)=买入
	STOCK::KDJResult kdj = CalcKDJ(subBars, GetParam().kdjPeriod);
	if (kdj.k > 80)
	{
		sig.kdj = 1;
		if (kdj.j > 100) sig.kdjStr = 3;
		else if (kdj.k >= 90) sig.kdjStr = 2;
		else sig.kdjStr = 1;
	}
	else if (kdj.k < 20)
	{
		sig.kdj = -1;
		if (kdj.j < 0) sig.kdjStr = 3;
		else if (kdj.k <= 10) sig.kdjStr = 2;
		else sig.kdjStr = 1;
	}

	// 5. W&R：超买(<20)=卖出，超卖(>80)=买入
	double wr6 = CalcWR(subBars, GetParam().wrPeriod);
	if (wr6 < 20)
	{
		sig.wr = 1;
		if (wr6 <= 5) sig.wrStr = 3;
		else if (wr6 <= 10) sig.wrStr = 2;
		else sig.wrStr = 1;
	}
	else if (wr6 > 80)
	{
		sig.wr = -1;
		if (wr6 >= 95) sig.wrStr = 3;
		else if (wr6 >= 90) sig.wrStr = 2;
		else sig.wrStr = 1;
	}

	return sig;
}

// ========== 实时指标信号检测（基于分时数据） ==========
CSignalAnalyzer::RealtimeSignal CSignalAnalyzer::CalcRealtimeSignalsFromTimeline(const std::vector<STOCK::TimelinePoint>& timeline, int endIndex /* = -1 */)
{
	RealtimeSignal sig;
	if (timeline.size() < 26)
		return sig;

	size_t lastIdx = (endIndex >= 0 && static_cast<size_t>(endIndex) < timeline.size()) ? static_cast<size_t>(endIndex) : timeline.size() - 1;
	if (lastIdx < 25)
		return sig;

	// 滚动聚合：每6根分时点合成一根标准5min Bar，真实计算OHLC与成交量
	// 分时点通常是1分钟采样，6根≈5分钟（含首尾）
	constexpr size_t AGG_SIZE = 6;
	std::vector<STOCK::Bar> bars;
	bars.reserve(lastIdx / AGG_SIZE + 1);

	for (size_t i = 0; i <= lastIdx; )
	{
		size_t batchEnd = i + AGG_SIZE;
		if (batchEnd > lastIdx + 1) batchEnd = lastIdx + 1;

		double o = timeline[i].price;
		double h = timeline[i].price;
		double l = timeline[i].price;
		double c = timeline[batchEnd - 1].price;
		double v = 0;
		for (size_t j = i; j < batchEnd; j++)
		{
			if (timeline[j].price > h) h = timeline[j].price;
			if (timeline[j].price < l) l = timeline[j].price;
			v += timeline[j].volume;
		}
		bars.push_back(STOCK::Bar(o, h, l, c, v, 0));
		i = batchEnd;
	}

	if (bars.size() < 26)
		return sig;

	// 直接复用已有的Bar版计算函数
	return CalcRealtimeSignals(bars, static_cast<int>(bars.size() - 1));
}

// ========== 多周期MACD趋势判定（日线→30min→5min→1min联动） ==========

void CSignalAnalyzer::CalcPeriodTrend(PeriodMacdSeq& seq)
{
	if (seq.data.empty()) return;
	const auto& last = seq.data.back();
	int redCnt = 0, greenCnt = 0;
	for (const auto& item : seq.data)
	{
		if (item.bar > 0) redCnt++;
		else greenCnt++;
	}

	seq.isAboveZero = last.isAboveZero;
	// 水上且红柱占多数 = 多头趋势
	seq.isLongTrend = last.isAboveZero && (redCnt > greenCnt);
	// 水下且绿柱占多数 = 空头趋势
	seq.isShortTrend = (!last.isAboveZero) && (greenCnt > redCnt);
}

void CSignalAnalyzer::CalcPeriodBarChange(PeriodMacdSeq& seq)
{
	if (seq.data.size() < 3) return;
	size_t n = seq.data.size();
	double bar0 = seq.data[n - 1].bar;
	double bar1 = seq.data[n - 2].bar;
	double bar2 = seq.data[n - 3].bar;

	// 当前红柱
	if (bar0 > 0)
	{
		seq.barGrowingRed = (bar0 > bar1 && bar1 > bar2);
		seq.barShrinkRed = (bar0 < bar1 && bar1 < bar2);
		seq.barGrowingGreen = false;
		seq.barShrinkGreen = false;
	}
	// 当前绿柱
	else
	{
		double abs0 = std::fabs(bar0);
		double abs1 = std::fabs(bar1);
		double abs2 = std::fabs(bar2);
		seq.barGrowingGreen = (abs0 > abs1 && abs1 > abs2);
		seq.barShrinkGreen = (abs0 < abs1 && abs1 < abs2);
		seq.barGrowingRed = false;
		seq.barShrinkRed = false;
	}
}

void CSignalAnalyzer::CalcPeriodDivergence(PeriodMacdSeq& seq, const std::vector<double>& priceSeq)
{
	seq.topDivergence = false;
	seq.botDivergence = false;

	// 需要至少10根K线才能判断背离
	if (seq.data.size() < 10 || priceSeq.size() < seq.data.size()) return;

	size_t n = seq.data.size();

	// 顶背离：价格创新高，但BAR峰值降低
	// 在最近区间内找两个红柱峰值区间进行比较
	{
		// 找最近两个红柱峰值
		std::vector<size_t> redPeaks; // 红柱峰值位置索引
		bool inRed = false;
		double peakVal = 0;
		size_t peakIdx = 0;
		for (size_t i = 0; i < n; i++)
		{
			if (seq.data[i].bar > 0)
			{
				if (!inRed) { inRed = true; peakVal = seq.data[i].bar; peakIdx = i; }
				else if (seq.data[i].bar > peakVal) { peakVal = seq.data[i].bar; peakIdx = i; }
			}
			else
			{
				if (inRed) { redPeaks.push_back(peakIdx); inRed = false; }
			}
		}
		if (inRed) redPeaks.push_back(peakIdx);

		// 至少两个红柱峰值区间才能比较
		if (redPeaks.size() >= 2)
		{
			size_t lastPeak = redPeaks[redPeaks.size() - 1];
			size_t prevPeak = redPeaks[redPeaks.size() - 2];
			// 价格创新高，但BAR峰值降低 = 顶背离
			if (priceSeq[lastPeak] > priceSeq[prevPeak] &&
				seq.data[lastPeak].bar < seq.data[prevPeak].bar)
			{
				seq.topDivergence = true;
			}
		}
	}

	// 底背离：价格创新低，但绿柱峰值缩小
	{
		std::vector<size_t> greenPeaks; // 绿柱谷值位置索引（绝对值最大的位置）
		bool inGreen = false;
		double troughVal = 0;
		size_t troughIdx = 0;
		for (size_t i = 0; i < n; i++)
		{
			if (seq.data[i].bar < 0)
			{
				double absBar = std::fabs(seq.data[i].bar);
				if (!inGreen) { inGreen = true; troughVal = absBar; troughIdx = i; }
				else if (absBar > troughVal) { troughVal = absBar; troughIdx = i; }
			}
			else
			{
				if (inGreen) { greenPeaks.push_back(troughIdx); inGreen = false; }
			}
		}
		if (inGreen) greenPeaks.push_back(troughIdx);

		// 至少两个绿柱峰值区间才能比较
		if (greenPeaks.size() >= 2)
		{
			size_t lastTrough = greenPeaks[greenPeaks.size() - 1];
			size_t prevTrough = greenPeaks[greenPeaks.size() - 2];
			// 价格创新低，但绿柱峰值缩小 = 底背离
			if (priceSeq[lastTrough] < priceSeq[prevTrough] &&
				std::fabs(seq.data[lastTrough].bar) < std::fabs(seq.data[prevTrough].bar))
			{
				seq.botDivergence = true;
			}
		}
	}
}

void CSignalAnalyzer::RefreshPeriodStatus(PeriodMacdSeq& seq, const std::vector<double>& price)
{
	CalcPeriodTrend(seq);
	CalcPeriodBarChange(seq);
	CalcPeriodDivergence(seq, price);
}

CSignalAnalyzer::PeriodMacdSeq CSignalAnalyzer::BuildPeriodMacdSeq(const std::vector<STOCK::Bar>& bars, PeriodType type)
{
	PeriodMacdSeq seq;
	seq.type = type;

	// 复用CalcMACDSeries计算完整MACD序列
	std::vector<double> difSeq, deaSeq, barSeq;
	CalcMACDSeries(bars, difSeq, deaSeq, barSeq);

	seq.data.resize(difSeq.size());
	for (size_t i = 0; i < difSeq.size(); i++)
	{
		seq.data[i].dif = difSeq[i];
		seq.data[i].dea = deaSeq[i];
		seq.data[i].bar = barSeq[i];
		seq.data[i].isAboveZero = (difSeq[i] > 0);
	}

	return seq;
}

CSignalAnalyzer::TradeResult CSignalAnalyzer::JudgeT0Trade(
	PeriodMacdSeq& daySeq,
	PeriodMacdSeq& m30Seq,
	PeriodMacdSeq& m5Seq,
	PeriodMacdSeq& m1Seq,
	const std::vector<double>& dayPrice,
	const std::vector<double>& m30Price,
	const std::vector<double>& m5Price,
	const std::vector<double>& m1Price
)
{
	TradeResult result;

	// 1. 刷新所有周期计算状态
	RefreshPeriodStatus(daySeq, dayPrice);
	RefreshPeriodStatus(m30Seq, m30Price);
	RefreshPeriodStatus(m5Seq, m5Price);
	RefreshPeriodStatus(m1Seq, m1Price);

	// ======================
	// 第一层：日线定全局格局
	// ======================
	bool dayStrongLong = daySeq.isLongTrend && !daySeq.topDivergence;
	bool dayStrongShort = daySeq.isShortTrend && !daySeq.botDivergence;
	bool dayHighRisk = daySeq.topDivergence;  // 日线顶背离，日内风险大
	bool dayBottomSafe = daySeq.botDivergence; // 日线底背离，下跌空间有限

	// ======================
	// 第二层：30分钟日内主线
	// ======================
	bool m30LongMain = m30Seq.isLongTrend;
	bool m30ShortMain = m30Seq.isShortTrend;

	// ======================
	// 场景1：日线多头强势（适合做正T低吸）
	// ======================
	if (dayStrongLong)
	{
		// 30分钟多头主线，5分钟绿柱缩短+1分钟底背离：回调低吸买点
		if (m30LongMain && m5Seq.barShrinkGreen && m1Seq.botDivergence)
		{
			result.sig = TradeSignal::BUY_T;
			result.desc = _T("日线多头+30分水上回调，5分绿柱衰竭+1分底背离，低吸正T");
			return result;
		}
		// 多层共振多头，持有或加仓
		if (m30LongMain && m5Seq.barGrowingRed && m1Seq.barGrowingRed)
		{
			result.sig = TradeSignal::HOLD;
			result.desc = _T("四层多头共振，持有不动，等1分顶背离止盈");
			return result;
		}
		// 小周期全部多头衰竭，止盈卖出T仓
		if (m5Seq.barShrinkRed && m1Seq.topDivergence)
		{
			result.sig = TradeSignal::SELL_T;
			result.desc = _T("日线多头但5/1分红柱衰竭顶背离，止盈T仓位");
			return result;
		}
	}

	// ======================
	// 场景2：日线空头（只适合逢高反T，禁止新开多仓）
	// ======================
	if (dayStrongShort)
	{
		// 小周期反弹红柱，顶背离，冲高卖出
		if (m5Seq.barGrowingRed && m1Seq.topDivergence)
		{
			result.sig = TradeSignal::SELL_T;
			result.desc = _T("日线空头，5/1分反弹见顶，冲高反T减仓");
			return result;
		}
		// 30分持续空头，小周期任何信号都不做多
		if (m30ShortMain && m5Seq.barGrowingGreen)
		{
			result.sig = TradeSignal::NO_OP;
			result.desc = _T("日线+30分双重空头，禁止低吸做多，观望");
			return result;
		}
	}

	// ======================
	// 场景3：日线高位顶背离，震荡高风险
	// ======================
	if (dayHighRisk)
	{
		if (m1Seq.topDivergence)
		{
			result.sig = TradeSignal::SELL_T;
			result.desc = _T("日线顶背离高风险，1分钟见顶立即卖出T仓");
			return result;
		}
		result.sig = TradeSignal::NO_OP;
		result.desc = _T("日线高位顶背离，严控仓位，不主动低吸");
		return result;
	}

	// ======================
	// 场景4：日线底部底背离，下跌有限
	// ======================
	if (dayBottomSafe)
	{
		if (m30Seq.barShrinkGreen && m1Seq.botDivergence)
		{
			result.sig = TradeSignal::BUY_T;
			result.desc = _T("日线底背离安全区间，30分绿柱衰竭，小周期共振低吸");
			return result;
		}
	}

	// 默认无明确信号，观望
	result.sig = TradeSignal::NO_OP;
	result.desc = _T("多周期信号冲突，无共振，等待更好机会");
	return result;
}

CSignalAnalyzer::TradeResult CSignalAnalyzer::CalcMultiPeriodMacdSignal(
	const std::vector<STOCK::Bar>& dayBars,
	const std::vector<STOCK::Bar>& m30Bars,
	const std::vector<STOCK::Bar>& m5Bars,
	const std::vector<STOCK::Bar>& m1Bars)
{
	TradeResult result;

	// 各周期至少需要26根K线才能计算MACD
	if (dayBars.size() < 26 || m30Bars.size() < 26 || m5Bars.size() < 26 || m1Bars.size() < 26)
	{
		result.sig = TradeSignal::NO_OP;
		result.desc = _T("K线数据不足，无法计算多周期MACD");
		return result;
	}

	// 构建各周期MACD序列
	PeriodMacdSeq daySeq = BuildPeriodMacdSeq(dayBars, PeriodType::DAY);
	PeriodMacdSeq m30Seq = BuildPeriodMacdSeq(m30Bars, PeriodType::M30);
	PeriodMacdSeq m5Seq = BuildPeriodMacdSeq(m5Bars, PeriodType::M5);
	PeriodMacdSeq m1Seq = BuildPeriodMacdSeq(m1Bars, PeriodType::M1);

	// 提取各周期收盘价序列
	std::vector<double> dayPrice, m30Price, m5Price, m1Price;
	dayPrice.reserve(dayBars.size());
	for (const auto& bar : dayBars) dayPrice.push_back(bar.close);
	m30Price.reserve(m30Bars.size());
	for (const auto& bar : m30Bars) m30Price.push_back(bar.close);
	m5Price.reserve(m5Bars.size());
	for (const auto& bar : m5Bars) m5Price.push_back(bar.close);
	m1Price.reserve(m1Bars.size());
	for (const auto& bar : m1Bars) m1Price.push_back(bar.close);

	return JudgeT0Trade(daySeq, m30Seq, m5Seq, m1Seq, dayPrice, m30Price, m5Price, m1Price);
}