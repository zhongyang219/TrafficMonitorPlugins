"""Portable source-level lifecycle regressions for HardwareMonitor.

These checks pin the initialization/recovery contract, NOT runtime behaviour of
LibreHardwareMonitor.dll or NVIDIA/CPU hardware.  Run in the repository root:

    python -m unittest discover -s tests/hardware_monitor -p 'test_*.py' -v

Hardware integration and an MSVC /clr x64 build are separate requirements.
"""

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
PLUGIN = ROOT / "Plugins" / "HardwareMonitor"


def source(name: str) -> str:
    return (PLUGIN / name).read_text(encoding="utf-8-sig")


def body(content: str, signature: str) -> str:
    """Extract one C++/CLI function body, including nested blocks."""
    start = content.index(signature)
    left = content.index("{", start)
    nesting = 1
    for i in range(left + 1, len(content)):
        if content[i] == "{":
            nesting += 1
        elif content[i] == "}":
            nesting -= 1
            if nesting == 0:
                return content[left + 1 : i]
    raise AssertionError(f"Unclosed function: {signature}")


class HardwareMonitorSourceContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.impl = source("HardwareMonitor.cpp")
        cls.helper = source("HardwareMonitorHelper.cpp")
        cls.items = source("HardwareMonitorItem.cpp")
        cls.settings = source("SettingsForm.cpp")
        cls.info = source("HardwareInfoForm.cpp")
        cls.header = source("HardwareMonitor.h")

    def test_disabled_cpu_and_gpu_applied_before_cold_open(self):
        init = body(self.impl, "void MonitorGlobal::Init()")
        load = body(self.impl, "void CHardwareMonitor::LoadConfig(")
        rebuild = body(self.impl, "bool MonitorGlobal::ReInit()")
        apply_flags = body(self.impl, "void CHardwareMonitor::ApplyHardwareEnableFlags(")

        self.assertNotIn("gcnew Computer()", init)
        self.assertNotIn("->Open()", init)
        self.assertLess(load.index("UpdateHardwareEnableFlags("), load.index("global->ReInit()"))
        self.assertLess(rebuild.index("ApplyHardwareEnableFlags(computer)"), rebuild.index("computer->Open()"))
        for name, field, default in (
            ("Cpu", "cpu", "true"), ("Gpu", "gpu", "true"),
            ("Motherboard", "motherboard", "true"), ("Storage", "storage", "true"),
            ("Battery", "battery", "false"), ("Network", "network", "false"),
            ("Memory", "memory", "false"), ("Controller", "controller", "false"),
            ("Psu", "psu", "false"),
        ):
            self.assertIn(f'ini.GetBool(L"hardware", L"Is{name}Enabled", {default})', load)
            self.assertIn(f"computer->Is{name}Enabled = m_hardware_flags.{field};", apply_flags)

    def test_temporary_driver_failure_preserves_flags_and_retries(self):
        rebuild = body(self.impl, "bool MonitorGlobal::ReInit()")
        poll = body(self.impl, "void CHardwareMonitor::DataRequired()")
        limit = body(self.impl, "bool CHardwareMonitor::CanAttemptReinit() const")
        save = body(self.impl, "void CHardwareMonitor::SaveConfig()")

        self.assertIn("catch (System::Exception^)", rebuild)
        self.assertIn("try { computer->Close(); }", rebuild)  # partial-open cleanup
        self.assertGreater(rebuild.index("try { computer->Close(); }"), rebuild.index("computer->Open()"))
        self.assertIn("computer = nullptr;", rebuild)
        self.assertIn("return computer != nullptr;", rebuild)
        self.assertIn("if (AcquireMonitorData())", poll)
        self.assertLess(poll.index("CanAttemptReinit()"), poll.index("->ReInit()"))
        self.assertIn("AcquireMonitorData();", poll)
        self.assertIn("now - m_last_reinit_tick >= REINIT_INTERVAL_MS", limit)
        self.assertIn("REINIT_INTERVAL_MS = 5000", self.header)
        self.assertIn("m_hardware_flags.cpu", save)
        self.assertIn("m_hardware_flags.gpu", save)
        self.assertNotIn("computer->IsCpuEnabled", save)  # can save while offline

    def test_sensor_resolution_after_recovery(self):
        rebuild = body(self.impl, "bool MonitorGlobal::ReInit()")
        load = body(self.impl, "void CHardwareMonitor::LoadConfig(")
        find = body(self.helper, "ISensor^ HardwareMonitorHelper::FindSensorByIdentifyer(")
        update = body(self.items, "void CHardwareMonitorItem::UpdateValue()")
        meta = body(self.items, "void CHardwareMonitorItem::UpdateMetadata()")

        self.assertGreater(rebuild.index("sensorMap->Clear()"), rebuild.index("computer->Open()"))
        self.assertIn("if (computer == nullptr)", find)
        self.assertIn("if (sensor != nullptr)", find)
        self.assertIn("sensorMap->Add(identifyer, sensor)", find)
        self.assertIn("m_items.emplace_back(item_info.identifyer", load)  # stable ID when offline
        self.assertIn("UpdateMetadata();", update)
        self.assertIn("if (sensor != nullptr)", meta)

    def test_ui_does_not_hold_pointers_through_rebuild(self):
        limit = body(self.impl, "bool CHardwareMonitor::CanAttemptReinit() const")
        settings_init = body(self.settings, "SettingsForm::SettingsForm(void)")
        close = body(self.settings, "void SettingsForm::OnFormClosing(")
        info_init = body(self.info, "HardwareInfoForm::HardwareInfoForm(void)")

        self.assertIn("monitor_form != nullptr", limit)
        self.assertIn("setttings_form != nullptr", limit)
        self.assertIn("GetHardwareEnableFlags()", settings_init)
        self.assertIn("UpdateHardwareEnableFlags(", close)
        self.assertIn("ApplyHardwareEnableFlags(", close)
        self.assertIn("computer != nullptr &&", info_init)


if __name__ == "__main__":
    unittest.main()
