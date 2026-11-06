#include "pch.h"
#include "NetFetch.h"
#include "DataManager.h"
#include "Common.h"
#include "LocalHttpProxy.h"
#include <afxinet.h>
#include <vector>

bool CNetFetch::s_prefer_proxy = false;
std::time_t CNetFetch::s_prefer_proxy_until = 0;

// 代理模式下，直连失败后"优先代理"状态的缓存时长（秒）
static const int PROXY_CACHE_SECONDS = 120;

// 解析 URL：scheme://host:port/path?query
// 成功时 isHttps、host、port、path 被赋值
static bool ParseURL(const std::wstring& url, bool& isHttps, std::wstring& host, int& port, std::wstring& path)
{
	isHttps = false;
	port = 80;
	host.clear();
	path = L"/";

	std::wstring rest;
	if (url.find(L"https://") == 0)
	{
		isHttps = true;
		port = 443;
		rest = url.substr(8);
	}
	else if (url.find(L"http://") == 0)
	{
		isHttps = false;
		port = 80;
		rest = url.substr(7);
	}
	else
	{
		return false;  // 不支持的协议
	}

	// 分离 host:port 和 path
	size_t slashPos = rest.find(L'/');
	std::wstring hostPort = (slashPos != std::wstring::npos) ? rest.substr(0, slashPos) : rest;
	if (slashPos != std::wstring::npos)
		path = rest.substr(slashPos);
	else
		path = L"/";

	// 分离 host 和 port
	size_t colonPos = hostPort.find(L':');
	if (colonPos != std::wstring::npos)
	{
		host = hostPort.substr(0, colonPos);
		try {
			port = std::stoi(hostPort.substr(colonPos + 1));
		} catch (...) {
			return false;
		}
	}
	else
	{
		host = hostPort;
	}

	return !host.empty();
}

// 统一的 URL 获取入口
// 正常模式（未启用 SOCKS5 代理）：直接访问网络
// 代理模式（启用了 SOCKS5 代理）：启动本地 HTTP 代理桥接，优先直连，失败回退到代理
bool CNetFetch::GetURL(const std::wstring& url, std::string& result,
	LPCTSTR user_agent, LPCTSTR headers)
{
	result.clear();

	// ========== 正常模式 ==========
	// 未启用 SOCKS5 代理，直接访问网络（绝大多数有正常网络的用户）
	if (!g_data.m_setting_data.m_use_socks5_proxy)
		return FetchOnce(url, result, user_agent, headers, std::wstring(), 10);

	// ========== 代理模式 ==========
	// 启用了 SOCKS5 代理：启动本地 HTTP 代理（把 SOCKS5 转为 HTTP 代理，供 WinINet 使用）
	// 若代理地址无效或启动失败，Start 内部会返回 false，此时回退到直连
	auto& localProxy = CLocalHttpProxy::Instance();
	if (!localProxy.Start(g_data.m_setting_data.m_socks5_proxy))
	{
		// 本地代理启动失败，回退到直连
		return FetchOnce(url, result, user_agent, headers, std::wstring(), 10);
	}
	std::wstring localProxyAddr = localProxy.GetProxyAddress();

	// 代理模式下优先直连（系统级代理如 Proxifier 会拦截直连请求走 SOCKS5）
	// 直连失败后走本地代理，缓存"优先代理"状态避免重复等待直连超时
	std::time_t now = std::time(nullptr);
	if (s_prefer_proxy && now < s_prefer_proxy_until)
	{
		// 近期直连失败，直接走本地代理
		if (FetchOnce(url, result, user_agent, headers, localProxyAddr, 10))
			return true;
		// 本地代理也失败，再试一次直连兜底（代理可能临时不可用）
		return FetchOnce(url, result, user_agent, headers, std::wstring(), 10);
	}

	// 探测直连（短超时，快速失败）
	if (FetchOnce(url, result, user_agent, headers, std::wstring(), 3))
	{
		s_prefer_proxy = false;
		return true;
	}

	// 直连失败：改走本地代理，并缓存"优先代理"状态
	s_prefer_proxy = true;
	s_prefer_proxy_until = now + PROXY_CACHE_SECONDS;
	return FetchOnce(url, result, user_agent, headers, localProxyAddr, 10);
}

// 单次 HTTP 请求
// proxy 为空表示直连，非空表示通过本地 HTTP 代理
bool CNetFetch::FetchOnce(const std::wstring& url, std::string& result,
	LPCTSTR user_agent, LPCTSTR headers,
	const std::wstring& proxy, int connect_timeout)
{
	result.clear();

	// 解析 URL
	bool isHttps = false;
	std::wstring host, path;
	int port = 80;
	if (!ParseURL(url, isHttps, host, port, path))
		return false;

	// 配置访问类型：直连 或 通过本地 HTTP 代理
	DWORD accessType = INTERNET_OPEN_TYPE_DIRECT;
	LPCTSTR proxyStr = NULL;
	if (!proxy.empty())
	{
		accessType = INTERNET_OPEN_TYPE_PROXY;
		proxyStr = proxy.c_str();
	}

	CString agent = (user_agent != nullptr && user_agent[0] != _T('\0')) ? user_agent : _T("Mozilla/5.0");

	CInternetSession session(agent, 1, accessType, proxyStr, NULL, 0);

	// 设置超时（缩短超时避免线程退出时长时间阻塞）
	session.SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, connect_timeout * 1000);
	session.SetOption(INTERNET_OPTION_SEND_TIMEOUT, 5000);
	session.SetOption(INTERNET_OPTION_RECEIVE_TIMEOUT, 5000);

	DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_KEEP_CONNECTION;
	if (isHttps)
		flags |= INTERNET_FLAG_SECURE;

	CHttpConnection* pConn = NULL;
	CHttpFile* pFile = NULL;

	try
	{
		pConn = session.GetHttpConnection(host.c_str(), (INTERNET_PORT)port);
		if (pConn == NULL)
			return false;

		pFile = pConn->OpenRequest(CHttpConnection::HTTP_VERB_GET, path.c_str(),
			NULL, 1, NULL, NULL, flags);
		if (pFile == NULL)
		{
			pConn->Close();
			delete pConn;
			return false;
		}

		// 添加请求头
		if (headers != nullptr && headers[0] != _T('\0'))
		{
			CString strHeaders(headers);
			strHeaders += _T("\r\n");
			pFile->AddRequestHeaders(strHeaders);
		}

		pFile->SendRequest();

		// 检查 HTTP 状态码
		DWORD statusCode = 0;
		pFile->QueryInfoStatusCode(statusCode);

		// 读取响应体
		char buf[8192];
		UINT nRead;
		while ((nRead = pFile->Read(buf, sizeof(buf))) > 0)
		{
			result.append(buf, nRead);
		}

		pFile->Close();
		delete pFile;
		pConn->Close();
		delete pConn;

		if (statusCode != 200 || result.empty())
			return false;

		return true;
	}
	catch (CInternetException* e)
	{
		e->Delete();
	}
	catch (...)
	{
	}

	if (pFile) { try { pFile->Close(); } catch (...) {} delete pFile; }
	if (pConn) { try { pConn->Close(); } catch (...) {} delete pConn; }
	return false;
}
