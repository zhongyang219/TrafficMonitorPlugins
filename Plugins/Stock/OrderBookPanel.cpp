#include "pch.h"
#include "OrderBookPanel.h"
#include "ChartColors.h"
#include "Common.h"
#include "DataManager.h"
#include <algorithm>
#include <limits>
#include <cmath>

// 成交量颜色（按实际成交额档位分色）
const COLORREF VOL_COL_NORMAL_BID = RGB(255, 13, 0);  // 买方正常单·浅红 216, 68, 68
const COLORREF VOL_COL_BIG_BID = RGB(255, 0, 255);    // 买方大单·深红
const COLORREF VOL_COL_HUGE_BID = RGB(102, 0, 102);   // 买方超大单·紫

const COLORREF VOL_COL_NORMAL_ASK = RGB(47, 158, 68);  // 卖方正常单·浅绿
const COLORREF VOL_COL_BIG_ASK = RGB(0, 230, 0);    // 卖方大单·深绿
const COLORREF VOL_COL_HUGE_ASK = RGB(10, 80, 55);    // 卖方超大单·墨绿

// 静态成员定义
const COLORREF COrderBookPanel::NET_RATIO_RED_COLORS[] = {
	RGB(240, 40, 40),   // 0-30
	RGB(180, 50, 50),   // 30-60
	RGB(130, 20, 40)    // 60以上
};
const COLORREF COrderBookPanel::NET_RATIO_GREEN_COLORS[] = {
	RGB(40, 240, 40),  // 0~30 浅亮绿（弱多）
	RGB(50, 180, 50),  // 30~60 中草绿（中多）
	RGB(20, 130, 40)   // 60以上 深墨绿（强多）
};

// ============================================================================
// 纯渲染层主入口：只读 MarketOrderBook 缓存数据，不做数据查询/计算
// ============================================================================

void COrderBookPanel::Draw(CDC& memDC, int left, int right, int height, const MarketOrderBook& book,
	const std::vector<STOCK::KLinePoint>& klineData,
	UIViewMode viewMode)
{
	// 精简版布局（23行）：
	// 0=最高/最低, 1-5=卖五~卖一, (选线分割不占行，在卖一与买一之间), 6-10=买一~买五,
	// 11=累计净流入, 12-21=10行成交明细, 22=区间净流入
	const int totalRows = 23;
	const int headerHeight = g_data.RDPI(26) + g_data.RDPI(20);
	const int obTitleH = g_data.RDPI(16);
	const int topOffset = headerHeight + obTitleH;
	const int panelW = right - left;
	memDC.FillSolidRect(left, headerHeight, panelW, obTitleH, RGB(245, 245, 245));
	const int rowHeight = (height - obTitleH) / totalRows;
	const int contentH = height - obTitleH;
	const int rem = contentH % totalRows;
	const int textX = left + g_data.RDPI(5) + 3;

	memDC.FillSolidRect(left, topOffset, panelW, contentH, RGB(250, 250, 250));
	memDC.SetBkMode(TRANSPARENT);

	LayoutContext lc;
	lc.left = left;
	lc.right = right;
	lc.height = height;
	lc.headerHeight = headerHeight;
	lc.obTitleH = obTitleH;
	lc.topOffset = topOffset;
	lc.panelW = panelW;
	lc.totalRows = totalRows;
	lc.rowHeight = rowHeight;
	lc.contentH = contentH;
	lc.rem = rem;
	lc.textX = textX;
	// 缓存布局供双击命中测试复用（与绘制共用同一套RowY/RowH公式）
	m_layout = lc;
	m_layoutValid = true;

	// 行较密，整体使用紧凑字体，避免S5-B5/明细/净流入各行文字相互挤压
	CFont* oldDrawFont = memDC.GetCurrentFont();
	LOGFONT dlf;
	oldDrawFont->GetLogFont(&dlf);
	{
		int absH = abs(dlf.lfHeight);
		int idealH = max(8, static_cast<int>(rowHeight * 0.82));
		idealH = min(absH, idealH);
		dlf.lfHeight = (dlf.lfHeight < 0) ? -idealH : idealH;
	}
	CFont drawCompactFont;
	drawCompactFont.CreateFontIndirect(&dlf);
	memDC.SelectObject(&drawCompactFont);

	// 数据模型未就绪时仅绘制背景
	if (!book.IsValid())
	{
		memDC.SelectObject(oldDrawFont);
		(void)viewMode; (void)klineData;
		return;
	}

	// 0: 最高/最低（直接使用模型预格式化文本）
	DrawHighLow(memDC, lc, book);
	STOCK::Volume maxVol = 0;
	std::vector<OrderBookRow> askRows;
	std::vector<OrderBookRow> bidRows;
	// 1-5: 卖五~卖一
	{
		askRows.reserve(5);
		for (int idx = 4; idx >= 0; --idx)
		{
			STOCK::Price price = book.GetAsk(idx).price;
			// 仅卖一(idx==0)显示后方累计主动成交量的瞬时变化量（模型缓存值，直接读取）
			long long delta = (idx == 0) ? book.GetOrderDeltaLots(price, true) : 0;
			auto row = BuildAskRow(book, idx, delta);
			if (row.totalVol > maxVol) maxVol = row.totalVol;
			askRows.push_back(row);
		}
	}

	// 6-10: 买一~买五
	{
		bidRows.reserve(5);
		for (int i = 0; i < 5; i++)
		{
			STOCK::Price price = book.GetBid(i).price;
			// 仅买一(idx==0)显示后方累计主动成交量的瞬时变化量（模型缓存值，直接读取）
			long long delta = (i == 0) ? book.GetOrderDeltaLots(price, false) : 0;
			auto row = BuildBidRow(book, i, delta);
			if (row.totalVol > maxVol) maxVol = row.totalVol;
			bidRows.push_back(row);
		}
	}

	DrawPriceRows(memDC, lc, askRows, 1, maxVol);
	DrawPriceRows(memDC, lc, bidRows, 6, maxVol);

	// 选线分割（净比00，画在卖一与买一之间，不单独占行）
	// 在买卖行绘制之后再画，避免被买一/卖一背景色盖住；2px线以两行边界为中线（上下各1px）
	DrawNetRatio00Ex(memDC, lc, book, lc.RowY(6) - 1);

	// 11: 累计净流入（整日）——直接读模型缓存值
	DrawNetInflowRow(memDC, lc, 11, L"累计净流入", book.GetCumNetInflow());

	// 12-21: 10行成交明细（最新在下）
	DrawTickMini(memDC, lc, 12, book);

	// 22: 区间净流入（当前10条明细）——直接读模型缓存值
	DrawNetInflowRow(memDC, lc, 22, L"区间净流入", book.GetIntervalNetInflow());

	memDC.SelectObject(oldDrawFont);

	// viewMode/klineData 仅用于旧完整版，精简版不再使用，避免未使用告警
	(void)viewMode; (void)klineData;
}

