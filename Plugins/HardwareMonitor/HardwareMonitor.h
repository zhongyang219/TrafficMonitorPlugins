// HardwareMonitor.h

#pragma once
#include <string>
#include "UpdateVisitor.h"
#include <map>
#include "PluginInterface.h"
#include "HardwareInfoForm.h"
#include "SettingsForm.h"
#include "HardwareMonitorItem.h"
#include <vector>
#include "CommonData.h"

using namespace System;
using namespace System::Collections::Generic;
using namespace LibreHardwareMonitor::Hardware;

namespace HardwareMonitor
{
    public class CHardwareMonitor : public ITMPlugin
	{
    private:
        CHardwareMonitor();
        virtual ~CHardwareMonitor();

    public:
        static CHardwareMonitor* GetInstance();

        const std::wstring& StringRes(const wchar_t* name);

        //添加一个监控项目。
        //如果监控项目已存在，返回false，否则返回true
        bool AddDisplayItem(ISensor^ sensor);

        //判断一个监控项目是否已存在
        bool IsDisplayItemExist(const std::wstring& identifyer);

        //移除一个监控项目
        //成功返回true，否则返回false
        bool RemoveDisplayItem(int index);

        //添加一个显卡显存监控项目（已占用显存/总显存）
        bool AddGpuMemoryDisplayItem(LibreHardwareMonitor::Hardware::IHardware^ hardware);
        bool AddGpuMemoryDisplayItem(const std::wstring& identifier);
        //为所有支持显存监控的显卡添加显存项目
        void AddGpuMemoryItemsForAllGpus();

        std::wstring GetItemName(const std::wstring& identifier);

        const std::wstring& GetConfigPath() const;

        void LoadConfig(const std::wstring& config_dir);
        void SaveConfig();

        int DPI(int pixel) const;

        ITrafficMonitor* GetMainApp() const;

        OptionSettings m_settings;
        TMSettings m_tm_settings;

        static void ShowErrorMessage(System::Exception^ e);

    private:
        static CHardwareMonitor* m_pIns;
        std::vector<CHardwareMonitorItem> m_items;
        std::wstring m_config_path;
        std::map<std::wstring, std::wstring> m_item_names;  //保存每个Sensor的identifier和名称的map

        // 通过 ITMPlugin 继承
        IPluginItem* GetItem(int index) override;
        void DataRequired() override;
        const wchar_t* GetInfo(PluginInfoIndex index) override;
        virtual const wchar_t* GetTooltipInfo() override;
        virtual OptionReturn ShowOptionsDialog(void* hParent) override;
        virtual void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
        virtual void* GetPluginIcon() override;

        virtual int GetCommandCount() override;
        virtual const wchar_t* GetCommandName(int command_index) override;
        virtual void OnPluginCommand(int command_index, void* hWnd, void* para) override;
        virtual void OnInitialize(ITrafficMonitor* pApp) override;

        //显存读数路径活性检测（每分钟一次的显存数值停滞监控）：
        //显存数值连续30分钟不变（或取不到值）时判定读数路径失效，
        //通过Computer.Reset()重建硬件组并清空传感器缓存
        void CheckGpuHardwareStaleness();

        //显存数值停滞监控：逐个检查显存项目的已占用值，
        //数值连续30分钟不变（或连续30分钟取不到值）时
        //判定读数路径失效（如Nvidia独显的NVAPI显示句柄失效），
        //返回true请求重建硬件组；同时维护各项目的监控基准。
        //调用方在重建后需重置监控基准以避免立即再次触发
        bool VramStallWatchdog(int now_tick);

    private:
        std::map<const wchar_t*, std::wstring> string_table;
        int m_dpi{ 96 };
        ITrafficMonitor* m_pMainApp{};
    };

    public ref class MonitorGlobal
    {
    public:
        MonitorGlobal();
        ~MonitorGlobal();
        static MonitorGlobal^ Instance()
        {
            if (m_instance == nullptr)
            {
                m_instance = gcnew MonitorGlobal();
            }
            return m_instance;
        }

        void Init();
        void UnInit();

        //显示器设置变化事件回调（Microsoft.Win32.SystemEvents.DisplaySettingsChanged），
        //显示器分辨率、布局或连接状态变化时由框架触发，
        //记录变化时刻并进入60秒显存缓存无缓存窗口
        void OnDisplaySettingsChanged(Object^ sender, EventArgs^ e);

        String^ GetString(String^ name);
        std::wstring GetStdWString(String^ name);
        Icon^ GetAppIcon();
        Icon^ GetIcon(String^ name);
        Resources::ResourceManager^ GetResourceManager();

        void ShowHardwareInfoDialog();

    private:
        //从resx资源文件载入一个图标，并指定图标大小
        Icon^ LoadIcon(String^ name, int icon_size);

    public:
        Computer^ computer;
        UpdateVisitor^ updateVisitor{};

        HardwareInfoForm^ monitor_form{};
        SettingsForm^ setttings_form{};
        SortedSet<String^>^ treeCollapseNodes{ gcnew SortedSet<String^>() };                    //保存“硬件信息”对话框树控件中折叠的节点
        Dictionary<String^, ISensor^>^ sensorMap{ gcnew Dictionary<String^, ISensor^>() };      //保存传感器的identifyer和传感器对象的映射

    private:
        Resources::ResourceManager^ resourceManager{};
        //保存所有图标
        Dictionary<String^, Icon^>^ iconsMap{};
        //显示器设置变化事件（Microsoft.Win32.SystemEvents.DisplaySettingsChanged）的
        //订阅句柄，用于UnInit时退订
        EventHandler^ display_settings_changed_handler{};

    private:
        static MonitorGlobal^ m_instance{};
    };
}


#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) ITMPlugin* TMPluginGetInstance();

#ifdef __cplusplus
}
#endif
