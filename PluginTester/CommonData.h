#pragma once

#include <string>
#include <vector>
#include <map>

#include "PluginInterface.h"

//插件信息
struct PluginInfo
{
    std::wstring file_path;      //文件路径
    HMODULE plugin_module{};  //dll module
    ITMPlugin* plugin{};      //插件对象
    std::vector<IPluginItem*> plugin_items; //插件提供的所有显示项目
    std::map<ITMPlugin::PluginInfoIndex, std::wstring> properties;    //插件属性
    std::wstring Property(ITMPlugin::PluginInfoIndex) const;
};

typedef ITMPlugin* (*pfTMPluginGetInstance)();


//语言
enum class Language
{
    FOLLOWING_SYSTEM,       //跟随系统
    ENGLISH,                //英语
    SIMPLIFIED_CHINESE      //简体中文
};
