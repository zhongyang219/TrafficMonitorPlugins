#pragma once

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

        //获取一个传感器数值的文本，并指定小数位数
        static System::String^ GetSensorValueText(LibreHardwareMonitor::Hardware::ISensor^ sensor, System::String^ unit, int decimal_place = 2, bool show_unit = true);

        //获取一个传感器的显示名称（由硬件、传感器类型、传感器组成）
        static System::String^ GetSensorDisplayName(LibreHardwareMonitor::Hardware::ISensor^ sensor);

        //获取一个传感器的唯一标识符
        static System::String^ GetSensorIdentifyer(LibreHardwareMonitor::Hardware::ISensor^ sensor);

        //获取默认的监控项目
        static void GetDefaultMonitorItem(System::Collections::Generic::List<LibreHardwareMonitor::Hardware::ISensor^>^ default_sensors);
    };
}