// ============================================================================
// 精简版盘口（新版Draw）子项实现（全部只读渲染）
// ============================================================================

void COrderBookPanel::DrawHighLow(CDC& memDC, const LayoutContext& lc, const MarketOrderBook& book)
{
	int rowY = lc.RowY(0);
	int rowH = lc.RowH(0);

	// 使用小字体
	CFont* oldFont = memDC.GetCurrentFont();
	LOGFONT lf;
	oldFont->GetLogFont(&lf);
	CFont smallFont;
	smallFont.CreateFontIndirect(&lf);
	memDC.SelectObject(&smallFont);

	// 使用模型预格式化的文本，不做计算
	memDC.FillSolidRect(lc.left, rowY, lc.panelW, rowH, RGB(220, 235, 250));
	int textY = rowY + max(0, (rowH - memDC.GetTextExtent(book.GetHighText()).cy) / 2);
	memDC.SetTextColor(RGB(128, 0, 128));
	memDC.TextOut(lc.textX, textY, book.GetHighText());
	int lowW = memDC.GetTextExtent(book.GetLowText()).cx;
	int lowX = lc.left + lc.panelW - lowW;
	memDC.SetTextColor(RGB(0, 100, 0));
	memDC.TextOut(lowX, textY, book.GetLowText());

	memDC.SelectObject(oldFont);
}

void COrderBookPanel::DrawNetRatio00Ex(CDC& memDC, const LayoutContext& lc, const MarketOrderBook& book, int barY)
{
	if (!book.HasNetRatio())
		return;
	double ratio = book.GetNetRatio();

	int barX = lc.textX + 29;
	int barW = lc.right - barX;
	if (barW <= 0)
		return;

	int midX = barX + barW / 2;
	int halfW = barW / 2;
	int fillW = static_cast<int>(std::sqrt(std::abs(ratio) / 100.0) * halfW);
	fillW = min(fillW, halfW);
	int dominantW = min(barW, halfW + fillW);

	COLORREF redColor = NET_RATIO_RED_COLORS[GetNetRatioColorIndex(ratio)];
	COLORREF greenColor = NET_RATIO_GREEN_COLORS[GetNetRatioColorIndex(ratio)];

	if (ratio > 0)
	{
		memDC.FillSolidRect(barX, barY, dominantW, 2, redColor);
		memDC.FillSolidRect(barX + dominantW, barY, barW - dominantW, 2, greenColor);
	}
	else if (ratio < 0)
	{
		memDC.FillSolidRect(barX, barY, dominantW, 2, greenColor);
		memDC.FillSolidRect(barX + dominantW, barY, barW - dominantW, 2, redColor);
	}
	else
	{
		memDC.FillSolidRect(barX, barY, barW, 2, RGB(230, 230, 230));
	}
	memDC.FillSolidRect(midX - 1, barY, 2, 2, RGB(255, 255, 255));
}

