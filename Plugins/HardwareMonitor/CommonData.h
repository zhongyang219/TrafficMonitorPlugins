#pragma once
#include <string>
#include <vector>

namespace HardwareMonitor
{
    struct ItemInfo
    {
        std::wstring identifyer;    //唯一标识符
        int decimal_places{ 2 };         //小数位数
        bool specify_value_width{ false };  //指定数值宽度（不含小数和单位）
        int value_width{ 4 };       //数值宽度

        ItemInfo()
        {}

        ItemInfo(const std::wstring& _identifyer)
            : identifyer(_identifyer)
        {}

        ItemInfo(const std::wstring& _identifyer, int _decimal_places)
            : identifyer(_identifyer), decimal_places(_decimal_places)
        {}
    };

    struct OptionSettings
    {
        std::vector<ItemInfo> items_info;     //所有监控项目
        ItemInfo& FindItemInfo(const std::wstring& identifyer);     //根据identifyer，在items_info找到ItemInfo对象

        bool hardware_info_auto_refresh{};              //监控信息界面是否自动刷新
        bool show_mouse_tooltip{};
    };

}