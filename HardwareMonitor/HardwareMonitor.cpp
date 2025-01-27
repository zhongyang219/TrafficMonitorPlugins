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
    }

    CHardwareMonitor::~CHardwareMonitor()
    {
    }

    CHardwareMonitor* CHardwareMonitor::GetInstance()
    {
        if (m_pIns == nullptr)
        {
            MonitorGlobal::Instance()->Init();
            m_pIns = new CHardwareMonitor();
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
            std::wstring identifyer = MonitorGlobal::ClrStringToStdWstring(sensor->Identifier->ToString());
            //检查监控项目是否存在
            bool exist = false;
            for (const auto& item : m_settings.item_identifyers)
            {
                if (item == identifyer)
                {
                    exist = true;
                    break;
                }
            }

            if (!exist)
            {
                m_settings.item_identifyers.push_back(identifyer);
                return true;
            }
        }
        return false;
    }

    bool CHardwareMonitor::RemoveDisplayItem(int index)
    {
        if (index >= 0 && index < static_cast<int>(m_settings.item_identifyers.size()))
        {
            auto iter = m_settings.item_identifyers.begin() + index;
            m_settings.item_identifyers.erase(iter);
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
        //载入要监控的项目
        int item_count = ini.GetInt(L"config", L"item_count");
        //添加监控的项目
        for (int i = 0; i < item_count; i++)
        {
            std::wstring app_name = L"item" + std::to_wstring(i);
            std::wstring identifyer = ini.GetString(app_name.c_str(), L"identifier");
            m_settings.item_identifyers.push_back(identifyer);
            ISensor^ sensor = HardwareMonitorHelper::FindSensorByIdentifyer(gcnew String(identifyer.c_str()));
            if (sensor != nullptr)
            {
                std::wstring item_name = MonitorGlobal::ClrStringToStdWstring(HardwareMonitorHelper::GetSensorDisplayName(sensor));
                m_item_names[identifyer] = item_name;
                m_items.emplace_back(identifyer, item_name);
            }
        }
    }

    void CHardwareMonitor::SaveConfig()
    {
        //保存监控的项目
        utilities::CIniHelper ini(m_config_path);
        ini.WriteInt(L"config", L"item_count", static_cast<int>(m_settings.item_identifyers.size()));
        int index = 0;
        for (const auto& item : m_settings.item_identifyers)
        {
            std::wstring app_name = L"item" + std::to_wstring(index);
            ini.WriteString(app_name.c_str(), L"identifier", item.c_str());
            index++;
        }
        ini.Save();
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
        if (MonitorGlobal::Instance()->monitor_form != nullptr)
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
        computer->IsCpuEnabled = true;
        computer->IsGpuEnabled = true;
        computer->IsMotherboardEnabled = true;
        computer->IsStorageEnabled = true;

        resourceManager = gcnew Resources::ResourceManager("HardwareMonitor.resource", Reflection::Assembly::GetExecutingAssembly());
        app_icon = dynamic_cast<Icon^>(resourceManager->GetObject("logo"));
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

    void MonitorGlobal::ShowHardwareInfoDialog()
    {
        HardwareInfoForm^ form = gcnew HardwareInfoForm();
        monitor_form = form;
        form->ShowDialog();
        monitor_form = nullptr;
    }

}



ITMPlugin* TMPluginGetInstance()
{
    return HardwareMonitor::CHardwareMonitor::GetInstance();
}
