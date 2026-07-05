#include "pch.h"
#include "StockFetchThread.h"
#include <afxwin.h>
#include <afxinet.h>

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

void CStockFetchThread::Start()
{
	if (m_started.exchange(true))
		return;

	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	m_stopping = false;
	// CREATE_SUSPENDED 以便安全获取线程句柄并设置 m_bAutoDelete = FALSE，
	// 从而在 Stop() 中可对其等待并手动释放
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
}

void CStockFetchThread::Stop()
{
	if (!m_started.exchange(false))
		return;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_stopping = true;
		m_has_task = false;
		m_task = nullptr;
		m_has_callAuction_task = false;
		m_callAuction_task = nullptr;
	}
	m_cv.notify_all();

	if (m_thread_handle != nullptr)
	{
		// 等待工作线程退出（HTTP 请求可能尚在执行，给予充足但有限的时间）
		WaitForSingleObject(m_thread_handle, 5000);
		CloseHandle(m_thread_handle);
		m_thread_handle = nullptr;
	}
	// m_bAutoDelete = FALSE，需手动释放 CWinThread 对象
	delete m_pThread;
	m_pThread = nullptr;
	m_busy = false;
	m_callAuction_busy = false;
}

void CStockFetchThread::PostTask(Task task)
{
	if (!m_started.load() || m_stopping.load())
		return;
	// 快速路径：线程正忙则直接丢弃，避免请求堆积
	if (m_busy.load())
		return;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		// 持锁后二次检查，防止与 Run() 中的取任务逻辑竞争
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
	// 快速路径：集合竞价任务正忙则直接丢弃
	if (m_callAuction_busy.load())
		return;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		// 持锁后二次检查
		if (m_callAuction_busy.load() || m_has_callAuction_task || m_stopping.load())
			return;
		m_callAuction_task = std::move(task);
		m_has_callAuction_task = true;
	}
	m_cv.notify_one();
}

UINT CStockFetchThread::ThreadProc(LPVOID pParam)
{
	CStockFetchThread* pThis = static_cast<CStockFetchThread*>(pParam);
	pThis->Run();
	return 0;
}

void CStockFetchThread::Run()
{
	// 工作线程设置插件模块状态，保证 MFC 资源/网络类（CInternetSession 等）正常工作
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	while (true)
	{
		Task task;
		bool isCallAuctionTask = false;
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_cv.wait(lock, [this]() {
				return m_stopping.load() || m_has_task || m_has_callAuction_task;
				});
			if (m_stopping.load())
				return;
			// 优先执行常规任务，再执行集合竞价任务
			if (m_has_task)
			{
				task = std::move(m_task);
				m_task = nullptr;
				m_has_task = false;
				m_busy = true;
				isCallAuctionTask = false;
			}
			else if (m_has_callAuction_task)
			{
				task = std::move(m_callAuction_task);
				m_callAuction_task = nullptr;
				m_has_callAuction_task = false;
				m_callAuction_busy = true;
				isCallAuctionTask = true;
			}
		}

		// 仅执行数据获取任务，不进行任何 UI 交互
		if (task)
		{
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
		}

		if (isCallAuctionTask)
			m_callAuction_busy = false;
		else
			m_busy = false;
	}
}