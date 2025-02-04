#include "stdafx.h"
#include "SettingsForm.h"
#include "HardwareMonitor.h"
#include "HardwareMonitorHelper.h"

namespace HardwareMonitor
{
    SettingsForm::SettingsForm(void)
    {
        this->StartPosition = FormStartPosition::CenterParent;
        this->Icon = MonitorGlobal::Instance()->GetAppIcon();
        InitializeComponent();
        this->MinimumSize = this->Size;
        UpdateItemList();

        EnableControls();

        //初始化要监控的硬件选项
        Computer^ computer = MonitorGlobal::Instance()->computer;
        cpuCheck->Checked = computer->IsCpuEnabled;
        gpuCheck->Checked = computer->IsGpuEnabled;
        motherBoardCheck->Checked = computer->IsMotherboardEnabled;
        storageCheck->Checked = computer->IsStorageEnabled;
        batteryCheck->Checked = computer->IsBatteryEnabled;
        networkCheck->Checked = computer->IsNetworkEnabled;
        memoryCheck->Checked = computer->IsMemoryEnabled;
        controllerCheck->Checked = computer->IsControllerEnabled;
        psuCheck->Checked = computer->IsPsuEnabled;

        showTooltipCheck->Checked = CHardwareMonitor::GetInstance()->m_settings.show_mouse_tooltip;

        decimalPlaceCombo->DropDownStyle = ComboBoxStyle::DropDownList;
        decimalPlaceCombo->Items->Add("0");
        decimalPlaceCombo->Items->Add("1");
        decimalPlaceCombo->Items->Add("2");
        decimalPlaceCombo->Items->Add("3");

        // 为ListBox的SelectedIndexChanged事件添加处理程序
        monitorItemListBox->SelectedIndexChanged += gcnew System::EventHandler(this, &SettingsForm::listBox_SelectedIndexChanged);
        decimalPlaceCombo->SelectedIndexChanged += gcnew EventHandler(this, &SettingsForm::OnDecimalPlaceComboBoxSelectedIndexChanged);

        // 为FormClosing事件添加处理程序
        this->FormClosing += gcnew FormClosingEventHandler(this, &SettingsForm::OnFormClosing);
    }

    void SettingsForm::UpdateItemList()
    {
        //清除列表
        monitorItemListBox->Items->Clear();
        identifyerList->Clear();

        //填充数据
        for (const auto& item : CHardwareMonitor::GetInstance()->m_settings.items_info)
        {
            String^ item_name = gcnew String(CHardwareMonitor::GetInstance()->GetItemName(item.identifyer).c_str());
            if (item_name->Length > 0)
            {
                monitorItemListBox->Items->Add(item_name);
                identifyerList->Add(gcnew String(item.identifyer.c_str()));
            }
        }
    }

    bool SettingsForm::IsSelectionValid()
    {
        return (monitorItemListBox->SelectedIndex >= 0 && monitorItemListBox->SelectedIndex < monitorItemListBox->Items->Count);
    }

    ItemInfo& SettingsForm::GetSelectedItemInfo()
    {
        int index = monitorItemListBox->SelectedIndex;
        if (index >= 0 && index < identifyerList->Count)
        {
            std::wstring identifyer = MonitorGlobal::ClrStringToStdWstring(identifyerList[index]);
            return CHardwareMonitor::GetInstance()->m_settings.FindItemInfo(identifyer);
        }
        static ItemInfo emptyInfo;
        return emptyInfo;
    }

    void SettingsForm::EnableControls()
    {
        bool selection_enabled = IsSelectionValid();
        removeSelectBtn->Enabled = selection_enabled;
        decimalPlaceCombo->Enabled = selection_enabled;
        specifyValueWidthCheck->Enabled = selection_enabled;
        valueWidthEdit->Enabled = selection_enabled && specifyValueWidthCheck->Checked;
    }