void COrderBookPanel::DrawNetInflowRow(CDC& memDC, const LayoutContext& lc, int rowIndex, const CString& label, double netInflow)
{
	int y = lc.RowY(rowIndex);
	int h = lc.RowH(rowIndex);

	// 行顶部分隔线
	CPen sepPen(PS_SOLID, 1, RGB(200, 200, 200));
	CPen* oldPen = memDC.GetCurrentPen();
	memDC.SelectObject(&sepPen);
	memDC.MoveTo(lc.left, y);
	memDC.LineTo(lc.right, y);
	memDC.SelectObject(oldPen);

	bool isPos = (netInflow >= 0);
	CString valTxt;
	valTxt.Format(L"%s%.2f万", isPos ? L"+" : L"-", std::abs(netInflow / 10000.0));

	memDC.SetTextColor(isPos ? COLOR_RED_UP : COLOR_GREEN_DOWN);
	int textY = y + max(0, (h - memDC.GetTextExtent(L"00").cy) / 2);
	memDC.TextOut(lc.textX, textY, label);
	int valW = memDC.GetTextExtent(valTxt).cx;
	memDC.TextOut(lc.right - valW - g_data.RDPI(2), textY, valTxt);
}

void COrderBookPanel::DrawTickMini(CDC& memDC, const LayoutContext& lc, int startRow, const MarketOrderBook& book)
{
	// 直接读模型的明细缓存（最新在前，共20条），盘口面板只用前10条
	const auto& lines = book.GetTransLines();
	const int showCount = min(10, static_cast<int>(lines.size()));
	if (showCount <= 0)
		return;

	CPen* oldPen = memDC.GetCurrentPen();

	// 最新在下：从最旧往上画，最新落在最后一行
	for (int i = showCount - 1; i >= 0; --i)
	{
		const auto& t = lines[i];
		int rowIdx = startRow + (showCount - 1 - i);
		int y = lc.RowY(rowIdx);
		int h = lc.RowH(rowIdx);

		bool isSell = (t.buyOrSell == 1);
		COLORREF lineColor = isSell ? COLOR_GREEN_DOWN : COLOR_RED_UP;

		CString priceStr;
		priceStr.Format(L"%.3f", t.price);
		CString timeStr(t.timeKey.c_str());
		CString volStr;
		volStr.Format(L"%lld", static_cast<long long>(t.vol));
		const wchar_t* dir = isSell ? L"S" : L"B";

		// 行间分隔线
		if (rowIdx > startRow)
		{
			CPen sepPen(PS_SOLID, 1, RGB(240, 240, 240));
			memDC.SelectObject(&sepPen);
			memDC.MoveTo(lc.left, y);
			memDC.LineTo(lc.right, y);
			memDC.SelectObject(oldPen);
		}

		int textY = y + max(0, (h - memDC.GetTextExtent(L"00").cy) / 2);

		// 左侧：时间 价格（中间空一格）
		CString leftTxt = timeStr + L"    " + priceStr;
		memDC.SetTextColor(lineColor);
		memDC.TextOut(lc.textX, textY, leftTxt);

		// 成交量：右对齐，紧挨 S/B；单独按成交额档位配色
		int volW = memDC.GetTextExtent(volStr).cx;
		int dirW = memDC.GetTextExtent(dir).cx;
		int sbX = lc.right - dirW - g_data.RDPI(2);
		int volX = sbX - volW - g_data.RDPI(12);
		volX = max(volX, lc.textX + memDC.GetTextExtent(leftTxt).cx + g_data.RDPI(8));

		double turnover = t.price * static_cast<double>(t.vol) * 100.0;
		COLORREF volColor;
		if (turnover >= 500000.0) {          // 超大单 ≥50万
			volColor = isSell ? VOL_COL_HUGE_ASK : VOL_COL_HUGE_BID;
			memDC.SetTextColor(RGB(160, 160, 160));
			memDC.TextOut(volX + 1, textY + 1, volStr);
		}
		else if (turnover >= 200000.0)     // 大单 20万~50万
			volColor = isSell ? VOL_COL_BIG_ASK : VOL_COL_BIG_BID;
		else                               // 正常单 <20万
			volColor = isSell ? VOL_COL_NORMAL_ASK : VOL_COL_NORMAL_BID;
		memDC.SetTextColor(volColor);
		memDC.TextOut(volX, textY, volStr);

		// 最右：S/B
		memDC.SetTextColor(lineColor);
		memDC.TextOut(sbX, textY, dir);
	}

	memDC.SelectObject(oldPen);
}

