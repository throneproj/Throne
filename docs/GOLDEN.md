# AL4AL/Throne — what we ship

Desktop successor to archived [AL4AL/nekoray](https://github.com/AL4AL/nekoray). Upstream: [throneproj/Throne](https://github.com/throneproj/Throne).

Also: [RELEASE.md](RELEASE.md) · [UPSTREAM.md](UPSTREAM.md) · [WINDOWS.md](WINDOWS.md)

## First release (0.1)

- Throne’s normal desktop protocols and packaging
- Share-link cert pins: `pcs` **or** `pinSHA256` (same as Nekoray)
- XHTTP via Throne’s Xray path (HTTP version follows Xray)

## Fork-only features (not upstream)

| Feature | Notes |
|---|---|
| **PPTP / L2TP egress (Linux)** | Preferences + main-window checkbox. Routes the **selected profile’s server** through NetworkManager **PPTP** or **L2TP/IPsec** (PSK). Cannot run with Tun Mode. With System Proxy, the VPN server is added to ignore-hosts. Diagnoses missing plugins and can install Ubuntu 24.04 packages (`network-manager-pptp` / `network-manager-l2tp` + deps) via `pkexec` with permission. |
| **Remove duplicates on update** | Checkbox on the Subscription edit dialog (same setting as Advanced). |

## Later (not before 0.1)

- Hiddify-style auto-select / subscription UX polish
- Extra open Mahsa bypass (no closed MahsaNG code)
- Rebrand, mobile

## Sync upstream

```bash
git fetch upstream
git merge upstream/dev
```
