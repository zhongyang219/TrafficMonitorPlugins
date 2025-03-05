// 这是主 DLL 文件。

#include "stdafx.h"

#include "HardwareMonitor.h"
#include <vector>
#include "HardwareMonitorHelper.h"
#include "../utilities/IniHelper.h"
#include "SettingsForm.h"
#include "Common.h"

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
            try
            {
                m_pIns = new CHardwareMonitor();
                MonitorGlobal::Instance()->Init();
            }
            catch (System::Exception^ e)
            {
                ShowErrorMessage(e);
            }
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
            str = MonitorGlobal::Instance()->GetStdWString(gcnew String(name));
            return str;
        }
    }

    bool CHardwareMonitor::AddDisplayItem(ISensor^ sensor)
    {
        if (sensor != nullptr)
        {
            std::wstring identifyer = Common::StringToStdWstring(HardwareMonitorHelper::GetSensorIdentifyer(sensor));
            //检查监控项目是否存在
            if (!IsDisplayItemExist(identifyer))
            {
                ItemInfo item_info;
                item_info.identifyer = identifyer;
                item_info.unit = Common::StringToStdWstring(HardwareMonitorHelper::GetSensorTypeDefaultUnit(sensor->SensorType));
                m_settings.items_info.push_back(item_info);
                return true;
            }
        }
        return false;
    }

    bool CHardwareMonitor::IsDisplayItemExist(const std::wstring& identifyer)
    {
        if (!identifyer.empty())
        {
            for (const auto& item : m_settings.items_info)
            {
                if (item.identifyer == identifyer)
                    return true;
            }
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
                return Common::StringToStdWstring(HardwareMonitorHelper::GetSensorDisplayName(sensor));
        }

        return std::wstring();
    }

    const std::wstring& CHardwareMonitor::GetConfigPath() const
    {
        return m_config_path;
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
            ItemInfo item_info;
            item_info.identifyer = ini.GetString(app_name.c_str(), L"identifier");
            item_info.decimal_places = ini.GetInt(app_name.c_str(), L"decimal_places", 1);
            item_info.specify_value_width = ini.GetBool(app_name.c_str(), L"specify_value_width", true);
            item_info.value_width = ini.GetInt(app_name.c_str(), L"value_width", 3);
            item_info.unit = ini.GetString(app_name.c_str(), L"unit");
            item_info.show_unit = ini.GetBool(app_name.c_str(), L"show_unit", true);
            m_settings.items_info.push_back(item_info);
            ISensor^ sensor = HardwareMonitorHelper::FindSensorByIdentifyer(gcnew String(item_info.identifyer.c_str()));
            if (sensor != nullptr)
            {
                std::wstring item_name = Common::StringToStdWstring(HardwareMonitorHelper::GetSensorDisplayName(sensor));
                std::wstring lable_text = Common::StringToStdWstring(Common::GetTranslatedString(sensor->Name));
                m_item_names[item_info.identifyer] = item_name;
                m_items.emplace_back(item_info.identifyer, item_name, lable_text);
            }
        }
        m_settings.hardware_info_auto_refresh = ini.GetBool(L"config", L"hardware_info_auto_refresh");
        m_settings.show_mouse_tooltip = ini.GetBool(L"config", L"show_mouse_tooltip", true);

        //如果配置文件为空，则认为是首次使用硬件监控插件，添加默认的监控项
        if (ini.IsEmpty())
        {
            List<ISensor^>^ default_sensors = gcnew List<ISensor^>();
            HardwareMonitorHelper::GetDefaultMonitorItem(default_sensors);
            for each (auto sensor in default_sensors)
            {
                std::wstring identifyer = Common::StringToStdWstring(HardwareMonitorHelper::GetSensorIdentifyer(sensor));
                std::wstring item_name = Common::StringToStdWstring(HardwareMonitorHelper::GetSensorDisplayName(sensor));
                std::wstring lable_text = Common::StringToStdWstring(Common::GetTranslatedString(sensor->Name));
                m_item_names[identifyer] = item_name;
                m_items.emplace_back(identifyer, item_name, lable_text);
                ItemInfo item_info;
                item_info.identifyer = identifyer;
                item_info.unit = Common::StringToStdWstring(HardwareMonitorHelper::GetSensorTypeDefaultUnit(sensor->SensorType));
                m_settings.items_info.push_back(item_info);
            }
        }

        //载入树控件的展开折叠状态
        std::vector<std::wstring> collapse_nodes;
        ini.GetStringList(L"other", L"hardware_info_collapsed_nodes", collapse_nodes, std::vector<std::wstring>());
        for (const auto& node_path : collapse_nodes)
            MonitorGlobal::Instance()->treeCollapseNodes->Add(gcnew String(node_path.c_str()));
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
            ini.WriteBool(app_name.c_str(), L"specify_value_width", item.specify_value_width);
            ini.WriteInt(app_name.c_str(), L"value_width", item.value_width);
            ini.WriteString(app_name.c_str(), L"unit", item.unit);
            ini.WriteBool(app_name.c_str(), L"show_unit", item.show_unit);
            index++;
        }
        ini.WriteBool(L"config", L"hardware_info_auto_refresh", m_settings.hardware_info_auto_refresh);
        ini.WriteBool(L"config", L"show_mouse_tooltip", m_settings.show_mouse_tooltip);

        //保存树控件的展开折叠状态
        std::vector<std::wstring> collapse_nodes;
        for each (auto str in MonitorGlobal::Instance()->treeCollapseNodes)
            collapse_nodes.push_back(Common::StringToStdWstring(str));
        ini.WriteStringList(L"other", L"hardware_info_collapsed_nodes", collapse_nodes);

        ini.Save();
    }

    int CHardwareMonitor::DPI(int pixel) const
    {
        return pixel * m_dpi / 96;
    }

    void CHardwareMonitor::ShowErrorMessage(System::Exception^ e)
    {
        String^ str = e->Message;
        str += "\r\n";
        str += e->StackTrace;
        MessageBox::Show(str, "TrafficMonitor HardwareMonitor plugin", MessageBoxButtons::OK, MessageBoxIcon::Error);
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
        try
        {
            //重新获取数据
            MonitorGlobal::Instance()->computer->Accept(MonitorGlobal::Instance()->updateVisitor);
            //更新“硬件信息”树节点
            if (MonitorGlobal::Instance()->monitor_form != nullptr && m_settings.hardware_info_auto_refresh)
                MonitorGlobal::Instance()->monitor_form->UpdateData();
            //更新“选项设置”中的节点
            if (MonitorGlobal::Instance()->setttings_form != nullptr)
                MonitorGlobal::Instance()->setttings_form->UpdateItemsValue();
            //更新所有显示项目
            for (auto& item : m_items)
            {
                item.UpdateValue();
            }
        }
        catch (System::Exception^)
        {
            //ShowErrorMessage(e);
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
        try
        {
            SettingsForm^ form = gcnew SettingsForm();
            MonitorGlobal::Instance()->setttings_form = form;
            if (form->ShowDialog() == DialogResult::OK)
            {
                MonitorGlobal::Instance()->setttings_form = nullptr;
                SaveConfig();
                return ITMPlugin::OR_OPTION_CHANGED;
            }
        }
        catch (System::Exception^ e)
        {
            MonitorGlobal::Instance()->setttings_form = nullptr;
            ShowErrorMessage(e);
        }
        finally
        {
            MonitorGlobal::Instance()->setttings_form = nullptr;
        }
        return ITMPlugin::OR_OPTION_UNCHANGED;
    }

    void CHardwareMonitor::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
    {
        switch (index)
        {
        case ITMPlugin::EI_CONFIG_DIR:
            try
            {
                //从配置文件读取配置
                LoadConfig(std::wstring(data));
            }
            catch (System::Exception^ e)
            {
                ShowErrorMessage(e);
            }
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
        iconsMap = gcnew Dictionary<String^, Icon^>();
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
    }

    void MonitorGlobal::UnInit()
    {
        computer->Close();
    }

    String^ MonitorGlobal::GetString(String^ name)
    {
        try
        {
            return resourceManager->GetString(name);
        }
        catch (System::Exception^)
        {
            return nullptr;
        }
    }

    std::wstring MonitorGlobal::GetStdWString(String^ name)
    {
        return Common::StringToStdWstring(GetString(name));
    }

    Icon^ MonitorGlobal::GetAppIcon()
    {
        return GetIcon("logo");
    }

    Icon^ MonitorGlobal::GetIcon(String^ name)
    {
        Icon^ icon;
        if (!iconsMap->TryGetValue(name, icon))
        {
            icon = LoadIcon(name, CHardwareMonitor::GetInstance()->DPI(16));
            iconsMap->Add(name, icon);
        }
        return icon;
    }

    Resources::ResourceManager^ MonitorGlobal::GetResourceManager()
    {
        return resourceManager;
    }

    void MonitorGlobal::ShowHardwareInfoDialog()
    {
        try
        {
            HardwareInfoForm^ form = gcnew HardwareInfoForm();
            monitor_form = form;
            form->ShowDialog();
            monitor_form = nullptr;
        }
        catch (System::Exception^ e)
        {
            monitor_form = nullptr;
            CHardwareMonitor::ShowErrorMessage(e);
        }
        finally
        {
            monitor_form = nullptr;
        }
    }

    Icon^ MonitorGlobal::LoadIcon(String^ name, int icon_size)
    {
        try
        {
            //载入图标
            Icon^ tmp = dynamic_cast<Icon^>(resourceManager->GetObject(name));
            //转换为新的尺寸
            return gcnew Icon(tmp, Size(icon_size, icon_size));
        }
        catch (System::Exception^)
        {
            return nullptr;
        }
    }
}



ITMPlugin* TMPluginGetInstance()
{
    return HardwareMonitor::CHardwareMonitor::GetInstance();
}
