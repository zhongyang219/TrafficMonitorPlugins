#pragma once

#include <string>

class IExternalIpProvider
{
public:
    virtual ~IExternalIpProvider() = default;
    virtual std::wstring GetName() const = 0;
    virtual bool GetExternalIp(std::wstring& ip) const = 0;
};
