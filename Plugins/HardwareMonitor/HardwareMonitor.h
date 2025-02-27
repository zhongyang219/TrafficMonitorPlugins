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
#include <map>
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

        int DPI(int pixel) const;

        OptionSettings m_settings;

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

    private:
        std::map<const wchar_t*, std::wstring> string_table;
        int m_dpi{ 96 };
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
