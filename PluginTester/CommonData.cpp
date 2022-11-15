#include "stdafx.h"
#include "CommonData.h"

std::wstring PluginInfo::Property(ITMPlugin::PluginInfoIndex index) const
{
    if (!properties.empty())
    {
        auto iter = properties.find(index);
        if (iter != properties.end())
            return iter->second;
    }
    return std::wstring();
}
