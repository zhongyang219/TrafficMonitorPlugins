#pragma once
#include "IExternalIpProvider.h"

class CTencentIpProvider :
    public IExternalIpProvider
{
public:
    std::wstring GetName() const override;
    bool GetExternalIp(std::wstring& ip) const override;
};
