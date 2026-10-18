#pragma once

// interactive false: no MessageBoxW, SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX and WER without UI; dumps are still written.
void Windows_SetCrashHandler(bool interactive = true);

// The process window station is visible (WSF_VISIBLE): false for a service in session 0.
bool Windows_WindowStationVisible();

// Until this runs, a crash produces no files. Call once Logging::Init() has.
void Windows_SetCrashDumpPath();

// WER LocalDumps catches what the exception filter cannot (__fastfail, heap corruption); needs admin, so it no-ops otherwise.
// onlyIfAbsent (headless): never overwrite a key the GUI or an admin wrote; the GUI and the service share Throne.exe's key.
void Windows_ConfigureWER(bool onlyIfAbsent = false);
