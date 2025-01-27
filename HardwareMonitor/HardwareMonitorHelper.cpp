#include "stdafx.h"
#include "HardwareMonitorHelper.h"
#include "HardwareMonitor.h"

using namespace LibreHardwareMonitor::Hardware;

namespace HardwareMonitor
{
    const wchar_t* HardwareMonitorHelper::GetSensorTypeName(SensorType type)
    {
        switch (type)
        {
        case SensorType::Voltage: return L"Voltage";
        case SensorType::Current: return L"Current";
        case SensorType::Power: return L"Power";
        case SensorType::Clock: return L"Clock";
        case SensorType::Temperature: return L"Temperature";
        case SensorType::Load: return L"Load";
        case SensorType::Frequency: return L"Frequency";
        case SensorType::Fan: return L"Fan";
        case SensorType::Flow: return L"Flow";
        case SensorType::Control: return L"Control";
        case SensorType::Level: return L"Level";
        case SensorType::Factor: return L"Factor";
        case SensorType::Data: return L"Data";
        case SensorType::SmallData: return L"SmallData";
        case SensorType::Throughput: return L"Throughput";
        case SensorType::TimeSpan: return L"TimeSpan";
        case SensorType::Energy: return L"Energy";
        case SensorType::Noise: return L"Noise";
        case SensorType::Conductivity: return L"Conductivity";
        case SensorType::Humidity: return L"Humidity";
        }
        return L"";
    }

    const wchar_t* HardwareMonitorHelper::GetSensorTypeUnit(SensorType type)
    {
        switch (type)
        {
        case SensorType::Voltage: return L"V";
        case SensorType::Current: return L"A";
        case SensorType::Power: return L"W";
        case SensorType::Clock: return L"MHz";
        case SensorType::Temperature: return L"¡ãC";
        case SensorType::Load: return L"%";
        case SensorType::Frequency: return L"";
        case SensorType::Fan: return L"RPM";
        case SensorType::Flow: return L"";
        case SensorType::Control: return L"%";
        case SensorType::Level: return L"";
        case SensorType::Factor: return L"";
        case SensorType::Data: return L"GB";
        case SensorType::SmallData: return L"MB";
        case SensorType::Throughput: return L"KB/s";
        case SensorType::TimeSpan: return L"";
        case SensorType::Energy: return L"";
        case SensorType::Noise: return L"";
        case SensorType::Conductivity: return L"";
        case SensorType::Humidity: return L"";
        }
        return L"";
    }

    LibreHardwareMonitor::Hardware::ISensor^ HardwareMonitorHelper::FindSensorByIdentifyer(System::String^ identifyer)
    {
        auto computer = MonitorGlobal::Instance()->computer;
        for (int i = 0; i < computer->Hardware->Count; i++)
        {
            auto hardware = computer->Hardware[i];
            for (int j = 0; j < hardware->Sensors->Length; j++)
            {
                auto sensor = hardware->Sensors[j];
                if (sensor->Identifier->ToString() == identifyer)
                {
                    return sensor;
                }
            }
        }
        return nullptr;
    }

    System::String^ HardwareMonitorHelper::GetSensorValueText(LibreHardwareMonitor::Hardware::ISensor^ sensor)
    {
        String^ sensor_str;
        if (sensor->Value.HasValue)
        {
            sensor_str += sensor->Value.Value.ToString("F2");
        }
        else
        {
            sensor_str += "--";
        }
        sensor_str += " ";
        sensor_str += gcnew String(GetSensorTypeUnit(sensor->SensorType));
        return sensor_str;
    }

    String^ HardwareMonitorHelper::GetSensorNameValueText(LibreHardwareMonitor::Hardware::ISensor^ sensor)
    {
        String^ sensor_str;
        sensor_str = sensor->Name;
        sensor_str += L"    ";
        sensor_str += GetSensorValueText(sensor);
        return sensor_str;
    }
    String^ HardwareMonitorHelper::GetSensorDisplayName(LibreHardwareMonitor::Hardware::ISensor^ sensor)
    {
        String^ name;
        IHardware^ hardware = sensor->Hardware;
        name = hardware->Name;
        name += "|";
        name += gcnew String(GetSensorTypeName(sensor->SensorType));
        name += "|";
        name += sensor->Name;
        return name;
    }
}
