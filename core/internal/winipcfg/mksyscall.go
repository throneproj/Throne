//go:build windows

/* SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2019-2022 WireGuard LLC. All Rights Reserved.
 */

package winipcfg

//go:generate go run golang.org/x/sys/windows/mkwinsyscall -output zwinipcfg_windows.go winipcfg.go