// ============================================================================
// 成交明细界面（MX模式）：显示最近20条成交（只读渲染层）
// ============================================================================
void COrderBookPanel::DrawTickDetail(CDC& memDC, int left, int right, int height, const MarketOrderBook& book)
{
	// MX模式下盘口未绘制，盘口双击命中测试失效
	m_layoutValid = false;

	// 内容区域（盘口标题栏下方）
	int headerHeight = g_data.RDPI(26) + g_data.RDPI(20);
	int obTitleH = g_data.RDPI(16);
	int topOffset = headerHeight + obTitleH;
	int contentH = height - obTitleH;
	if (contentH <= 0)
		return;
	int textX = left + g_data.RDPI(5) + 3;

	// 绘制盘口标题栏背景，避免残留其它模式内容
	memDC.FillSolidRect(left, headerHeight, right - left, obTitleH, RGB(245, 245, 245));

	// 复用模型同一份20条明细缓存，MX模式全量显示
	const auto& lines = book.GetTransLines();
	const int showCount = min(20, static_cast<int>(lines.size()));
	if (showCount <= 0)
		return;

	// 明细区从第二行开始，顶部汇总行：整日累计净流入（直接读模型缓存值）
	const int rowH = max(1, contentH / 22);
	CPen* oldPen = memDC.GetCurrentPen();

	CFont* oldTickFont = memDC.GetCurrentFont();
	LOGFONT lf;
	oldTickFont->GetLogFont(&lf);
	{
		int absH = abs(lf.lfHeight);
		int idealH = max(8, static_cast<int>(rowH * 0.85));
		idealH = min(absH, idealH);
		lf.lfHeight = (lf.lfHeight < 0) ? -idealH : idealH;
	}
	CFont tickSmallFont;
	tickSmallFont.CreateFontIndirect(&lf);
	memDC.SelectObject(&tickSmallFont);

	// 顶部汇总行
	{
		double cumNetInflow = book.GetCumNetInflow();   // 直接读模型缓存
		bool isPos = (cumNetInflow >= 0);
		double wanVal = cumNetInflow / 10000.0;
		CString labelTxt = L"累计净流入:";
		CString valTxt;
		valTxt.Format(L"%s%.2f万", isPos ? L"+" : L"-", std::abs(wanVal));

		memDC.SetTextColor(isPos ? COLOR_RED_UP : COLOR_GREEN_DOWN);
		int textY = topOffset + max(0, (rowH - memDC.GetTextExtent(L"00").cy) / 2);
		memDC.TextOut(textX, textY, labelTxt);
		// 金额右对齐到面板右缘
		int valW = memDC.GetTextExtent(valTxt).cx;
		memDC.TextOut(right - valW - g_data.RDPI(2), textY, valTxt);

		// 与明细区之间画分隔线
		CPen sepPen(PS_SOLID, 1, RGB(200, 200, 200));
		memDC.SelectObject(&sepPen);
		int sepY = topOffset + rowH - 1;
		memDC.MoveTo(left, sepY);
		memDC.LineTo(right, sepY);
		memDC.SelectObject(oldPen);
	}

	// 明细区从第二行开始
	int rowY = topOffset + rowH;
	// 缓存按最新在前，从最旧往上画，最新时间显示在最下方一行
	for (int i = showCount - 1; i >= 0; --i)
	{
		const auto& t = lines[i];
		const wchar_t* dir = (t.buyOrSell == 1) ? L"S" : L"B";
		CString priceStr;
		priceStr.Format(L"%.3f", t.price);
		CString timeStr(t.timeKey.c_str());
		CString volStr;
		volStr.Format(L"%lld", static_cast<long long>(t.vol));

		bool isSell = (t.buyOrSell == 1);
		COLORREF lineColor = isSell ? COLOR_GREEN_DOWN : COLOR_RED_UP;

		double turnover = t.price * static_cast<double>(t.vol) * 100.0;
		COLORREF volColor;
		if (turnover >= 500000.0) {          // 超大单 ≥50万
			volColor = isSell ? VOL_COL_HUGE_ASK : VOL_COL_HUGE_BID;
		}
		else if (turnover >= 200000.0)     // 大单 20万~50万
			volColor = isSell ? VOL_COL_BIG_ASK : VOL_COL_BIG_BID;
		else                               // 正常单 <20万
			volColor = isSell ? VOL_COL_NORMAL_ASK : VOL_COL_NORMAL_BID;

		// 各行底部画分隔线
		if (i < showCount - 1)
		{
			CPen sepPen(PS_SOLID, 1, RGB(230, 230, 230));
			memDC.SelectObject(&sepPen);
			memDC.MoveTo(left, rowY);
			memDC.LineTo(right, rowY);
			memDC.SelectObject(oldPen);
		}

		int textY = rowY + max(0, (rowH - memDC.GetTextExtent(L"00").cy) / 2);

		// 左侧：时间 价格（中间空4格）
		CString leftTxt;
		leftTxt = timeStr + L"    " + priceStr;
		memDC.SetTextColor(lineColor);
		memDC.TextOut(textX, textY, leftTxt);

		// 成交量：右对齐，紧挨 S/B 左侧
		int volW = memDC.GetTextExtent(volStr).cx;
		int dirW = memDC.GetTextExtent(dir).cx;
		int sbX = right - dirW - g_data.RDPI(2);
		int volX = sbX - volW - g_data.RDPI(12);
		volX = max(volX, textX + memDC.GetTextExtent(leftTxt).cx + g_data.RDPI(8));

		if (turnover >= 500000.0) {          // 超大单 ≥50万
			memDC.SetTextColor(RGB(160, 160, 160));
			memDC.TextOut(volX + 1, textY + 1, volStr);
		}
		memDC.SetTextColor(volColor);
		memDC.TextOut(volX, textY, volStr);

		// 最右：S/B 右对齐
		memDC.SetTextColor(lineColor);
		memDC.TextOut(sbX, textY, dir);

		rowY += rowH;
	}

	// 底部汇总行：区间净流入（当前明细的买卖差，直接读模型缓存值）
	{
		int footY = topOffset + 21 * rowH;
		double netInflow = book.GetIntervalNetInflow();

		// 分隔线（与汇总行上方）
		{
			CPen sepPen(PS_SOLID, 1, RGB(200, 200, 200));
			memDC.SelectObject(&sepPen);
			memDC.MoveTo(left, footY);
			memDC.LineTo(right, footY);
			memDC.SelectObject(oldPen);
		}

		int textY = footY + max(0, (rowH - memDC.GetTextExtent(L"00").cy) / 2);
		bool isPositive = (netInflow >= 0);
		memDC.SetTextColor(isPositive ? COLOR_RED_UP : COLOR_GREEN_DOWN);

		CString labelTxt = L"净流入:";
		CString valTxt;
		valTxt.Format(L"%s%.2f万", isPositive ? L"+" : L"-", std::abs(netInflow / 10000.0));
		memDC.TextOut(textX, textY, labelTxt);
		// 金额右对齐到面板右缘
		int valW = memDC.GetTextExtent(valTxt).cx;
		memDC.TextOut(right - valW - g_data.RDPI(2), textY, valTxt);
	}

	memDC.SelectObject(oldTickFont);
	memDC.SelectObject(oldPen);
}

