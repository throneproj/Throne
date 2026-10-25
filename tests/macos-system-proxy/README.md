# macOS System Proxy regression tests

Build and run without starting Throne or changing host network settings:

```sh
cmake -S tests/macos-system-proxy -B build/proxy-tests -DCMAKE_PREFIX_PATH=/path/to/Qt/macos
cmake --build build/proxy-tests --parallel 2
ctest --test-dir build/proxy-tests --output-on-failure
```

Only QtCore and a C++20 compiler are needed. On macOS the test executable also compiles and links the real SystemConfiguration adapter, but **never calls it**. Network mutations use an in-memory backend; command failure tests execute temporary shell scripts instead of networksetup. Lock tests kill only a child test process.

## Transaction and recovery

The macOS implementation saves a versioned, atomic write-ahead journal before changing any service. Each record contains a service ID, protocol, original values (including absent keys), and the values Throne intends to apply. An apply failure triggers rollback. Failed recovery records remain on disk for a later retry; a corrupt or unwritable journal prevents new mutations.

The record is per user, across different appdata directories, at `QStandardPaths::GenericConfigLocation/Throne/system-proxy/state.json`. The directory is 0700 and the file is 0600. An owner lock remains held for the lifetime of the transaction. Another live Throne instance cannot replay it. After a crash, Qt's stale-lock handling allows the next GUI launch to replay it before a profile auto-starts. A second Apply first completes the previous transaction so it cannot replace the true original snapshot with Throne's own endpoint.

`networksetup -listallnetworkservices` still selects enabled services. Any command failure aborts discovery. SystemConfiguration maps those names to stable IDs in the current network location and reads the original proxy values. The writes use authorized SystemConfiguration sessions rather than separate networksetup writes, so comparison and mutation share one preferences lock. This also allows absent keys to be removed precisely and keeps keychain credentials intact. macOS remains responsible for authorizing network preferences access; no helper, setuid change, launchd job or security-setting change is installed.

Restoration compares the current HTTP/HTTPS/SOCKS endpoint, enabled flag and authentication flag with the journal. If they differ, that protocol is relinquished to preserve the later edit. Each update merges just that protocol's keys. PAC, WPAD, bypass lists, user names, passwords and other protocols are not replaced. Removed services are not recreated; added services are not included in an older transaction. A committed restoration whose activation failed is safely retried without rewriting changed settings.

## Scope and limitations

- Force Quit cannot run code in the terminated process. This change restores on the **next GUI launch**; it does not promise unattended restoration while Throne remains closed.
- Immediate cleanup would require an independently supervised process with its own authorization and explicit ownership protocol. That is a separate architectural change, not included here.
- An old leftover proxy without this implementation's journal is not guessed at or disabled.
- DNS, TUN, routes, PF/kill switches and issue #2025 are outside this change.
- The tests simulate failures and third-party edits; they do not exercise a real authorization dialog or mutate real network preferences. Live VPN, sleep/wake, Wi-Fi changes and reboot require a separate controlled integration test.
