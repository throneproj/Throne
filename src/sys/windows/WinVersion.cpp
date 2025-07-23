// The MIT License (MIT)
// Windows Version Detection 1.2.0
// Copyright (C) 2022 - 2023 by Shao Voon Wong (shaovoon@yahoo.com)
//
// http://opensource.org/licenses/MIT

// History
// 1.0.0: First Releease
// 1.1.0: Added IsBuildNumGreaterOrEqual()
// 1.2.0: Removed the Windows 11 build number check to set major version to 11.

#include "include/sys/windows/WinVersion.h"
#include <Windows.h>

extern "C" NTSTATUS __stdcall RtlGetVersion(OSVERSIONINFOEXW * lpVersionInformation);

bool WinVersion::GetVersion(VersionInfo& info)
{
	OSVERSIONINFOEXW osv;
	osv.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
	if (RtlGetVersion(&osv) == 0)
	{
		info.Major = osv.dwMajorVersion;
		info.Minor = osv.dwMinorVersion;
		info.BuildNum = osv.dwBuildNumber;

		return true;
	}
	return false;
}

bool WinVersion::IsBuildNumGreaterOrEqual(unsigned int buildNumber)
{
	VersionInfo info;
	if (GetVersion(info))
	{
		return (info.BuildNum >= buildNumber);
	}
	return false;
}
