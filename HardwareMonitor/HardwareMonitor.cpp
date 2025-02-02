// 这是主 DLL 文件。

#include "stdafx.h"

#include "HardwareMonitor.h"
#include <vector>
#include "HardwareMonitorHelper.h"
#include "../utilities/IniHelper.h"
#include "SettingsForm.h"

namespace HardwareMonitor
{
    CHardwareMonitor* CHardwareMonitor::m_pIns = nullptr;


    CHardwareMonitor::CHardwareMonitor()
    {
        //获取当前屏幕DPI值
        Graphics^ graphics = Graphics::FromHwnd(IntPtr::Zero);
        m_dpi = static_cast<int>(graphics->DpiX);
    }

    CHardwareMonitor::~CHardwareMonitor()
    {
    }

    CHardwareMonitor* CHardwareMonitor::GetInstance()
    {
        if (m_pIns == nullptr)
        {
            m_pIns = new CHardwareMonitor();
            MonitorGlobal::Instance()->Init();
        }
        return m_pIns;
    }

    const std::wstring& CHardwareMonitor::StringRes(const wchar_t* name)
    {
        auto iter = string_table.find(name);
        if (iter != string_table.end())
        {
            return iter->second;
        }
        else
        {
            std::wstring& str = string_table[name];
            str = MonitorGlobal::Instance()->GetStdWString(name);
            return str;
        }
    }

    bool CHardwareMonitor::AddDisplayItem(ISensor^ sensor)
    {
        if (sensor != nullptr)
        {
            std::wstring identifyer = MonitorGlobal::ClrStringToStdWstring(HardwareMonitorHelper::GetSensorIdentifyer(sensor));
            //检查监控项目是否存在
            if (!IsDisplayItemExist(identifyer))
            {
                m_settings.items_info.push_back(identifyer);
                return true;
            }
        }
        return false;
    }

    bool CHardwareMonitor::IsDisplayItemExist(const std::wstring& identifyer)
    {
        for (const auto& item : m_settings.items_info)
        {
            if (item.identifyer == identifyer)
                return true;
        }
        return false;
    }

    bool CHardwareMonitor::RemoveDisplayItem(int index)
    {
        if (index >= 0 && index < static_cast<int>(m_settings.items_info.size()))
        {
            auto iter = m_settings.items_info.begin() + index;
            m_settings.items_info.erase(iter);
            return true;
        }
        return false;
    }

    std::wstring CHardwareMonitor::GetItemName(const std::wstring& identifier)
    {
        auto iter = m_item_names.find(identifier);
        if (iter != m_item_names.end())
        {
            return iter->second;
        }
        else
        {
            ISensor^ sensor = HardwareMonitorHelper::FindSensorByIdentifyer(gcnew String(identifier.c_str()));
            if (sensor != nullptr)
                return MonitorGlobal::ClrStringToStdWstring(HardwareMonitorHelper::GetSensorDisplayName(sensor));
        }

        return std::wstring();
    }

    void CHardwareMonitor::LoadConfig(const std::wstring& config_dir)
    {
        m_config_path = config_dir + L"HardwareMonitor.ini";
        utilities::CIniHelper ini(m_config_path);

        //载入要监控的硬件
        Computer^ computer = MonitorGlobal::Instance()->computer;
        computer->IsCpuEnabled = ini.GetBool(L"hardware", L"IsCpuEnabled", true);                   //CPU
        computer->IsGpuEnabled = ini.GetBool(L"hardware", L"IsGpuEnabled", true);                   //显卡
        computer->IsMotherboardEnabled = ini.GetBool(L"hardware", L"IsMotherboardEnabled", true);   //主板
        computer->IsStorageEnabled = ini.GetBool(L"hardware", L"IsStorageEnabled", true);           //硬盘
        computer->IsBatteryEnabled = ini.GetBool(L"hardware", L"IsBatteryEnabled", false);          //电池
        computer->IsNetworkEnabled = ini.GetBool(L"hardware", L"IsNetworkEnabled", false);          //网络
        computer->IsMemoryEnabled = ini.GetBool(L"hardware", L"IsMemoryEnabled", false);            //内存
        computer->IsControllerEnabled = ini.GetBool(L"hardware", L"IsControllerEnabled", false);    //风扇控制器
        computer->IsPsuEnabled = ini.GetBool(L"hardware", L"IsPsuEnabled", false);                  //电源

        MonitorGlobal::Instance()->computer->Accept(MonitorGlobal::Instance()->updateVisitor);

        //载入要监控的项目
        int item_count = ini.GetInt(L"config", L"item_count");
        //添加监控的项目
        for (int i = 0; i < item_count; i++)
        {
            std::wstring app_name = L"item" + std::to_wstring(i);
            std::wstring identifyer = ini.GetString(app_name.c_str(), L"identifier");
            int decimal_places = ini.GetInt(app_name.c_str(), L"decimal_places", 2);
            m_settings.items_info.emplace_back(identifyer, decimal_places);
            ISensor^ sensor = HardwareMonitorHelper::FindSensorByIdentifyer(gcnew String(identifyer.c_str()));
            if (sensor != nullptr)
            {
                std::wstring item_name = MonitorGlobal::ClrStringToStdWstring(HardwareMonitorHelper::GetSensorDisplayName(sensor));
                m_item_names[identifyer] = item_name;
                m_items.emplace_back(identifyer, item_name);
            }
        }
        m_settings.hardware_info_auto_refresh = ini.GetBool(L"config", L"hardware_info_auto_refresh");
        m_settings.show_mouse_tooltip = ini.GetBool(L"config", L"show_mouse_tooltip", true);
    }

