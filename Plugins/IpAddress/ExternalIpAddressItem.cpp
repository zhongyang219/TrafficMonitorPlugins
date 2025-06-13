#include "pch.h"
#include "ExternalIpAddressItem.h"
#include "DataManager.h"

const wchar_t* CExternalIpAddressItem::GetItemName() const
{
    return g_data.StringRes(IDS_EXTERNAL_IP_ADDRESS);
}

const wchar_t* CExternalIpAddressItem::GetItemId() const
{
    return L"Fb0b18cr";
}

const wchar_t* CExternalIpAddressItem::GetItemLableText() const
{
    return L"";
}

const wchar_t* CExternalIpAddressItem::GetItemValueText() const
{
    static std::wstring ipv4_addr;
    if (g_data.GetExternalIPv4Address(ipv4_addr))
        return ipv4_addr.c_str();
    return L"";
}

const wchar_t* CExternalIpAddressItem::GetItemValueSampleText() const
{
    return L"000.000.000.000";
}