// ============================================================================
// 辅助渲染函数（纯GDI绘制，不做数据计算）
// ============================================================================

void COrderBookPanel::DrawOrderBookRowText(CDC& memDC, const OrderBookRow& row, int x, int y, int rowWidth,
	int rowLeft, int rowTop, int rowHeight, STOCK::Volume maxVol)
{
	COLORREF textColor;
	if (row.darkBackground)
		textColor = RGB(255, 255, 255);
	else
		textColor = row.priceColor;
	memDC.SetTextColor(textColor);

	// 粗体字体
	CFont* pOldFont = nullptr;
	CFont boldFont;
	if (row.bold)
	{
		pOldFont = memDC.GetCurrentFont();
		LOGFONT lf;
		pOldFont->GetLogFont(&lf);
		lf.lfWeight = FW_BOLD;
		boldFont.CreateFontIndirect(&lf);
		memDC.SelectObject(&boldFont);
	}
	// 绘制价格背景色（仅买一/卖一行有背景色）+价格
	int nPriceWidth = memDC.GetTextExtent(row.strPrice).cx;
	if (row.fillBackground)
	{
		memDC.FillSolidRect(x, y, nPriceWidth, rowHeight, row.backgroundColor);
	}
	// 基金净值横线：当该行价格与净值（保留3位小数）位于同一行时，绘制一条2像素的紫色净值横线
	if (row.IOPV > 0.1)
	{
		//       净值高出本档价 lastDigit/10 个价位，故横线距行底高度 = lastDigit/10 * 行高；
		//       GDI坐标从上往下，因此 lineY = rowTop + 行高*(10-lastDigit)/10。
		//       例如净值为1.2344（高出本档价1.234正好4个万分位），横线绘制在距行底40%（自上而下60%）处
		long long iopvScaled = std::llround(row.IOPV * 10000.0);   // 净值放大到万分位整数，取最后一位
		int lastDigit = static_cast<int>(iopvScaled % 10);

		int lineY = rowTop + rowHeight * (10 - lastDigit) / 10;
		int lineRight = x + nPriceWidth;   // 当前行右边界
		CPen navPen(PS_SOLID, 2, RGB(160, 32, 240));   // 基金净值紫色，与分时净值线颜色一致
		CPen* pOldPen = memDC.SelectObject(&navPen);
		memDC.MoveTo(rowLeft + 7, lineY);
		memDC.LineTo(lineRight, lineY);
		memDC.SelectObject(pOldPen);
	}
	//绘制价格文本
	memDC.TextOut(x, y, row.strPrice);

	if (row.bold && pOldFont)
		memDC.SelectObject(pOldFont);

	if (row.strVolume.IsEmpty())
	{
		// 只有右对齐后缀
		if (!row.diffVol.IsEmpty())
		{
			CFont* oldFont = memDC.GetCurrentFont();
			LOGFONT lf;
			oldFont->GetLogFont(&lf);
			CFont smallFont;
			smallFont.CreateFontIndirect(&lf);
			memDC.SelectObject(&smallFont);
			memDC.SetTextColor(row.diffVolColor);
			int suffixW = memDC.GetTextExtent(row.diffVol).cx;
			memDC.TextOut(x + rowWidth - suffixW - g_data.RDPI(4), y + g_data.RDPI(1), row.diffVol);
			memDC.SelectObject(oldFont);
		}
		return;
	}

	int volumeX = x + nPriceWidth;
	int volWidth = (rowWidth - nPriceWidth) / 2; // 预留一半宽度给成交量，另一半给累计成交量和瞬时变化量
	//背景色统一先画，再画文字：累计成交量背景画在挂单量背景之后、所有文字之前，
	//避免累计背景矩形覆盖挂单量文字（挂单量带"/min"时文字更宽，容易被右侧背景压住）

	//绘制挂单量背景色，默认用淡蓝色
	int volBackWidth = volWidth * row.orderRatio;
	memDC.FillSolidRect(volumeX + 3, y + 1, volBackWidth - 2, rowHeight - 1, row.volumeColor);

	// 累计买卖占比背景色（先于所有文字绘制，避免覆盖挂单量文字）
	// 有排队规则时照样绘制（长度基于累计成交量计算，与普通行一致），只是文字换成剩余排队量
	if (row.totalVol > 0)
	{
		double widthRatio = static_cast<double>(row.totalVol) / maxVol;
		int curWidth = static_cast<int>(std::round(volWidth * widthRatio));
		if (curWidth < 1) curWidth = 1;
		int sumVolBuyX = volumeX + volWidth + 2;
		int buyWidth = static_cast<int>(curWidth * row.sumVolRatio);
		memDC.FillSolidRect(sumVolBuyX, y + 1, buyWidth, rowHeight - 1, RGB(255, 159, 159));
		int sellWidth = curWidth - buyWidth;
		memDC.FillSolidRect(sumVolBuyX + buyWidth, y + 1, sellWidth, rowHeight - 1, RGB(95, 255, 50));
	}

	CFont* oldFont = memDC.GetCurrentFont();
	LOGFONT lf;
	oldFont->GetLogFont(&lf);
	CFont smallFont;
	smallFont.CreateFontIndirect(&lf);
	memDC.SelectObject(&smallFont);
	memDC.SetTextColor(row.darkBackground ? RGB(255, 255, 200) : textColor);
	memDC.TextOut(volumeX, y + g_data.RDPI(1), row.strVolume);

	// 右对齐绘制累计成交量/剩余排队量与瞬时变化量（背景已在前面统一绘制，这里只画文字）
	// 布局：[...strVol] [diffVol] [sumVol/remainVol] 右边距
	int rightEdge = x + rowWidth - g_data.RDPI(4);
	{
		const CString& rightText = row.remainVol.IsEmpty() ? row.sumVol : row.remainVol;
		if (!rightText.IsEmpty())
		{
			COLORREF rightColor = row.remainVol.IsEmpty() ? row.sumVolColor : row.remainVolColor;
			memDC.SetTextColor(rightColor);
			int cumVolW = memDC.GetTextExtent(rightText).cx;
			memDC.TextOut(rightEdge - cumVolW, y + g_data.RDPI(1), rightText);
			rightEdge -= cumVolW;
		}
	}
	if (!row.diffVol.IsEmpty())
	{
		memDC.SetTextColor(row.diffVolColor);
		int raSuffixW = memDC.GetTextExtent(row.diffVol).cx;
		memDC.TextOut(rightEdge - raSuffixW, y + g_data.RDPI(1), row.diffVol);
	}
	memDC.SelectObject(oldFont);
}

