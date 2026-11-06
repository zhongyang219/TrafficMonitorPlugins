#include "pch.h"
#include "StockFetchThread.h"
#include "DataManager.h"
#include "Common.h"
#include "Stock.h"
#include "StockHttpFetcher.h"
#include "FloatingWnd.h"
#include <afxwin.h>
#include <afxinet.h>
#include <shlwapi.h>
#include <tlhelp32.h>

// 图表数据类型索引（与 m_chart_last_fetch 数组对应）
// 按间隔从短到长排列，遍历时从前往后即可保证短间隔任务优先
// 日K线不在定时队列中，SetFocusStockId 时获取一次即可
enum ChartType
{
	CHART_IOPV = 0,			// ETF基金IOPV（3秒）
	CHART_TIMELINE,			// 分时图（10秒）
	CHART_MIN5_KLINE,		// 5分钟K线（60秒）
	CHART_MIN30_KLINE,		// 30分钟K线（600秒）
	CHART_COUNT
};

CStockFetchThread& CStockFetchThread::Instance()
{
	static CStockFetchThread instance;
	return instance;
}

CStockFetchThread::CStockFetchThread()
{
}

CStockFetchThread::~CStockFetchThread()
{
	Stop();
}

void CStockFetchThread::StartExternalProcess()
{
	if (m_hExternalProcess != nullptr)
	{
		CCommon::WriteLog(L"[ExternalProcess] 已有进程在运行，跳过", g_data.m_log_path.c_str());
		return;
	}

	// 检查系统中是否已有 getPrice.exe 进程在运行（防止残留进程导致重复启动）
	{
		HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnap != INVALID_HANDLE_VALUE)
		{
			PROCESSENTRY32W pe{};
			pe.dwSize = sizeof(pe);
			if (Process32FirstW(hSnap, &pe))
			{
				do
				{
					if (_wcsicmp(pe.szExeFile, L"getPrice.exe") == 0)
					{
						m_dwExternalPid = pe.th32ProcessID;
						// 打开进程句柄，以便 StopExternalProcess 能正确关闭它
						m_hExternalProcess = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pe.th32ProcessID);
						if (m_hExternalProcess != nullptr)
						{
							CCommon::WriteLog(L"[ExternalProcess] 发现已有getPrice.exe进程，已接管管理", g_data.m_log_path.c_str());
						}
						else
						{
							CCommon::WriteLog(L"[ExternalProcess] 发现已有getPrice.exe进程，但无法打开句柄，跳过启动", g_data.m_log_path.c_str());
						}
						CloseHandle(hSnap);
						return;
					}
				} while (Process32NextW(hSnap, &pe));
			}
			CloseHandle(hSnap);
		}
	}

	// 跨进程互斥：TrafficMonitor 可能有多个进程加载插件，确保只启动一次 getPrice.exe
	HANDLE hMutex = OpenMutexW(SYNCHRONIZE, FALSE, L"Local\\StockGetPriceStartMutex");
	if (hMutex != nullptr)
	{
		// 互斥体已存在，说明另一个进程已经启动了 getPrice.exe
		CCommon::WriteLog(L"[ExternalProcess] 其他进程已启动getPrice，跳过", g_data.m_log_path.c_str());
		CloseHandle(hMutex);
		return;
	}

	// 创建互斥体（首次创建），用于标记 getPrice.exe 已被某个进程启动
	hMutex = CreateMutexW(nullptr, TRUE, L"Local\\StockGetPriceStartMutex");
	if (hMutex == nullptr)
	{
		CCommon::WriteLog(L"[ExternalProcess] 创建互斥体失败，跳过", g_data.m_log_path.c_str());
		return;
	}
	// 保存互斥体句柄，Stop 时释放
	m_hStartMutex = hMutex;

	// 获取当前DLL所在目录
	wchar_t dllPath[MAX_PATH]{};
	HMODULE hMod = nullptr;
	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCWSTR)&CStockFetchThread::Instance, &hMod))
	{
		CCommon::WriteLog(L"[ExternalProcess] GetModuleHandleEx 失败", g_data.m_log_path.c_str());
		return;
	}
	GetModuleFileNameW(hMod, dllPath, MAX_PATH);
	// 去掉文件名，保留目录
	PathRemoveFileSpecW(dllPath);

	// 拼接 getPrice.exe 路径
	std::wstring exePath = std::wstring(dllPath) + L"\\getPrice.exe";

	if (!PathFileExistsW(exePath.c_str()))
	{
		CCommon::WriteLog(L"[ExternalProcess] getPrice.exe 不存在，跳过", g_data.m_log_path.c_str());
		return;
	}

	// 创建 Job Object，确保 PyInstaller 子进程随主进程一起终止
	m_hJob = CreateJobObjectW(nullptr, nullptr);
	if (m_hJob == nullptr)
	{
		wchar_t logBuf[128]{};
		swprintf_s(logBuf, L"[ExternalProcess] CreateJobObject 失败, 错误码=%u", GetLastError());
		CCommon::WriteLog(logBuf, g_data.m_log_path.c_str());
		return;
	}
	JOBOBJECT_BASIC_LIMIT_INFORMATION jbli{};
	jbli.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	SetInformationJobObject(m_hJob, JobObjectBasicLimitInformation, &jbli, sizeof(jbli));

	STARTUPINFOW si{};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi{};
	// CREATE_NO_WINDOW 抑制控制台窗口，CREATE_SUSPENDED 先挂起以便加入Job后再恢复
	if (CreateProcessW(exePath.c_str(), nullptr, nullptr, nullptr, FALSE,
		CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, dllPath, &si, &pi))
	{
		// 将主进程加入 Job，PyInstaller 创建的子进程会自动继承 Job
		AssignProcessToJobObject(m_hJob, pi.hProcess);
		ResumeThread(pi.hThread);

		m_hExternalProcess = pi.hProcess;
		m_dwExternalPid = pi.dwProcessId;
		CloseHandle(pi.hThread);
	}
	else
	{
		wchar_t logBuf[128]{};
		swprintf_s(logBuf, L"[ExternalProcess] CreateProcess 失败, 错误码=%u", GetLastError());
		CCommon::WriteLog(logBuf, g_data.m_log_path.c_str());
		CloseHandle(m_hJob);
		m_hJob = nullptr;
	}
}

