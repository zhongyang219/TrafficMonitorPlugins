#pragma once
#include <string>
#include <ctime>

// 网络抓取模块：封装通过 WinINet（CInternetSession）发起 HTTP 请求的逻辑，
// 并自动在"直连"与"SOCKS5 代理"之间选择，避免在各业务模块重复实现。
//
// 选路策略：
//  - 未配置代理时，只走直连（绝大多数有正常网络的用户）。
//  - 已配置代理时，优先直连；直连失败再回退到 SOCKS5 代理。
//    直连失败后缓存"优先代理"状态一段时间，避免每次请求都先等直连超时；
//    缓存过期后再次探测直连，以便网络恢复时自动切回直连。
//
// 代理实现：
//  - WinINet 不支持 SOCKS5，只支持 HTTP 代理。
//  - 当配置 SOCKS5 代理时，启动本地 HTTP CONNECT 代理（CLocalHttpProxy），
//    把 WinINet 的请求通过 SOCKS5 转发到目标服务器。
//  - WinINet 用系统 SChannel TLS 栈做握手，指纹和浏览器一致，
//    可绕过基于 curl TLS 指纹的 WAF 拦截（如东方财富）。
class CNetFetch
{
public:
	// 统一入口：根据配置自动选择直连或 SOCKS5 代理发起 GET 请求。
	// user_agent、headers 可为空。成功时 result 写入响应体（原始字节）。
	static bool GetURL(const std::wstring& url, std::string& result,
		LPCTSTR user_agent, LPCTSTR headers);

private:
	// 用 WinINet 发起一次请求。
	// proxy 形如 "127.0.0.1:12345"（本地 HTTP 代理），为空表示直连；
	// connect_timeout 为 TCP 连接超时秒数。
	static bool FetchOnce(const std::wstring& url, std::string& result,
		LPCTSTR user_agent, LPCTSTR headers,
		const std::wstring& proxy, int connect_timeout);

	// 直连失败后"优先代理"状态的缓存（避免每次都等直连超时）
	static bool s_prefer_proxy;
	static std::time_t s_prefer_proxy_until;
};
