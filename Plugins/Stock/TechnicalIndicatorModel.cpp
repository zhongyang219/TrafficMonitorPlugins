#include "pch.h"
#include "TechnicalIndicatorModel.h"
#include "ChartColors.h"
#include <cmath>

// ============================================================================
// 技术指标数据模型实现：只在序列更新时计算并缓存，供渲染层只读
// ============================================================================

// 按视图模式选择MACD参数（与原IndicatorChart::DrawMacdChartArea一致）
// 5分钟K线用7,15,5；分时(1分钟)用6,12,4；30分钟用9,19,7；日K用默认12,26,9
void TechnicalIndicatorModel::GetMACDParams(UIViewMode viewMode, int& shortP, int& longP, int& signalP) const
{
	if (viewMode == UI_VIEW_MIN30_KLINE) { shortP = 9; longP = 19; signalP = 7; }
	else if (viewMode == UI_VIEW_MIN5_KLINE) { shortP = 7; longP = 15; signalP = 5; }
	else if (viewMode < UI_VIEW_MIN5_KLINE) { shortP = 6; longP = 12; signalP = 4; }
	else { shortP = 12; longP = 26; signalP = 9; }
}

// 按视图模式选择KDJ参数（与原IndicatorChart逻辑一致）
// 5分钟用8,3,3；分时(1分钟)用7,3,3；30分钟和日K用默认9,3,3
void TechnicalIndicatorModel::GetKDJParams(UIViewMode viewMode, int& n, int& m1, int& m2) const
{
	if (viewMode == UI_VIEW_MIN5_KLINE) { n = 8; m1 = 3; m2 = 3; }
	else if (viewMode == UI_VIEW_TIMELINE) { n = 7; m1 = 3; m2 = 3; }
	else { n = 9; m1 = 3; m2 = 3; }
}

void TechnicalIndicatorModel::Reset()
{
	m_valid = false;
	m_macd.clear();
	m_macdCross.clear();
	m_kdj.clear();
	m_kdjCross.clear();
	m_wr.clear();
	m_rsi.clear();
	m_volMa5.clear();
	m_volMa10.clear();
	m_bollUpper.clear();
	m_bollMid.clear();
	m_bollLower.clear();
	m_difUpIntervals.clear();
	m_lastMacdValid = m_lastKdjValid = m_lastWrValid = m_lastRsiValid = false;
	m_difTrendArrows = 0;
	m_difTrendDir = 0;
	m_difTrendColor = CLR_INVALID;
}

// 全量更新：所有副图指标基于fullTimeline（转分时序列）；W&R/RSI在K线模式下用K线版计算
void TechnicalIndicatorModel::UpdateAll(const std::vector<STOCK::TimelinePoint>& fullTimeline,
	const std::vector<STOCK::KLinePoint>& klineData, UIViewMode viewMode)
{
	m_viewMode = viewMode;
	const int n = static_cast<int>(fullTimeline.size());
	m_valid = (n > 0);
	if (n == 0)
	{
		Reset();
		return;
	}

	UpdateMACD(fullTimeline, viewMode);
	UpdateKDJ(fullTimeline, viewMode);
	UpdateWR(fullTimeline, klineData, viewMode);
	UpdateRSI(fullTimeline, klineData, viewMode);
	UpdateVolumeMA(fullTimeline);
	UpdateBOLL(fullTimeline);
}

// ---- MACD：指标序列 + 金叉死叉 + 最新有效值缓存 ----
void TechnicalIndicatorModel::UpdateMACD(const std::vector<STOCK::TimelinePoint>& fullTimeline, UIViewMode viewMode)
{
	int shortP = 12, longP = 26, signalP = 9;
	GetMACDParams(viewMode, shortP, longP, signalP);
	m_macd = CStockIndicator::CalculateTimelineMACD(fullTimeline, shortP, longP, signalP);
	m_macdCross = CStockIndicator::DetectMACDCross(m_macd);

	m_lastMacdValid = false;
	m_lastDif = m_lastDea = 0;
	for (int i = static_cast<int>(m_macd.size()) - 1; i >= 0; --i)
	{
		if (m_macd[i].valid)
		{
			m_lastDif = m_macd[i].dif;
			m_lastDea = m_macd[i].dea;
			m_lastMacdValid = true;
			break;
		}
	}

	UpdateDIFTrend(viewMode);
	UpdateDIFUpIntervals();
}

