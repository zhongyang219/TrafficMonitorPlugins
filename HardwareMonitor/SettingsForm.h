#pragma once

namespace HardwareMonitor {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;

	/// <summary>
	/// SettingsForm 摘要
	/// </summary>
	public ref class SettingsForm : public System::Windows::Forms::Form
	{
	public:
        SettingsForm(void);

        void UpdateItemList();

	protected:
		/// <summary>
		/// 清理所有正在使用的资源。
		/// </summary>
		~SettingsForm()
		{
			if (components)
			{
				delete components;
			}
		}
    private: System::Windows::Forms::ListBox^ monitorItemListBox;
    private: System::Windows::Forms::Button^ removeSelectBtn;
    private: System::Windows::Forms::Label^ label1;
    private: System::Windows::Forms::Button^ cancelBtn;
    private: System::Windows::Forms::Button^ addItemBtn;


    protected:
        void listBox_SelectedIndexChanged(System::Object^ sender, System::EventArgs^ e);

	private:
		/// <summary>
		/// 必需的设计器变量。
		/// </summary>
		System::ComponentModel::Container ^components;

#pragma region Windows Form Designer generated code
		/// <summary>
		/// 设计器支持所需的方法 - 不要修改
		/// 使用代码编辑器修改此方法的内容。
		/// </summary>
		void InitializeComponent(void)
		{
            System::ComponentModel::ComponentResourceManager^ resources = (gcnew System::ComponentModel::ComponentResourceManager(SettingsForm::typeid));
            this->monitorItemListBox = (gcnew System::Windows::Forms::ListBox());
            this->removeSelectBtn = (gcnew System::Windows::Forms::Button());
            this->label1 = (gcnew System::Windows::Forms::Label());
            this->cancelBtn = (gcnew System::Windows::Forms::Button());
            this->addItemBtn = (gcnew System::Windows::Forms::Button());
            this->SuspendLayout();
            // 
            // monitorItemListBox
            // 
            resources->ApplyResources(this->monitorItemListBox, L"monitorItemListBox");
            this->monitorItemListBox->FormattingEnabled = true;
            this->monitorItemListBox->Name = L"monitorItemListBox";
            // 
            // removeSelectBtn
            // 
            resources->ApplyResources(this->removeSelectBtn, L"removeSelectBtn");
            this->removeSelectBtn->Name = L"removeSelectBtn";
            this->removeSelectBtn->UseVisualStyleBackColor = true;
            this->removeSelectBtn->Click += gcnew System::EventHandler(this, &SettingsForm::removeSelectBtn_Click);
            // 
            // label1
            // 
            resources->ApplyResources(this->label1, L"label1");
            this->label1->Name = L"label1";
            // 
            // cancelBtn
            // 
            resources->ApplyResources(this->cancelBtn, L"cancelBtn");
            this->cancelBtn->DialogResult = System::Windows::Forms::DialogResult::Cancel;
            this->cancelBtn->Name = L"cancelBtn";
            this->cancelBtn->UseVisualStyleBackColor = true;
            // 
            // addItemBtn
            // 
            resources->ApplyResources(this->addItemBtn, L"addItemBtn");
            this->addItemBtn->Name = L"addItemBtn";
            this->addItemBtn->UseVisualStyleBackColor = true;
            this->addItemBtn->Click += gcnew System::EventHandler(this, &SettingsForm::addItemBtn_Click);
            // 
            // SettingsForm
            // 
            resources->ApplyResources(this, L"$this");
            this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
            this->Controls->Add(this->cancelBtn);
            this->Controls->Add(this->label1);
            this->Controls->Add(this->addItemBtn);
            this->Controls->Add(this->removeSelectBtn);
            this->Controls->Add(this->monitorItemListBox);
            this->Name = L"SettingsForm";
            this->ResumeLayout(false);
            this->PerformLayout();

        }
#pragma endregion
    private: System::Void removeSelectBtn_Click(System::Object^ sender, System::EventArgs^ e);
    private: System::Void addItemBtn_Click(System::Object^ sender, System::EventArgs^ e);
};
}