int COrderBookPanel::GetNetRatioColorIndex(double ratio)
{
	double absRatioValue = std::abs(ratio);
	if (absRatioValue <= 30) return 0;
	if (absRatioValue <= 60) return 1;
	return 2;
}

// 从模型行数据构建渲染行（价格/量/后缀格式化为文本），不做数据查询
COrderBookPanel::OrderBookRow COrderBookPanel::BuildAskRow(const MarketOrderBook& book, int idx, long long delta) const
{
	MarketOrderBook::Level level = book.GetAsk(idx);
	double price = level.price;
	long long volume = level.volume / 100;

	OrderBookRow row;
	const double eps = 1e-6; // 万分之一的容错
	double truncIopv = std::trunc(book.GetIOPV() * 1000.0) / 1000.0;
	if (fabs(price - truncIopv) < eps)
	{
		row.IOPV = book.GetIOPV();
	}
	row.strPrice = book.IsETF() ? CCommon::FormatETFPrice(price) : CCommon::FormatFloat(price);
	row.strVolume.Format(_T(" %lld"), static_cast<long long>(volume));
	row.volumeColor = RGB(163, 255, 172);

	// 累计成交量（卖盘显示主动买成交量，模型缓存值直接读取）
	auto cumVol = book.GetCumVol(price);
	row.totalVol = cumVol.first + cumVol.second;
	// 该档位存在排队监视规则时：右列改显剩余排队量（独立字段，蓝色）；背景条仍按累计成交量计算
	long long remain = book.GetRemainQueueAt(price, true);
	if (remain != (std::numeric_limits<long long>::min)())
	{
		row.remainVol.Format(_T(" %lld"), remain);
		row.hasRemainQueue = true;
		// 瞬时变化量改为蓝色负数（-|delta|）；无变化(delta==0)不显示
		if (delta != 0)
		{
			row.diffVol.Format(_T("-%lld"), static_cast<long long>(std::abs(delta)));
			row.diffVolColor = RGB(0, 0, 255);
		}
		// 背景条照常绘制：长度与占比都用累计成交量数据（仅不显示累计数字）
		row.sumVolRatio = row.totalVol == 0 ? 0 : (double)cumVol.first / row.totalVol;
	}
	else
	{
		if (cumVol.first > 0)
		{
			row.sumVol.Format(_T(" %lld"), static_cast<long long>(cumVol.first));
			row.sumVolRatio = cumVol.second == 0 ? 1 : (double)cumVol.first / row.totalVol;
			row.sumVolColor = RGB(128, 0, 128);
		}
		if (delta != 0)
		{
			row.diffVol.Format(_T("%s%lld"), delta > 0 ? _T("+") : _T("-"), static_cast<long long>(std::abs(delta)));
			row.diffVolColor = COLOR_RED_UP;
		}
	}
	row.priceColor = COLOR_RED_UP;
	if (book.IsHighestPrice(price))
	{
		row.priceColor = RGB(128, 0, 128);
		row.bold = true;
	}

	// 卖一背景色（仅当前价格=卖一时显示）
	if (idx == 0 && book.IsCurrentPrice(price))
	{
		row.fillBackground = true;
		row.backgroundColor = RGB(255, 200, 200);
	}

	row.orderRatio = book.GetOrderRowRatio(level.volume);
	return row;
}