// DIF尾部趋势缓存（线性回归只在序列更新时算一次）
void TechnicalIndicatorModel::UpdateDIFTrend(UIViewMode viewMode)
{
	m_difTrendArrows = 0;
	m_difTrendDir = 0;
	m_difTrendColor = CLR_INVALID;

	int trendEndIdx = -1;
	for (int i = static_cast<int>(m_macd.size()) - 1; i >= 0; --i)
	{
		if (m_macd[i].valid) { trendEndIdx = i; break; }
	}
	if (trendEndIdx < 0)
		return;

	std::vector<MACDData> subMacd(m_macd.begin(), m_macd.begin() + trendEndIdx + 1);
	// 1分钟用6根，5分钟用8根，30分钟和日K用10根
	int lookback = 10;
	if (viewMode == UI_VIEW_MIN5_KLINE) lookback = 8;
	else if (viewMode == UI_VIEW_TIMELINE) lookback = 6;
	auto trend = CStockIndicator::CalcDIFTrend(subMacd, lookback);
	if (!trend.valid || trend.slope == 0.0)
		return;

	bool isUp = trend.slope > 0;
	double refDif = std::abs(m_macd[trendEndIdx].dif);
	double base = (refDif > 0.001) ? refDif : 0.001;
	double absSlope = std::abs(trend.slope);
	m_difTrendArrows = 1;
	if (absSlope >= base * 0.3) m_difTrendArrows = 3;
	else if (absSlope >= base * 0.15) m_difTrendArrows = 2;
	m_difTrendDir = isUp ? 1 : -1;
	m_difTrendColor = (m_macd[trendEndIdx].dea >= 0) ? COLOR_RED_UP : COLOR_GREEN_DOWN;
}

// DIF上行区间检测（降噪：微小变化用阈值忽略；上升趋势需连续3根下降才闭合）
// 回升并入规则：下降后转升时，只有升破下降前最高点才算延续当前上升趋势，
// 否则当前区间以最高点为结束边界，从回落低点重新开一段候选区间；
// 实时效果：遇到明显下降后区间的绘制边界先停在最高点处，等后续数据
// （未满3连降 且 升破最高点）再行延伸并入
void TechnicalIndicatorModel::UpdateDIFUpIntervals()
{
	m_difUpIntervals.clear();

	// 防御：m_macd由CalculateTimelineMACD返回，长度若与fullTimeline不一致
	// （数据源瞬时异常时），后续所有索引访问都会越界崩溃——先钳制扫描范围
	int nMacd = static_cast<int>(m_macd.size());
	if (nMacd <= 1)
		return;	 // 序列过短无法构成区间，跳过检测但不清空其它指标缓存

	// 相对阈值：以DIF量级的0.1%为最小变化幅度，过滤小数位噪声毛刺
	const int kMinUpSteps = 3;   // 区间内最少上升步数（过滤短促微抖区间）
	const int kMinDownSteps = 2; // 连续下降根数达到此值才闭合区间
	double threshold = 0.00001;
	for (int i = nMacd - 1; i >= 0; --i)
	{
		if (m_macd[i].valid)
		{
			threshold = (std::abs(m_macd[i].dif) > 0.001)
				? std::abs(m_macd[i].dif) * 0.001 : 0.00001;
			break;
		}
	}

	auto scanIntervals = [&](std::vector<std::pair<int, int>>& intervals) {
		int startIdx = -1;          // 候选/已确认上行区间起点（最低点）
		int upCount = 0;            // 区间内上升步数（不含起点），>=kMinUpSteps 才算确认开启
		bool confirmed = false;     // 区间是否已确认开启（upCount>=3）
		int peakIdx = -1;           // 区间内最高点索引
		double peakVal = 0.0;       // 区间内DIF最高值（反弹是否破前高的基准）
		bool lockedPeak = false;    // 高点是否已锁定（确认后出现过回落）
		int consecutiveDownCnt = 0; // 连续下跌计数

		auto closeInterval = [&]() {
			// 只有 confirmed（>=3根上涨）才记录；不足3根的不算上涨区间
			if (startIdx >= 0 && confirmed && peakIdx > startIdx
				&& m_macd[peakIdx].dif > m_macd[startIdx].dif + threshold)
			{
				intervals.push_back({ startIdx, peakIdx });
			}
			startIdx = peakIdx = -1;
			upCount = 0;
			confirmed = false;
			lockedPeak = false;
			consecutiveDownCnt = 0;
			peakVal = 0.0;
			};

		for (int i = 1; i < nMacd; ++i)
		{
			if (!m_macd[i].valid || !m_macd[i - 1].valid) { closeInterval(); continue; }
			double diff = m_macd[i].dif - m_macd[i - 1].dif;
			double currDif = m_macd[i].dif;

			if (diff > threshold)
			{
				// 上涨，重置连续下跌
				consecutiveDownCnt = 0;
				if (startIdx < 0)
				{
					// 建立候选区间，起点为最低点 i-1，已有1根上升步
					startIdx = i - 1;
					peakIdx = i;
					peakVal = currDif;
					upCount = 1;
					lockedPeak = false;
				}
				else if (!confirmed)
				{
					// 尚未凑齐3根：继续累计
					peakIdx = i;
					peakVal = currDif;
					++upCount;
					if (upCount >= kMinUpSteps)
						confirmed = true;   // 连续3根以上上涨（不含起点），正式开启
				}
				else if (!lockedPeak)
				{
					// 已确认、未回落：刷新最高点，趋势延续
					peakIdx = i;
					peakVal = currDif;
					++upCount;
				}
				else
				{
					// 已确认、已回落锁定高点：当前为反弹
					if (currDif > peakVal)
					{
						// ✅ 反弹突破下跌前最高点：解锁，延续本区间
						lockedPeak = false;
						peakIdx = i;
						peakVal = currDif;
						++upCount;
					}
					else
					{
						// ❌ 反弹未破前高：关闭旧区间，从本次低点重开候选
						closeInterval();
						startIdx = i - 1;
						peakIdx = i;
						peakVal = currDif;
						upCount = 1;        // confirmed 保持 false，需重新凑3根
					}
				}
			}
			else if (diff < -threshold)
			{
				// 明显下跌
				if (startIdx < 0) continue;
				if (!confirmed)
				{
					// 未凑齐3根就下跌：候选作废，不计为上涨区间
					closeInterval();
				}
				else
				{
					if (!lockedPeak) lockedPeak = true;
					++consecutiveDownCnt;
					if (consecutiveDownCnt >= kMinDownSteps)
						closeInterval();
				}
			}
			else
			{
				// 微变区间
				if (diff < 0)
				{
					// 微小阴跌
					if (startIdx < 0) continue;
					if (!confirmed)
					{
						closeInterval();    // 未确认即下跌，候选作废
					}
					else
					{
						if (!lockedPeak) lockedPeak = true;
						++consecutiveDownCnt;
						if (consecutiveDownCnt >= kMinDownSteps)
							closeInterval();
					}
				}
				else
				{
					// 横盘持平：下跌计数保持不变，高点锁定状态也保持
				}
			}
		}
		closeInterval();   // 末尾补录（confirmed 才记录）
		};

	scanIntervals(m_difUpIntervals);

	// 后置合并："回升未破前高切新段"会把一次抬升拆成多小段（浅回落
	// 只隔几根），间隔不超过kMinDownSteps+1根且后段峰值不低于前段
	// 峰值时并成一段（峰值递减说明趋势走弱，保持分段）
	if (m_difUpIntervals.size() >= 2)
	{
		std::vector<std::pair<int, int>> merged;
		merged.push_back(m_difUpIntervals[0]);
		for (size_t k = 1; k < m_difUpIntervals.size(); ++k)
		{
			auto& last = merged.back();
			bool gapSmall = m_difUpIntervals[k].first - last.second <= kMinDownSteps + 1;
			// 区间端点在生成时均经nMacd范围校验，此处再防御一次越界
			bool inRange = m_difUpIntervals[k].second >= 0 && m_difUpIntervals[k].second < nMacd
				&& last.second >= 0 && last.second < nMacd;
			bool peakRises = inRange && m_macd[m_difUpIntervals[k].second].dif >= m_macd[last.second].dif;
			if (gapSmall && peakRises)
				last.second = m_difUpIntervals[k].second;   // 并入：取峰值更高的远端点
			else
				merged.push_back(m_difUpIntervals[k]);
		}
		m_difUpIntervals.swap(merged);
	}
}

