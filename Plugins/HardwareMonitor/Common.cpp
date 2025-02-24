#include "stdafx.h"
#include "Common.h"
#include "HardwareMonitor.h"

namespace HardwareMonitor
{
    void Common::SetButtonIcon(Button^ button, String^ resName)
    {
        Icon^ icon = MonitorGlobal::Instance()->GetIcon(resName);
        if (icon != nullptr && button != nullptr)
        {
            button->Image = icon->ToBitmap(); // 将 Icon 转换为 Bitmap
            button->ImageAlign = ContentAlignment::MiddleCenter; // 设置图标对齐方式
            button->TextImageRelation = TextImageRelation::ImageBeforeText; // 设置图标和文本的关系
        }
    }

    void Common::SwapListBoxItems(ListBox^ listBox, int index1, int index2)
    {
        // 检查索引是否有效
        if (index1 < 0 || index1 >= listBox->Items->Count ||
            index2 < 0 || index2 >= listBox->Items->Count)
        {
            return;
        }

        // 获取两个项的值
        Object^ item1 = listBox->Items[index1];
        Object^ item2 = listBox->Items[index2];

        // 交换两个项的值
        listBox->Items[index1] = item2;
        listBox->Items[index2] = item1;
    }
}