COrderBookPanel::OrderBookRow COrderBookPanel::BuildBidRow(const MarketOrderBook& book, int idx, long long delta) const
{
	MarketOrderBook::Level level = book.GetBid(idx);
	double price = level.price;
	long long volume = level.volume / 100;

	OrderBookRow row;
	const double eps = 1e-6;
	double truncIopv = std::trunc(book.GetIOPV() * 1000.0) / 1000.0;
	if (fabs(price - truncIopv) < eps)
	{
		row.IOPV = book.GetIOPV();
	}

	row.strPrice = book.IsETF() ? CCommon::FormatETFPrice(price) : CCommon::FormatFloat(price);
	row.strVolume.Format(_T(" %lld"), static_cast<long long>(volume));

	// 累计成交量（买盘显示主动卖成交量，模型缓存值直接读取）
	auto cumVol = book.GetCumVol(price);
	row.totalVol = cumVol.first + cumVol.second;
	// 该档位存在排队监视规则标记（决定右列与瞬时变化量的显示方式）
	long long remain = book.GetRemainQueueAt(price, false);
	bool hasRuleRemain = remain != (std::numeric_limits<long long>::min)();

	//瞬时变化量：规则行改蓝色负数（-|delta|），无变化不显示；否则原逻辑绿色
	if (hasRuleRemain)
	{
		if (delta != 0)
		{
			row.diffVol.Format(_T("-%lld"), static_cast<long long>(std::abs(delta)));
			row.diffVolColor = RGB(0, 0, 255);
		}
	}
	else if (delta != 0)
	{
		row.diffVol.Format(_T("%s%lld"), delta > 0 ? _T("+") : _T("-"), static_cast<long long>(std::abs(delta)));
		row.diffVolColor = COLOR_GREEN_DOWN;
	}

	// 该档位存在排队监视规则时：右列改显剩余排队量（独立字段，蓝色）；背景条仍按累计成交量计算
	if (hasRuleRemain)
	{
		row.remainVol.Format(_T(" %lld"), remain);
		row.hasRemainQueue = true;
		// 背景条照常绘制：占比用累计成交量（仅不显示累计数字）
		row.sumVolRatio = row.totalVol == 0 ? 0 : (double)cumVol.first / row.totalVol;
	}
	else if (cumVol.second > 0)
	{
		row.sumVol.Format(_T(" %lld"), static_cast<long long>(cumVol.second));
		row.sumVolRatio = cumVol.second == 0 ? 1 : (double)cumVol.first / row.totalVol;
		row.sumVolColor = RGB(0, 100, 0);
	}
	row.volumeColor = RGB(254, 202, 215);
	row.priceColor = COLOR_GREEN_DOWN;
	if (book.IsLowestPrice(price))
	{
		row.priceColor = RGB(0, 100, 0);
		row.bold = true;
	}

	// 买一背景色（仅当前价格=买一时显示）
	if (idx == 0 && book.IsCurrentPrice(price))
	{
		row.fillBackground = true;
		row.backgroundColor = RGB(200, 255, 200);
	}

	row.orderRatio = book.GetOrderRowRatio(level.volume);
	return row;
}

