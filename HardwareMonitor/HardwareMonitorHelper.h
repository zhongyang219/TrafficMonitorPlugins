#pragma once
namespace HardwareMonitor
{
    class HardwareMonitorHelper
    {
    public:
        static System::String^ GetSensorTypeName(LibreHardwareMonitor::Hardware::SensorType type);
        static const wchar_t* GetSensorTypeUnit(LibreHardwareMonitor::Hardware::SensorType type);
        static LibreHardwareMonitor::Hardware::ISensor^ FindSensorByIdentifyer(System::String^ identifyer);
        static System::String^ GetSensorValueText(LibreHardwareMonitor::Hardware::ISensor^ sensor, int decimal_place = 2);
        static System::String^ GetSensorNameValueText(LibreHardwareMonitor::Hardware::ISensor^ sensor);
        static System::String^ GetSensorDisplayName(LibreHardwareMonitor::Hardware::ISensor^ sensor);
    };
}

