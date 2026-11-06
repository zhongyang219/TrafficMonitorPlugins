#include "pch.h"
#include "TdxTcpClient.h"
#include "Common.h"
#include <cstring>

// 全局共享内存行情客户端实例（从 DataManager.cpp 迁入）
CTdxTcpClient g_tdx_client;

CTdxTcpClient::CTdxTcpClient()
	: m_hMap(nullptr)
	, m_hEventA(nullptr)
	, m_hEventB(nullptr)
	, m_pShare(nullptr)
	, m_last_seq(0)
	, m_connected(false)
{
}

CTdxTcpClient::~CTdxTcpClient()
{
	StopListening();
	Disconnect();
}

bool CTdxTcpClient::Connect(const char* /*ip*/, uint16_t /*port*/)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_connected)
		return true;

	// 打开双缓冲共享内存（总长度 2 * sizeof(ShareMemHeader)）
	m_hMap = OpenFileMappingA(FILE_MAP_READ, FALSE, SHARE_NAME);
	if (!m_hMap)
		return false;

	// 映射整个双缓冲区
	m_pShare = (ShareMemHeader*)MapViewOfFile(m_hMap, FILE_MAP_READ, 0, 0, 0);
	if (!m_pShare)
	{
		CloseHandle(m_hMap);
		m_hMap = nullptr;
		return false;
	}

	// 打开两个自动重置事件
	m_hEventA = OpenEventA(EVENT_ALL_ACCESS, FALSE, EVENT_NAME_A);
	if (!m_hEventA)
	{
		UnmapViewOfFile(m_pShare);
		m_pShare = nullptr;
		CloseHandle(m_hMap);
		m_hMap = nullptr;
		return false;
	}
	m_hEventB = OpenEventA(EVENT_ALL_ACCESS, FALSE, EVENT_NAME_B);
	if (!m_hEventB)
	{
		CloseHandle(m_hEventA);
		m_hEventA = nullptr;
		UnmapViewOfFile(m_pShare);
		m_pShare = nullptr;
		CloseHandle(m_hMap);
		m_hMap = nullptr;
		return false;
	}

	m_last_seq = 0;
	m_connected = true;
	return true;
}

void CTdxTcpClient::Disconnect()
{
	StopListening();  // 先停止监听线程，避免访问已释放的共享内存
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_pShare)
	{
		UnmapViewOfFile(m_pShare);
		m_pShare = nullptr;
	}
	if (m_hMap)
	{
		CloseHandle(m_hMap);
		m_hMap = nullptr;
	}
	if (m_hEventA)
	{
		CloseHandle(m_hEventA);
		m_hEventA = nullptr;
	}
	if (m_hEventB)
	{
		CloseHandle(m_hEventB);
		m_hEventB = nullptr;
	}
	m_connected = false;
	m_last_seq = 0;
}

bool CTdxTcpClient::IsConnected() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_connected;
}

// 从指定偏移读取一个缓冲区的行情数据（不加锁，调用方负责同步）
// bufIndex=0 读缓冲A（偏移0），bufIndex=1 读缓冲B（偏移 sizeof(ShareMemHeader)）
static void ReadBuffer(ShareMemHeader* base, int bufIndex, std::vector<QuoteItem>& outItems, unsigned int& outSeq)
{
	outSeq = 0;
	ShareMemHeader* hdr = reinterpret_cast<ShareMemHeader*>(
		reinterpret_cast<char*>(base) + bufIndex * sizeof(ShareMemHeader));

	int cnt = hdr->item_count;
	if (cnt < 0 || cnt > MAX_WATCH_NUM)
		return;

	outItems.clear();
	outItems.reserve(cnt);
	for (int i = 0; i < cnt; i++)
		outItems.push_back(hdr->items[i]);
	outSeq = hdr->seq;
}

void CTdxTcpClient::SetQuotesCallback(OnQuotesCallback callback)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_callback = callback;
}

void CTdxTcpClient::StartListening(const std::wstring& logPath)
{
	if (m_listening.load())
		return;

	m_log_path = logPath;

	// 确保共享内存已连接
	if (!IsConnected())
	{
		if (!Connect())
		{
			CCommon::WriteLog(L"[TDX] 共享内存打开失败，监听线程未启动（Python端未启动？）", m_log_path.c_str());
			return;
		}
		CCommon::WriteLog(L"[TDX] 共享内存连接成功，启动监听线程", m_log_path.c_str());
	}

	// 创建退出信号事件（手动重置）
	m_hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	if (!m_hStopEvent)
		return;

	m_listening.store(true);
	m_listen_thread = std::thread(&CTdxTcpClient::ListenProc, this);
}

void CTdxTcpClient::StopListening()
{
	if (!m_listening.load())
		return;

	m_listening.store(false);
	if (m_hStopEvent)
		SetEvent(m_hStopEvent);

	if (m_listen_thread.joinable())
		m_listen_thread.join();

	if (m_hStopEvent)
	{
		CloseHandle(m_hStopEvent);
		m_hStopEvent = nullptr;
	}
}

void CTdxTcpClient::ListenProc()
{
	// 双缓冲被动等待：同时监听 EventA、EventB、StopEvent
	HANDLE handles[3] = { m_hEventA, m_hEventB, m_hStopEvent };
	unsigned int old_seq = 0;
	unsigned int cur_seq = 0;
	std::vector<QuoteItem> items;

	while (m_listening.load())
	{
		DWORD ret = WaitForMultipleObjects(3, handles, FALSE, INFINITE);

		// 退出信号
		if (!m_listening.load() || ret == WAIT_OBJECT_0 + 2)
			return;

		// 行情事件触发：按返回值确定读取哪个缓冲区
		if (ret == WAIT_OBJECT_0 || ret == WAIT_OBJECT_0 + 1)
		{
			int bufIndex = (ret == WAIT_OBJECT_0) ? 0 : 1;
			cur_seq = 0;
			items.clear();

			{
				std::lock_guard<std::mutex> lock(m_mutex);
				if (!m_connected || !m_pShare)
					continue;

				old_seq = m_last_seq;
				ReadBuffer(m_pShare, bufIndex, items, cur_seq);
				m_last_seq = cur_seq;
			}

			// seq 变化才回调，避免重复处理相同数据
			if (cur_seq != 0 && cur_seq != old_seq && !items.empty() && m_callback)
				m_callback(items);
		}
	}
}