    void CHardwareMonitor::SaveConfig()
    {
        utilities::CIniHelper ini(m_config_path);

        //保存要监控的硬件
        Computer^ computer = MonitorGlobal::Instance()->computer;
        ini.WriteBool(L"hardware", L"IsCpuEnabled", computer->IsCpuEnabled);
        ini.WriteBool(L"hardware", L"IsGpuEnabled", computer->IsGpuEnabled);
        ini.WriteBool(L"hardware", L"IsMotherboardEnabled", computer->IsMotherboardEnabled);
        ini.WriteBool(L"hardware", L"IsStorageEnabled", computer->IsStorageEnabled);
        ini.WriteBool(L"hardware", L"IsBatteryEnabled", computer->IsBatteryEnabled);
        ini.WriteBool(L"hardware", L"IsNetworkEnabled", computer->IsNetworkEnabled);
        ini.WriteBool(L"hardware", L"IsMemoryEnabled", computer->IsMemoryEnabled);
        ini.WriteBool(L"hardware", L"IsControllerEnabled", computer->IsControllerEnabled);
        ini.WriteBool(L"hardware", L"IsPsuEnabled", computer->IsPsuEnabled);

        //保存监控的项目
        ini.WriteInt(L"config", L"item_count", static_cast<int>(m_settings.items_info.size()));
        int index = 0;
        for (const auto& item : m_settings.items_info)
        {
            std::wstring app_name = L"item" + std::to_wstring(index);
            ini.WriteString(app_name.c_str(), L"identifier", item.identifyer.c_str());
            ini.WriteInt(app_name.c_str(), L"decimal_places", item.decimal_places);
            index++;
        }
        ini.WriteBool(L"config", L"hardware_info_auto_refresh", m_settings.hardware_info_auto_refresh);
        ini.WriteBool(L"config", L"show_mouse_tooltip", m_settings.show_mouse_tooltip);

        ini.Save();
    }

    int CHardwareMonitor::DPI(int pixel) const
    {
        return pixel * m_dpi / 96;
    }

    IPluginItem* CHardwareMonitor::GetItem(int index)
    {
        if (index >= 0 && index < static_cast<int>(m_items.size()))
        {
            return &m_items[index];
        }
        return nullptr;
    }

    void CHardwareMonitor::DataRequired()
    {
        //重新获取数据
        MonitorGlobal::Instance()->computer->Accept(MonitorGlobal::Instance()->updateVisitor);
        //更新“硬件信息”树节点
        if (MonitorGlobal::Instance()->monitor_form != nullptr && m_settings.hardware_info_auto_refresh)
            MonitorGlobal::Instance()->monitor_form->UpdateData();
        //更新所有显示项目
        for (auto& item : m_items)
        {
            item.UpdateValue();
        }
    }

    const wchar_t* CHardwareMonitor::GetInfo(PluginInfoIndex index)
    {
        static std::wstring str;
        switch (index)
        {
        case TMI_NAME:
            return StringRes(L"PluginName").c_str();
        case TMI_DESCRIPTION:
            return StringRes(L"PluginDescription").c_str();
        case TMI_AUTHOR:
            return L"zhongyang219";
        case TMI_COPYRIGHT:
            return L"Copyright (C) by Zhong Yang 2025";
        case ITMPlugin::TMI_URL:
            return L"https://github.com/zhongyang219/TrafficMonitorPlugins";
            break;
        case TMI_VERSION:
            return L"1.00";
        default:
            break;
        }
        return L"";
    }

