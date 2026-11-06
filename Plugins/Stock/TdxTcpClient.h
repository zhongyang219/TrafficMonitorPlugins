#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <windows.h>

// 共享内存行情数据结构（与Python端一致）
#define MAX_WATCH_NUM 32
// 双缓冲共享内存（总长度 2 * sizeof(ShareMemHeader)）
#define SHARE_NAME "Local\\TdxQuoteShareDoubleBuf"
// 双缓冲事件：A 对应偏移0，B 对应偏移 sizeof(ShareMemHeader)
#define EVENT_NAME_A "Local\\TdxQuoteEventA"
#define EVENT_NAME_B "Local\\TdxQuoteEventB"

#pragma pack(push, 1)

typedef struct QuoteItem
{
	char code[16];       // 股票代码
	char name[32];    	 // 股票名称
	double price;        // 现价
	double open;         // 开盘
	double high;         // 最高
	double low;          // 最低
	double pre_close;    // 昨收

	double bid1;         // 买1价
	long long bid_vol1;  // 买1量
	double bid2;
	long long bid_vol2;
	double bid3;
	long long bid_vol3;
	double bid4;
	long long bid_vol4;
	double bid5;
	long long bid_vol5;

	double ask1;         // 卖1价
	long long ask_vol1;  // 卖1量
	double ask2;
	long long ask_vol2;
	double ask3;
	long long ask_vol3;
	double ask4;
	long long ask_vol4;
	double ask5;
	long long ask_vol5;

	long long vol;       // 总手
	double amount;       // 成交额
	long long inner_vol; // 内盘
	long long outer_vol; // 外盘
	long long cur_vol;   // 现手
} QuoteItem;
#pragma pack(pop)
struct ShareMemHeader
{
	unsigned int seq;                    // 序列号（Python端每次写入递增）
	int item_count;                      // 行情条目数
	QuoteItem items[MAX_WATCH_NUM];      // 行情数据
};
// 行情数据到达回调（在监听线程中被调用）
using OnQuotesCallback = std::function<void(const std::vector<QuoteItem>&)>;

// 通达信共享内存行情客户端
// 通过读取Python端写入的共享内存获取实时行情，零延迟无网络开销
class CTdxTcpClient
{
public:
	CTdxTcpClient();
	~CTdxTcpClient();

	// 打开共享内存和互斥体（线程安全）
	bool Connect(const char* ip = nullptr, uint16_t port = 0);
	// 关闭共享内存（线程安全）
	void Disconnect();
	// 是否已连接（共享内存已打开）
	bool IsConnected() const;

	// 设置行情数据到达回调（监听线程读到新数据时调用）
	void SetQuotesCallback(OnQuotesCallback callback);
	// 启动监听线程（被动等待事件，数据到达立即回调）
	// 内部会先 Connect 确保共享内存已打开，失败则不启动
	void StartListening(const std::wstring& logPath);
	// 停止监听线程（阻塞等待线程退出）
	void StopListening();
	// 监听线程是否在运行
	bool IsListening() const { return m_listening.load(); }

private:
	mutable std::mutex m_mutex;
	HANDLE m_hMap;       // 共享内存句柄
	HANDLE m_hEventA;    // 缓冲区A事件（偏移0）
	HANDLE m_hEventB;    // 缓冲区B事件（偏移 sizeof(ShareMemHeader)）
	ShareMemHeader* m_pShare;  // 共享内存映射指针（基址，双缓冲总长 2*sizeof(ShareMemHeader)）
	unsigned int m_last_seq;   // 上次读取的序列号
	bool m_connected;

	// 监听线程：被动等待事件，避免2秒轮询
	std::thread m_listen_thread;
	std::atomic<bool> m_listening{ false };
	HANDLE m_hStopEvent{ nullptr };  // 手动重置事件，用于通知监听线程退出
	OnQuotesCallback m_callback;
	std::wstring m_log_path;

	// 监听线程主循环
	void ListenProc();
};

// 全局实例（供工作线程直接访问共享内存行情）
extern CTdxTcpClient g_tdx_client;
