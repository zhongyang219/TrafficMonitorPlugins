#include "stdafx.h"
#include "SettingsForm.h"
#include "HardwareMonitor.h"
#include "HardwareMonitorHelper.h"

namespace HardwareMonitor
{
    SettingsForm::SettingsForm(void)
    {
        this->StartPosition = FormStartPosition::CenterParent;
        InitializeComponent();

        //Ìî³äÊý¾Ý
        for (const auto& item : CHardwareMonitor::GetInstance()->GetAllDisplayItems())
        {
            String^ item_name = gcnew String(item.GetItemName());
            monitorItemListBox->Items->Add(item_name);
        }
    }

    System::Void SettingsForm::removeSelectBtn_Click(System::Object^ sender, System::EventArgs^ e)
    {
        return System::Void();
    }

}
