#include "pch.h"
#include "DummyIpProvider.h"

std::wstring CDummyIpProvider::GetName() const
{
    return L"Disabled";
}

bool CDummyIpProvider::GetExternalIp(std::wstring& ip) const
{
    ip.clear();
    return false;
}
