/*
Copyright 2015 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#pragma once

#include <string>
#include "afxwin.h"

// CSettings dialog

class CSettings : public CDialog
{
	DECLARE_DYNAMIC(CSettings)

public:
	CSettings(CWnd* pParent, const std::wstring& exeDir, const std::wstring& wpt10Dir) noexcept;   // standard constructor
	~CSettings();

// Dialog Data
	enum { IDD = IDD_SETTINGS };

	// These settings are written and read by the creator of this object.
	std::wstring heapTracingExes_;
	std::wstring WSMonitoredProcesses_;
	bool bExpensiveWSMonitoring_ = false;
	std::wstring extraKernelFlags_;
	std::wstring extraKernelStacks_;
	std::wstring extraUserProviders_;
	std::wstring perfCounters_;
	bool bUseOtherKernelLogger_ = false;
	bool bBackgroundTracing_ = true;
	bool bChromeDeveloper_ = false;
	bool bIdentifyChromeProcessesCPU_ = false;
	bool bAutoViewTraces_ = false;
	bool bRecordPreTrace_ = false;
	bool bHeapStacks_ = false;
	bool bVirtualAllocStacks_ = false;
	bool bVersionChecks_ = false;
	bool bRecordTraceCommand_ = false;
	uint64_t chromeKeywords_ = 0;

protected:
	CEdit btHeapTracingExe_;
	CEdit btWSMonitoredProcesses_;
	CButton btExpensiveWSMonitoring_;
	CEdit btExtraKernelFlags_;
	CEdit btExtraStackwalks_;
	CEdit btExtraUserProviders_;
	CEdit btPerfCounters_;

	CButton btCopyStartupProfile_;

	CButton btUseOtherKernelLogger_;
	CButton btRecordTraceCommand_;
	CButton btChromeDeveloper_;
	CButton btIdentifyChromeProcessesCPU_;
	CButton btBackgroundMonitoring_;
	CButton btAutoViewTraces_;
	CButton btRecordPreTrace_;
	CButton btHeapStacks_;
	CButton btVirtualAllocStacks_;
	CButton btVersionChecks_;
	CCheckListBox btChromeCategories_;

	CToolTipCtrl toolTip_;

	virtual void DoDataExchange(CDataExchange* pDX) override;    // DDX/DDV support
	virtual BOOL OnInitDialog() override;

	const std::wstring exeDir_;
	// Same meaning as in CUIforETWDlg
	const std::wstring wpt10Dir_;

	DECLARE_MESSAGE_MAP()
	afx_msg void OnOK() override;
public:
	afx_msg void OnBnClickedCopystartupprofile();
	afx_msg BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnBnClickedChromedeveloper();
	afx_msg void OnBnClickedAutoviewtraces() noexcept;
	afx_msg void OnBnClickedHeapstacks() noexcept;
	afx_msg void OnBnClickedVirtualallocstacks() noexcept;
	afx_msg void OnBnClickedExpensivews() noexcept;
	afx_msg void OnBnClickedCheckfornewversions() noexcept;
	afx_msg void OnBnClickedSelectPerfCounters();
	afx_msg void OnBnClickedUseOtherKernelLogger() noexcept;
	afx_msg void OnBnClickedRecordPreTrace() noexcept;
	afx_msg void OnBnClickedIdentifyChromeCpu() noexcept;
	afx_msg void OnBnClickedBackgroundMonitoring() noexcept;

	CSettings& operator=(const CSettings&) = delete;
	CSettings& operator=(const CSettings&&) = delete;
	CSettings(const CSettings&) = delete;
	CSettings(const CSettings&&) = delete;
  afx_msg void OnBnClickedRecordTraceCommand();
};
