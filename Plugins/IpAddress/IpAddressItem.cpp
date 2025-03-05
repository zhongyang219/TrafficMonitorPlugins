#include "pch.h"
#include "IpAddressItem.h"
#include "DataManager.h"

const wchar_t* CIpAddressItem::GetItemName() const
{
    return g_data.StringRes(IDS_PLUGIN_ITEM_NAME);
}

const wchar_t* CIpAddressItem::GetItemId() const
{
    return L"Fd0b18cq";
}

const wchar_t* CIpAddressItem::GetItemLableText() const
{
    return L"";
}

const wchar_t* CIpAddressItem::GetItemValueText() const
{
    static std::wstring ipv4_addr;
    if (g_data.GetLocalIPv4Address(ipv4_addr))
        return ipv4_addr.c_str();
    return L"";
}

const wchar_t* CIpAddressItem::GetItemValueSampleText() const
{
    static std::wstring ipv4_addr;
    if (g_data.GetLocalIPv4Address(ipv4_addr))
        return ipv4_addr.c_str();
    return L"000.000.000.000";
}
