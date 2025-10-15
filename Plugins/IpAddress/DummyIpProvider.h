#pragma once
#include "IExternalIpProvider.h"
#include "DataManager.h"

class CDummyIpProvider :
    public IExternalIpProvider
{
public:
    std::wstring GetName() const override;
    bool GetExternalIp(std::wstring& ip) const override;
};
