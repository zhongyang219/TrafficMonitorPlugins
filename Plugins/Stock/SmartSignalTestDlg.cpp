#include "pch.h"
#include "SmartSignalTestDlg.h"
#include "DataManager.h"
#include <Stock.h>
#include <algorithm>

CString CSmartSignalTestDlg::BoolToText(bool value)
{
	return value ? _T("是") : _T("否");
}

CString CSmartSignalTestDlg::TrendStateToText(STOCK::TrendState30m state)
{
	switch (state)
	{
	case STOCK::TrendState30m::STATE_WEAK:
		return _T("弱势");
	case STOCK::TrendState30m::STATE_WEAK_SHAKE:
		return _T("弱震荡");
	case STOCK::TrendState30m::STATE_STRONG:
		return _T("强势");
	case STOCK::TrendState30m::STATE_SHAKE:
	default:
		return _T("震荡");
	}
}

CString CSmartSignalTestDlg::Signal5mToText(STOCK::Signal5m signal)
{
	switch (signal)
	{
	case STOCK::Signal5m::SIG_BUY:
		return _T("买入");
	case STOCK::Signal5m::SIG_SELL:
		return _T("卖出");
	case STOCK::Signal5m::SIG_NONE:
	default:
		return _T("无信号");
	}
}

void CSmartSignalTestDlg::Show(const std::wstring& stockId, int timelineIndex,
	bool isKLineMode, bool isMin5KLineMode,
	double pendingTradePrice, const CString& pendingTradeTime,
	CWnd* pParentWnd)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	std::vector<STOCK::Bar> allBars5, allBars30;
	std::vector<std::string> allBars5Time;
	std::vector<STOCK::TimelinePoint> timelineData;
	{
		std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
		auto stockData = g_data.GetStockData(stockId);
		if (!stockData)
		{
			MessageBox(pParentWnd ? pParentWnd->GetSafeHwnd() : NULL, _T("未获取到当前股票数据，无法测试买卖点检测。"), _T("买卖点检测测试"), MB_ICONINFORMATION);
			return;
		}

		auto min5KLineObj = stockData->getMin5KLineData();
		auto min30KLineObj = stockData->getMin30KLineData();
		if (min5KLineObj)
		{
			allBars5.reserve(min5KLineObj->data.size());
			allBars5Time.reserve(min5KLineObj->data.size());
			for (const auto& kp : min5KLineObj->data)
			{
				allBars5.push_back(STOCK::Bar::FromKLinePoint(kp));
				allBars5Time.push_back(kp.day);
			}
		}
		if (min30KLineObj)
		{
			allBars30.reserve(min30KLineObj->data.size());
			for (const auto& kp : min30KLineObj->data)
				allBars30.push_back(STOCK::Bar::FromKLinePoint(kp));
		}
		// 分时图模式获取分时数据
		if (!isKLineMode)
		{
			auto timelineObj = stockData->getTimelineData();
			if (timelineObj)
				timelineData = timelineObj->data;
		}
	}

	// 分时图模式：使用1分钟粒度信号
	if (!isKLineMode && timelineData.size() >= 120 && !allBars30.empty())
	{
		std::vector<STOCK::Bar> bars1m = CSignalAnalyzer::ConvertTimelineToBars(timelineData);
		int barIndex = max(0, min(timelineIndex, static_cast<int>(bars1m.size()) - 1));

		CString mappedBarTime;
		if (barIndex >= 0 && barIndex < static_cast<int>(timelineData.size()))
			mappedBarTime = CString(timelineData[barIndex].fullTime.c_str());

		auto ar = CSignalAnalyzer::AnalyzeSignalAtFromTimeline(bars1m, allBars30, barIndex);

		CString clickedBatchText;
		if (ar.clickedSignal)
		{
			// 综合clickedSignal和风控判定：如果信号方向被风控拦截，显示风控禁止
			bool isForbidByRisk = false;
			CString forbidRiskReason;
			if (ar.clickedSignal->isBuy && ar.batchForbidBuy)
			{
				isForbidByRisk = true;
				forbidRiskReason = ar.batchForbidBuyReason;
			}
			else if (!ar.clickedSignal->isBuy && ar.batchForbidSell)
			{
				isForbidByRisk = true;
				forbidRiskReason = ar.batchForbidSellReason;
			}

			if (ar.clickedSignal->isForbid || isForbidByRisk)
			{
				CString reason = ar.clickedSignal->isForbid ? ar.clickedSignal->reason : (forbidRiskReason.IsEmpty() ? _T("风控") : forbidRiskReason);
				clickedBatchText.Format(_T("%s（%s）"),
					ar.clickedSignal->isBuy ? _T("买入") : _T("卖出"),
					reason.GetString());
			}
			else
			{
				clickedBatchText.Format(_T("%s（%s）"),
					ar.clickedSignal->isBuy ? _T("买入") : _T("卖出"),
					ar.clickedSignal->reason.GetString());
			}
		}
		else
		{
			// clickedSignal为空但风控拦截时，也显示风控信息
			if (ar.batchForbidBuy && ar.batchHasBuy)
				clickedBatchText = _T("买入（风控禁止：" + ar.batchForbidBuyReason + _T("）"));
			else if (ar.batchForbidSell && ar.batchHasSell)
				clickedBatchText = _T("卖出（风控禁止：" + ar.batchForbidSellReason + _T("）"));
			else
				clickedBatchText = _T("无信号");
		}

		CString latestSignalText = clickedBatchText;

		CString bollPos;
		if (ar.boll.up > 0 && ar.boll.dn > 0)
		{
			double lastClose = bars1m[barIndex].close;
			if (lastClose >= ar.boll.up) bollPos = _T("上轨上方");
			else if (lastClose <= ar.boll.dn) bollPos = _T("下轨下方");
			else if (lastClose >= ar.boll.mid) bollPos = _T("中轨上方");
			else bollPos = _T("中轨下方");
		}
		else bollPos = _T("N/A");

		CString macdState;
		if (ar.macd.dif > 0 && ar.macd.macd_bar > 0) macdState = _T("多头增强");
		else if (ar.macd.dif > 0 && ar.macd.macd_bar <= 0) macdState = _T("多头减弱");
		else if (ar.macd.dif < 0 && ar.macd.macd_bar < 0) macdState = _T("空头增强");
		else if (ar.macd.dif < 0 && ar.macd.macd_bar >= 0) macdState = _T("空头减弱");
		else macdState = _T("零轴");

		CString kdjState;
		if (ar.kdj.k > 80) kdjState = _T("超买");
		else if (ar.kdj.k < 20) kdjState = _T("超卖");
		else kdjState = _T("中性");

		CString wrState;
		if (ar.wr < 20) wrState = _T("超买");
		else if (ar.wr > 80) wrState = _T("超卖");
		else wrState = _T("中性");

		CString rsiState;
		if (ar.rsi > 70) rsiState = _T("超买");
		else if (ar.rsi < 30) rsiState = _T("超卖");
		else rsiState = _T("中性");

		CString riskInfo;
		if (ar.batchForbidBuy || ar.batchForbidSell)
		{
			riskInfo.Format(_T("禁止买入=%s(%s) 禁止卖出=%s(%s)"),
				BoolToText(ar.batchForbidBuy).GetString(), ar.batchForbidBuyReason.GetString(),
				BoolToText(ar.batchForbidSell).GetString(), ar.batchForbidSellReason.GetString());
		}
		else
			riskInfo = _T("无拦截");

		CString sellDiag;
		sellDiag.Format(_T("必要=%s(触轨=%s 红柱缩=%s) 辅助=%d/2(KDJ=%s RSI=%s WR=%s 量=%s)"),
			BoolToText(ar.sellNecessary).GetString(),
			BoolToText(ar.sellS1).GetString(), BoolToText(ar.sellS2_redShrink).GetString(),
			ar.sellAuxCount,
			BoolToText(ar.sellA1).GetString(), BoolToText(ar.sellA2).GetString(), BoolToText(ar.sellA3).GetString(), BoolToText(ar.sellA4).GetString());

		CString buyDiag;
		buyDiag.Format(_T("必要=%s(触轨=%s 绿柱缩=%s) 辅助=%d/2(KDJ=%s RSI=%s WR=%s 量=%s)"),
			BoolToText(ar.buyNecessary).GetString(),
			BoolToText(ar.buyB1).GetString(), BoolToText(ar.buyB2_greenShrink).GetString(),
			ar.buyAuxCount,
			BoolToText(ar.buyA1).GetString(), BoolToText(ar.buyA2).GetString(), BoolToText(ar.buyA3).GetString(), BoolToText(ar.buyA4).GetString());

		CString msg;
		msg.Format(_T("=== %s %.3f %s [1分钟] ===\r\n")
			_T("\r\n【信号】%s\r\n")
			_T("【30分钟趋势】%s（置信度%d%%，弱%.1f/强%.1f）\r\n")
			_T("【风控】%s\r\n")
			_T("\r\n【指标】\r\n")
			_T("BOLL: 带宽%.4f 位置%s 上轨=%.4f 下轨=%.4f\r\n")
			_T("MACD: DIF=%.3f DEA=%.3f BAR=%.3f %s\r\n")
			_T("KDJ:  K=%.1f D=%.1f J=%.1f %s\r\n")
			_T("WR:   %.1f %s\r\n")
			_T("RSI:  %.1f %s\r\n")
			_T("\r\n【卖出条件】%s\r\n")
			_T("【买入条件】%s\r\n")
			_T("\r\n【批量判定链】hasSell=%s hasBuy=%s forbidBuy=%s forbidSell=%s 过滤=%s\r\n"),
			CString(stockId.c_str()).GetString(),
			pendingTradePrice,
			mappedBarTime.GetString(),
			latestSignalText.GetString(),
			TrendStateToText(ar.trendResult.state).GetString(),
			ar.trendResult.confidence, ar.trendResult.weakScore, ar.trendResult.strongScore,
			riskInfo.GetString(),
			ar.boll.bandwidth, bollPos.GetString(), ar.boll.up, ar.boll.dn,
			ar.macd.dif, ar.macd.dea, ar.macd.macd_bar, macdState.GetString(),
			ar.kdj.k, ar.kdj.d, ar.kdj.j, kdjState.GetString(),
			ar.wr, wrState.GetString(),
			ar.rsi, rsiState.GetString(),
			sellDiag.GetString(),
			buyDiag.GetString(),
			BoolToText(ar.batchHasSell).GetString(),
			BoolToText(ar.batchHasBuy).GetString(),
			BoolToText(ar.batchForbidBuy).GetString(),
			BoolToText(ar.batchForbidSell).GetString(),
			ar.batchFilterReason.GetString());

		// 弹框定位
		CRect wndRect;
		if (pParentWnd && pParentWnd->GetSafeHwnd())
			pParentWnd->GetWindowRect(&wndRect);
		int msgX = wndRect.right - 8;
		CRect workArea;
		SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
		int msgBottom = workArea.bottom + 8;
		static int g_msgBoxX = 0;
		static int g_msgBoxBottom = 0;
		g_msgBoxX = msgX;
		g_msgBoxBottom = msgBottom;
		static HHOOK g_hHook = NULL;
		static auto cbtProc = [](int nCode, WPARAM wParam, LPARAM lParam) -> LRESULT {
			if (nCode == HCBT_ACTIVATE)
			{
				HWND hWnd = (HWND)wParam;
				wchar_t cls[32] = {};
				GetClassNameW(hWnd, cls, 32);
				if (wcscmp(cls, L"#32770") == 0)
				{
					RECT rect;
					::GetWindowRect(hWnd, &rect);
					int w = rect.right - rect.left;
					int h = rect.bottom - rect.top;
					int x = g_msgBoxX;
					int y = g_msgBoxBottom - h;
					::SetWindowPos(hWnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
					if (g_hHook) { UnhookWindowsHookEx(g_hHook); g_hHook = NULL; }
				}
			}
			return CallNextHookEx(g_hHook, nCode, wParam, lParam);
			};
		if (g_hHook) { UnhookWindowsHookEx(g_hHook); g_hHook = NULL; }
		g_hHook = SetWindowsHookExW(WH_CBT, cbtProc, NULL, GetCurrentThreadId());
		MessageBox(pParentWnd ? pParentWnd->GetSafeHwnd() : NULL, msg, _T("信号分析(1分钟)"), MB_ICONINFORMATION);
		if (g_hHook) { UnhookWindowsHookEx(g_hHook); g_hHook = NULL; }
		return;
	}

	// K线模式：使用5分钟K线信号（原有逻辑）
	if (allBars5.empty() || allBars30.empty())
	{
		CString msg;
		msg.Format(_T("股票代码：%s\r\n5分钟K线数量：%zu\r\n30分钟K线数量：%zu\r\n双击价格：%.3f\r\n双击时间：%s\r\n\r\n缺少5分钟或30分钟K线数据，无法进行信号分析。"),
			CString(stockId.c_str()), allBars5.size(), allBars30.size(), pendingTradePrice, pendingTradeTime.GetString());
		MessageBox(pParentWnd ? pParentWnd->GetSafeHwnd() : NULL, msg, _T("信号分析"), MB_ICONINFORMATION);
		return;
	}

	auto extractDatePart = [](const std::string& timeText) -> std::string {
		auto spacePos = timeText.find(' ');
		return spacePos != std::string::npos ? timeText.substr(0, spacePos) : std::string();
		};
	auto extractTimeMinute = [](const std::string& timeText) -> int {
		std::string t = timeText;
		auto spacePos = t.find(' ');
		if (spacePos != std::string::npos)
			t = t.substr(spacePos + 1);
		if (t.length() < 5)
			return -1;
		int hour = -1;
		int minute = -1;
		if (sscanf_s(t.substr(0, 5).c_str(), "%d:%d", &hour, &minute) != 2)
			return -1;
		if (hour < 0 || minute < 0 || minute >= 60)
			return -1;
		return hour * 60 + minute;
		};

	CStringA pendingTradeTimeA(pendingTradeTime);
	int clickedMinute = extractTimeMinute(std::string(pendingTradeTimeA.GetString()));
	std::string latestDate;
	for (auto it = allBars5Time.rbegin(); it != allBars5Time.rend(); ++it)
	{
		latestDate = extractDatePart(*it);
		if (!latestDate.empty())
			break;
	}
	int barIndex = -1;
	if (isMin5KLineMode)
	{
		barIndex = max(0, min(timelineIndex, static_cast<int>(allBars5.size()) - 1));
	}
	else if (clickedMinute >= 0)
	{
		for (int i = 0; i < static_cast<int>(allBars5Time.size()); i++)
		{
			if (!latestDate.empty() && extractDatePart(allBars5Time[i]) != latestDate)
				continue;
			int barMinute = extractTimeMinute(allBars5Time[i]);
			if (barMinute < 0)
				continue;
			if (barMinute <= clickedMinute)
				barIndex = i;
			else
				break;
		}
	}
	if (barIndex < 0)
		barIndex = static_cast<int>(allBars5.size()) - 1;
	barIndex = max(0, min(barIndex, static_cast<int>(allBars5.size()) - 1));

	CString mappedBarTime;
	if (barIndex >= 0 && barIndex < static_cast<int>(allBars5Time.size()))
		mappedBarTime = CString(allBars5Time[barIndex].c_str());

	// 统一信号分析（与走势图绘制使用相同算法）
	auto ar = CSignalAnalyzer::AnalyzeSignalAt(allBars5, allBars30, barIndex);

	CString clickedBatchText;
	if (ar.clickedSignal)
	{
		// 综合clickedSignal和风控判定：如果信号方向被风控拦截，显示风控禁止
		bool isForbidByRisk = false;
		CString forbidRiskReason;
		if (ar.clickedSignal->isBuy && ar.batchForbidBuy)
		{
			isForbidByRisk = true;
			forbidRiskReason = ar.batchForbidBuyReason;
		}
		else if (!ar.clickedSignal->isBuy && ar.batchForbidSell)
		{
			isForbidByRisk = true;
			forbidRiskReason = ar.batchForbidSellReason;
		}

		if (ar.clickedSignal->isForbid || isForbidByRisk)
		{
			CString reason = ar.clickedSignal->isForbid ? ar.clickedSignal->reason : (forbidRiskReason.IsEmpty() ? _T("风控") : forbidRiskReason);
			clickedBatchText.Format(_T("%s（%s）"),
				ar.clickedSignal->isBuy ? _T("买入") : _T("卖出"),
				reason.GetString());
		}
		else
		{
			clickedBatchText.Format(_T("%s（%s）"),
				ar.clickedSignal->isBuy ? _T("买入") : _T("卖出"),
				ar.clickedSignal->reason.GetString());
		}
	}
	else
	{
		if (ar.batchForbidBuy && ar.batchHasBuy)
			clickedBatchText = _T("买入（风控禁止：" + ar.batchForbidBuyReason + _T("）"));
		else if (ar.batchForbidSell && ar.batchHasSell)
			clickedBatchText = _T("卖出（风控禁止：" + ar.batchForbidSellReason + _T("）"));
		else
			clickedBatchText = _T("无信号");
	}

	// 综合判定直接使用BatchDetectSignals结果（与走势图绘制一致）
	CString latestSignalText = clickedBatchText;

	CString bollPos;
	if (ar.boll.up > 0 && ar.boll.dn > 0)
	{
		double lastClose = allBars5[barIndex].close;
		if (lastClose >= ar.boll.up) bollPos = _T("上轨上方");
		else if (lastClose <= ar.boll.dn) bollPos = _T("下轨下方");
		else if (lastClose >= ar.boll.mid) bollPos = _T("中轨上方");
		else bollPos = _T("中轨下方");
	}
	else bollPos = _T("N/A");

	CString macdState;
	if (ar.macd.dif > 0 && ar.macd.macd_bar > 0) macdState = _T("多头增强");
	else if (ar.macd.dif > 0 && ar.macd.macd_bar <= 0) macdState = _T("多头减弱");
	else if (ar.macd.dif < 0 && ar.macd.macd_bar < 0) macdState = _T("空头增强");
	else if (ar.macd.dif < 0 && ar.macd.macd_bar >= 0) macdState = _T("空头减弱");
	else macdState = _T("零轴");

	CString kdjState;
	if (ar.kdj.k > 80) kdjState = _T("超买");
	else if (ar.kdj.k < 20) kdjState = _T("超卖");
	else kdjState = _T("中性");

	CString wrState;
	if (ar.wr < 20) wrState = _T("超买");
	else if (ar.wr > 80) wrState = _T("超卖");
	else wrState = _T("中性");

	CString rsiState;
	if (ar.rsi > 70) rsiState = _T("超买");
	else if (ar.rsi < 30) rsiState = _T("超卖");
	else rsiState = _T("中性");

	CString riskInfo;
	if (ar.batchForbidBuy || ar.batchForbidSell)
	{
		riskInfo.Format(_T("禁止买入=%s(%s) 禁止卖出=%s(%s)"),
			BoolToText(ar.batchForbidBuy).GetString(), ar.batchForbidBuyReason.GetString(),
			BoolToText(ar.batchForbidSell).GetString(), ar.batchForbidSellReason.GetString());
	}
	else
		riskInfo = _T("无拦截");

	CString sellDiag;
	sellDiag.Format(_T("必要=%s(触轨=%s 红柱缩=%s) 辅助=%d/2(KDJ=%s RSI=%s WR=%s 量=%s)"),
		BoolToText(ar.sellNecessary).GetString(),
		BoolToText(ar.sellS1).GetString(), BoolToText(ar.sellS2_redShrink).GetString(),
		ar.sellAuxCount,
		BoolToText(ar.sellA1).GetString(), BoolToText(ar.sellA2).GetString(), BoolToText(ar.sellA3).GetString(), BoolToText(ar.sellA4).GetString());

	CString buyDiag;
	buyDiag.Format(_T("必要=%s(触轨=%s 绿柱缩=%s) 辅助=%d/2(KDJ=%s RSI=%s WR=%s 量=%s)"),
		BoolToText(ar.buyNecessary).GetString(),
		BoolToText(ar.buyB1).GetString(), BoolToText(ar.buyB2_greenShrink).GetString(),
		ar.buyAuxCount,
		BoolToText(ar.buyA1).GetString(), BoolToText(ar.buyA2).GetString(), BoolToText(ar.buyA3).GetString(), BoolToText(ar.buyA4).GetString());

	CString msg;
	msg.Format(_T("=== %s %.3f(H%.3f/L%.3f) %s ===\r\n")
		_T("\r\n【信号】%s\r\n")
		_T("【30分钟趋势】%s（置信度%d%%，弱%.1f/强%.1f）\r\n")
		_T("【5分钟信号】%s\r\n")
		_T("【风控】%s\r\n")
		_T("\r\n【指标】\r\n")
		_T("BOLL: 带宽%.4f 位置%s 上轨=%.4f 下轨=%.4f\r\n")
		_T("MACD: DIF=%.3f DEA=%.3f BAR=%.3f %s\r\n")
		_T("KDJ:  K=%.1f D=%.1f J=%.1f %s\r\n")
		_T("WR:   %.1f %s\r\n")
		_T("RSI:  %.1f %s\r\n")
		_T("\r\n【卖出条件】%s\r\n")
		_T("【买入条件】%s\r\n")
		_T("\r\n【批量判定链】hasSell=%s hasBuy=%s forbidBuy=%s forbidSell=%s 过滤=%s\r\n"),
		CString(stockId.c_str()).GetString(),
		pendingTradePrice,
		allBars5[barIndex].high,
		allBars5[barIndex].low,
		mappedBarTime.GetString(),
		latestSignalText.GetString(),
		TrendStateToText(ar.trendResult.state).GetString(),
		ar.trendResult.confidence, ar.trendResult.weakScore, ar.trendResult.strongScore,
		Signal5mToText(ar.sig5m).GetString(),
		riskInfo.GetString(),
		ar.boll.bandwidth, bollPos.GetString(), ar.boll.up, ar.boll.dn,
		ar.macd.dif, ar.macd.dea, ar.macd.macd_bar, macdState.GetString(),
		ar.kdj.k, ar.kdj.d, ar.kdj.j, kdjState.GetString(),
		ar.wr, wrState.GetString(),
		ar.rsi, rsiState.GetString(),
		sellDiag.GetString(),
		buyDiag.GetString(),
		BoolToText(ar.batchHasSell).GetString(),
		BoolToText(ar.batchHasBuy).GetString(),
		BoolToText(ar.batchForbidBuy).GetString(),
		BoolToText(ar.batchForbidSell).GetString(),
		ar.batchFilterReason.GetString());

	// 计算弹框位置：左侧与股票对话框右边对齐，底部与系统状态栏平齐
	CRect wndRect;
	if (pParentWnd && pParentWnd->GetSafeHwnd())
		pParentWnd->GetWindowRect(&wndRect);
	int msgX = wndRect.right - 8;
	CRect workArea;
	SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
	int msgBottom = workArea.bottom + 8;

	// 全局变量传递位置给钩子回调
	static int g_msgBoxX = 0;
	static int g_msgBoxBottom = 0;
	g_msgBoxX = msgX;
	g_msgBoxBottom = msgBottom;

	// CBT钩子：MessageBox激活时定位（HCBT_ACTIVATE时窗口已完成布局）
	static HHOOK g_hMsgBoxHook = NULL;
	g_hMsgBoxHook = SetWindowsHookEx(WH_CBT,
		[](int nCode, WPARAM wParam, LPARAM lParam) -> LRESULT {
			if (nCode == HCBT_ACTIVATE)
			{
				HWND hWnd = (HWND)wParam;
				wchar_t cls[8] = {};
				GetClassName(hWnd, cls, 8);
				if (wcscmp(cls, L"#32770") == 0)
				{
					RECT rc;
					::GetWindowRect(hWnd, &rc);
					int w = rc.right - rc.left;
					int h = rc.bottom - rc.top;
					int x = g_msgBoxX;
					int y = g_msgBoxBottom - h;
					::SetWindowPos(hWnd, NULL, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
					// 定位后立即卸载钩子
					UnhookWindowsHookEx(g_hMsgBoxHook);
					g_hMsgBoxHook = NULL;
				}
			}
			return CallNextHookEx(g_hMsgBoxHook, nCode, wParam, lParam);
		}, NULL, GetCurrentThreadId());

	MessageBox(pParentWnd ? pParentWnd->GetSafeHwnd() : NULL, msg, _T("信号分析"), MB_ICONINFORMATION);
	if (g_hMsgBoxHook) { UnhookWindowsHookEx(g_hMsgBoxHook); g_hMsgBoxHook = NULL; }
}
