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
    private: System::Windows::Forms::GroupBox^ groupBox1;
    private: System::Windows::Forms::CheckBox^ gpuCheck;
    private: System::Windows::Forms::CheckBox^ cpuCheck;
    private: System::Windows::Forms::CheckBox^ networkCheck;
    private: System::Windows::Forms::CheckBox^ batteryCheck;
    private: System::Windows::Forms::CheckBox^ storageCheck;
    private: System::Windows::Forms::CheckBox^ motherBoardCheck;
    private: System::Windows::Forms::Label^ label2;
    private: System::Windows::Forms::CheckBox^ showTooltipCheck;
    private: System::Windows::Forms::Label^ label3;
    private: System::Windows::Forms::ComboBox^ decimalPlaceCombo;

    private: System::Windows::Forms::Button^ addItemBtn;


    protected:
        void listBox_SelectedIndexChanged(System::Object^ sender, System::EventArgs^ e);
        void OnDecimalPlaceComboBoxSelectedIndexChanged(Object^ sender, EventArgs^ e);
        void OnFormClosing(Object^ sender, FormClosingEventArgs^ e);

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
            this->groupBox1 = (gcnew System::Windows::Forms::GroupBox());
            this->networkCheck = (gcnew System::Windows::Forms::CheckBox());
            this->batteryCheck = (gcnew System::Windows::Forms::CheckBox());
            this->storageCheck = (gcnew System::Windows::Forms::CheckBox());
            this->motherBoardCheck = (gcnew System::Windows::Forms::CheckBox());
            this->gpuCheck = (gcnew System::Windows::Forms::CheckBox());
            this->cpuCheck = (gcnew System::Windows::Forms::CheckBox());
            this->label2 = (gcnew System::Windows::Forms::Label());
            this->showTooltipCheck = (gcnew System::Windows::Forms::CheckBox());
            this->label3 = (gcnew System::Windows::Forms::Label());
            this->decimalPlaceCombo = (gcnew System::Windows::Forms::ComboBox());
            this->groupBox1->SuspendLayout();
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
            // groupBox1
            // 
            resources->ApplyResources(this->groupBox1, L"groupBox1");
            this->groupBox1->Controls->Add(this->networkCheck);
            this->groupBox1->Controls->Add(this->batteryCheck);
            this->groupBox1->Controls->Add(this->storageCheck);
            this->groupBox1->Controls->Add(this->motherBoardCheck);
            this->groupBox1->Controls->Add(this->gpuCheck);
            this->groupBox1->Controls->Add(this->cpuCheck);
            this->groupBox1->Name = L"groupBox1";
            this->groupBox1->TabStop = false;
            // 
            // networkCheck
            // 
            resources->ApplyResources(this->networkCheck, L"networkCheck");
            this->networkCheck->Name = L"networkCheck";
            this->networkCheck->UseVisualStyleBackColor = true;
            // 
            // batteryCheck
            // 
            resources->ApplyResources(this->batteryCheck, L"batteryCheck");
            this->batteryCheck->Name = L"batteryCheck";
            this->batteryCheck->UseVisualStyleBackColor = true;
            // 
            // storageCheck
            // 
            resources->ApplyResources(this->storageCheck, L"storageCheck");
            this->storageCheck->Name = L"storageCheck";
            this->storageCheck->UseVisualStyleBackColor = true;
            // 
            // motherBoardCheck
            // 
            resources->ApplyResources(this->motherBoardCheck, L"motherBoardCheck");
            this->motherBoardCheck->Name = L"motherBoardCheck";
            this->motherBoardCheck->UseVisualStyleBackColor = true;
            // 
            // gpuCheck
            // 
            resources->ApplyResources(this->gpuCheck, L"gpuCheck");
            this->gpuCheck->Name = L"gpuCheck";
            this->gpuCheck->UseVisualStyleBackColor = true;
            // 
            // cpuCheck
            // 
            resources->ApplyResources(this->cpuCheck, L"cpuCheck");
            this->cpuCheck->Name = L"cpuCheck";
            this->cpuCheck->UseVisualStyleBackColor = true;
            // 
            // label2
            // 
            resources->ApplyResources(this->label2, L"label2");
            this->label2->Name = L"label2";
            // 
            // showTooltipCheck
            // 
            resources->ApplyResources(this->showTooltipCheck, L"showTooltipCheck");
            this->showTooltipCheck->Name = L"showTooltipCheck";
            this->showTooltipCheck->UseVisualStyleBackColor = true;
            // 
            // label3
            // 
            resources->ApplyResources(this->label3, L"label3");
            this->label3->Name = L"label3";
            // 
            // decimalPlaceCombo
            // 
            resources->ApplyResources(this->decimalPlaceCombo, L"decimalPlaceCombo");
            this->decimalPlaceCombo->FormattingEnabled = true;
            this->decimalPlaceCombo->Name = L"decimalPlaceCombo";
            // 
            // SettingsForm
            // 
            resources->ApplyResources(this, L"$this");
            this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
            this->Controls->Add(this->decimalPlaceCombo);
            this->Controls->Add(this->label3);
            this->Controls->Add(this->showTooltipCheck);
            this->Controls->Add(this->label2);
            this->Controls->Add(this->groupBox1);
            this->Controls->Add(this->cancelBtn);
            this->Controls->Add(this->label1);
            this->Controls->Add(this->addItemBtn);
            this->Controls->Add(this->removeSelectBtn);
            this->Controls->Add(this->monitorItemListBox);
            this->Name = L"SettingsForm";
            this->groupBox1->ResumeLayout(false);
            this->groupBox1->PerformLayout();
            this->ResumeLayout(false);
            this->PerformLayout();

        }
#pragma endregion
    private: System::Void removeSelectBtn_Click(System::Object^ sender, System::EventArgs^ e);
    private: System::Void addItemBtn_Click(System::Object^ sender, System::EventArgs^ e);
};
}