void CStockFetchThread::StopExternalProcess()
{
	if (m_hExternalProcess == nullptr)
		return;

	// 关闭 Job Object 句柄，JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE 会自动终止 Job 内所有进程
	// 包括 PyInstaller 引导进程及其创建的 Python 子进程
	if (m_hJob != nullptr)
	{
		CloseHandle(m_hJob);
		m_hJob = nullptr;
	}
	else
	{
		// 接管的残留进程没有 Job Object，需要手动终止
		TerminateProcess(m_hExternalProcess, 0);
	}

	// 等待主进程退出
	WaitForSingleObject(m_hExternalProcess, 2000);
	CloseHandle(m_hExternalProcess);
	m_hExternalProcess = nullptr;
	m_dwExternalPid = 0;

	// 释放跨进程互斥体，允许后续重新启动 getPrice.exe
	if (m_hStartMutex != nullptr)
	{
		CloseHandle(m_hStartMutex);
		m_hStartMutex = nullptr;
	}
}

void CStockFetchThread::Start()
{
	if (m_started.exchange(true))
	{
		CCommon::WriteLog(L"[FetchThread] 已启动过，跳过", g_data.m_log_path.c_str());
		return;
	}

	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	m_stopping = false;

	// 启动外部共享内存程序
	StartExternalProcess();

	// 注册共享内存行情回调并启动监听线程（被动等待事件，零延迟）
	g_tdx_client.SetQuotesCallback([this](const std::vector<QuoteItem>& items) {
		OnQuotesReceived(items);
	});
	g_tdx_client.StartListening(g_data.m_log_path);

	// 启动图表线程（即时任务/集合竞价/后台任务/图表定时获取）
	CWinThread* pThread = AfxBeginThread(ThreadProc, this, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
	if (pThread == nullptr)
	{
		m_started = false;
		return;
	}
	pThread->m_bAutoDelete = FALSE;
	m_thread_handle = pThread->m_hThread;
	m_pThread = pThread;
	ResumeThread(m_thread_handle);

	// 启动实时行情线程（所有股票价格+五档，独立运行不阻塞图表线程）
	CWinThread* pRtThread = AfxBeginThread(RealtimeThreadProc, this, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
	if (pRtThread == nullptr)
	{
		m_started = false;
		return;
	}
	pRtThread->m_bAutoDelete = FALSE;
	m_realtime_thread_handle = pRtThread->m_hThread;
	m_realtime_pThread = pRtThread;
	ResumeThread(m_realtime_thread_handle);
}

void CStockFetchThread::Stop()
{
	if (!m_started.exchange(false))
		return;

	m_stopping = true;

	// 唤醒图表线程
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_has_task = false;
		m_task = nullptr;
		m_has_callAuction_task = false;
		m_callAuction_task = nullptr;
		m_background_tasks.clear();
	}
	m_cv.notify_all();

	// 唤醒实时行情线程
	m_realtime_cv.notify_all();

	// 等待图表线程退出（最多2秒，超时则强制终止）
	if (m_thread_handle != nullptr)
	{
		DWORD waitResult = WaitForSingleObject(m_thread_handle, 2000);
		if (waitResult != WAIT_OBJECT_0)
			TerminateThread(m_thread_handle, 0);
		CloseHandle(m_thread_handle);
		m_thread_handle = nullptr;
	}
	delete m_pThread;
	m_pThread = nullptr;
	m_busy = false;
	m_callAuction_busy = false;

	// 等待实时行情线程退出（最多2秒，超时则强制终止）
	if (m_realtime_thread_handle != nullptr)
	{
		DWORD waitResult = WaitForSingleObject(m_realtime_thread_handle, 2000);
		if (waitResult != WAIT_OBJECT_0)
			TerminateThread(m_realtime_thread_handle, 0);
		CloseHandle(m_realtime_thread_handle);
		m_realtime_thread_handle = nullptr;
	}
	delete m_realtime_pThread;
	m_realtime_pThread = nullptr;
	m_realtime_last_fetch = 0;

	// 停止共享内存监听线程（阻塞等待退出）
	g_tdx_client.StopListening();

	// 关闭外部共享内存程序
	StopExternalProcess();
}

void CStockFetchThread::PostTask(Task task)
{
	if (!m_started.load() || m_stopping.load())
		return;
	if (m_busy.load())
		return;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_busy.load() || m_has_task || m_stopping.load())
			return;
		m_task = std::move(task);
		m_has_task = true;
	}
	m_cv.notify_one();
}