    void SettingsForm::listBox_SelectedIndexChanged(System::Object^ sender, System::EventArgs^ e)
    {
        // 根据是否有选中项启用或禁用按钮
        EnableControls();

        //设置选中项对应的设置控件
        if (IsSelectionValid())
        {
            ItemInfo& item_info = GetSelectedItemInfo();
            if (item_info.decimal_places >= 0 && item_info.decimal_places < decimalPlaceCombo->Items->Count)
            {
                decimalPlaceCombo->SelectedIndex = item_info.decimal_places;
            }
            else
            {
                decimalPlaceCombo->SelectedIndex = -1;
            }

            specifyValueWidthCheck->Checked = item_info.specify_value_width;
            valueWidthEdit->Value = item_info.value_width;
        }
    }

    void SettingsForm::OnDecimalPlaceComboBoxSelectedIndexChanged(Object^ sender, EventArgs^ e)
    {
        if (IsSelectionValid())
        {
            ItemInfo& item_info = GetSelectedItemInfo();
            item_info.decimal_places = decimalPlaceCombo->SelectedIndex;
        }
    }

    void SettingsForm::OnFormClosing(Object^ sender, FormClosingEventArgs^ e)
    {
        //更新要监控的硬件设置
        Computer^ computer = MonitorGlobal::Instance()->computer;
        try
        {
            computer->IsCpuEnabled = cpuCheck->Checked;
            computer->IsGpuEnabled = gpuCheck->Checked;
            computer->IsMotherboardEnabled = motherBoardCheck->Checked;
            computer->IsStorageEnabled = storageCheck->Checked;
            computer->IsBatteryEnabled = batteryCheck->Checked;
            computer->IsNetworkEnabled = networkCheck->Checked;
            computer->IsMemoryEnabled = memoryCheck->Checked;
            computer->IsControllerEnabled = controllerCheck->Checked;
            computer->IsPsuEnabled = psuCheck->Checked;
        }
        catch(System::Exception^ e)
        {}

        CHardwareMonitor::GetInstance()->m_settings.show_mouse_tooltip = showTooltipCheck->Checked;

        //保存设置
        CHardwareMonitor::GetInstance()->SaveConfig();
    }

    System::Void SettingsForm::removeSelectBtn_Click(System::Object^ sender, System::EventArgs^ e)
    {
        int selectedIndex = monitorItemListBox->SelectedIndex;
        // 删除ListBox中的选中项
        if (selectedIndex >= 0)
        {
            // 弹出MessageBox询问用户是否要删除
            System::Windows::Forms::DialogResult result = System::Windows::Forms::MessageBox::Show(
                MonitorGlobal::Instance()->GetString(L"RemoveMonitorItemInquery"),
                MonitorGlobal::Instance()->GetString(L"PluginName"),
                System::Windows::Forms::MessageBoxButtons::OKCancel, 
                System::Windows::Forms::MessageBoxIcon::Question);
            // 如果用户点击“确定”，则删除选中项
            if (result == System::Windows::Forms::DialogResult::OK)
            {
                monitorItemListBox->Items->RemoveAt(selectedIndex);
                CHardwareMonitor::GetInstance()->RemoveDisplayItem(selectedIndex);
            }
        }
    }

    System::Void SettingsForm::addItemBtn_Click(System::Object^ sender, System::EventArgs^ e)
    {
        MonitorGlobal::Instance()->ShowHardwareInfoDialog();
        UpdateItemList();
    }

    System::Void SettingsForm::specifyValueWidthCheck_CheckedChanged(System::Object^ sender, System::EventArgs^ e)
    {
        if (IsSelectionValid())
        {
            ItemInfo& item_info = GetSelectedItemInfo();
            item_info.specify_value_width = specifyValueWidthCheck->Checked;
        }
        EnableControls();
    }

    System::Void SettingsForm::valueWidthEdit_ValueChanged(System::Object^ sender, System::EventArgs^ e)
    {
        if (IsSelectionValid())
        {
            ItemInfo& item_info = GetSelectedItemInfo();
            item_info.value_width = static_cast<int>(valueWidthEdit->Value);
        }
    }

}
