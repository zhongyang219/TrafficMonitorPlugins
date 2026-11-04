#pragma once
#include <string>

namespace HardwareMonitor
{
    class HardwareMonitorHelper
    {
    public:
        //获取传感器类型名称
        static System::String^ GetSensorTypeName(LibreHardwareMonitor::Hardware::SensorType type);

        //根据硬件类型获取图标资源名称
        static System::String^ GetHardwareIconResName(LibreHardwareMonitor::Hardware::HardwareType type);

        //根据传感器类型获取数据的单位列表
        //可能会有多个可用单位，默认的单位总是为列表的第一个元素
        static System::Collections::Generic::List<System::String^>^ GetSensorTypeUnit(LibreHardwareMonitor::Hardware::SensorType type);

        //根据传感器类型获取数据的默认单位
        static System::String^ GetSensorTypeDefaultUnit(LibreHardwareMonitor::Hardware::SensorType type);

        //根据一个传感器的唯一标识符查找一个传感器
        static LibreHardwareMonitor::Hardware::ISensor^ FindSensorByIdentifyer(System::String^ identifyer);

        //根据指定单位，获取一个传感器数值
        static float GetSensorValue(LibreHardwareMonitor::Hardware::ISensor^ sensor, System::String^ unit);

        //获取一个传感器数值的文本，并指定小数位数
        static System::String^ GetSensorValueText(LibreHardwareMonitor::Hardware::ISensor^ sensor, System::String^ unit, int decimal_place = 2, bool show_unit = true, bool seperate_with_space = true);

        //获取一个传感器的显示名称（由硬件、传感器类型、传感器组成）
        static System::String^ GetSensorDisplayName(LibreHardwareMonitor::Hardware::ISensor^ sensor);

        //获取一个传感器的唯一标识符
        static System::String^ GetSensorIdentifyer(LibreHardwareMonitor::Hardware::ISensor^ sensor);

        //获取默认的监控项目
        static void GetDefaultMonitorItem(System::Collections::Generic::List<LibreHardwareMonitor::Hardware::ISensor^>^ default_sensors);

        //////////////////////////////////////////////////////////////////////////
        // 显卡显存项目（已占用显存/总显存）
        //////////////////////////////////////////////////////////////////////////

        //判断一个硬件是否为显卡
        static bool IsGpuHardware(LibreHardwareMonitor::Hardware::IHardware^ hardware);

        //获取显卡显存项目的标识符（它不是真实的传感器，而是一个虚拟的监控项目）
        static System::String^ GetGpuMemoryItemIdentifier(LibreHardwareMonitor::Hardware::IHardware^ hardware);

        //判断一个标识符是否为显卡显存项目
        static bool IsGpuMemoryItemIdentifier(System::String^ identifier);
        static bool IsGpuMemoryItemIdentifier(const std::wstring& identifier);

        //从显存项目的标识符中获取显卡硬件的标识符
        static System::String^ GetGpuMemoryHardwareIdentifier(System::String^ identifier);
        static std::wstring GetGpuMemoryHardwareIdentifier(const std::wstring& identifier);

        //根据硬件标识符在硬件树中精确查找硬件
        static LibreHardwareMonitor::Hardware::IHardware^ FindHardwareByIdentifier(System::String^ identifier);

        //判断一个显卡是否有可用的显存传感器
        static bool HasGpuMemorySensors(LibreHardwareMonitor::Hardware::IHardware^ hardware);

        //获取显卡的已占用显存和总显存，单位为GB。
        //解析结果（硬件和传感器引用）会按identifier缓存：
        //首次调用执行硬件树遍历和传感器名称匹配，之后直接从缓存读取传感器数值，
        //查找失败时不写缓存，下次调用自动重试
        static bool GetGpuMemoryValues(const std::wstring& identifier, float% used_gb, float% total_gb);
        static bool GetGpuMemoryValues(System::String^ identifier, float% used_gb, float% total_gb);

        //获取显存项目的数值文本（完整格式“已占用/总量”），unit可以为G、GB或%。
        //unit为%时输出整数百分比（如“94 %”），与CPU/内存等百分比项目一致
        static System::String^ GetGpuMemoryValueText(System::String^ identifier, System::String^ unit, int decimal_place = 1, bool show_unit = true, bool seperate_with_space = true);

        //获取显存项目的短格式数值文本（任务栏窗口固定显示短格式），unit可以为G、GB或%：
        //G/GB输出已占用显存（如“45.1G”“45.1 GB”），%输出整数百分比（如“98 %”），与CPU/内存等百分比项目一致
        static System::String^ GetGpuMemoryShortValueText(System::String^ identifier, System::String^ unit, int decimal_place = 1, bool show_unit = true, bool seperate_with_space = true);

        //基于已取到的数值生成完整格式文本“已占用/总量”（或整数百分比），不执行任何硬件查找。
        //与GetGpuMemoryValueText的单位和空格约定一致，用于“单次取数、分别格式化”的热路径
        static std::wstring FormatGpuMemoryFullValueText(float used_gb, float total_gb, const std::wstring& unit, int decimal_place = 1, bool show_unit = true, bool seperate_with_space = true);

        //基于已取到的数值生成短格式文本（已占用显存或整数百分比），不执行任何硬件查找。
        //与GetGpuMemoryShortValueText的单位和空格约定一致
        static std::wstring FormatGpuMemoryShortValueText(float used_gb, float total_gb, const std::wstring& unit, int decimal_place = 1, bool show_unit = true, bool seperate_with_space = true);

        //清空显存传感器引用缓存（硬件或传感器可能变化时调用，如监控硬件设置更改后）
        static void ClearGpuMemoryCache();

        //清空普通传感器引用缓存（FindSensorByIdentifyer 的 sensorMap，
        //覆盖 GPU Load、温度等非显存项目）
        static void ClearSensorCache();

        //仅清空 sensorMap 中的 GPU Load 条目（GPU 占用百分比项目），
        //不清除 CPU、内存、主板等与显示器布局变化无关的传感器缓存
        static void ClearGpuLoadSensorCache();

        //同时清空显存缓存和 GPU Load 普通传感器缓存，
        //供显示器布局变化后的窗口期和稳定期间周期重建使用
        static void ClearGpuRelatedCaches();

        //同时清空显存缓存和全部普通传感器缓存，
        //供硬件设置变更或插件退出等需要整体重建缓存的场景使用
        static void ClearAllCaches();

        //获取显存项目的单位文本（含与数字之间的空格约定）：
        //G为紧凑单位，与数字之间不加空格，如“45.1G”；
        //选GB说明任务栏窗口宽度宽裕，单位前加空格排版更好，如“45.1 GB”；
        //%遵循“数值和单位使用空格分隔”设置，与CPU/内存等百分比项目一致，如“94 %”
        static System::String^ GetGpuMemoryUnitText(System::String^ unit, bool show_unit, bool seperate_with_space);

        //获取显存项目的显示名称，格式为“显卡名称/显存已占用/总显存”
        static System::String^ GetGpuMemoryDisplayName(System::String^ identifier);

        //获取显存项目的标签文本
        static System::String^ GetGpuMemoryLabel();
    };
}