void CStockFetchThread::PostCallAuctionTask(Task task)
{
	if (!m_started.load() || m_stopping.load())
		return;
	if (m_callAuction_busy.load())
		return;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_callAuction_busy.load() || m_has_callAuction_task || m_stopping.load())
			return;
		m_callAuction_task = std::move(task);
		m_has_callAuction_task = true;
	}
	m_cv.notify_one();
}

void CStockFetchThread::PostBackgroundTask(Task task)
{
	if (!m_started.load() || m_stopping.load())
		return;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_stopping.load())
			return;
		m_background_tasks.push_back(std::move(task));
	}
	m_cv.notify_one();
}

void CStockFetchThread::SetFocusStockId(const std::wstring& stockId)
{
	bool changed = false;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_focus_stock_id == stockId)
			return;
		m_focus_stock_id = stockId;
		// 重置图表计时器，使线程立即获取新股数据
		memset(m_chart_last_fetch, 0, sizeof(m_chart_last_fetch));
		changed = true;
	}
	if (changed)
	{
		m_cv.notify_one();
		// 日K线数据每天只变化一次，切换股票时获取一次即可，无需定时轮询
		// 通过 PostTask 投递到工作线程执行，避免在主线程做网络请求
		PostTask([stockId]() {
			CStockFetchThread::Instance().FetchDayKLine(stockId, 750);
			});
	}
}

