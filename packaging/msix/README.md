# Throne MSIX packaging

Built by `script/pack_msix.ps1` during `script/pack_release.sh windows <tag>`.

Follows Microsoft’s manual desktop packaging flow:

- https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-manual-conversion
- https://learn.microsoft.com/en-us/windows/msix/package/create-app-package-with-makeappx-tool

## Package identity

| Field | Default |
|---|---|
| Name | `AL4AL.Throne` |
| Publisher | `WINDOWS_CERT_SUBJECT` or `CN=AL4AL` |
| Min OS | Windows 10 1809 (`10.0.17763.0`) — matches the modern Inno payload |
| Trust | `packagedClassicApp` + `runFullTrust` + `allowElevation` |

The Publisher DN **must** match the Authenticode certificate used to sign the `.msix`.

## Local dry-run

```powershell
powershell -ExecutionPolicy Bypass -File script/create_windows_dev_cert.ps1
powershell -ExecutionPolicy Bypass -File script/pack_msix.ps1 -SourceDir deployment\windows-amd64 -Tag 1.0.5 -Architecture x64
Add-AppxPackage -Path deployment\Throne-1.0.5-windows-amd64.msix
```

## App Installer

Copy `Throne.appinstaller.template.xml`, fill placeholders, host over HTTPS next to the `.msix`.
