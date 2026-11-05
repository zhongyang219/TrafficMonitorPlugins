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

        std::wstring GetItemName(const std::wstring& identifier);

        const std::wstring& GetConfigPath() const;

        void LoadConfig(const std::wstring& config_dir);
        void SaveConfig();

        //快照当前要监控的硬件开关设置（LoadConfig 和 SettingsForm::OnFormClosing 调用）。
        //重建 Computer 后需要按快照重新应用这些开关（见 ApplyHardwareEnableFlags）
        void UpdateHardwareEnableFlags(bool cpu, bool gpu, bool motherboard, bool storage,
            bool battery, bool network, bool memory, bool controller, bool psu);
        //把快照的硬件开关应用到 computer。LHM 只在 Computer::Open() 时按开关枚举硬件，
        //Open 之后再设置开关是无效的，因此必须在 Open 之前调用
        void ApplyHardwareEnableFlags(Computer^ computer);

        int DPI(int pixel) const;

        ITrafficMonitor* GetMainApp() const;

        OptionSettings m_settings;
        TMSettings m_tm_settings;

        static void ShowErrorMessage(System::Exception^ e);

    private:
        //执行一次取数（Accept + 更新窗体 + 更新显示项目）。成功返回 true；
        //Computer 未初始化或取数抛出异常时返回 false（异常不再静默吞掉，否则所有数值会冻结）
        bool AcquireMonitorData();
        //是否允许重建 Computer：限流（REINIT_INTERVAL_MS），且"硬件信息"/"选项设置"窗体未打开
        //(窗体持有旧 Computer 的 ISensor 引用，重建期间打开的窗体会一直显示失效数据)
        bool CanAttemptReinit() const;

        static CHardwareMonitor* m_pIns;
        std::vector<CHardwareMonitorItem> m_items;
        std::wstring m_config_path;
        std::map<std::wstring, std::wstring> m_item_names;  //保存每个Sensor的identifier和名称的map

        //硬件开关快照（见 UpdateHardwareEnableFlags），默认值与 LoadConfig 中 ini 的默认值一致
        bool m_hw_cpu{ true };
        bool m_hw_gpu{ true };
        bool m_hw_motherboard{ true };
        bool m_hw_storage{ true };
        bool m_hw_battery{ false };
        bool m_hw_network{ false };
        bool m_hw_memory{ false };
        bool m_hw_controller{ false };
        bool m_hw_psu{ false };

        unsigned int m_last_reinit_tick{};                 //上次尝试重建 Computer 的时刻（Environment::TickCount）
        static constexpr unsigned int REINIT_INTERVAL_MS = 5000;   //重建 Computer 的最小间隔（毫秒），驱动安装期间避免频繁重建

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
        //关闭旧的 Computer 并重新枚举硬件（显卡驱动更新后恢复传感器布局用）。
        //返回 false 表示重建失败（例如驱动正在安装），此时 computer 置空，调用方稍后重试。
        //重建成功后会清空 sensorMap（传感器对象属于旧的 Computer，已全部失效）
        bool ReInit();

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
