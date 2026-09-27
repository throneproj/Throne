// Package mobile is the gomobile-bound surface of ThroneCore for Android. It owns the sing-box
// instance (the fork's root box.New over a context this package builds) and the in-process Xray
// instances, mirroring internal/rpc and internal/boxmain without their IPC, signal and desktop-only
// wiring; none of those packages may be imported here.
package mobile

import (
	"os"
	"runtime"

	C "github.com/sagernet/sing-box/constant"
	E "github.com/sagernet/sing/common/exceptions"
	xcore "github.com/xtls/xray-core/core"

	// Pins the gomobile bind runtime in go.mod the same way the sing-box fork does.
	_ "github.com/sagernet/gomobile"
	// Links every Xray protocol into the AAR; ThroneCore/internal/distro/all must stay out because it
	// starts the desktop's netlink monitors at init.
	_ "github.com/xtls/xray-core/main/distro/all"
)

type SetupOptions struct {
	BasePath    string
	WorkingPath string
	TempPath    string
	LogMaxLines int32
	Debug       bool
}

var (
	sBasePath    string
	sWorkingPath string
	sTempPath    string
	sDebug       bool
)

func Setup(options *SetupOptions) error {
	if options == nil {
		return E.New("nil setup options")
	}
	sBasePath = options.BasePath
	sWorkingPath = options.WorkingPath
	sTempPath = options.TempPath
	sDebug = options.Debug
	pump.setLimit(int(options.LogMaxLines))
	for _, dir := range []string{sBasePath, sWorkingPath, sTempPath} {
		if dir == "" {
			continue
		}
		if err := os.MkdirAll(dir, 0o755); err != nil {
			return E.Cause(err, "create ", dir)
		}
	}
	// Like the desktop's XRAY_LOCATION_ASSET: without it Xray looks for geoip.dat/geosite.dat next to
	// app_process in /system/bin. Set from Go because the runtime copied the environment at load time.
	if sBasePath != "" {
		if err := os.Setenv("XRAY_LOCATION_ASSET", sBasePath); err != nil {
			return E.Cause(err, "set Xray asset location")
		}
	}
	return nil
}

func Version() string {
	return C.Version
}

func XrayVersion() string {
	return xcore.Version()
}

func GoVersion() string {
	return runtime.Version() + ", " + runtime.GOOS + "/" + runtime.GOARCH
}
