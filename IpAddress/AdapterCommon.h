#pragma once
#include "Common.h"
#include <vector>

//保存一个网络连接信息
struct NetWorkConection
{
	std::wstring description;		//网络描述（获取自GetAdapterInfo）
    std::wstring ip_address{ L"-.-.-.-" };	//IP地址
    std::wstring subnet_mask{ L"-.-.-.-" };	//子网掩码
    std::wstring default_gateway{ L"-.-.-.-" };	//默认网关
};

class CAdapterCommon
{
public:
	CAdapterCommon();
	~CAdapterCommon();

	//获取网络连接列表，填充网络描述、IP地址、子网掩码、默认网关信息
	static void GetAdapterInfo(std::vector<NetWorkConection>& adapters);

	//刷新网络连接列表中的IP地址、子网掩码、默认网关信息
	static void RefreshIpAddress(std::vector<NetWorkConection>& adapters);

};

