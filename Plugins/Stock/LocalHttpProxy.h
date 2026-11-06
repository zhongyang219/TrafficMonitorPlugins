#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

// 本地 HTTP CONNECT 代理 + SOCKS5 客户端
//
// 背景：WinINet 不支持 SOCKS5 代理，只支持 HTTP 代理。而 curl.exe 通过 SOCKS5
// 访问 HTTPS 时，TLS 指纹（JA3）会被东方财富等服务的 WAF 拦截。浏览器和 WinINet
// 都用 Windows 系统 SChannel TLS 栈，指纹一致，可正常访问。
//
// 解决方案：在本地启动一个 HTTP 代理（监听 127.0.0.1:随机端口），把 WinINet 的
// 请求通过 SOCKS5 代理转发到目标服务器。这样 WinINet 用系统 SChannel 做 TLS 握手，
// 指纹和浏览器一致，绕过 WAF 拦截。
//
// 工作流程：
//  1. WinINet 发送 "CONNECT push2.eastmoney.com:443 HTTP/1.1" 到本地代理（HTTPS）
//     或 "GET http://host/path HTTP/1.1"（HTTP）
//  2. 本地代理解析目标主机和端口
//  3. 本地代理通过 SOCKS5 代理建立到目标的 TCP 连接
//  4. CONNECT 请求：返回 "200 Connection Established"，建立隧道，双向转发
//     非 CONNECT 请求：转发修改后的请求（URL 改为路径），双向转发响应
//  5. TLS 握手由 WinINet 完成（系统 SChannel，指纹和浏览器一致）
class CLocalHttpProxy
{
public:
	static CLocalHttpProxy& Instance();

	// 启动本地代理
	// socks5_proxy: SOCKS5 代理地址，如 "127.0.0.1:1080"
	// 已启动时若 socks5_proxy 变化，会先停止再重启
	bool Start(const std::wstring& socks5_proxy);

	// 停止代理
	void Stop();

	// 获取本地代理地址，如 "127.0.0.1:12345"（未启动时返回空）
	std::wstring GetProxyAddress() const { return m_proxy_address; }

	// 是否已启动
	bool IsRunning() const { return m_running.load(); }

private:
	CLocalHttpProxy();
	~CLocalHttpProxy();
	CLocalHttpProxy(const CLocalHttpProxy&) = delete;
	CLocalHttpProxy& operator=(const CLocalHttpProxy&) = delete;

	// 监听线程
	void ListenLoop();

	// 工作线程（处理一个客户端连接）
	void ClientLoop(SOCKET clientSocket);

	// 通过 SOCKS5 建立到目标的 TCP 连接
	// host: 目标主机名（域名），port: 目标端口
	// 成功返回连接 socket，失败返回 INVALID_SOCKET
	SOCKET ConnectViaSocks5(const std::string& host, int port);

	// 解析 SOCKS5 代理地址（"127.0.0.1:1080" → IP 和端口）
	bool ParseSocks5Address(const std::wstring& addr, std::string& ip, int& port);

	// 初始化 WinSock（引用计数）
	bool EnsureWinSock();

private:
	std::wstring m_socks5_proxy;        // SOCKS5 代理地址（原始）
	std::string m_socks5_ip;            // SOCKS5 代理 IP
	int m_socks5_port;                  // SOCKS5 代理端口
	SOCKET m_listen_socket;             // 监听 socket
	int m_listen_port;                  // 监听端口
	std::wstring m_proxy_address;       // 本地代理地址 "127.0.0.1:port"
	std::thread m_listen_thread;        // 监听线程
	std::atomic<bool> m_running;        // 是否正在运行
	std::atomic<bool> m_stopping;       // 是否正在停止

	// 活动工作线程计数（用于 Stop 时等待）
	std::atomic<int> m_active_workers;

	// WinSock 引用计数
	static std::atomic<int> s_wsa_ref_count;
	static std::mutex s_wsa_mutex;
};
