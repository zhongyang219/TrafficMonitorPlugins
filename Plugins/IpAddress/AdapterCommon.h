#pragma once
#include <map>
#include "Common.h"

//����һ������������Ϣ
struct NetWorkConection
{
	std::wstring description;		//������������ȡ��GetAdapterInfo��
    std::wstring ip_address{ L"-.-.-.-" };	//IP��ַ
    std::wstring subnet_mask{ L"-.-.-.-" };	//��������
    std::wstring default_gateway{ L"-.-.-.-" };	//Ĭ������
};

class CAdapterCommon
{
public:
	CAdapterCommon();
	~CAdapterCommon();

	//��ȡ���������б��������������IP��ַ���������롢Ĭ��������Ϣ
	static void GetAdapterInfo(std::map<std::wstring, NetWorkConection>& adapters);

	//ˢ�����������б��е�IP��ַ���������롢Ĭ��������Ϣ
	static void RefreshIpAddress(std::map<std::wstring, NetWorkConection>& adapters);

};
