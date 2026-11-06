#pragma once

#include <string>
#include <vector>
#include <ctime>
#include "StockDef.h"

// 股票数据 HTTP 获取器
// 职责：仅负责网络数据获取（HTTP 请求 + 响应解析为原始数据），不写入任何 DataManager/stockMarket 状态
// 调用方（CStockFetchThread）获取到数据后，调用 CDataManager 的 Apply*/Update 方法完成存储
//
// 设计说明：
//  - 简单接口（分时/K线/IOPV等）返回原始响应体字符串，由 DataManager 的 LoadXXXDataByJson 解析
//  - API 专属解析（东方财富流通股本、筹码K线换手率）由本类解析为结构体返回
//  - 东方财富 WAF 拦截后 10 分钟内不再尝试（m_eastmoney_fail_until）
class CStockHttpFetcher
{
public:
	// 实时行情（新浪）：onlyNonAG=true 时仅获取非A股代码
	// outCodes 返回实际请求的代码列表，outResp 返回响应体；无代码或请求失败返回 false
	bool FetchRealtimeHtml(const std::vector<std::wstring>& allCodes, bool onlyNonAG,
		std::vector<std::wstring>& outCodes, std::string& outResp);
	// 内外盘（腾讯）：includeAG=true 含A股；港股代码需转 r_ 前缀
	bool FetchInnerOuterHtml(const std::vector<std::wstring>& allCodes, bool includeAG, std::string& outResp);
	// 集合竞价（腾讯，仅A股）
	bool FetchCallAuctionHtml(const std::vector<std::wstring>& allCodes,
		std::vector<std::wstring>& outCodes, std::string& outResp);

	// 分时图（新浪），失败返回 false
	bool FetchTimeline(const std::wstring& code, std::string& outResp);
	// 日K线（新浪）
	bool FetchDayKLine(const std::wstring& code, int days, std::string& outResp);
	// 5分钟K线（新浪）
	bool FetchMin5KLine(const std::wstring& code, int datalen, std::string& outResp);
	// 30分钟K线（新浪）
	bool FetchMin30KLine(const std::wstring& code, int datalen, std::string& outResp);
	// ETF基金IOPV（上交所/天天基金）
	bool FetchFundIOPV(const std::wstring& code, std::string& outResp);

	// 流通股本（东方财富 f85）；失败设置 10 分钟失败缓存并返回 false
	bool FetchStockBasicCirculating(const std::wstring& code, STOCK::Volume& outShares);
	// 筹码分布K线（东方财富含换手率，失败回退新浪按 circulatingAShares 计算换手率）
	bool FetchChipKLines(const std::wstring& code, STOCK::Volume circulatingAShares,
		std::vector<STOCK::ChipKLinePoint>& outKlines);

private:
	// 东方财富接口失败缓存：WAF 拦截 WinINet 后，一段时间内不再尝试
	// 值为失败截止时间戳（秒），0 表示未缓存
	time_t m_eastmoney_fail_until{ 0 };
};

// 全局实例（与 g_data 同模式，供工作线程直接访问）
extern CStockHttpFetcher g_http_fetcher;
