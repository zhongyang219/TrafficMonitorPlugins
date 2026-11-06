#pragma once

#include <afxwin.h>

// 图表绘制共享颜色常量
// 注：这些颜色宏在各图表模块（CStockListPanel/COrderBookPanel/CChartPrice 等）间共享

#define COLOR_WHITE               RGB(255, 255, 255)
#define COLOR_BLACK               RGB(0, 0, 0)

#define COLOR_RED_UP              RGB(179, 64, 65)    // 红色-上涨/盈利
#define COLOR_GREEN_DOWN          RGB(44, 144, 51)    // 绿色-下跌/亏损
#define COLOR_GRAY_TEXT           RGB(102, 102, 102)  // 灰色文字
#define COLOR_GRAY_GRID           RGB(200, 200, 200)  // 浅灰网格线
#define COLOR_GRAY_MIDDLE         RGB(140, 140, 140)  // 中灰虚线
#define COLOR_DARK_GRAY_BORDER    RGB(80, 80, 80)     // 深灰边框
#define COLOR_GRAY_PURPLE         RGB(154, 151, 157)  // 灰紫色
#define COLOR_BLUE_COST           RGB(0, 43, 204)     // 深蓝色成本线

#define COLOR_BLUE_AVG1           RGB(0, 0, 230)      // 蓝色-1年均线
#define COLOR_GREEN_AVG2          RGB(0, 166, 235)    // 2年均线
#define COLOR_GREEN_AVG3          RGB(169, 102, 186)  // 3年均线
#define COLOR_LIGHT_BLUE          RGB(210, 235, 255)  // 淡蓝色背景
#define COLOR_LIGHT_GREEN         RGB(210, 245, 210)  // 淡绿色背景
#define COLOR_LIGHT_RED_BG        RGB(255, 228, 225)  // MACD图DIF上行区间浅红背景
#define COLOR_GOLDEN              RGB(200, 150, 0)    // 黄金色
#define COLOR_DARK_ORANGE         RGB(199, 110, 0)    // 暗橙色
#define COLOR_MACD_SUB            RGB(180, 180, 180)  // MACD辅助线浅灰色

// 涨幅/盈亏背景颜色
#define COLOR_BG_RED              RGB(220, 80, 80)    // >= 5% 红色背景
#define COLOR_BG_PURPLE           RGB(160, 50, 160)   // >= 10% 紫色背景
#define COLOR_BG_GREEN            RGB(60, 160, 70)    // <= -5% 绿色背景
#define COLOR_BG_DARK_GREEN       RGB(20, 100, 40)    // <= -10% 墨绿色背景
