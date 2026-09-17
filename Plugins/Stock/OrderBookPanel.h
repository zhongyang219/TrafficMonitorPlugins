#pragma once

#include <StockDef.h>
#include "Common.h"
#include <string>
#include <vector>

// 盘口面板渲染层（纯视图）
// 职责：只读 MarketOrderBook 数据模型的缓存结果并完成 GDI 绘制，
//       不做任何数据查询（数据库/StockData）或指标计算。
// 数据由 StockData::orderBook 在行情推送时更新（数据与视图分离）。

class COrderBookPanel
{
public:
	// 绘制盘口面板
	// left, right: 面板左右边界（含主标题栏下方的盘口标题栏区域）
	// height: 面板总高度（含盘口标题栏）
	// book: 盘口数据模型（只读缓存）
	// viewMode/klineData: 仅用于旧完整版占位，精简版不再使用
	void Draw(CDC& memDC, int left, int right, int height, const MarketOrderBook& book,
		const std::vector<STOCK::KLinePoint>& klineData,
		UIViewMode viewMode);

	// 绘制成交明细界面（MX模式）：显示最近20条成交
	// left,right: 面板左右边界；height: 面板总高度（含盘口标题栏）
	void DrawTickDetail(CDC& memDC, int left, int right, int height, const MarketOrderBook& book);

	// 双击命中测试：检测点是否在右侧盘口的最高/最低行(0)或买卖十档(1~5=卖五~卖一, 6~10=买一~买五)上
	// 命中时返回true并输出rowIdx；仅盘口(Draw)最近一次绘制时生效，
	// 明细(MX)模式/筹码峰模式因盘口未绘制而返回false。clientPos为FloatingWnd客户区坐标
	bool HitTestOrderBook(const CPoint& clientPos, int& rowIdx) const;

private:
	// 盘口行渲染数据（仅含显示样式，不含任何行情计算结果）
	struct OrderBookRow
	{		STOCK::Price IOPV{ 0.0 };  // 净值
		CString strPrice;	//价格
		CString strVolume;	//挂单量
		COLORREF volumeColor{ RGB(0,0,0) };  // 挂单量颜色
		CString diffVol;  // 右对齐的瞬时变化量（+N/-N）
		COLORREF diffVolColor{ RGB(0,0,0) };  // 右对齐后缀颜色
		CString sumVol;      // 累计成交量后缀（显示在瞬时变化量前面）
		STOCK::Volume totalVol{ 0 };  // 累计成交量（手）
		COLORREF sumVolColor{ RGB(0,0,0) };  // 累计成交量后缀颜色
		CString remainVol;   // 剩余排队量（有排队规则时替代sumVol显示）
		COLORREF remainVolColor{ RGB(0,0,255) };  // 剩余排队量颜色（蓝）
		COLORREF priceColor;
		bool fillBackground{ false };
		COLORREF backgroundColor;
		bool darkBackground{ false };  // 深色背景时文字改白色
		bool bold{ false };   // 粗体
		double orderRatio{ 0.0 };  // 卖一/买一后方挂单量占比（0~1），用于绘制背景色
		double sumVolRatio{ 0.0 };  // 累计买入与累计卖出成交量占比（0~1），用于绘制背景色
		bool hasRemainQueue{ false };  // 该行显示剩余排队量（背景条用单色满宽，非红绿双色）
	};

	// 布局上下文（由Draw计算，传递给各子函数）
	struct LayoutContext
	{
		int left;
		int right;
		int height;
		int headerHeight;
		int obTitleH;
		int topOffset;
		int panelW;
		int totalRows;
		int rowHeight;
		int contentH;
		int rem;
		int textX;
		// 计算第i行(0-based)的Y坐标
		int RowY(int i) const
		{
			if (i < rem) return topOffset + i * (rowHeight + 1);
			else return topOffset + rem * (rowHeight + 1) + (i - rem) * rowHeight;
		}
		// 计算第i行的高度
		int RowH(int i) const
		{
			return (i < rem) ? (rowHeight + 1) : rowHeight;
		}
	};

	// ===== 精简版盘口（新版Draw）用到的子项 =====
	// 绘制最高/最低行（直接使用模型预格式化文本）
	void DrawHighLow(CDC& memDC, const LayoutContext& lc, const MarketOrderBook& book);
	// 绘制选线分割（净比00柱状图，barY为绘制行的Y坐标）—— 从模型读缓存净比
	void DrawNetRatio00Ex(CDC& memDC, const LayoutContext& lc, const MarketOrderBook& book, int barY);
	// 绘制净流入汇总行（label在左、金额右对齐，正红负绿）
	void DrawNetInflowRow(CDC& memDC, const LayoutContext& lc, int rowIndex, const CString& label, double netInflow);
	// 绘制行明细（从startRow开始，最多10行，最新在下，直接读模型明细缓存）
	void DrawTickMini(CDC& memDC, const LayoutContext& lc, int startRow, const MarketOrderBook& book);

	// 辅助：绘制单行盘口文本（含小号后缀、右对齐后缀）
	// x, y: 文本绘制坐标；rowWidth: 文本区宽度
	// rowLeft, rowTop, rowHeight: 当前行的左边界、顶部、高度（基金净值紫色横线据此定位）
	void DrawOrderBookRowText(CDC& memDC, const OrderBookRow& row, int x, int y, int rowWidth,
		int rowLeft, int rowTop, int rowHeight, STOCK::Volume maxVol);

	// 辅助：绘制一组盘口行
	void DrawPriceRows(CDC& memDC, const LayoutContext& lc, const std::vector<OrderBookRow>& rows, int startRow, STOCK::Volume maxVol);

	// 辅助：获取净比颜色索引（0=0~30, 1=30~60, 2=60+）
	static int GetNetRatioColorIndex(double ratio);

	// 辅助：从模型行数据构建渲染行（价格/量/后缀格式化为文本），不做数据查询
	OrderBookRow BuildAskRow(const MarketOrderBook& book, int idx, long long delta) const;
	OrderBookRow BuildBidRow(const MarketOrderBook& book, int idx, long long delta) const;

private:
	static const COLORREF NET_RATIO_RED_COLORS[3];
	static const COLORREF NET_RATIO_GREEN_COLORS[3];

	// 最近一次Draw缓存的盘口布局（供双击命中测试复用，与绘制共用同一套RowY/RowH公式）
	// m_showTickDetail/筹码峰模式下盘口未绘制，置为无效
	LayoutContext m_layout;
	bool m_layoutValid{ false };
};
