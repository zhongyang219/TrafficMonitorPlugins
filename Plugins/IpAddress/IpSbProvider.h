#pragma once
#include "IExternalIpProvider.h"

class CIpSbProvider :
    public IExternalIpProvider
{
public:
    virtual std::wstring GetName() const override;
    virtual bool GetExternalIp(std::wstring& ip) const override;
};
