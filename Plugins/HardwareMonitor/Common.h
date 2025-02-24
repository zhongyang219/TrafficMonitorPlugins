#pragma once
using namespace System;
using namespace System::Drawing;
using namespace System::Windows::Forms;

namespace HardwareMonitor
{
    class Common
    {
    public:
        //为一个按钮设置图标
        static void SetButtonIcon(Button^ button, String^ resName);

        //交换ListBox的两个项
        static void SwapListBoxItems(ListBox^ listBox, int index1, int index2);

    };
}

