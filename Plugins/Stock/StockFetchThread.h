#pragma once
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string>
#include <deque>

// 股票数据获取专用工作线程
// 将所有网络数据获取与 UI 交互彻底分离：
//  - 持有一个长期运行的后台线程，避免每次请求都创建/销毁线程
//  - 工作线程只执行数据获取任务（HTTP 请求），不触碰任何 MFC/UI 对象
//  - 实时行情/集合竞价：通过 PostTask/PostCallAuctionTask 投递，忙时丢弃
//  - 后台任务：通过 PostBackgroundTask 投递，排队执行，不丢弃
//  - 图表数据（分时/K线/IOPV）：线程自主定时获取，外部只需设置关注的股票ID
//  - 切换股票时调用 SetFocusStockId，线程自动重置计时器并立即获取新股数据
class CStockFetchThread
{
public:
	using Task = std::function<void()>;

	static CStockFetchThread& Instance();

	// 启动工作线程（幂等，重复调用安全）
	void Start();
	// 通知工作线程退出并等待其结束（在主线程退出前调用）
	void Stop();

	// 投递一个常规实时行情数据获取任务到工作线程
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

	// 设置当前关注的股票ID（图表数据将针对此股票定时获取）
	// 切换股票时自动重置图表计时器，使线程立即获取新股数据
	void SetFocusStockId(const std::wstring& stockId);
	// 获取当前关注的股票ID
	std::wstring GetFocusStockId() const;

private:
	CStockFetchThread();
	~CStockFetchThread();
	CStockFetchThread(const CStockFetchThread&) = delete;
	CStockFetchThread& operator=(const CStockFetchThread&) = delete;

	static UINT ThreadProc(LPVOID pParam);
	void Run();

	// 各图表数据类型的获取间隔（秒）
	static time_t GetChartInterval(int type);
	// 各图表数据类型的上次获取时间
	time_t m_chart_last_fetch[4] = {};  // IOPV, Timeline, Min5KLine, Min30KLine

	mutable std::mutex m_mutex;
	std::condition_variable m_cv;
	std::atomic<bool> m_started{ false };
	std::atomic<bool> m_stopping{ false };

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
};