std::wstring CStockFetchThread::GetFocusStockId() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_focus_stock_id;
}

time_t CStockFetchThread::GetChartInterval(int type)
{
	switch (type)
	{
	case CHART_TIMELINE:		return 10;		// 分时图：盘中实时变化，10秒
	case CHART_MIN5_KLINE:		return 60;		// 5分钟K线：60秒
	case CHART_MIN30_KLINE:		return 600;		// 30分钟K线：10分钟
	case CHART_IOPV:			return 3;		// ETF基金IOPV：实时估值，3秒
	default: return 10;
	}
}

UINT CStockFetchThread::ThreadProc(LPVOID pParam)
{
	CStockFetchThread* pThis = static_cast<CStockFetchThread*>(pParam);
	pThis->Run();
	return 0;
}

void CStockFetchThread::Run()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	// 初始化时获取一次全量数据
	FetchAllData();

	while (true)
	{
		// 1. 先检查是否有投递的即时任务（日K线/集合竞价/后台任务）
		Task task;
		bool isCallAuctionTask = false;
		bool isBackgroundTask = false;
		{
			std::lock_guard<std::mutex> lock(m_mutex);

			// 优先级：常规任务 > 集合竞价 > 后台任务 > 图表定时任务（实时行情在独立线程）
			if (m_has_task)
			{
				task = std::move(m_task);
				m_task = nullptr;
				m_has_task = false;
				m_busy = true;
			}
			else if (m_has_callAuction_task)
			{
				task = std::move(m_callAuction_task);
				m_callAuction_task = nullptr;
				m_has_callAuction_task = false;
				m_callAuction_busy = true;
				isCallAuctionTask = true;
			}
			else if (!m_background_tasks.empty())
			{
				task = std::move(m_background_tasks.front());
				m_background_tasks.pop_front();
				isBackgroundTask = true;
			}
		}

		// 执行即时任务
		if (task)
		{
			if (m_stopping.load())
				return;
			try
			{
				task();
			}
			catch (CInternetException* e)
			{
				e->Delete();
			}
			catch (...)
			{
			}
			if (isCallAuctionTask)
				m_callAuction_busy = false;
			else if (!isBackgroundTask)
				m_busy = false;
			// UI刷新由1秒定时器检查dirty标识驱动，此处仅更新数据
			continue;
		}

		// 2. 检查图表定时任务
		std::wstring stockId;
		time_t now = time(nullptr);
		int chartType = -1;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_stopping.load())
				return;
			stockId = m_focus_stock_id;

			if (!stockId.empty())
			{
				// 查找间隔已到的图表任务
				// enum 按间隔从短到长排列，从前往后遍历保证短间隔任务优先执行
				for (int i = 0; i < CHART_COUNT; i++)
				{
					// IOPV 仅对基金代码获取，且仅在交易时段获取
					if (i == CHART_IOPV &&
						(!CCommon::IsFundCode(stockId) || !CCommon::IsMarketSession()))
						continue;

					time_t interval = GetChartInterval(i);
					time_t elapsed = now - m_chart_last_fetch[i];
					if (elapsed >= interval)
					{
						chartType = i;
						break;
					}
				}

				// 调度诊断日志（限频10秒）
				static time_t lastScheduleLog = 0;
				if (now - lastScheduleLog >= 10)
				{
					lastScheduleLog = now;
				}
			}
			else
			{
				// 调度诊断日志（限频10秒）
				static time_t lastEmptyLog = 0;
				if (now - lastEmptyLog >= 10)
				{
					lastEmptyLog = now;
					CCommon::WriteLog("[ChartThread] stockId empty, skip chart tasks", g_data.m_log_path.c_str());
				}
			}
		}

		// 执行图表任务
		if (chartType >= 0)
		{
			if (m_stopping.load())
				return;
			try
			{
				switch (chartType)
				{
				case CHART_TIMELINE:
					FetchTimeline(stockId);
					break;
				case CHART_MIN5_KLINE:
					FetchMin5KLine(stockId, 250);
					break;
				case CHART_MIN30_KLINE:
					FetchMin30KLine(stockId, 250);
					break;
				case CHART_IOPV:
					FetchFundIOPV(stockId);
					break;
				}
			}
			catch (CInternetException* e)
			{
				e->Delete();
			}
			catch (...)
			{
			}

			// 更新该类型的上次获取时间
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_chart_last_fetch[chartType] = time(nullptr);
			}
			// UI刷新由1秒定时器检查dirty标识驱动，此处仅更新数据
			continue;  // 执行完一个图表任务后立即检查下一个
		}

		// 2.5 其余基金净值（IOPV）定时获取：轮询每个基金，保证非关注基金也持续更新
		// 关注基金由上面的 CHART_IOPV 高频获取，这里只负责剩余基金
		if (CCommon::IsMarketSession())
		{
			bool doSweep = false;
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				if (m_stopping.load())
					return;
				time_t now = time(nullptr);
				if (now - m_all_fund_iopv_last_fetch >= ALL_FUND_IOPV_INTERVAL)
				{
					m_all_fund_iopv_last_fetch = now;
					doSweep = true;
				}
			}
			if (doSweep)
			{
				if (m_stopping.load())
					return;
				try
				{
					FetchAllFundsIOPV();
				}
				catch (CInternetException* e)
				{
					e->Delete();
				}
				catch (...)
				{
				}
				continue;  // 立即检查下一个任务/到期项
			}
		}

		// 3. 没有即时任务也没有到时的图表任务，计算等待时间
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			if (m_stopping.load())
				return;

			// 计算最近的图表任务还需要等多久
			time_t minWaitSec = 1;  // 默认1秒轮询一次（确保能及时响应新投递的任务）
			if (!m_focus_stock_id.empty())
			{
				now = time(nullptr);
				for (int i = 0; i < CHART_COUNT; i++)
				{
					if (i == CHART_IOPV && !CCommon::IsFundCode(m_focus_stock_id))
						continue;
					time_t interval = GetChartInterval(i);
					time_t elapsed = now - m_chart_last_fetch[i];
					time_t remaining = interval - elapsed;
					if (remaining > 0 && remaining < minWaitSec)
						minWaitSec = remaining;
				}
			}

			// 等待：可被 PostTask/PostBackgroundTask/SetFocusStockId/Stop 提前唤醒
			m_cv.wait_for(lock, std::chrono::seconds(minWaitSec));
		}
	}
}

