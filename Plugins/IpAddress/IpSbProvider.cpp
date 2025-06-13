#include "pch.h"
#include "IpSbProvider.h"
#include "Common.h"
#include "../utilities/Common.h"

std::wstring CIpSbProvider::GetName() const
{
    return L"ip.sb";
}

bool CIpSbProvider::GetExternalIp(std::wstring& ip) const
{
    std::string result;
    if (CCommon::GetURL(L"https://api.ip.sb/ip", result, true))
    {
        utilities::StringHelper::StringNormalize(result);
        ip = CCommon::StrToUnicode(result.c_str(), true);
        return true;
    }
    return false;
}
