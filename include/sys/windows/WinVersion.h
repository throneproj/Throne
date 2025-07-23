// The MIT License (MIT)
// Windows Version Detection 1.2.0
// Copyright (C) 2022 - 2023 by Shao Voon Wong (shaovoon@yahoo.com)
//
// http://opensource.org/licenses/MIT

// History
// 1.0.0: First Releease
// 1.1.0: Added IsBuildNumGreaterOrEqual()
// 1.2.0: Removed the Windows 11 build number check to set major version to 11.

#pragma once

struct VersionInfo
{
	VersionInfo() : Major(0), Minor(0), BuildNum(0) {}
	unsigned int Major;
	unsigned int Minor;
	unsigned int BuildNum;
};

struct BuildNumber
{
	enum BN : unsigned int
	{
		Windows_Vista = 6002,
		Windows_7 = 7601,
		Windows_8 = 9200,
		Windows_8_1 = 9600,
		Windows_10_1507 = 10240,
		Windows_10_1511 = 10586,
		Windows_10_1607 = 14393,
		Windows_10_1703 = 15063,
		Windows_10_1709 = 16299,
		Windows_10_1803 = 17134,
		Windows_10_1809 = 17763,
		Windows_10_1903 = 18362,
		Windows_10_1909 = 18363,
		Windows_10_2004 = 19041,
		Windows_10_20H2 = 19042,
		Windows_10_21H1 = 19043,
		Windows_10_21H2 = 19044,
		Windows_10_22H2 = 19045,
		Windows_11_21H2 = 22000,
		Windows_11_22H2 = 22621,
	};
};

class WinVersion
{
public:
	static bool GetVersion(VersionInfo& info);
	static bool IsBuildNumGreaterOrEqual(unsigned int buildNumber);
};