    const wchar_t* CHardwareMonitor::GetTooltipInfo()
    {
        if (m_settings.show_mouse_tooltip)
        {
            static std::wstring tooltip_info;
            tooltip_info.clear();
            for (size_t i{}; i < m_items.size(); i++)
            {
                const auto& item = m_items[i];
                if (i > 0)
                    tooltip_info += L"\r\n";
                tooltip_info += item.GetItemName();
                tooltip_info += L": ";
                tooltip_info += item.GetItemValueText();
            }
            return tooltip_info.c_str();
        }
        else
        {
            return L"";
        }
    }

    ITMPlugin::OptionReturn CHardwareMonitor::ShowOptionsDialog(void* hParent)
    {
        SettingsForm^ form = gcnew SettingsForm();
        if (form->ShowDialog() == DialogResult::OK)
        {
            SaveConfig();
            return ITMPlugin::OR_OPTION_CHANGED;
        }
        return ITMPlugin::OR_OPTION_UNCHANGED;
    }

    void CHardwareMonitor::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
    {
        switch (index)
        {
        case ITMPlugin::EI_CONFIG_DIR:
            //从配置文件读取配置
            LoadConfig(std::wstring(data));
            break;
        default:
            break;
        }

    }

    void* CHardwareMonitor::GetPluginIcon()
    {
        return MonitorGlobal::Instance()->GetAppIcon()->Handle.ToPointer();
    }

    int CHardwareMonitor::GetCommandCount()
    {
        return 1;
    }

    const wchar_t* CHardwareMonitor::GetCommandName(int command_index)
    {
        return StringRes(L"HardwareInfo").c_str();
    }

    void CHardwareMonitor::OnPluginCommand(int command_index, void* hWnd, void* para)
    {
        if (command_index == 0)
        {
            MonitorGlobal::Instance()->ShowHardwareInfoDialog();
        }
    }


    ////////////////////////////////////////////////////////////////////////////////////
    MonitorGlobal::MonitorGlobal()
    {

    }

    MonitorGlobal::~MonitorGlobal()
    {

    }

    void MonitorGlobal::Init()
    {
        updateVisitor = gcnew UpdateVisitor();
        computer = gcnew Computer();
        computer->Open();

        resourceManager = gcnew Resources::ResourceManager("HardwareMonitor.resource", Reflection::Assembly::GetExecutingAssembly());
        app_icon = LoadIcon("logo", CHardwareMonitor::GetInstance()->DPI(16));
    }

    void MonitorGlobal::UnInit()
    {
        computer->Close();
    }

    std::wstring MonitorGlobal::ClrStringToStdWstring(System::String^ str)
    {
        if (str == nullptr)
        {
            return std::wstring();
        }
        else
        {
            const wchar_t* chars = (const wchar_t*)(Runtime::InteropServices::Marshal::StringToHGlobalUni(str)).ToPointer();
            std::wstring os = chars;
            Runtime::InteropServices::Marshal::FreeHGlobal(IntPtr((void*)chars));
            return os;
        }
    }

    String^ MonitorGlobal::GetString(const wchar_t* name)
    {
        return resourceManager->GetString(gcnew String(name));
    }

    std::wstring MonitorGlobal::GetStdWString(const wchar_t* name)
    {
        return ClrStringToStdWstring(GetString(name));
    }

    Icon^ MonitorGlobal::GetAppIcon()
    {
        return app_icon;
    }

    Resources::ResourceManager^ MonitorGlobal::GetResourceManager()
    {
        return resourceManager;
    }

    void MonitorGlobal::ShowHardwareInfoDialog()
    {
        HardwareInfoForm^ form = gcnew HardwareInfoForm();
        monitor_form = form;
        form->ShowDialog();
        monitor_form = nullptr;
    }

    Icon^ MonitorGlobal::LoadIcon(String^ name, int icon_size)
    {
        //载入图标
        Icon^ tmp = dynamic_cast<Icon^>(resourceManager->GetObject(name));
        //转换为新的尺寸
        return gcnew Icon(tmp, Size(icon_size, icon_size));
    }
}



ITMPlugin* TMPluginGetInstance()
{
    return HardwareMonitor::CHardwareMonitor::GetInstance();
}