UINT CStockFetchThread::RealtimeThreadProc(LPVOID pParam)
{
	CStockFetchThread* pThis = static_cast<CStockFetchThread*>(pParam);
	pThis->RunRealtime();
	return 0;
}

void CStockFetchThread::RunRealtime()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	time_t lastReconnectAttempt = 0;

	while (true)
	{
		if (m_stopping.load())
			return;

		bool bTradingSession = CCommon::IsMarketSession();
		bool bFullDay = g_data.m_setting_data.m_full_day == 1;
		bool bHttpFetch = bTradingSession || bFullDay;

		// ===== 非交易时段：等待，但可被 Stop 唤醒 =====
		if (!bHttpFetch)
		{
			std::unique_lock<std::mutex> lock(m_realtime_mutex);
			if (m_stopping.load())
				return;
			m_realtime_cv.wait_for(lock, std::chrono::seconds(5));
			continue;
		}

		// ===== 交易/全日时段：通过 HTTP 获取数据 =====
		time_t now = time(nullptr);
		time_t interval = bTradingSession ? REALTIME_INTERVAL_TRADING : REALTIME_INTERVAL_LUNCH;

		// 共享内存未连接时，定期尝试重连（Python 端可能后启动）
		if (!g_tdx_client.IsListening() && now - lastReconnectAttempt >= 5)
		{
			lastReconnectAttempt = now;
			g_tdx_client.StartListening(g_data.m_log_path);
		}

		if (g_data.m_setting_data.m_stock_codes.empty())
		{
			// 无股票代码时重置文本
			if (m_realtime_last_fetch != 0)
			{
				g_data.ResetText();
				m_realtime_last_fetch = now;
			}
			std::unique_lock<std::mutex> lock(m_realtime_mutex);
			if (m_stopping.load())
				return;
			m_realtime_cv.wait_for(lock, std::chrono::seconds(5));
			continue;
		}

		// HTTP 补充限频：
		// - 共享内存已连接：只获取港股（非A股），共享内存负责A股
		// - 共享内存未连接：获取所有数据（含A股）
		time_t elapsed = now - m_last_http_supplement;
		if (elapsed >= interval)
		{
			FetchRealtimeByHttp(g_tdx_client.IsListening());
			m_last_http_supplement = time(nullptr);
			m_realtime_last_fetch = m_last_http_supplement;
			continue;
		}

		// 等待到下次获取，可被 Stop 提前唤醒
		std::unique_lock<std::mutex> lock(m_realtime_mutex);
		if (m_stopping.load())
			return;
		m_realtime_cv.wait_for(lock, std::chrono::seconds(interval - elapsed));
	}
}

