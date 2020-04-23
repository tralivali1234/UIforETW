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

#include "stdafx.h"
#include "ChildProcess.h"
#include "Utility.h"

#include <vector>

static const _TCHAR* const kPipeName = _T("\\\\.\\PIPE\\UIforETWPipe");

ChildProcess::ChildProcess(string_type exePath, bool printFailedExitCodes) noexcept
	: exePath_(std::move(exePath)), printFailedExitCodes_(printFailedExitCodes)
{
	// Create the pipe here so that it is guaranteed to be created before
	// we try starting the process.
	hPipe_ = CreateNamedPipe(kPipeName,
		PIPE_ACCESS_DUPLEX | PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
		PIPE_WAIT,
		1,
		1024 * 16,
		1024 * 16,
		NMPWAIT_USE_DEFAULT_WAIT,
		NULL);
	hChildThread_ = CreateThread(0, 0, ListenerThreadStatic, this, 0, 0);

	hOutputAvailable_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

ChildProcess::~ChildProcess()
{
	if (hProcess_)
	{
		// Always get the exit code since this also waits for the process to exit.
		const DWORD exitCode = GetExitCode();
		if (printFailedExitCodes_ && exitCode)
			outputPrintf(_T("Process exit code was %08x (%lu)\n"), exitCode, exitCode);
		CloseHandle(hProcess_);
	}
	if (hOutputAvailable_)
	{
		CloseHandle(hOutputAvailable_);
	}
}

bool ChildProcess::IsStillRunning() noexcept
{
	HANDLE handles[] =
	{
		hProcess_,
		hOutputAvailable_,
	};
	const DWORD waitIndex = WaitForMultipleObjects(ARRAYSIZE(handles), &handles[0], FALSE, INFINITE);
	// Return true if hProcess_ was not signaled.
	return waitIndex != 0;
}

ChildProcess::string_type ChildProcess::RemoveOutputText()
{
	CSingleLock locker(&outputLock_);
	string_type result = processOutput_;
	processOutput_ = _T("");
	return result;
}

DWORD WINAPI ChildProcess::ListenerThreadStatic(LPVOID pVoidThis)
{
	SetCurrentThreadName("Child-process listener");
	ChildProcess* pThis = static_cast<ChildProcess*>(pVoidThis);
	return pThis->ListenerThread();
}

DWORD ChildProcess::ListenerThread()
{
	// wait for someone to connect to the pipe
	if (ConnectNamedPipe(hPipe_, NULL) || GetLastError() == ERROR_PIPE_CONNECTED)
	{
		// Acquire the lock while writing to processOutput_
		char buffer[1024];
		DWORD dwRead;
		while (ReadFile(hPipe_, buffer, sizeof(buffer) - 1, &dwRead, NULL) != FALSE)
		{
			if (dwRead > 0)
			{
				CSingleLock locker(&outputLock_);
				buffer[dwRead] = 0;
#ifdef OUTPUT_DEBUG_STRINGS
				OutputDebugStringA(buffer);
#endif
#ifdef _UNICODE
				processOutput_ += AnsiToUnicode(buffer);
#else
				processOutput_ += buffer;
#endif
			}
			SetEvent(hOutputAvailable_);
		}
	}
	else
	{
#ifdef OUTPUT_DEBUG_STRINGS
			OutputDebugString(_T("Connect failed.\n"));
#endif
	}

	DisconnectNamedPipe(hPipe_);

	return 0;
}

_Pre_satisfies_(!(this->hProcess_))
bool ChildProcess::Run(bool showCommand, string_type args)
{
	ASSERT(!hProcess_);

	if (showCommand)
		outputPrintf(_T("%s\n"), args.c_str());

	SECURITY_ATTRIBUTES security = { sizeof(security), 0, TRUE };

	hStdOutput_ = CreateFile(kPipeName, GENERIC_WRITE, 0, &security,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, INVALID_HANDLE_VALUE);
	if (hStdOutput_ == INVALID_HANDLE_VALUE)
		return false;
	if (!DuplicateHandle(GetCurrentProcess(), hStdOutput_, GetCurrentProcess(),
		&hStdError_, 0, TRUE, DUPLICATE_SAME_ACCESS))
		return false;
	// This is slightly dodgy since the pipe handle is write-only, but it keeps
	// Python happy by giving it a valid hStdInput handle.
	if (!DuplicateHandle(GetCurrentProcess(), hStdOutput_, GetCurrentProcess(),
		&hStdInput_, 0, TRUE, DUPLICATE_SAME_ACCESS))
		return false;

	STARTUPINFO startupInfo = {};
	startupInfo.hStdOutput = hStdOutput_;
	startupInfo.hStdError = hStdError_;
	startupInfo.hStdInput = hStdInput_;
	startupInfo.dwFlags = STARTF_USESTDHANDLES;

	PROCESS_INFORMATION processInfo = {};
	const DWORD flags = CREATE_NO_WINDOW;
	// Wacky CreateProcess rules say args has to be writable!
	std::vector<_TCHAR> argsCopy(args.size() + 1);
	_tcscpy_s(&argsCopy[0], argsCopy.size(), args.c_str());
	const BOOL success = CreateProcess(exePath_.c_str(), &argsCopy[0], NULL, NULL,
		TRUE, flags, NULL, NULL, &startupInfo, &processInfo);
	if (success)
	{
		CloseHandle(processInfo.hThread);
		hProcess_ = processInfo.hProcess;
		return true;
	}
	else
	{
		outputPrintf(_T("Error %lu starting %s, %s\n"), GetLastError(), exePath_.c_str(), args.c_str());
	}

	return false;
}

DWORD ChildProcess::GetExitCode()
{
	if (!hProcess_)
		return 0;
	// Don't allow getting the exit code unless the process has exited.
	WaitForCompletion(true);
	DWORD result;
	(void)GetExitCodeProcess(hProcess_, &result);
	return result;
}

ChildProcess::string_type ChildProcess::GetOutput()
{
	if (!hProcess_)
		return _T("");
	WaitForCompletion(false);
	return RemoveOutputText();
}

void ChildProcess::WaitForCompletion(bool printOutput)
{
	if (hProcess_)
	{
		// This looks like a busy loop, but it isn't. IsStillRunning()
		// waits until the process exits or sends more output, so this
		// is actually an idle loop.
		while (IsStillRunning())
		{
			if (printOutput)
			{
				string_type output = RemoveOutputText();
				outputPrintf(_T("%s"), output.c_str());
			}
		}
		// This isn't technically needed, but removing it would make
		// me nervous.
		WaitForSingleObject(hProcess_, INFINITE);
	}

	// Once the process is finished we have to close the stderr/stdout/stdin
	// handles so that the listener thread will exit. We also have to
	// close these if the process never started.
	if (hStdError_ != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hStdError_);
		hStdError_ = INVALID_HANDLE_VALUE;
	}
	if (hStdOutput_ != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hStdOutput_);
		hStdOutput_ = INVALID_HANDLE_VALUE;
	}
	if (hStdInput_ != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hStdInput_);
		hStdInput_ = INVALID_HANDLE_VALUE;
	}

	// Wait for the listener thread to exit.
	if (hChildThread_)
	{
		WaitForSingleObject(hChildThread_, INFINITE);
		CloseHandle(hChildThread_);
		hChildThread_ = 0;
	}

	// Clean up.
	if (hPipe_ != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hPipe_);
		hPipe_ = INVALID_HANDLE_VALUE;
	}

	if (printOutput)
	{
		// Now that the child thread has exited we can finally read
		// the last of the child-process output.
		string_type output = RemoveOutputText();
		if (!output.empty())
			outputPrintf(_T("%s"), output.c_str());
	}
}
