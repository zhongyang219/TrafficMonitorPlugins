// 这是主 DLL 文件。

#include "stdafx.h"

#include <atomic>
#include "HardwareMonitor.h"
#include <vector>
#include "HardwareMonitorHelper.h"
#include "../utilities/IniHelper.h"
#include "SettingsForm.h"
#include "Common.h"

namespace HardwareMonitor
{
    //////////////////////////////////////////////////////////////////
    // 显示器布局变化监听（纯.NET实现，不依赖Win32 API）
    //
    // 显示器连接/断开（如只保留核显显示器、断开独显显示器）后，
    // LibreHardwareMonitorLib内部会重新枚举GPU硬件对象，
    // 导致显存传感器缓存中引用的旧硬件/旧传感器不再被更新，
    // 显示值冻结在变化前的数值上。
    //
    // 订阅Microsoft.Win32.SystemEvents.DisplaySettingsChanged事件
    // （该事件由框架内部监视窗口接收系统WM_DISPLAYCHANGE广播后触发，
    // 显示器分辨率、布局或连接状态变化时产生；框架自带监听窗口和
    // 消息循环，与订阅线程无关）：
    // 布局变化后系统不会立刻就绪（本地LLM引擎重新分配显存、
    // 硬件重新枚举完成都有延迟），事件回调记录变化时刻并进入
    // 60秒“无缓存窗口”：DataRequired()在窗口内每次刷新前都清空
    // 显存传感器缓存并重新解析硬件和传感器引用，保证窗口结束时
    // 缓存被稳定对象回填，之后恢复正常缓存热路径。
    // 平时热路径不做任何额外操作，只有布局变化后的60秒窗口内
    // 每次刷新多一次硬件解析。显示器布局切换不是高频操作，
    // 60秒窗口用于覆盖GPU/显示子系统延迟稳定、句柄失效后重新枚举的过程。
    //
    // 布局稳定期间另有每分钟一次的兜底缓存重建：只在到期的那次
    // DataRequired() 前清空缓存，随后重新解析一次并继续缓存；
    // 用于覆盖DisplaySettingsChanged漏报、窗口结束后才失效的句柄，
    // 以及驱动/GPU电源状态等其他原因导致的缓存失效。
    //////////////////////////////////////////////////////////////////
    namespace
    {
        //布局变化后无缓存窗口的时长（毫秒）
        //60秒：显示器布局切换后GPU/显示子系统可能需要较长时间才稳定，
        //10秒窗口不足以覆盖句柄失效后的重新枚举过程。
        const int DISPLAY_CHANGE_CACHE_WINDOW_MS = 60 * 1000;

        //最近一次布局变化的单调时间（Environment.TickCount，毫秒），
        //0表示从未变化。事件回调与DataRequired可能不在同一线程，
        //用原子变量。
        std::atomic<int> g_display_change_tick{ 0 };

        //记录布局变化时刻，开始无缓存窗口
        void MarkDisplayChanged()
        {
            g_display_change_tick.store((int)System::Environment::TickCount);
        }

        //是否处于布局变化后的无缓存窗口内
        //（无符号减法，TickCount自然回绕不影响结果）
        bool InDisplayChangeCacheWindow()
        {
            int last = g_display_change_tick.load();
            if (last == 0)
                return false;
            int now = (int)System::Environment::TickCount;
            return (unsigned int)(now - last) < DISPLAY_CHANGE_CACHE_WINDOW_MS;
        }

        //////////////////////////////////////////////////////////////////
        // 稳定布局期间的周期性缓存重建
        //
        // 显示器布局稳定后，缓存中的硬件/传感器句柄仍可能因驱动、
        // GPU电源状态或其他原因失效，而DisplaySettingsChanged不一定
        // 能覆盖这些变化。这里每分钟做一次“完整查询”：
        // 清空显存缓存 -> 下一次取数重新遍历当前硬件树并匹配传感器
        // -> 成功后继续走缓存热路径。
        // 不是每分钟持续禁用缓存，也不是每次刷新都校验句柄，
        // 因此开销仅为每分钟一次硬件树解析。
        //////////////////////////////////////////////////////////////////
        const int PERIODIC_CACHE_CLEAR_INTERVAL_MS = 60 * 1000;

