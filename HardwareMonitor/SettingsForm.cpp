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
        for (const auto& identifyer : CHardwareMonitor::GetInstance()->m_settings.item_identifyers)
        {
            String^ item_name = gcnew String(CHardwareMonitor::GetInstance()->GetItemName(identifyer).c_str());
            if (item_name->Length > 0)
                monitorItemListBox->Items->Add(item_name);
        }
    }

    System::Void SettingsForm::removeSelectBtn_Click(System::Object^ sender, System::EventArgs^ e)
    {
        return System::Void();
    }

}