void CStockFetchThread::OnQuotesReceived(const std::vector<QuoteItem>& items)
{
	// 监听线程回调：仅更新 DataManager（线程安全，内部有锁）
	// HTTP 补充（港股等非A股数据）由 RunRealtime 统一限频调度
	g_data.UpdateRealtimeFromQuotes(items);
	m_realtime_last_fetch = time(nullptr);
	// 通知浮动窗口盘口数据更新
	Stock::Instance().NotifyFloatingWndOrderBookUpdate();
}

// ===== 数据获取编排方法实现（fetcher + apply）=====
// 每个方法完成"获取→解析→存储"的完整流程，由工作线程调用

void CStockFetchThread::FetchRealtimeByHttp(bool onlyNonAG)
{
	const auto& codes = g_data.m_setting_data.m_stock_codes;
	if (codes.empty())
	{
		g_data.ResetText();
		return;
	}

	// 实时行情（新浪）
	std::vector<std::wstring> outCodes;
	std::string resp;
	if (g_http_fetcher.FetchRealtimeHtml(codes, onlyNonAG, outCodes, resp))
		g_data.ApplyRealtimeData(outCodes, resp);

	// 内外盘（腾讯）：onlyNonAG=true 时 includeAG=false（仅非A股），反之含A股
	std::string ioResp;
	if (g_http_fetcher.FetchInnerOuterHtml(codes, !onlyNonAG, ioResp))
		g_data.ApplyInnerOuterData(ioResp);

	// 通知浮动窗口盘口数据更新
	Stock::Instance().NotifyFloatingWndOrderBookUpdate();
}

void CStockFetchThread::FetchCallAuction()
{
	std::vector<std::wstring> outCodes;
	std::string resp;
	if (g_http_fetcher.FetchCallAuctionHtml(g_data.m_setting_data.m_stock_codes, outCodes, resp))
		g_data.ApplyCallAuctionData(resp);
}

void CStockFetchThread::FetchTimeline(const std::wstring& code)
{
	std::string resp;
	bool ok = g_http_fetcher.FetchTimeline(code, resp);
	g_data.ApplyTimeline(code, resp, ok);
}

void CStockFetchThread::FetchDayKLine(const std::wstring& code, int days)
{
	std::string resp;
	bool ok = g_http_fetcher.FetchDayKLine(code, days, resp);
	g_data.ApplyDayKLine(code, resp, ok);
}