        //上次周期性清空缓存的时间；0表示尚未执行过
        std::atomic<int> g_last_periodic_cache_clear_tick{ 0 };

        void ClearGpuMemoryCacheIfPeriodicDue()
        {
            int now = (int)System::Environment::TickCount;
            int last = g_last_periodic_cache_clear_tick.load();
            if (last == 0 || (unsigned int)(now - last) >= PERIODIC_CACHE_CLEAR_INTERVAL_MS)
            {
                HardwareMonitorHelper::ClearGpuMemoryCache();
                g_last_periodic_cache_clear_tick.store(now);
            }
        }
    }

    void MonitorGlobal::OnDisplaySettingsChanged(Object^ sender, EventArgs^ e)
    {
        MarkDisplayChanged();
    }

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

    bool CHardwareMonitor::AddGpuMemoryDisplayItem(IHardware^ hardware)
    {
        if (!HardwareMonitorHelper::IsGpuHardware(hardware) || !HardwareMonitorHelper::HasGpuMemorySensors(hardware))
            return false;
        String^ identifier = HardwareMonitorHelper::GetGpuMemoryItemIdentifier(hardware);
        if (String::IsNullOrEmpty(identifier))
            return false;
        return AddGpuMemoryDisplayItem(Common::StringToStdWstring(identifier));
    }

    bool CHardwareMonitor::AddGpuMemoryDisplayItem(const std::wstring& identifier)
    {
        if (!HardwareMonitorHelper::IsGpuMemoryItemIdentifier(identifier))
            return false;
        if (IsDisplayItemExist(identifier))
            return false;

        String^ item_identifier = gcnew String(identifier.c_str());
        String^ hardware_identifier = HardwareMonitorHelper::GetGpuMemoryHardwareIdentifier(item_identifier);
        if (HardwareMonitorHelper::FindHardwareByIdentifier(hardware_identifier) == nullptr)
            return false;

        std::wstring item_name = Common::StringToStdWstring(HardwareMonitorHelper::GetGpuMemoryDisplayName(item_identifier));
        std::wstring label_text = Common::StringToStdWstring(HardwareMonitorHelper::GetGpuMemoryLabel());

        ItemInfo item_info;
        item_info.identifyer = identifier;
        //显存项目默认使用G单位：任务栏窗口显示“45.1G”这样的短格式，宽度最省
        item_info.unit = L"G";
        item_info.decimal_places = 1;
        //不指定数值宽度，示例文本直接使用当前数值，使显示区域保持紧凑
        item_info.specify_value_width = false;
        item_info.value_width = 3;
        item_info.show_unit = true;
        m_settings.items_info.push_back(item_info);

        m_item_names[identifier] = item_name;
        m_items.emplace_back(identifier, item_name, label_text);
        return true;
    }

    void CHardwareMonitor::AddGpuMemoryItemsForAllGpus()
    {
        Computer^ computer = MonitorGlobal::Instance()->computer;
        for each (IHardware ^ hardware in computer->Hardware)
        {
            AddGpuMemoryDisplayItem(hardware);
            for each (IHardware ^ sub_hardware in hardware->SubHardware)
                AddGpuMemoryDisplayItem(sub_hardware);
        }
    }

