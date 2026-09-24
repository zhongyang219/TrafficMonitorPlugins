#include "pch.h"
#include "StatusBarPanel.h"
#include "ChartColors.h"
#include "Common.h"
#include "DataManager.h"
#include <Stock.h>
#include <algorithm>
#include <map>
#include <ctime>
#include <limits>

/// 绘制标题栏
void CStatusBarPanel::DrawMainHeader(CDC& memDC, const STOCK::StockInfo& realtimeData, int windowWidth, int headerHeight,
	int leftStart, int textMaxWidth)
{
	double diff = realtimeData.GetChangeAmount();
	double diffPercent = realtimeData.GetChangePercent();
	COLORREF diffColor = diff >= 0 ? COLOR_RED_UP : COLOR_GREEN_DOWN;

	// 标题格式：(股票代码)股票名称：
	CString prefixTxt;
	prefixTxt.Format(_T("%s:"), realtimeData.displayName.c_str());

	CString currentTxt = realtimeData.IsETF() ?
		CCommon::FormatETFPrice(realtimeData.currentPrice) :
		CCommon::FormatFloat(realtimeData.currentPrice);

	CString diffTxt;
	if (diff >= 0)
		diffTxt.Format(_T(" +%.2f%% "), diffPercent);
	else
		diffTxt.Format(_T(" %.2f%% "), diffPercent);

	// MACD趋势信号标签
	auto segs = CSignalAnalyzer::CalcTrendSegments(realtimeData);
	CString macdTxt;
	for (const auto& seg : segs) macdTxt += seg.text;

	// 文字可用区域：左侧从按钮区之后开始，右侧留出关闭按钮宽度
	const int rightMargin = g_data.RDPI(24);
	if (textMaxWidth <= 0)
		textMaxWidth = max(0, windowWidth - leftStart - rightMargin);

	// 逐级缩小字体直到总宽度放得下（最低到50%），保证标题不与右侧按钮重叠
	CSize prefixSize(0, 0), currentSize(0, 0), diffSize(0, 0), macdSize(0, 0);
	int totalWidth = 0;
	CFont* pOldFont = nullptr;
	CFont shrinkFont;
	for (int percent = 100; percent >= 50; percent -= 5)
	{
		if (percent < 100)
		{
			// 基于当前DC字体创建缩放字体
			TEXTMETRIC tm;
			memDC.GetTextMetrics(&tm);
			LOGFONT lf;
			CFont* curFont = memDC.GetCurrentFont();
			if (curFont)
				curFont->GetLogFont(&lf);
			else
				memset(&lf, 0, sizeof(lf));
			lf.lfHeight = -(max(8, tm.tmHeight * percent / 100));
			shrinkFont.DeleteObject();
			shrinkFont.CreateFontIndirect(&lf);
			pOldFont = memDC.SelectObject(&shrinkFont);
		}
		prefixSize = memDC.GetTextExtent(prefixTxt);
		currentSize = memDC.GetTextExtent(currentTxt);
		diffSize = memDC.GetTextExtent(diffTxt);
		macdSize = memDC.GetTextExtent(macdTxt);
		totalWidth = prefixSize.cx + currentSize.cx + diffSize.cx + macdSize.cx;
		if (totalWidth <= textMaxWidth)
			break;
	}

	// 标题文字从最左侧（leftStart）开始显示，过长时通过缩小字体容纳
	int startX = leftStart;
	int centerY = headerHeight / 2;

	memDC.SetTextColor(COLOR_GRAY_TEXT);
	memDC.TextOut(startX, centerY - prefixSize.cy / 2, prefixTxt);

	int curX = startX + prefixSize.cx;
	memDC.SetTextColor(diffColor);
	memDC.TextOut(curX, centerY - currentSize.cy / 2, currentTxt);

	curX += currentSize.cx;
	memDC.TextOut(curX, centerY - diffSize.cy / 2, diffTxt);

	// 绘制MACD趋势信号
	int dwrawY = centerY - macdSize.cy / 2;
	if (!macdTxt.IsEmpty())
	{
		curX += diffSize.cx;
		for (const auto& seg : segs)
		{
			memDC.SetTextColor(seg.color);
			memDC.TextOut(curX, dwrawY, seg.text);
			curX += memDC.GetTextExtent(seg.text).cx;
		}
	}

	// 恢复原字体，避免缩放字体泄漏到后续绘制
	if (pOldFont)
		memDC.SelectObject(pOldFont);
}