void CStockFetchThread::FetchMin5KLine(const std::wstring& code, int datalen)
{
	std::string resp;
	bool ok = g_http_fetcher.FetchMin5KLine(code, datalen, resp);
	g_data.ApplyMin5KLine(code, resp, ok);
}

void CStockFetchThread::FetchMin30KLine(const std::wstring& code, int datalen)
{
	std::string resp;
	bool ok = g_http_fetcher.FetchMin30KLine(code, datalen, resp);
	g_data.ApplyMin30KLine(code, resp, ok);
}

void CStockFetchThread::FetchFundIOPV(const std::wstring& code)
{
	std::string resp;
	bool ok = g_http_fetcher.FetchFundIOPV(code, resp);
	g_data.ApplyFundIOPV(code, resp, ok);
}

void CStockFetchThread::FetchAllFundsIOPV()
{
	// 收集所有基金代码，排除关注基金（关注基金由 CHART_IOPV 高频获取，这里只负责其余基金）
	const auto& codes = g_data.m_setting_data.m_stock_codes;
	std::wstring focusId = GetFocusStockId();
	std::vector<std::wstring> fundCodes;
	fundCodes.reserve(codes.size());
	for (const auto& code : codes)
	{
		if (CCommon::IsFundCode(code) && code != focusId)
			fundCodes.push_back(code);
	}
	if (fundCodes.empty())
		return;

	// 轮询取出本轮要获取的基金
	std::wstring fundCode;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_all_fund_iopv_index >= fundCodes.size())
			m_all_fund_iopv_index = 0;
		fundCode = fundCodes[m_all_fund_iopv_index];
		m_all_fund_iopv_index++;
	}

	FetchFundIOPV(fundCode);
}

void CStockFetchThread::FetchStockBasic(const std::wstring& code)
{
	STOCK::Volume shares = 0;
	bool ok = g_http_fetcher.FetchStockBasicCirculating(code, shares);
	g_data.ApplyStockBasic(code, shares, ok);
}

void CStockFetchThread::FetchChipDistribution(const std::wstring& code)
{
	// 1. 检查数据库是否有当日缓存
	if (g_data.TryApplyCachedChipDistribution(code))
		return;

	// 2. 获取流通股本（筹码计算需要）
	STOCK::Volume circulatingAShares = g_data.GetCirculatingAShares(code);
	if (circulatingAShares <= 0)
	{
		FetchStockBasic(code);
		circulatingAShares = g_data.GetCirculatingAShares(code);
	}

	// 3. 获取筹码K线并计算
	std::vector<STOCK::ChipKLinePoint> klines;
	if (g_http_fetcher.FetchChipKLines(code, circulatingAShares, klines))
		g_data.ApplyChipDistribution(code, klines, circulatingAShares);
}

void CStockFetchThread::FetchAllData()
{
	// 初始化时获取一次实时行情和集合竞价数据
	FetchRealtimeByHttp(false);
	FetchCallAuction();

	// 预加载所有股票的日K线、基础数据和筹码分布
	for (const auto& code : g_data.m_setting_data.m_stock_codes)
	{
		if (m_stopping.load())
			return;
		if (!g_data.HasKLineCache(code, STOCK::Period::DAY))
			FetchDayKLine(code, 750);
		if (!g_data.HasKLineCache(code, STOCK::Period::MIN5))
			FetchMin5KLine(code, 250);
		if (!g_data.HasKLineCache(code, STOCK::Period::MIN30))
			FetchMin30KLine(code, 250);
		if (CCommon::IsFundCode(code) )
			FetchFundIOPV(code);
		FetchStockBasic(code);
		FetchChipDistribution(code);
	}

	// 获取关注股票的图表数据
	std::wstring focusId = GetFocusStockId();
	if (!focusId.empty())
	{
		FetchTimeline(focusId);		
	}
}