    std::wstring CHardwareMonitor::GetItemName(const std::wstring& identifier)
    {
        auto iter = m_item_names.find(identifier);
        if (iter != m_item_names.end())
        {
            return iter->second;
        }
        else if (HardwareMonitorHelper::IsGpuMemoryItemIdentifier(identifier))
        {
            return Common::StringToStdWstring(HardwareMonitorHelper::GetGpuMemoryDisplayName(gcnew String(identifier.c_str())));
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
            item_info.show_notify = ini.GetBool(app_name.c_str(), L"show_notify", false);
            item_info.notify_value = ini.GetDouble(app_name.c_str(), L"notify_value");
            m_settings.items_info.push_back(item_info);
            if (HardwareMonitorHelper::IsGpuMemoryItemIdentifier(item_info.identifyer))
            {
                //显卡显存项目不是真实的传感器
                String^ item_identifier = gcnew String(item_info.identifyer.c_str());
                std::wstring item_name = Common::StringToStdWstring(HardwareMonitorHelper::GetGpuMemoryDisplayName(item_identifier));
                std::wstring lable_text = Common::StringToStdWstring(HardwareMonitorHelper::GetGpuMemoryLabel());
                m_item_names[item_info.identifyer] = item_name;
                m_items.emplace_back(item_info.identifyer, item_name, lable_text);
            }
            else
            {
                ISensor^ sensor = HardwareMonitorHelper::FindSensorByIdentifyer(gcnew String(item_info.identifyer.c_str()));
                if (sensor != nullptr)
                {
                    std::wstring item_name = Common::StringToStdWstring(HardwareMonitorHelper::GetSensorDisplayName(sensor));
                    std::wstring lable_text = Common::StringToStdWstring(Common::GetTranslatedString(sensor->Name));
                    m_item_names[item_info.identifyer] = item_name;
                    m_items.emplace_back(item_info.identifyer, item_name, lable_text);
                }
            }
        }
        m_settings.hardware_info_auto_refresh = ini.GetBool(L"config", L"hardware_info_auto_refresh");
        m_settings.show_mouse_tooltip = ini.GetBool(L"config", L"show_mouse_tooltip", true);
        m_settings.gpu_memory_item_added = ini.GetBool(L"config", L"gpu_memory_item_added", false);

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

        //首次运行此版本时，为每个显卡自动添加一个“已占用显存/总显存”项目
        if (!m_settings.gpu_memory_item_added)
        {
            AddGpuMemoryItemsForAllGpus();
            m_settings.gpu_memory_item_added = true;
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
            ini.WriteBool(app_name.c_str(), L"show_notify", item.show_notify);
            ini.WriteDouble(app_name.c_str(), L"notify_value", item.notify_value);
            index++;
        }
        ini.WriteBool(L"config", L"hardware_info_auto_refresh", m_settings.hardware_info_auto_refresh);
        ini.WriteBool(L"config", L"show_mouse_tooltip", m_settings.show_mouse_tooltip);
        ini.WriteBool(L"config", L"gpu_memory_item_added", m_settings.gpu_memory_item_added);

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
        try
        {
            //显示器布局变化（显示器连接/断开等）后系统未立刻就绪，
            //60秒无缓存窗口内：每次刷新前清空缓存并重新解析硬件和
            //传感器引用，保证窗口结束时缓存被稳定对象回填，
            //窗口结束后自动恢复正常缓存热路径
            if (InDisplayChangeCacheWindow())
            {
                HardwareMonitorHelper::ClearGpuMemoryCache();
            }
            else
            {
                //布局稳定期间兜底：每分钟清空一次缓存，下一次取数
                //重新遍历硬件树并匹配传感器，然后继续缓存。
                ClearGpuMemoryCacheIfPeriodicDue();
            }
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
                tooltip_info += item.GetTooltipValueText();
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
        computer = gcnew Computer();
        computer->Open();

        //订阅显示器设置变化事件（纯.NET，事件由系统WM_DISPLAYCHANGE广播驱动），
        //布局变化后进入60秒显存缓存无缓存窗口
        display_settings_changed_handler = gcnew EventHandler(this, &MonitorGlobal::OnDisplaySettingsChanged);
        Microsoft::Win32::SystemEvents::DisplaySettingsChanged += display_settings_changed_handler;

        resourceManager = gcnew Resources::ResourceManager("HardwareMonitor.resource", Reflection::Assembly::GetExecutingAssembly());
    }

    void MonitorGlobal::UnInit()
    {
        if (display_settings_changed_handler != nullptr)
        {
            Microsoft::Win32::SystemEvents::DisplaySettingsChanged -= display_settings_changed_handler;
            display_settings_changed_handler = nullptr;
        }
        computer->Close();
        //硬件关闭后缓存的传感器引用失效，清空显存传感器缓存
        HardwareMonitorHelper::ClearGpuMemoryCache();
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
