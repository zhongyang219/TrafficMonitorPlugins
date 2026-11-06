#include "pch.h"
#include "LocalHttpProxy.h"
#include "Common.h"
#include "DataManager.h"
#include <sstream>

// 静态成员定义
std::atomic<int> CLocalHttpProxy::s_wsa_ref_count(0);
std::mutex CLocalHttpProxy::s_wsa_mutex;

CLocalHttpProxy& CLocalHttpProxy::Instance()
{
	static CLocalHttpProxy instance;
	return instance;
}

CLocalHttpProxy::CLocalHttpProxy()
	: m_socks5_port(0)
	, m_listen_socket(INVALID_SOCKET)
	, m_listen_port(0)
	, m_running(false)
	, m_stopping(false)
	, m_active_workers(0)
{
}

CLocalHttpProxy::~CLocalHttpProxy()
{
	Stop();
}

bool CLocalHttpProxy::EnsureWinSock()
{
	std::lock_guard<std::mutex> lock(s_wsa_mutex);
	if (s_wsa_ref_count.fetch_add(1) == 0)
	{
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			s_wsa_ref_count.fetch_sub(1);
			return false;
		}
	}
	return true;
}

bool CLocalHttpProxy::ParseSocks5Address(const std::wstring& addr, std::string& ip, int& port)
{
	// addr 形如 "127.0.0.1:1080"
	size_t colon = addr.find(L':');
	if (colon == std::wstring::npos) return false;
	std::wstring wip = addr.substr(0, colon);
	std::wstring wport = addr.substr(colon + 1);
	try {
		port = std::stoi(wport);
	} catch (...) {
		return false;
	}
	// IP 地址都是 ASCII，逐字符转换（避免 wchar_t→char 截断警告）
	ip.reserve(wip.size());
	for (wchar_t ch : wip)
		ip.push_back(static_cast<char>(ch));
	return !ip.empty() && port > 0;
}