void CStatusBarPanel::DrawRelatedStatusBar(CDC& memDC, int w, int topBarY, int singleBarHeight, const std::wstring& stockId, int viewMode)
{
	const int GAP = 2;

	std::vector<std::wstring> relatedCodes = g_data.GetRelatedStockCodes(stockId);
	bool isRelatedMode = !relatedCodes.empty();
	if (relatedCodes.empty())
	{
		// 没有关联股票时使用默认指数：上证指数、中证银行、恒生科技
		relatedCodes = { L"sh000001", L"sz399986", L"rt_hkHSTECH" };
	}
	const int relatedCount = static_cast<int>(relatedCodes.size());

	// 从已计算好的均幅数据中获取（由UpdateRelatedStocksAvgDiff在行情更新时计算）
	bool showAvgDiff = isRelatedMode && relatedCount >= 1;
	double avgDiffPercent = 0.0;
	double minAvgDiff = 0.0;
	double maxAvgDiff = 0.0;
	CString minAvgValueStr, avgValueStr, maxAvgValueStr, trendArrowStr;
	if (showAvgDiff)
	{
		auto avgData = g_data.GetAvgDiffData(stockId);
		minAvgDiff = avgData.minVal;
		maxAvgDiff = avgData.maxVal;
		avgDiffPercent = avgData.currentVal;
		// 均幅数据全为0说明尚未计算，不显示均幅区域
		if (minAvgDiff == 0.0 && maxAvgDiff == 0.0 && avgDiffPercent == 0.0)
			showAvgDiff = false;
	}
	if (showAvgDiff)
	{
		minAvgValueStr = CCommon::prefixFormat(minAvgDiff);
		avgValueStr = CCommon::prefixFormat(avgDiffPercent);
		maxAvgValueStr = CCommon::prefixFormat(maxAvgDiff);

		// 计算趋势箭头：分时界面用1分钟趋势，5分钟界面用5分钟趋势
		RegResult trend = (viewMode < UI_VIEW_MIN5_KLINE)
			? g_data.Get1MinAvgTrend(stockId)
			: g_data.Get5MinAvgTrend(stockId);
		trendArrowStr = _T("|"); // 默认竖线
		if (trend.valid)
		{
			if (trend.r2 >= 0.1 || std::abs(trend.slope) >= 0.001)
			{
				// 箭头强度：低1个、中2个、高3个
				int arrowCount = trend.getArrawCount();

				if (trend.slope > 0)
					trendArrowStr = CString(_T('↑'), arrowCount);
				else
					trendArrowStr = CString(_T('↓'), arrowCount);
			}
		}
	}

	// 动态计算字体大小：先测量实际文字宽度，再逐步缩小直到适合
	CFont* pOldFont = nullptr;
	CFont dynFont;
	CFont avgFont; // 右侧均幅区域固定字体
	{
		const int minFont = 8;
		const int maxFont = 14;

		// 右侧均幅区域始终使用最大字号
		if (showAvgDiff)
		{
			avgFont.CreateFont(-g_data.RDPI(maxFont), 0, 0, 0, FW_NORMAL, 0, 0, 0,
				DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
				DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, _T("微软雅黑"));
		}

		if (!isRelatedMode)
		{
			// 非关联模式直接用最大字号
			dynFont.CreateFont(-g_data.RDPI(maxFont), 0, 0, 0, FW_NORMAL, 0, 0, 0,
				DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
				DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, _T("微软雅黑"));
			pOldFont = memDC.SelectObject(&dynFont);
		}
		else
		{
			// 关联模式：用实际测量法，从最大字号开始尝试，逐步缩小直到文字总宽度适合
			// 右侧预留盘口宽度(RDPI168)不参与均分（均幅区域已在其范围内），仅左侧区域用于字体计算
			// 趋势箭头在均幅区域左侧，也需要预留空间
			const int orderBookWidthMeasure = g_data.RDPI(168);
			int trendArrowWidth = 0;
			if (showAvgDiff && !trendArrowStr.IsEmpty())
			{
				// 用最大字号测量趋势箭头宽度（趋势箭头使用avgFont固定字号）
				CFont tmpAvgFont;
				tmpAvgFont.CreateFont(-g_data.RDPI(maxFont), 0, 0, 0, FW_NORMAL, 0, 0, 0,
					DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
					DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, _T("微软雅黑"));
				CFont* pOldTmpFont = memDC.SelectObject(&tmpAvgFont);
				trendArrowWidth = memDC.GetTextExtent(trendArrowStr).cx + GAP;
				memDC.SelectObject(pOldTmpFont);
				tmpAvgFont.DeleteObject();
			}
			int availableWidth = w - orderBookWidthMeasure - trendArrowWidth;

			std::lock_guard<std::mutex> lockMeasure(Stock::Instance().m_stockDataMutex);

			// 预先构建所有文本字符串
			struct StockText { CString nameStr; CString changeStr; bool valid; };
			std::vector<StockText> stockTexts(relatedCount);
			for (int i = 0; i < relatedCount; i++)
			{
				auto stockData = g_data.GetStockData(relatedCodes[i]);
				if (stockData && stockData->info.is_ok)
				{
					const auto& info = stockData->info;
					double diffPercent = info.GetChangePercent();
					stockTexts[i].nameStr = info.GetStockListName() + _T(": ");
					stockTexts[i].changeStr = CCommon::prefixFormat(diffPercent);
					stockTexts[i].valid = true;
				}
				else
				{
					stockTexts[i].nameStr = CString(relatedCodes[i].c_str()) + _T(" --");
					stockTexts[i].valid = false;
				}
			}

			int fontSize = maxFont;

			while (fontSize >= minFont)
			{
				// 创建临时字体测量
				CFont tmpFont;
				tmpFont.CreateFont(-g_data.RDPI(fontSize), 0, 0, 0, FW_NORMAL, 0, 0, 0,
					DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
					DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, _T("微软雅黑"));
				CFont* pOldTmp = memDC.SelectObject(&tmpFont);

				// 计算所有股票文字总宽度（两行布局：上行名称+趋势箭头，下行涨跌幅，每只股票占 max(两行) 宽度）
				int totalWidth = GAP; // 左侧起始边距
				CString trendArrowTxt = _T("↑"); // 名称后可能有一个趋势箭头，测量按包含箭头计
				for (int i = 0; i < relatedCount; i++)
				{
					if (stockTexts[i].valid)
					{
						int nameW = memDC.GetTextExtent(stockTexts[i].nameStr).cx + GAP
							+ memDC.GetTextExtent(trendArrowTxt).cx;
						int changeW = memDC.GetTextExtent(stockTexts[i].changeStr).cx;
						totalWidth += max(nameW, changeW) + GAP;
					}
					else
					{
						totalWidth += memDC.GetTextExtent(stockTexts[i].nameStr).cx + GAP;
					}
				}

				memDC.SelectObject(pOldTmp);
				tmpFont.DeleteObject();

				// 检查是否适合左侧可用区域（去掉预留128像素）
				if (totalWidth <= availableWidth)
					break; // 当前字号适合

				fontSize--;
			}
			if (fontSize < minFont) fontSize = minFont;

			dynFont.CreateFont(-g_data.RDPI(fontSize), 0, 0, 0, FW_NORMAL, 0, 0, 0,
				DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
				DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, _T("微软雅黑"));
			pOldFont = memDC.SelectObject(&dynFont);
		}
	}

	std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
	if (isRelatedMode)
	{
		// 关联模式：左侧区域每只股票等分一列，单行显示（涨跌幅覆盖绘制在浅灰名称上面）
		// 左侧可用区域：窗口总宽度减去右侧盘口宽度(ORDER_BOOK_WIDTH)
		// 均幅区域绘制在 w-RDPI(128) 处，本身已落在盘口宽度(RDPI168)范围内，无需再单独扣除
		// ORDER_BOOK_WIDTH 为 RDPI(168)，此处避免依赖 FloatingWnd，同口径按 RDPI(168) 计算
		const int orderBookW = g_data.RDPI(168);
		int arrowZoneW = 0;
		if (showAvgDiff && !trendArrowStr.IsEmpty())
			arrowZoneW = GAP + memDC.GetTextExtent(trendArrowStr).cx; // 趋势箭头绘制在均幅区域左侧
		int availW = w - orderBookW - arrowZoneW;
		const int colWidth = availW / max(relatedCount, 1);
		for (int i = 0; i < relatedCount; i++)
		{
			auto stockData = g_data.GetStockData(relatedCodes[i]);
			int colX = i * colWidth; // 等分列宽，占满左侧可用区域
			int textX = colX + GAP;

			if (stockData && stockData->info.is_ok)
			{
				const auto& info = stockData->info;
				double diffPercent = info.GetChangePercent();
				CString nameStr = info.GetStockListName();

				// 趋势箭头追加到涨跌幅后面
				auto trend = stockData->CalTimeLineTrend();
				bool hasTrend = trend.valid && trend.slope != 0.0;
				COLORREF trendColor = COLOR_BLACK;
				CString strTrend;
				if (hasTrend)
				{
					strTrend = trend.slope > 0 ? _T("↑") : _T("↓");
					trendColor = trend.slope > 0 ? COLOR_RED_UP : COLOR_GREEN_DOWN;
				}

				// 单行显示：名称和涨跌幅共用同一位置，涨跌幅覆盖绘制在名称上面
				// 名称用浅灰色作背景衬托，涨跌幅用红/绿色覆盖，箭头跟在涨跌幅后面
				int textY = topBarY + g_data.RDPI(2);
				//memDC.SetTextColor(COLOR_GRAY_GRID); // 浅灰色名称
				//memDC.TextOut(textX, textY, nameStr);

				memDC.SetTextColor(CCommon::GetProfitLossColor(diffPercent));
				CString changeStr = CCommon::prefixFormat(diffPercent);
				memDC.TextOut(textX, textY, changeStr);
				int changeW = memDC.GetTextExtent(changeStr).cx;
				if (hasTrend)
				{
					memDC.SetTextColor(trendColor);
					memDC.TextOut(textX + changeW + GAP, textY, strTrend);
				}
			}
			else
			{
				CString nameStr = CString(relatedCodes[i].c_str()) + _T(" --");
				memDC.SetTextColor(COLOR_GRAY_PURPLE);
				memDC.TextOut(textX, topBarY + g_data.RDPI(2), nameStr);
			}
		}

		// 右侧预留128像素显示：最小值 均值 最大值（固定字体大小，红绿背景）
		if (showAvgDiff)
		{
			int avgAreaWidth = g_data.RDPI(128);
			int avgAreaX = w - avgAreaWidth - GAP;
			int avgAreaH = singleBarHeight;
			int avgAreaY = topBarY;

			// 红绿颜色深度由均值在区间中的位置决定
			double range = maxAvgDiff - minAvgDiff;
			int redIdx, greenIdx;
			if (range == 0)
			{
				redIdx = 0;
				greenIdx = 2;
			}
			else
			{
				double posRatio = (avgDiffPercent - minAvgDiff) / range;
				posRatio = max(0.0, min(1.0, posRatio));
				redIdx = static_cast<int>(posRatio * 2 + 0.5);
				greenIdx = 2 - redIdx;
				redIdx = max(0, min(2, redIdx));
				greenIdx = max(0, min(2, greenIdx));
			}
			COLORREF redColor = AVG_RED_COLORS[redIdx];
			COLORREF greenColor = AVG_GREEN_COLORS[greenIdx];

			// 绘制红绿背景
			if (avgDiffPercent <= minAvgDiff)
			{
				memDC.FillSolidRect(avgAreaX, avgAreaY, avgAreaWidth, avgAreaH, greenColor);
			}
			else if (avgDiffPercent >= maxAvgDiff)
			{
				memDC.FillSolidRect(avgAreaX, avgAreaY, avgAreaWidth, avgAreaH, redColor);
			}
			else
			{
				double ratio = (avgDiffPercent - minAvgDiff) / range;
				int redWidth = static_cast<int>(ratio * avgAreaWidth);
				redWidth = max(0, min(redWidth, avgAreaWidth));
				int greenWidth = avgAreaWidth - redWidth;

				memDC.FillSolidRect(avgAreaX, avgAreaY, redWidth, avgAreaH, redColor);
				memDC.FillSolidRect(avgAreaX + redWidth, avgAreaY, greenWidth, avgAreaH, greenColor);
			}

			// 切换到固定字体绘制均幅区域
			CFont* pPrevFont = memDC.SelectObject(&avgFont);

			// 128像素区域三等分
			int thirdWidth = avgAreaWidth / 3;

			// 趋势指示器（128像素左侧，间隔2像素，无背景）
			if (!trendArrowStr.IsEmpty())
			{
				TCHAR firstChar = trendArrowStr.GetAt(0);
				if (firstChar == _T('↑'))
					memDC.SetTextColor(COLOR_RED_UP);
				else if (firstChar == _T('↓'))
					memDC.SetTextColor(COLOR_GREEN_DOWN);
				else
					memDC.SetTextColor(RGB(0, 0, 0));
				int trendX = avgAreaX - GAP - memDC.GetTextExtent(trendArrowStr).cx;
				memDC.TextOut(trendX, avgAreaY + g_data.RDPI(2), trendArrowStr);
			}

			// 最小值（白色文字）
			memDC.SetTextColor(RGB(255, 255, 255));
			int minX = avgAreaX + (thirdWidth - memDC.GetTextExtent(minAvgValueStr).cx) / 2;
			memDC.TextOut(minX, avgAreaY + g_data.RDPI(2), minAvgValueStr);

			// 均值（白色文字）
			int avgX = avgAreaX + thirdWidth + (thirdWidth - memDC.GetTextExtent(avgValueStr).cx) / 2;
			memDC.TextOut(avgX, avgAreaY + g_data.RDPI(2), avgValueStr);

			// 最大值（白色文字）
			int maxX = avgAreaX + thirdWidth * 2 + (thirdWidth - memDC.GetTextExtent(maxAvgValueStr).cx) / 2;
			memDC.TextOut(maxX, avgAreaY + g_data.RDPI(2), maxAvgValueStr);

			// 恢复之前的字体
			memDC.SelectObject(pPrevFont);
		}
	}
	else
	{
		// 非关联模式（默认指数）：等分列宽布局
		const int colWidth = w / max(relatedCount, 1);
		for (int i = 0; i < relatedCount; i++)
		{
			auto stockData = g_data.GetStockData(relatedCodes[i]);
			int colX = i * colWidth;
			int textX = colX + GAP;

			if (stockData && stockData->info.is_ok)
			{
				const auto& info = stockData->info;
				double diffPercent = info.GetPriceChange();

				CString nameStr = info.GetStockListName();
				CString priceStr;
				priceStr.Format(_T("%.2f"), info.currentPrice);
				CString changeStr = CCommon::prefixFormat(diffPercent, _T("%"));

				memDC.SetTextColor(COLOR_BLACK);
				memDC.TextOut(textX, topBarY + g_data.RDPI(2), nameStr);
				textX += memDC.GetTextExtent(nameStr).cx + GAP;

				memDC.SetTextColor(CCommon::GetProfitLossColor(diffPercent));
				memDC.TextOut(textX, topBarY + g_data.RDPI(2), priceStr);
				textX += memDC.GetTextExtent(priceStr).cx + GAP;

				memDC.SetTextColor(CCommon::GetProfitLossColor(diffPercent));
				memDC.TextOut(textX, topBarY + g_data.RDPI(2), changeStr);
			}
			else
			{
				CString nameStr = CString(relatedCodes[i].c_str());
				memDC.SetTextColor(COLOR_GRAY_PURPLE);
				memDC.TextOut(textX, topBarY + g_data.RDPI(2), nameStr + _T(" --"));
			}
		}
	}

	// 恢复原始字体
	if (pOldFont != nullptr)
	{
		memDC.SelectObject(pOldFont);
		dynFont.DeleteObject();
	}
}