void COrderBookPanel::DrawPriceRows(CDC& memDC, const LayoutContext& lc, const std::vector<OrderBookRow>& rows, int startRow, STOCK::Volume maxVol)
{
	for (int i = 0; i < static_cast<int>(rows.size()); i++)
	{
		int y = lc.RowY(startRow + i);
		int h = lc.RowH(startRow + i);
		int textVCenter = max(0, (h - memDC.GetTextExtent(rows[i].strPrice).cy) / 2);
		DrawOrderBookRowText(memDC, rows[i], lc.textX, y + textVCenter, lc.right - lc.textX, lc.left, y, h, maxVol);
	}
}

// 双击命中测试：复用最近一次 Draw 缓存的布局，检测最高/最低行(0)与买卖十档(1~10)
// 行号语义与绘制完全一致：0=最高/最低行, 1~5=卖五~卖一, 6~10=买一~买五
bool COrderBookPanel::HitTestOrderBook(const CPoint& clientPos, int& rowIdx) const
{
	rowIdx = -1;
	if (!m_layoutValid)
		return false;

	const LayoutContext& lc = m_layout;
	// 仅限首行+十档区域：最高/最低行到买五行底
	if (clientPos.x < lc.left || clientPos.x >= lc.right)
		return false;

	int top = lc.RowY(0);
	int bottom = lc.RowY(10) + lc.RowH(10);
	if (clientPos.y < top || clientPos.y >= bottom)
		return false;

	// 逐行匹配RowY/RowH（rem不均匀分配与绘制完全一致），盘口标题栏与mLayout之外的行不命中
	for (int i = 0; i <= 10; i++)
	{
		if (clientPos.y >= lc.RowY(i) && clientPos.y < lc.RowY(i) + lc.RowH(i))
		{
			rowIdx = i;
			return true;
		}
	}
	return false;
}
