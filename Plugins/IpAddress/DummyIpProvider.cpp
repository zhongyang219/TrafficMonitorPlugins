#include "pch.h"
#include "DummyIpProvider.h"

std::wstring CDummyIpProvider::GetName() const
{
    return L"Dummy";
}

bool CDummyIpProvider::GetExternalIp(std::wstring& ip) const
{
    ip = g_data.m_setting_data.dummy_ip_value;
    return true;
}