bool CLocalHttpProxy::Start(const std::wstring& socks5_proxy)
{
	// 已启动且地址未变，直接返回
	if (m_running.load() && m_socks5_proxy == socks5_proxy)
		return true;

	// 地址变化或重启，先停止
	if (m_running.load())
		Stop();

	std::string ip;
	int port;
	if (!ParseSocks5Address(socks5_proxy, ip, port))
	{
		CCommon::WriteLog(L"[LocalHttpProxy] invalid socks5 address", g_data.m_log_path.c_str());
		return false;
	}

	m_socks5_proxy = socks5_proxy;
	m_socks5_ip = ip;
	m_socks5_port = port;

	if (!EnsureWinSock())
	{
		CCommon::WriteLog(L"[LocalHttpProxy] WSAStartup failed", g_data.m_log_path.c_str());
		return false;
	}

	// 创建监听 socket
	m_listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_listen_socket == INVALID_SOCKET)
	{
		CCommon::WriteLog(L"[LocalHttpProxy] create listen socket failed", g_data.m_log_path.c_str());
		return false;
	}

	// 允许地址重用，避免重启时绑定失败
	BOOL opt = TRUE;
	setsockopt(m_listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

	// 绑定 127.0.0.1:0（随机端口）
	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = 0;
	if (::bind(m_listen_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		CCommon::WriteLog(L"[LocalHttpProxy] bind failed", g_data.m_log_path.c_str());
		closesocket(m_listen_socket);
		m_listen_socket = INVALID_SOCKET;
		return false;
	}

	if (listen(m_listen_socket, 10) == SOCKET_ERROR)
	{
		CCommon::WriteLog(L"[LocalHttpProxy] listen failed", g_data.m_log_path.c_str());
		closesocket(m_listen_socket);
		m_listen_socket = INVALID_SOCKET;
		return false;
	}

	// 获取绑定的端口
	sockaddr_in bound = {};
	int boundLen = sizeof(bound);
	if (getsockname(m_listen_socket, (sockaddr*)&bound, &boundLen) == SOCKET_ERROR)
	{
		closesocket(m_listen_socket);
		m_listen_socket = INVALID_SOCKET;
		return false;
	}
	m_listen_port = ntohs(bound.sin_port);

	// 构造代理地址
	m_proxy_address = L"127.0.0.1:" + std::to_wstring(m_listen_port);

	m_stopping.store(false);
	m_running.store(true);

	// 启动监听线程
	m_listen_thread = std::thread(&CLocalHttpProxy::ListenLoop, this);

	return true;
}

void CLocalHttpProxy::Stop()
{
	if (!m_running.load()) return;

	m_stopping.store(true);

	// 关闭监听 socket，使 accept 返回
	if (m_listen_socket != INVALID_SOCKET)
	{
		closesocket(m_listen_socket);
		m_listen_socket = INVALID_SOCKET;
	}

	// 等待监听线程结束
	if (m_listen_thread.joinable())
		m_listen_thread.join();

	// 等待所有工作线程结束（最多等待 3 秒）
	for (int i = 0; i < 30 && m_active_workers.load() > 0; ++i)
		Sleep(100);

	m_running.store(false);
	m_stopping.store(false);
	m_proxy_address.clear();
}

void CLocalHttpProxy::ListenLoop()
{
	while (!m_stopping.load())
	{
		sockaddr_in clientAddr = {};
		int clientLen = sizeof(clientAddr);
		SOCKET clientSocket = accept(m_listen_socket, (sockaddr*)&clientAddr, &clientLen);
		if (clientSocket == INVALID_SOCKET)
			break;  // 监听 socket 已关闭

		if (m_stopping.load())
		{
			closesocket(clientSocket);
			break;
		}

		// 为每个连接创建工作线程（detached）
		m_active_workers.fetch_add(1);
		std::thread worker(&CLocalHttpProxy::ClientLoop, this, clientSocket);
		worker.detach();
	}
}

SOCKET CLocalHttpProxy::ConnectViaSocks5(const std::string& host, int port)
{
	// 连接到 SOCKS5 代理
	SOCKET socks = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (socks == INVALID_SOCKET) return INVALID_SOCKET;

	// 设置连接超时（10 秒）
	DWORD timeout = 10000;
	setsockopt(socks, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
	setsockopt(socks, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

	sockaddr_in proxyAddr = {};
	proxyAddr.sin_family = AF_INET;
	proxyAddr.sin_port = htons((u_short)m_socks5_port);
	if (inet_pton(AF_INET, m_socks5_ip.c_str(), &proxyAddr.sin_addr) != 1)
	{
		closesocket(socks);
		return INVALID_SOCKET;
	}

	if (connect(socks, (sockaddr*)&proxyAddr, sizeof(proxyAddr)) == SOCKET_ERROR)
	{
		closesocket(socks);
		return INVALID_SOCKET;
	}

	// SOCKS5 握手：版本5，1种认证方法，无认证
	unsigned char hello[3] = { 0x05, 0x01, 0x00 };
	if (send(socks, (const char*)hello, 3, 0) != 3)
	{
		closesocket(socks);
		return INVALID_SOCKET;
	}

	unsigned char resp[2] = {};
	if (recv(socks, (char*)resp, 2, 0) != 2 || resp[0] != 0x05 || resp[1] != 0x00)
	{
		closesocket(socks);
		return INVALID_SOCKET;
	}

	// SOCKS5 CONNECT 请求：版本5，CONNECT，保留，域名类型，域名长度，域名，端口
	std::vector<unsigned char> req;
	req.push_back(0x05);  // 版本
	req.push_back(0x01);  // CONNECT
	req.push_back(0x00);  // 保留
	req.push_back(0x03);  // 域名类型
	req.push_back((unsigned char)host.size());  // 域名长度
	req.insert(req.end(), host.begin(), host.end());  // 域名
	req.push_back((unsigned char)((port >> 8) & 0xFF));  // 端口高字节
	req.push_back((unsigned char)(port & 0xFF));  // 端口低字节

	if (send(socks, (const char*)req.data(), (int)req.size(), 0) != (int)req.size())
	{
		closesocket(socks);
		return INVALID_SOCKET;
	}

	// 接收响应：前 4 字节固定
	unsigned char connectResp[262] = {};  // 最大响应长度
	int received = 0;
	while (received < 4)
	{
		int n = recv(socks, (char*)connectResp + received, (int)(sizeof(connectResp) - received), 0);
		if (n <= 0)
		{
			closesocket(socks);
			return INVALID_SOCKET;
		}
		received += n;
	}

	// 检查响应
	if (connectResp[0] != 0x05 || connectResp[1] != 0x00)
	{
		closesocket(socks);
		return INVALID_SOCKET;
	}

	// 根据地址类型，接收剩余的响应
	int addrFieldLen = 0;
	switch (connectResp[3])
	{
	case 0x01: addrFieldLen = 4; break;   // IPv4
	case 0x03: addrFieldLen = connectResp[4]; break;  // 域名（第5字节是长度）
	case 0x04: addrFieldLen = 16; break;  // IPv6
	default:
		closesocket(socks);
		return INVALID_SOCKET;
	}

	// 计算还需要接收多少字节
	int needTotal;
	if (connectResp[3] == 0x03)
		needTotal = 4 + 1 + addrFieldLen + 2;  // 4 + 长度字节 + 域名 + 端口
	else
		needTotal = 4 + addrFieldLen + 2;  // 4 + 地址 + 端口

	while (received < needTotal)
	{
		int n = recv(socks, (char*)connectResp + received, needTotal - received, 0);
		if (n <= 0)
		{
			closesocket(socks);
			return INVALID_SOCKET;
		}
		received += n;
	}

	return socks;  // 连接成功
}

void CLocalHttpProxy::ClientLoop(SOCKET clientSocket)
{
	// 设置超时（30 秒）
	DWORD timeout = 30000;
	setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
	setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

	// 读取客户端请求头（直到 \r\n\r\n）
	char buf[8192];
	int totalRead = 0;
	char* headerEnd = nullptr;
	while (totalRead < (int)sizeof(buf) - 1)
	{
		int n = recv(clientSocket, buf + totalRead, (int)sizeof(buf) - 1 - totalRead, 0);
		if (n <= 0)
		{
			closesocket(clientSocket);
			m_active_workers.fetch_sub(1);
			return;
		}
		totalRead += n;
		buf[totalRead] = '\0';
		headerEnd = strstr(buf, "\r\n\r\n");
		if (headerEnd != nullptr) break;
	}

	if (headerEnd == nullptr)
	{
		const char* resp = "HTTP/1.1 400 Bad Request\r\n\r\n";
		send(clientSocket, resp, (int)strlen(resp), 0);
		closesocket(clientSocket);
		m_active_workers.fetch_sub(1);
		return;
	}

	int headerLen = (int)(headerEnd - buf) + 4;  // 包含 \r\n\r\n

	// 解析第一行
	char* lineEnd = strstr(buf, "\r\n");
	if (lineEnd == nullptr)
	{
		closesocket(clientSocket);
		m_active_workers.fetch_sub(1);
		return;
	}

	std::string firstLine(buf, lineEnd - buf);
	SOCKET remoteSocket = INVALID_SOCKET;

	if (firstLine.compare(0, 8, "CONNECT ") == 0)
	{
		// CONNECT 请求：CONNECT host:port HTTP/1.1
		std::string hostPort = firstLine.substr(8);
		size_t spacePos = hostPort.find(' ');
		if (spacePos != std::string::npos)
			hostPort = hostPort.substr(0, spacePos);

		size_t colonPos = hostPort.find(':');
		std::string host;
		int port = 443;
		if (colonPos != std::string::npos)
		{
			host = hostPort.substr(0, colonPos);
			port = std::atoi(hostPort.substr(colonPos + 1).c_str());
		}
		else
		{
			host = hostPort;
		}

		if (host.empty())
		{
			const char* resp = "HTTP/1.1 400 Bad Request\r\n\r\n";
			send(clientSocket, resp, (int)strlen(resp), 0);
			closesocket(clientSocket);
			m_active_workers.fetch_sub(1);
			return;
		}

		// 通过 SOCKS5 连接
		remoteSocket = ConnectViaSocks5(host, port);
		if (remoteSocket == INVALID_SOCKET)
		{
			const char* resp = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
			send(clientSocket, resp, (int)strlen(resp), 0);
			closesocket(clientSocket);
			m_active_workers.fetch_sub(1);
			return;
		}

		// 返回 200 Connection Established
		const char* established = "HTTP/1.1 200 Connection Established\r\n\r\n";
		if (send(clientSocket, established, (int)strlen(established), 0) <= 0)
		{
			closesocket(clientSocket);
			closesocket(remoteSocket);
			m_active_workers.fetch_sub(1);
			return;
		}

		// 转发 CONNECT 请求头之后的额外数据（TLS 握手等）
		int extraLen = totalRead - headerLen;
		if (extraLen > 0)
		{
			if (send(remoteSocket, buf + headerLen, extraLen, 0) <= 0)
			{
				closesocket(clientSocket);
				closesocket(remoteSocket);
				m_active_workers.fetch_sub(1);
				return;
			}
		}
	}
	else
	{
		// 普通 HTTP 请求：GET http://host:port/path HTTP/1.1
		size_t methodEnd = firstLine.find(' ');
		if (methodEnd == std::string::npos)
		{
			const char* resp = "HTTP/1.1 400 Bad Request\r\n\r\n";
			send(clientSocket, resp, (int)strlen(resp), 0);
			closesocket(clientSocket);
			m_active_workers.fetch_sub(1);
			return;
		}
		std::string method = firstLine.substr(0, methodEnd);

		size_t urlStart = methodEnd + 1;
		size_t urlEnd = firstLine.find(' ', urlStart);
		if (urlEnd == std::string::npos)
		{
			const char* resp = "HTTP/1.1 400 Bad Request\r\n\r\n";
			send(clientSocket, resp, (int)strlen(resp), 0);
			closesocket(clientSocket);
			m_active_workers.fetch_sub(1);
			return;
		}
		std::string url = firstLine.substr(urlStart, urlEnd - urlStart);

		// 解析 URL: http://host:port/path
		std::string host;
		int port = 80;
		std::string path = "/";

		if (url.compare(0, 7, "http://") == 0)
		{
			std::string hostPortPath = url.substr(7);
			size_t slashPos = hostPortPath.find('/');
			std::string hostPort = (slashPos != std::string::npos) ? hostPortPath.substr(0, slashPos) : hostPortPath;
			if (slashPos != std::string::npos)
				path = hostPortPath.substr(slashPos);

			size_t colonPos = hostPort.find(':');
			if (colonPos != std::string::npos)
			{
				host = hostPort.substr(0, colonPos);
				port = std::atoi(hostPort.substr(colonPos + 1).c_str());
			}
			else
			{
				host = hostPort;
			}
		}
		else if (url.compare(0, 8, "https://") == 0)
		{
			// HTTPS 请求不应走这里（应该用 CONNECT），但容错处理
			std::string hostPortPath = url.substr(8);
			size_t slashPos = hostPortPath.find('/');
			std::string hostPort = (slashPos != std::string::npos) ? hostPortPath.substr(0, slashPos) : hostPortPath;
			if (slashPos != std::string::npos)
				path = hostPortPath.substr(slashPos);

			size_t colonPos = hostPort.find(':');
			if (colonPos != std::string::npos)
			{
				host = hostPort.substr(0, colonPos);
				port = std::atoi(hostPort.substr(colonPos + 1).c_str());
			}
			else
			{
				host = hostPort;
				port = 443;
			}
		}
		else
		{
			// 不是绝对 URL，无法代理
			const char* resp = "HTTP/1.1 400 Bad Request\r\n\r\n";
			send(clientSocket, resp, (int)strlen(resp), 0);
			closesocket(clientSocket);
			m_active_workers.fetch_sub(1);
			return;
		}

		if (host.empty())
		{
			const char* resp = "HTTP/1.1 400 Bad Request\r\n\r\n";
			send(clientSocket, resp, (int)strlen(resp), 0);
			closesocket(clientSocket);
			m_active_workers.fetch_sub(1);
			return;
		}

		// 通过 SOCKS5 连接
		remoteSocket = ConnectViaSocks5(host, port);
		if (remoteSocket == INVALID_SOCKET)
		{
			const char* resp = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
			send(clientSocket, resp, (int)strlen(resp), 0);
			closesocket(clientSocket);
			m_active_workers.fetch_sub(1);
			return;
		}

		// 构造修改后的请求：METHOD /path HTTP/1.1\r\n + 其余请求头
		// 逐行处理请求头：跳过 Proxy-Connection 和 Connection 头（避免 keep-alive 导致
		// 服务器不关闭连接、WinINet 无法判断响应结束），最后添加 Connection: close
		std::string modifiedRequest = method + " " + path + " HTTP/1.1\r\n";
		int firstLineLen = (int)(lineEnd - buf) + 2;  // 第一行 + \r\n
		int restHeaderLen = headerLen - firstLineLen;  // 包含 \r\n\r\n
		if (restHeaderLen > 0)
		{
			const char* p = buf + firstLineLen;
			const char* end = buf + headerLen;  // 指向 \r\n\r\n 之后
			while (p < end)
			{
				const char* lineStart = p;
				const char* lineEnd2 = (const char*)memchr(lineStart, '\r', end - lineStart);
				if (lineEnd2 == nullptr || lineEnd2 + 1 >= end || lineEnd2[1] != '\n')
					break;
				ptrdiff_t lineLen = lineEnd2 - lineStart;
				// 跳过空行（\r\n）
				if (lineLen == 0)
				{
					p = lineEnd2 + 2;
					continue;
				}
				// 跳过 Connection 和 Proxy-Connection 头（不区分大小写）
				if (lineLen > 12 && (_strnicmp(lineStart, "Connection:", 11) == 0))
				{
					p = lineEnd2 + 2;
					continue;
				}
				if (lineLen > 18 && (_strnicmp(lineStart, "Proxy-Connection:", 17) == 0))
				{
					p = lineEnd2 + 2;
					continue;
				}
				modifiedRequest.append(lineStart, lineLen);
				modifiedRequest.append("\r\n");
				p = lineEnd2 + 2;
			}
		}
		// 添加 Connection: close，让服务器发完响应后关闭连接
		modifiedRequest.append("Connection: close\r\n\r\n");

		// 发送修改后的请求
		if (send(remoteSocket, modifiedRequest.data(), (int)modifiedRequest.size(), 0) <= 0)
		{
			closesocket(clientSocket);
			closesocket(remoteSocket);
			m_active_workers.fetch_sub(1);
			return;
		}

		// 转发请求头之后的额外数据（请求体，如果有）
		int extraLen = totalRead - headerLen;
		if (extraLen > 0)
		{
			if (send(remoteSocket, buf + headerLen, extraLen, 0) <= 0)
			{
				closesocket(clientSocket);
				closesocket(remoteSocket);
				m_active_workers.fetch_sub(1);
				return;
			}
		}
	}

	// 双向转发数据（正确处理 TCP 半关闭）
	// 当一端关闭发送方向时（recv 返回 0），只 shutdown 另一端的发送方向，
	// 继续转发另一个方向的数据，直到两端都关闭。
	// 这避免了 WinINet 发送完请求后关闭发送方向时，响应数据还没读完就断开的问题。
	{
		char relayBuf[8192];
		bool clientClosed = false;  // 客户端不再发送数据
		bool remoteClosed = false;  // 远程不再发送数据

		while (!m_stopping.load() && !(clientClosed && remoteClosed))
		{
			fd_set readSet;
			FD_ZERO(&readSet);
			if (!clientClosed)
				FD_SET(clientSocket, &readSet);
			if (!remoteClosed)
				FD_SET(remoteSocket, &readSet);

			timeval tv;
			tv.tv_sec = 1;
			tv.tv_usec = 0;

			int ret = select(0, &readSet, nullptr, nullptr, &tv);
			if (ret == SOCKET_ERROR)
				break;
			if (ret == 0)
				continue;  // 超时，继续循环（检查 m_stopping）

			if (!clientClosed && FD_ISSET(clientSocket, &readSet))
			{
				int n = recv(clientSocket, relayBuf, sizeof(relayBuf), 0);
				if (n <= 0)
				{
					// 客户端不再发送数据，通知远程（半关闭发送方向）
					shutdown(remoteSocket, SD_SEND);
					clientClosed = true;
				}
				else if (send(remoteSocket, relayBuf, n, 0) <= 0)
				{
					break;
				}
			}

			if (!remoteClosed && FD_ISSET(remoteSocket, &readSet))
			{
				int n = recv(remoteSocket, relayBuf, sizeof(relayBuf), 0);
				if (n <= 0)
				{
					// 远程不再发送数据，通知客户端（半关闭发送方向）
					shutdown(clientSocket, SD_SEND);
					remoteClosed = true;
				}
				else if (send(clientSocket, relayBuf, n, 0) <= 0)
				{
					break;
				}
			}
		}
	}

	closesocket(clientSocket);
	closesocket(remoteSocket);
	m_active_workers.fetch_sub(1);
}
