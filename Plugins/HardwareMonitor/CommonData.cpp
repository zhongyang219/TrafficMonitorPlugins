#include "stdafx.h"
#include "CommonData.h"

namespace HardwareMonitor
{
    ItemInfo& OptionSettings::FindItemInfo(const std::wstring& identifyer)
    {
        for (auto& item_info : items_info)
        {
            if (item_info.identifyer == identifyer)
                return item_info;
        }

        static ItemInfo empty_info;
        return empty_info;
    }
}

