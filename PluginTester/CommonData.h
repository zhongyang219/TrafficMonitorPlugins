#pragma once

#include <string>
#include <vector>
#include <map>

#include "PluginInterface.h"

//�����Ϣ
struct PluginInfo
{
    std::wstring file_path;      //�ļ�·��
    HMODULE plugin_module{};  //dll module
    ITMPlugin* plugin{};      //�������
    std::vector<IPluginItem*> plugin_items; //����ṩ��������ʾ��Ŀ
    std::map<ITMPlugin::PluginInfoIndex, std::wstring> properties;    //�������
    std::wstring Property(ITMPlugin::PluginInfoIndex) const;
};

typedef ITMPlugin* (*pfTMPluginGetInstance)();


//����
enum class Language
{
    FOLLOWING_SYSTEM,       //����ϵͳ
    ENGLISH,                //Ӣ��
    SIMPLIFIED_CHINESE,     //��������
    RUSSIAN
};