// ---- KDJ：指标序列 + 金叉死叉 + 最新有效值缓存 ----
void TechnicalIndicatorModel::UpdateKDJ(const std::vector<STOCK::TimelinePoint>& fullTimeline, UIViewMode viewMode)
{
	int kdjN, kdjM1, kdjM2;
	GetKDJParams(viewMode, kdjN, kdjM1, kdjM2);
	m_kdj = CStockIndicator::CalculateTimelineKDJ(fullTimeline, kdjN, kdjM1, kdjM2);
	m_kdjCross = CStockIndicator::DetectKDJCross(m_kdj);

	m_lastKdjValid = false;
	m_lastK = m_lastD = m_lastJ = 0;
	for (int i = static_cast<int>(m_kdj.size()) - 1; i >= 0; --i)
	{
		if (m_kdj[i].valid)
		{
			m_lastK = m_kdj[i].k;
			m_lastD = m_kdj[i].d;
			m_lastJ = m_kdj[i].j;
			m_lastKdjValid = true;
			break;
		}
	}
}

// W&R：K线模式用K线版计算，分时用分时版
void TechnicalIndicatorModel::UpdateWR(const std::vector<STOCK::TimelinePoint>& fullTimeline,
	const std::vector<STOCK::KLinePoint>& klineData, UIViewMode viewMode)
{
	const bool isKLineMode = (viewMode >= UI_VIEW_MIN5_KLINE);
	if (isKLineMode && !klineData.empty())
		m_wr = CStockIndicator::CalculateKLineWR(klineData);
	else
		m_wr = CStockIndicator::CalculateTimelineWR(fullTimeline);

	m_lastWrValid = false;
	m_lastWr1 = m_lastWr2 = 0;
	for (int i = static_cast<int>(m_wr.size()) - 1; i >= 0; --i)
	{
		if (m_wr[i].valid)
		{
			m_lastWr1 = m_wr[i].wr1;
			m_lastWr2 = m_wr[i].wr2;
			m_lastWrValid = true;
			break;
		}
	}
}

