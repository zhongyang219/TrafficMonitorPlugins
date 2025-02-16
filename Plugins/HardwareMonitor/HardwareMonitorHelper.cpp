#include "stdafx.h"
#include "HardwareMonitorHelper.h"
#include "HardwareMonitor.h"
#include <gcroot.h>

using namespace LibreHardwareMonitor::Hardware;

namespace HardwareMonitor
{
    System::String^ HardwareMonitorHelper::GetSensorTypeName(SensorType type)
    {
        switch (type)
        {
        case SensorType::Voltage: return MonitorGlobal::Instance()->GetString(L"Voltage");
        case SensorType::Current: return MonitorGlobal::Instance()->GetString(L"Current");
        case SensorType::Power: return MonitorGlobal::Instance()->GetString(L"Power");
        case SensorType::Clock: return MonitorGlobal::Instance()->GetString(L"Clock");
        case SensorType::Temperature: return MonitorGlobal::Instance()->GetString(L"Temperature");
        case SensorType::Load: return MonitorGlobal::Instance()->GetString(L"Load");
        case SensorType::Frequency: return MonitorGlobal::Instance()->GetString(L"Frequency");
        case SensorType::Fan: return MonitorGlobal::Instance()->GetString(L"Fan");
        case SensorType::Flow: return MonitorGlobal::Instance()->GetString(L"Flow");
        case SensorType::Control: return MonitorGlobal::Instance()->GetString(L"Control");
        case SensorType::Level: return MonitorGlobal::Instance()->GetString(L"Level");
        case SensorType::Factor: return MonitorGlobal::Instance()->GetString(L"Factor");
        case SensorType::Data: return MonitorGlobal::Instance()->GetString(L"Data");
        case SensorType::SmallData: return MonitorGlobal::Instance()->GetString(L"Data");
        case SensorType::Throughput: return MonitorGlobal::Instance()->GetString(L"Throughput");
        case SensorType::TimeSpan: return MonitorGlobal::Instance()->GetString(L"TimeSpan");
        case SensorType::Energy: return MonitorGlobal::Instance()->GetString(L"Energy");
        case SensorType::Noise: return MonitorGlobal::Instance()->GetString(L"Noise");
        case SensorType::Conductivity: return MonitorGlobal::Instance()->GetString(L"Conductivity");
        case SensorType::Humidity: return MonitorGlobal::Instance()->GetString(L"Humidity");
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
        case SensorType::Temperature: return L"°C";
        case SensorType::Load: return L"%";
        case SensorType::Frequency: return L"";
        case SensorType::Fan: return L"RPM";
        case SensorType::Flow: return L"";
        case SensorType::Control: return L"%";
        case SensorType::Level: return L"%";
        case SensorType::Factor: return L"";
        case SensorType::Data: return L"GB";
        case SensorType::SmallData: return L"MB";
        case SensorType::Throughput: return L"KB/s";
        case SensorType::TimeSpan: return L"";
        case SensorType::Energy: return L"mWh";
        case SensorType::Noise: return L"";
        case SensorType::Conductivity: return L"";
        case SensorType::Humidity: return L"";
        }
        return L"";
    }

    //查找一个硬件下的传感器
    //func为一个lambda表达式，原型为 bool(ISensor^)
    template<class Func>
    static ISensor^ FindSensorInHardware(IHardware^ hardware, Func func)
    {
        //遍历传感器
        for each (ISensor ^ sensor in hardware->Sensors)
        {
            if (func(sensor))
                return sensor;
        }
        //遍历子硬件
        for each (IHardware ^ sub_hardware in hardware->SubHardware)
        {
            ISensor^ sensor = FindSensorInHardware(sub_hardware, func);
            if (sensor != nullptr)
                return sensor;
        }
        return nullptr;

    }

    ISensor^ HardwareMonitorHelper::FindSensorByIdentifyer(System::String^ identifyer)
    {
        auto computer = MonitorGlobal::Instance()->computer;
        gcroot<String^> _identifyer = identifyer;       //使用gcroot包装托管类指针，以便被lambda捕获
        for each (IHardware^ hardware in computer->Hardware)
        {
            ISensor^ sensor = FindSensorInHardware(hardware, [&](ISensor^ _sensor) {
                return GetSensorIdentifyer(_sensor) == _identifyer;
            });
            if (sensor != nullptr)
                return sensor;
        }
        return nullptr;
    }

    System::String^ HardwareMonitorHelper::GetSensorValueText(ISensor^ sensor, int decimal_place)
    {
        String^ sensor_str;
        if (sensor->Value.HasValue)
        {
            float value = sensor->Value.Value;
            if (sensor->SensorType == SensorType::Throughput)   //网速除以1000转换为KB
                value /= 1000.0;
            if (sensor->SensorType == SensorType::Power && sensor->Name == L"Discharge Rate")   //放电功率显示为负数
                value = -value;
            if (sensor->SensorType == SensorType::Current && sensor->Name == L"Discharge Current")   //放电电流显示为负数
                value = -value;
            String^ formatString = String::Format("F{0}", decimal_place);
            sensor_str += value.ToString(formatString);
        }
        else
        {
            sensor_str += "--";
        }
        sensor_str += " ";
        sensor_str += gcnew String(GetSensorTypeUnit(sensor->SensorType));
        return sensor_str;
    }

    String^ HardwareMonitorHelper::GetSensorNameValueText(ISensor^ sensor)
    {
        String^ sensor_str;
        sensor_str = sensor->Name;
        sensor_str += L"    ";
        sensor_str += GetSensorValueText(sensor);
        return sensor_str;
    }

    String^ HardwareMonitorHelper::GetSensorDisplayName(ISensor^ sensor)
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

    System::String^ HardwareMonitorHelper::GetSensorIdentifyer(ISensor^ sensor)
    {
        //不使用sensor->Identifier，因为sensor->Identifier代表的传感器并不总是固定的
        //return sensor->Identifier->ToString();

        String^ path;
        IHardware^ hardware = sensor->Hardware;
        while (hardware != nullptr)
        {
            //Hardware部分仍然使用它自己的Identifier
            path = hardware->Identifier + "/" + path;
            hardware = hardware->Parent;
        }
        //Sensor部分使用“类型+名称”
        path += sensor->SensorType.ToString();
        path += "/";
        path += sensor->Name;
        return path;
    }
}
