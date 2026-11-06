#pragma once
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string>
#include <deque>
#include "TdxTcpClient.h"  // QuoteItem

// 股票数据获取专用工作线程
// 将所有网络数据获取与 UI 交互彻底分离：
//  - 持有两个长期运行的后台线程，避免每次请求都创建/销毁线程
//  - 实时行情线程：自主定时获取所有股票价格+五档（盘中2秒，午休60秒）
//  - 图表线程：处理即时任务/集合竞价/后台任务/图表定时获取（分时/K线/IOPV）
//  - 两个线程独立运行，互不阻塞
//  - 切换股票时调用 SetFocusStockId，图表线程自动重置计时器并立即获取新股数据
class CStockFetchThread
{
public:
	using Task = std::function<void()>;

	static CStockFetchThread& Instance();

	// 启动工作线程（实时行情线程 + 图表线程，幂等，重复调用安全）
	void Start();
	// 通知所有工作线程退出并等待其结束（在主线程退出前调用）
	void Stop();

	// 启动外部共享内存程序（getPrice.exe）
	void StartExternalProcess();
	// 关闭外部共享内存程序
	void StopExternalProcess();

	// 投递一个常规任务到工作线程（用于日K线等一次性请求）
	// 线程未启动、正在退出或当前正忙于执行同类任务时，忽略本次请求
	void PostTask(Task task);
	// 投递一个集合竞价数据获取任务到工作线程
	void PostCallAuctionTask(Task task);
	// 投递一个后台任务到工作线程（排队执行，不丢弃）
	// 用于预加载、筹码峰等不可丢弃的任务
	void PostBackgroundTask(Task task);

	// 工作线程是否正忙于执行常规任务
	bool IsBusy() const { return m_busy.load(); }
	// 工作线程是否正忙于执行集合竞价任务
	bool IsCallAuctionBusy() const { return m_callAuction_busy.load(); }

	// 设置当前关注股票ID（图表数据将针对此股票定时获取）
	// 切换股票时自动重置图表计时器，使线程立即获取新股数据
	void SetFocusStockId(const std::wstring& stockId);
	// 获取当前关注的股票ID
	std::wstring GetFocusStockId() const;

	// ===== 数据获取编排方法（fetcher + apply）=====
	// 由调用方（Run/RunRealtime/外部PostTask lambda）在工作线程中调用
	// 每个方法完成"获取→解析→存储"的完整流程

	// 实时行情（HTTP）：onlyNonAG=true 仅获取非A股（A股由共享内存提供）
	void FetchRealtimeByHttp(bool onlyNonAG);
	// 集合竞价（腾讯，仅A股）
	void FetchCallAuction();
	// 分时图（新浪）
	void FetchTimeline(const std::wstring& code);
	// 日K线（新浪）
	void FetchDayKLine(const std::wstring& code, int days = 750);
	// 5分钟K线（新浪）
	void FetchMin5KLine(const std::wstring& code, int datalen = 250);
	// 30分钟K线（新浪）
	void FetchMin30KLine(const std::wstring& code, int datalen = 250);
	// ETF基金IOPV
	void FetchFundIOPV(const std::wstring& code);
	// 流通股本（东方财富 f85）
	void FetchStockBasic(const std::wstring& code);
	// 筹码分布（含DB缓存检查+K线获取+计算入库）
	void FetchChipDistribution(const std::wstring& code);
	// 初始化全量数据获取（线程启动时调用一次）
	void FetchAllData();

private:
	CStockFetchThread();
	~CStockFetchThread();
	CStockFetchThread(const CStockFetchThread&) = delete;
	CStockFetchThread& operator=(const CStockFetchThread&) = delete;

	static UINT ThreadProc(LPVOID pParam);
	void Run();

	// 实时行情独立线程
	static UINT RealtimeThreadProc(LPVOID pParam);
	void RunRealtime();

	// 共享内存行情数据到达回调（在 CTdxTcpClient 监听线程中被调用）
	// 更新 DataManager + 限频触发 HTTP 补充（港股等非A股数据）
	void OnQuotesReceived(const std::vector<QuoteItem>& items);

	// 各图表数据类型的获取间隔（秒）
	static time_t GetChartInterval(int type);
	// 各图表数据类型的上次获取时间
	time_t m_chart_last_fetch[4] = {};  // IOPV, Timeline, Min5KLine, Min30KLine

	// 其余基金净值（IOPV）定时获取：轮询每个非关注基金代码，保证所有基金持续更新
	time_t m_all_fund_iopv_last_fetch = 0;  // 上次执行全基金轮询的时间
	size_t m_all_fund_iopv_index = 0;       // 本轮待获取的基金下标（轮询）
	static const time_t ALL_FUND_IOPV_INTERVAL = 3;  // 每次轮询间隔（秒）
	void FetchAllFundsIOPV();               // 获取下一个非关注基金的IOPV

	// 实时行情定时获取
	time_t m_realtime_last_fetch = 0;	// 上次获取实时行情的时间
	static const time_t REALTIME_INTERVAL_TRADING = 2;	// 盘中实时行情间隔（秒）
	static const time_t REALTIME_INTERVAL_LUNCH = 60;	// 午休期间间隔（秒）

	// HTTP 补充（港股等非A股数据）的上次触发时间，用于限频
	time_t m_last_http_supplement = 0;

	mutable std::mutex m_mutex;
	std::condition_variable m_cv;
	std::atomic<bool> m_started{ false };
	std::atomic<bool> m_stopping{ false };

	// 实时行情线程独立的同步设施（与图表线程互不干扰）
	std::mutex m_realtime_mutex;
	std::condition_variable m_realtime_cv;

	// 常规任务（实时行情）
	std::atomic<bool> m_busy{ false };
	Task m_task;
	bool m_has_task{ false };

	// 集合竞价任务
	std::atomic<bool> m_callAuction_busy{ false };
	Task m_callAuction_task;
	bool m_has_callAuction_task{ false };

	// 后台任务队列（预加载、筹码峰等，排队执行不丢弃）
	std::deque<Task> m_background_tasks;

	// 当前关注的股票ID（图表数据获取目标）
	std::wstring m_focus_stock_id;

	HANDLE m_thread_handle{ nullptr };
	class CWinThread* m_pThread{ nullptr };

	// 实时行情线程
	HANDLE m_realtime_thread_handle{ nullptr };
	class CWinThread* m_realtime_pThread{ nullptr };

	// 外部共享内存进程
	HANDLE m_hExternalProcess{ nullptr };
	DWORD m_dwExternalPid{ 0 };
	HANDLE m_hJob{ nullptr };		// Job Object，确保子进程随主进程一起终止
	HANDLE m_hStartMutex{ nullptr };	// 跨进程互斥体，防止多个TM进程重复启动getPrice.exe
};
