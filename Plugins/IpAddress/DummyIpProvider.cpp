#include "pch.h"
#include "DummyIpProvider.h"

std::wstring CDummyIpProvider::GetName() const
{
    return L"Dummy";
}

bool CDummyIpProvider::GetExternalIp(std::wstring& ip) const
{
    ip = L"<disabled>";
    return true;
}
