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
        ISensor^ sensor = HardwareMonitorHelper::FindSensorByIdentifyer(gcnew String(identifier.c_str()));
        if (sensor != nullptr)
            return Common::StringToStdWstring(HardwareMonitorHelper::GetSensorDisplayName(sensor));
        auto iter = m_item_names.find(identifier);
        if (iter != m_item_names.end())
        {
            return iter->second;
        }
        return identifier;
    }

    const std::wstring& CHardwareMonitor::GetConfigPath() const
    {
        return m_config_path;
    }

    void CHardwareMonitor::LoadConfig(const std::wstring& config_dir)
    {
        m_config_path = config_dir + L"HardwareMonitor.ini";
        utilities::CIniHelper ini(m_config_path);

        // Read the complete configuration before touching the hardware provider.
        // A transient Open/Update failure must not discard the saved selection.
        UpdateHardwareEnableFlags(ini.GetBool(L"hardware", L"IsCpuEnabled", true),
            ini.GetBool(L"hardware", L"IsGpuEnabled", true),
            ini.GetBool(L"hardware", L"IsMotherboardEnabled", true),
            ini.GetBool(L"hardware", L"IsStorageEnabled", true),
            ini.GetBool(L"hardware", L"IsBatteryEnabled", false),
            ini.GetBool(L"hardware", L"IsNetworkEnabled", false),
            ini.GetBool(L"hardware", L"IsMemoryEnabled", false),
            ini.GetBool(L"hardware", L"IsControllerEnabled", false),
            ini.GetBool(L"hardware", L"IsPsuEnabled", false));

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
            item_info.show_notify = ini.GetBool(app_name.c_str(), L"show_notify", false);
            item_info.notify_value = ini.GetDouble(app_name.c_str(), L"notify_value");
            m_settings.items_info.push_back(item_info);
            // The host enumerates these pointers once. Keep an item even when
            // its sensor is unavailable, and resolve its value/name on a later poll.
            m_item_names[item_info.identifyer] = item_info.identifyer;
            m_items.emplace_back(item_info.identifyer, item_info.identifyer, item_info.identifyer);
        }
        m_settings.hardware_info_auto_refresh = ini.GetBool(L"config", L"hardware_info_auto_refresh");
        m_settings.show_mouse_tooltip = ini.GetBool(L"config", L"show_mouse_tooltip", true);

        //载入树控件的展开折叠状态
        std::vector<std::wstring> collapse_nodes;
        ini.GetStringList(L"other", L"hardware_info_collapsed_nodes", collapse_nodes, std::vector<std::wstring>());
        for (const auto& node_path : collapse_nodes)
            MonitorGlobal::Instance()->treeCollapseNodes->Add(gcnew String(node_path.c_str()));

        auto global = MonitorGlobal::Instance();
        try
        {
            ApplyHardwareEnableFlags(global->computer);
            if (global->computer != nullptr)
            {
                global->computer->Accept(global->updateVisitor);
                // Metadata is needed during the host's initial item enumeration.
                // Value notifications must wait for DataRequired/OnInitialize.
                for (auto& item : m_items)
                    item.UpdateMetadata();
            }
        }
        catch (System::Exception^)
        {
            // Keep the loaded choices and let DataRequired retry the provider.
            global->UnInit();
        }

        //如果配置文件为空，则认为是首次使用硬件监控插件，添加默认的监控项
        if (ini.IsEmpty() && global->computer != nullptr)
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
    }

    void CHardwareMonitor::SaveConfig()
    {
        utilities::CIniHelper ini(m_config_path);

        //保存要监控的硬件
        ini.WriteBool(L"hardware", L"IsCpuEnabled", m_hardware_flags.cpu);
        ini.WriteBool(L"hardware", L"IsGpuEnabled", m_hardware_flags.gpu);
        ini.WriteBool(L"hardware", L"IsMotherboardEnabled", m_hardware_flags.motherboard);
        ini.WriteBool(L"hardware", L"IsStorageEnabled", m_hardware_flags.storage);
        ini.WriteBool(L"hardware", L"IsBatteryEnabled", m_hardware_flags.battery);
        ini.WriteBool(L"hardware", L"IsNetworkEnabled", m_hardware_flags.network);
        ini.WriteBool(L"hardware", L"IsMemoryEnabled", m_hardware_flags.memory);
        ini.WriteBool(L"hardware", L"IsControllerEnabled", m_hardware_flags.controller);
        ini.WriteBool(L"hardware", L"IsPsuEnabled", m_hardware_flags.psu);

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
            ini.WriteBool(app_name.c_str(), L"show_notify", item.show_notify);
            ini.WriteDouble(app_name.c_str(), L"notify_value", item.notify_value);
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

    ITrafficMonitor* CHardwareMonitor::GetMainApp() const
    {
        return m_pMainApp;
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
        if (AcquireMonitorData())
            return;

        //取数失败或 Computer 未初始化（初始化失败/驱动更新后失效）：
        //显卡驱动更新等场景会让 LHM(0.9.4) 内部按旧布局分配的传感器数组越界并抛出异常，
        //原来的做法是把异常静默吞掉，导致所有显示项目的数值一直冻结到程序重启。
        //这里改为限流重建 Computer（按新驱动状态重新枚举硬件），重建成功后立即重取一次数据
        if (CanAttemptReinit())
        {
            m_last_reinit_tick = static_cast<unsigned int>(Environment::TickCount);
            if (MonitorGlobal::Instance()->ReInit())
            {
                //重建后立即取一次数；若仍失败，数值保持冻结，下个监控周期继续走这里的恢复流程
                AcquireMonitorData();
            }
        }
    }

    bool CHardwareMonitor::AcquireMonitorData()
    {
        try
        {
            auto computer = MonitorGlobal::Instance()->computer;
            if (computer == nullptr)    //初始化失败或上次重建失败（例如显卡驱动正在安装）
                return false;
            //重新获取数据
            computer->Accept(MonitorGlobal::Instance()->updateVisitor);
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
            return true;
        }
        catch (System::Exception^)
        {
            //异常必须向上传递为“取数失败”，让 DataRequired 走重建恢复流程
            return false;
        }
    }

    bool CHardwareMonitor::CanAttemptReinit() const
    {
        //窗体持有旧 Computer 的 ISensor 引用，打开期间跳过重建，等窗体关闭后的下个周期再试
        if (MonitorGlobal::Instance()->monitor_form != nullptr || MonitorGlobal::Instance()->setttings_form != nullptr)
            return false;
        unsigned int now = static_cast<unsigned int>(Environment::TickCount);
        return m_last_reinit_tick == 0 || now - m_last_reinit_tick >= REINIT_INTERVAL_MS;
    }

    void CHardwareMonitor::UpdateHardwareEnableFlags(bool cpu, bool gpu, bool motherboard, bool storage,
        bool battery, bool network, bool memory, bool controller, bool psu)
    {
        m_hardware_flags.cpu = cpu;
        m_hardware_flags.gpu = gpu;
        m_hardware_flags.motherboard = motherboard;
        m_hardware_flags.storage = storage;
        m_hardware_flags.battery = battery;
        m_hardware_flags.network = network;
        m_hardware_flags.memory = memory;
        m_hardware_flags.controller = controller;
        m_hardware_flags.psu = psu;
    }

    const HardwareEnableFlags& CHardwareMonitor::GetHardwareEnableFlags() const
    {
        return m_hardware_flags;
    }

    void CHardwareMonitor::ApplyHardwareEnableFlags(Computer^ computer)
    {
        if (computer == nullptr)
            return;
        computer->IsCpuEnabled = m_hardware_flags.cpu;
        computer->IsGpuEnabled = m_hardware_flags.gpu;
        computer->IsMotherboardEnabled = m_hardware_flags.motherboard;
        computer->IsStorageEnabled = m_hardware_flags.storage;
        computer->IsBatteryEnabled = m_hardware_flags.battery;
        computer->IsNetworkEnabled = m_hardware_flags.network;
        computer->IsMemoryEnabled = m_hardware_flags.memory;
        computer->IsControllerEnabled = m_hardware_flags.controller;
        computer->IsPsuEnabled = m_hardware_flags.psu;
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
            return L"1.0.1";
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
        case ITMPlugin::EI_DRAW_TASKBAR_WND:
            m_tm_settings.is_taskbar = _wtoi(data);
            break;
        case ITMPlugin::EI_MAIN_WND_SPERATE_WITH_SPACE:
            m_tm_settings.main_wnd_sperate_with_space = _wtoi(data);
            break;
        case ITMPlugin::EI_TASKBAR_WND_SPERATE_WITH_SPACE:
            m_tm_settings.taskbar_sperate_with_space = _wtoi(data);
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

    void CHardwareMonitor::OnInitialize(ITrafficMonitor* pApp)
    {
        m_pMainApp = pApp;
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
        //先创建资源管理器（与 Computer 无关），Computer 打开失败也不影响字符串资源的获取
        resourceManager = gcnew Resources::ResourceManager("HardwareMonitor.resource", Reflection::Assembly::GetExecutingAssembly());
        try
        {
            computer = gcnew Computer();
            computer->Open();
        }
        catch (System::Exception^)
        {
            //Open 失败时置空，DataRequired 会通过 ReInit 限流重试恢复；
            //异常继续抛给 GetInstance 的 catch 显示错误信息（保持原有行为）
            computer = nullptr;
            throw;
        }
    }

    void MonitorGlobal::UnInit()
    {
        if (computer != nullptr)
        {
            try
            {
                computer->Close();
            }
            catch (System::Exception^) { /*关闭期间的托管异常不允许逃逸出析构路径*/ }
            computer = nullptr;
        }
        sensorMap->Clear();
    }

    bool MonitorGlobal::ReInit()
    {
        //显卡驱动更新后，旧的 Computer 持有的 NVAPI 句柄和按旧布局分配的传感器数组全部失效
        //(Update 时抛出越界异常)，Close 后重新 Open 才会按新驱动状态重新枚举硬件
        if (computer != nullptr)
        {
            try
            {
                computer->Close();
            }
            catch (System::Exception^) { /*旧的 Computer 已经失效，关闭失败也继续重建*/ }
            computer = nullptr;
        }
        if (updateVisitor == nullptr)
            updateVisitor = gcnew UpdateVisitor();
        try
        {
            computer = gcnew Computer();
            // Apply the saved choices before enumerating the replacement provider.
            CHardwareMonitor::GetInstance()->ApplyHardwareEnableFlags(computer);
            computer->Open();
        }
        catch (System::Exception^)
        {
            //重建失败（例如驱动正在安装、NVAPI 暂不可用）：保持未初始化状态，调用方稍后重试
            computer = nullptr;
        }
        //传感器缓存里的 ISensor 对象属于旧的 Computer，已全部失效，必须清空重新解析
        sensorMap->Clear();
        return computer != nullptr;
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
