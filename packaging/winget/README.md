# winget manifests (AL4AL.Throne)

Official Windows Package Manager docs:
https://learn.microsoft.com/en-us/windows/package-manager/package/manifest

## Generate from a built installer

```powershell
powershell -ExecutionPolicy Bypass -File script/generate_winget_manifest.ps1 -Tag al4al-0.1.0 `
  -InstallerPath deployment/Throne-al4al-0.1.0-windows-universal-installer.exe
```

Output lands in `packaging/winget/generated/AL4AL.Throne/`.

## Publish to winget-pkgs

1. Sign the universal Inno installer (and ideally the payload EXEs) with an Authenticode certificate that chains to a Microsoft Trusted Root.
2. Publish the GitHub Release so `InstallerUrl` is publicly reachable.
3. Open a PR against [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs) with the generated YAML trio.
4. Validate locally:

```powershell
winget validate packaging/winget/generated/AL4AL.Throne
winget install --manifest packaging/winget/generated/AL4AL.Throne
```

`InstallerType: inno` lets winget apply the standard `/SILENT` and `/VERYSILENT` switches automatically.