// RSI：K线模式用K线版计算，分时用分时版
void TechnicalIndicatorModel::UpdateRSI(const std::vector<STOCK::TimelinePoint>& fullTimeline,
	const std::vector<STOCK::KLinePoint>& klineData, UIViewMode viewMode)
{
	const bool isKLineMode = (viewMode >= UI_VIEW_MIN5_KLINE);
	if (isKLineMode && !klineData.empty())
		m_rsi = CStockIndicator::CalculateKLineRSI(klineData);
	else
		m_rsi = CStockIndicator::CalculateTimelineRSI(fullTimeline);

	m_lastRsiValid = false;
	m_lastRsi1 = m_lastRsi2 = 0;
	for (int i = static_cast<int>(m_rsi.size()) - 1; i >= 0; --i)
	{
		if (m_rsi[i].valid)
		{
			m_lastRsi1 = m_rsi[i].rsi1;
			m_lastRsi2 = m_rsi[i].rsi2;
			m_lastRsiValid = true;
			break;
		}
	}
}

// 成交量MA5/MA10（滑动窗口求和，只在序列更新时算一次）
void TechnicalIndicatorModel::UpdateVolumeMA(const std::vector<STOCK::TimelinePoint>& fullTimeline)
{
	const int n = static_cast<int>(fullTimeline.size());
	m_volMa5.assign(n, 0.0);
	m_volMa10.assign(n, 0.0);
	auto calcVolMA = [&](int period, std::vector<double>& out) {
		double sum = 0.0;
		for (int i = 0; i < n; ++i)
		{
			sum += static_cast<double>(fullTimeline[i].volume);
			if (i >= period)
				sum -= static_cast<double>(fullTimeline[i - period].volume);
			if (i >= period - 1)
				out[i] = sum / period;
		}
		};
	calcVolMA(5, m_volMa5);
	calcVolMA(10, m_volMa10);
}

// BOLL（短横线上中下轨，使用与原TimelineChart一致的N=20,K=2）
void TechnicalIndicatorModel::UpdateBOLL(const std::vector<STOCK::TimelinePoint>& fullTimeline)
{
	const int n = static_cast<int>(fullTimeline.size());
	m_bollUpper.assign(n, 0.0);
	m_bollMid.assign(n, 0.0);
	m_bollLower.assign(n, 0.0);
	const int N = 20;
	const int K = 2;
	for (int i = 0; i < n; ++i)
	{
		if (i < N - 1)
			continue;
		double sum = 0;
		for (int j = i - N + 1; j <= i; ++j)
			sum += fullTimeline[j].price;
		double ma = sum / N;
		double variance = 0;
		for (int j = i - N + 1; j <= i; ++j)
		{
			double diff = fullTimeline[j].price - ma;
			variance += diff * diff;
		}
		double stddev = std::sqrt(variance / N);
		m_bollMid[i] = ma;
		m_bollUpper[i] = ma + K * stddev;
		m_bollLower[i] = ma - K * stddev;
	}
}

// 标题栏缓存取值接口
bool TechnicalIndicatorModel::GetLastMACD(double& dif, double& dea) const
{
	dif = m_lastDif;
	dea = m_lastDea;
	return m_lastMacdValid;
}

bool TechnicalIndicatorModel::GetLastKDJ(double& k, double& d, double& j) const
{
	k = m_lastK;
	d = m_lastD;
	j = m_lastJ;
	return m_lastKdjValid;
}

bool TechnicalIndicatorModel::GetLastWR(double& wr1, double& wr2) const
{
	wr1 = m_lastWr1;
	wr2 = m_lastWr2;
	return m_lastWrValid;
}

bool TechnicalIndicatorModel::GetLastRSI(double& rsi1, double& rsi2) const
{
	rsi1 = m_lastRsi1;
	rsi2 = m_lastRsi2;
	return m_lastRsiValid;
}

// DIF尾部趋势（缓存值）
bool TechnicalIndicatorModel::GetDIFTrend(int index, int& arrowCount, int& isUp, COLORREF& arrowColorRef_unused) const
{
	(void)index;
	arrowCount = m_difTrendArrows;
	isUp = m_difTrendDir;
	arrowColorRef_unused = m_difTrendColor;
	return m_difTrendArrows > 0;
}