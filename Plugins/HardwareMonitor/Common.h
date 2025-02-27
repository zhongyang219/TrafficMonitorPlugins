#pragma once
#include <string>

using namespace System;
using namespace System::Drawing;
using namespace System::Windows::Forms;
using namespace System::Collections::Generic;

namespace HardwareMonitor
{
    class Common
    {
    public:
        //将System::String^转换为std::wstring
        static std::wstring StringToStdWstring(String^ str);

        //根据一个字符获取翻译后的字符串，如果没有翻译则返回自身
        static String^ GetTranslatedString(String^ str);

        //为一个按钮设置图标
        static void SetButtonIcon(Button^ button, String^ resName);

        //交换ListBox的两个项
        static void SwapListBoxItems(ListBox^ listBox, int index1, int index2);

        //将一个窗口的大小保存到配置文件
        static void SaveFormSize(Form^ form, const std::wstring& name);

        //从配置文件恢复窗口的大小
        static void LoadFormSize(Form^ form, const std::wstring& name);

        //将一个树控件的所有节点的展开状态保存到treeExpandStatusMap
        static void SaveTreeNodeExpandStatus(TreeView^ tree, SortedSet<String^>^ treeCollapseNodes);

        //从treeExpandStatusMap恢复一个树控件所有节点的展开状态
        static void RestoreTreeNodeExpandStatus(TreeView^ tree, SortedSet<String^>^ treeCollapseNodes);
    };
}

