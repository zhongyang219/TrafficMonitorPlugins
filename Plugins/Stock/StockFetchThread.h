#pragma once
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>

// 股票数据获取专用工作线程
// 将网络数据获取与 UI 交互彻底分离：
//  - 持有一个长期运行的后台线程，避免每次请求都创建/销毁线程
//  - 工作线程只执行数据获取任务（HTTP 请求），不触碰任何 MFC/UI 对象
//  - 通过 PostTask/PostCallAuctionTask 投递任务；线程忙时丢弃新请求，避免请求堆积
//  - 两种任务类型独立忙检测，互不阻塞投递
//  - UI 线程可通过 IsBusy()/IsCallAuctionBusy() 查询状态
class CStockFetchThread
{
public:
	using Task = std::function<void()>;

	static CStockFetchThread& Instance();

	// 启动工作线程（幂等，重复调用安全）
	void Start();
	// 通知工作线程退出并等待其结束（在主线程退出前调用）
	void Stop();
	// 投递一个常规数据获取任务到工作线程
	// 线程未启动、正在退出或当前正忙于执行同类任务时，忽略本次请求
	void PostTask(Task task);
	// 投递一个集合竞价数据获取任务到工作线程
	// 线程未启动、正在退出或当前正忙于执行同类任务时，忽略本次请求
	void PostCallAuctionTask(Task task);
	// 工作线程是否正忙于执行常规任务
	bool IsBusy() const { return m_busy.load(); }
	// 工作线程是否正忙于执行集合竞价任务
	bool IsCallAuctionBusy() const { return m_callAuction_busy.load(); }

private:
	CStockFetchThread();
	~CStockFetchThread();
	CStockFetchThread(const CStockFetchThread&) = delete;
	CStockFetchThread& operator=(const CStockFetchThread&) = delete;

	static UINT ThreadProc(LPVOID pParam);
	void Run();

	mutable std::mutex m_mutex;
	std::condition_variable m_cv;
	std::atomic<bool> m_started{ false };
	std::atomic<bool> m_stopping{ false };

	// 常规任务
	std::atomic<bool> m_busy{ false };
	Task m_task;
	bool m_has_task{ false };

	// 集合竞价任务
	std::atomic<bool> m_callAuction_busy{ false };
	Task m_callAuction_task;
	bool m_has_callAuction_task{ false };

	HANDLE m_thread_handle{ nullptr };
	class CWinThread* m_pThread{ nullptr };
};