void CStatusBarPanel::DrawSystemStatusBar(CDC& memDC, int w, int bottomBarY, int singleBarHeight)
{
	const int GAP = 2;

	std::vector<std::wstring> statusBarCodes = g_data.GetStatusBarStockCodes();
	if (statusBarCodes.empty())
	{
		// 没有配置时使用默认指数：上证指数、中证银行、恒生科技
		statusBarCodes = { L"sh000001", L"sz399986", L"rt_hkHSTECH" };
	}
	const int sbCount = static_cast<int>(statusBarCodes.size());
	// 下行状态栏使用等分列宽布局
	const int colWidth = w / max(sbCount, 1);

	std::lock_guard<std::mutex> lock(Stock::Instance().m_stockDataMutex);
	for (int i = 0; i < sbCount; i++)
	{
		auto stockData = g_data.GetStockData(statusBarCodes[i]);
		int colX = i * colWidth;
		int textX = colX + GAP;

		if (stockData && stockData->info.is_ok)
		{
			const auto& info = stockData->info;
			double diffPercent = info.GetPriceChange();
			CString nameStr = info.GetStockListName();
			CString priceStr;
			priceStr.Format(_T("%.2f"), info.currentPrice);
			CString changeStr = CCommon::prefixFormat(diffPercent, _T("%"));

			memDC.SetTextColor(COLOR_BLACK);
			memDC.TextOut(textX, bottomBarY + g_data.RDPI(2), nameStr);
			textX += memDC.GetTextExtent(nameStr).cx + GAP;

			memDC.SetTextColor(CCommon::GetProfitLossColor(diffPercent));
			memDC.TextOut(textX, bottomBarY + g_data.RDPI(2), priceStr);
			textX += memDC.GetTextExtent(priceStr).cx + GAP;

			memDC.SetTextColor(CCommon::GetProfitLossColor(diffPercent));
			memDC.TextOut(textX, bottomBarY + g_data.RDPI(2), changeStr);
		}
		else
		{
			CString nameStr = CString(statusBarCodes[i].c_str());
			memDC.SetTextColor(COLOR_GRAY_PURPLE);
			memDC.TextOut(textX, bottomBarY + g_data.RDPI(2), nameStr + _T(" --"));
		}
	}
}