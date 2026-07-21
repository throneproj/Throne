#!/usr/bin/python3

import curses
import json
import os
import shutil
from zipfile import ZipFile
from platform import machine
from tempfile import TemporaryDirectory
from urllib.request import urlopen, urlretrieve
from pathlib import Path
from typing import Union

REPO = "throneproj/Throne"

APPDIR = Path("/opt/Throne")
DESKTOPDIR = Path("/usr/share/applications")

CONFIGDIR = Path.home() / ".config/Throne"
AUTOSTARTDIR = Path.home() / ".config/autostart"


# [Tools and utils]
def get_arch():
    arch = machine().lower()
    if arch in ("x86_64", "amd64"):
        return "amd64"
    elif arch in ("arm64", "aarch64"):
        return "arm64"
    else:
        return arch


# Direct alternative to linux native install command
def install(src: Path, dest: Path, mode: int) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dest)
    os.chmod(dest, mode)


# Removes a path if it exists, ignores missing paths
def remove_path(path: Path) -> bool:
    if path.is_symlink() or path.is_file():
        path.unlink(missing_ok=True)
        return True
    if path.is_dir():
        shutil.rmtree(path, ignore_errors=True)
        return True
    return False


# Returns "version" or "unknown" when installation exists
# Returns None when neither the Version file nor the Throne executable are found
def get_installed_version(path: Path) -> Union[str, None]:
    if (path / "Version").exists():
        with open(path / Path("Version"), "r") as file:
            return file.read().strip()

    if (path / "Throne").exists():
        return "unknown"

    return None


def get_github_releases() -> dict:
    def parse_release(data: dict) -> dict:
        return {
            "version": data["tag_name"],
            "assets": {
                "amd64": "".join(
                    [
                        i["browser_download_url"]
                        for i in data["assets"]
                        if "linux" in i["name"] and "amd64.zip" in i["name"]
                    ]
                ),
                "arm64": "".join(
                    [
                        i["browser_download_url"]
                        for i in data["assets"]
                        if "linux" in i["name"] and "arm64.zip" in i["name"]
                    ]
                ),
            },
        }

    result = {"stable": [], "unstable": []}

    # I don't think this loop is reasonable in any way
    # But just in case stable and unstable versions will somehow become 30 releases apart
    page = 1
    while True:
        data: dict = {}
        with urlopen(
            f"https://api.github.com/repos/{REPO}/releases?page={page}"
        ) as response:
            data = json.loads(response.read())

        result["stable"].extend([parse_release(i) for i in data if not i["prerelease"]])
        result["unstable"].extend([parse_release(i) for i in data if i["prerelease"]])

        if len(result["stable"]) >= 1 and len(result["unstable"]) >= 1:
            break

        page += 1

    return result


# [Little framework for curses]
C_ACC, C_DIM, C_OK = 1, 2, 3


def init_colors():
    curses.start_color()
    curses.use_default_colors()
    curses.init_pair(C_ACC, curses.COLOR_CYAN, -1)
    curses.init_pair(C_DIM, curses.COLOR_WHITE, -1)
    curses.init_pair(C_OK, curses.COLOR_CYAN, -1)


def bar(scr, y, x, glyph, text, color=C_ACC):
    scr.addstr(y, x, glyph, curses.color_pair(color) | curses.A_BOLD)
    scr.addstr(y, x + 2, text)


def select(scr, y, question, options):
    idx = 0
    while True:
        bar(scr, y, 0, "> ", question, C_ACC)
        for i, opt in enumerate(options):
            row = y + 1 + i
            marker = ">" if i == idx else " "
            attr = (
                curses.A_BOLD | curses.color_pair(C_ACC)
                if i == idx
                else curses.color_pair(C_DIM)
            )
            scr.addstr(row, 0, f"│  {marker} {opt}", attr)
        scr.addstr(y + 1 + len(options), 0, "└")
        scr.refresh()
        k = scr.getch()
        if k in (curses.KEY_UP, ord("k")):
            idx = (idx - 1) % len(options)
        elif k in (curses.KEY_DOWN, ord("j")):
            idx = (idx + 1) % len(options)
        elif k in (curses.KEY_ENTER, 10, 13):
            scr.move(y, 0)
            scr.clrtobot()
            bar(scr, y, 0, "> ", question, C_DIM)
            scr.addstr(y + 1, 0, f"│  > {options[idx]}", curses.color_pair(C_DIM))
            scr.refresh()
            return options[idx], y + 2


class Progress:
    def __init__(self, scr, y, label, width=30):
        self.scr, self.y, self.label, self.width = scr, y, label, width
        scr.addstr(y, 0, f"> {label}")
        scr.refresh()

    def update(self, done, total):
        pct = 0 if not total else min(done / total, 1.0)
        filled = int(self.width * pct)
        bar_str = "#" * filled + "-" * (self.width - filled)
        mb = (
            f"{done / 1024 / 1024:.1f}/{total / 1024 / 1024:.1f}MB"
            if total
            else f"{done / 1024 / 1024:.1f}MB"
        )
        self.scr.addstr(self.y + 1, 0, f"│  [{bar_str}] {int(pct * 100):3d}%  {mb}")
        self.scr.clrtoeol()
        self.scr.refresh()
        if pct >= 1.0:
            self.finish()

    def hook(self, block_num, block_size, total_size):
        self.update(block_num * block_size, total_size)

    def finish(self):
        self.scr.addstr(self.y, 0, f"> {self.label}", curses.color_pair(C_DIM))
        self.scr.clrtoeol()
        self.scr.refresh()

    def next_y(self):
        return self.y + 2


def message(scr, y, text, ok=True):
    glyph, color = ("> ", C_OK) if ok else ("> ", C_DIM)
    bar(scr, y, 0, glyph, text, color)
    scr.refresh()
    return y + 1


def run(main):
    def wrapped(scr):
        curses.curs_set(0)
        init_colors()
        scr.clear()
        scr.addstr(0, 0, "┌  Welcome to Throne CLI manager!")
        scr.addstr(1, 0, "|")
        main(scr, 2)

    with open("/dev/tty") as tty:
        os.dup2(tty.fileno(), 0)

    curses.wrapper(wrapped)


# [Mainloop]
def main(scr, y):
    arch = get_arch()
    version = get_installed_version(APPDIR)
    if version:
        y = message(scr, y, f"Looks like you already have Throne ({version}) installed")
        bar(scr, y, 0, "|", "", C_DIM)
        scr.refresh()
        y += 1

    releases = get_github_releases()

    y = message(scr, y, f"Lastest Stable release: {releases['stable'][0]['version']}")
    y = message(
        scr, y, f"Lastest Unstable release: {releases['unstable'][0]['version']}"
    )

    bar(scr, y, 0, "|", "", C_DIM)
    scr.refresh()
    y += 1

    action, y = select(
        scr,
        y,
        "What would you like to do?",
        ["Install", "Uninstall"],
    )

    if action == "Install":
        branch, y = select(
            scr,
            y,
            "Which branch would you like?",
            ["Stable", "Unstable"],
        )

        with TemporaryDirectory() as tmp:
            tmpdir = Path(tmp)

            release = releases[branch.lower()][0]
            version = release["version"]
            url = release["assets"][arch]

            p = Progress(scr, y, f"Downloading Throne ({version}) from GitHub")
            urlretrieve(
                url,
                tmpdir / "Throne.zip",
                p.hook,
            )
            y = p.next_y()

            y = message(scr, y, "Unpacking zip archive")
            with ZipFile(tmpdir / "Throne.zip", "r") as zip:
                zip.extractall(tmpdir)

            y = message(scr, y, "Creating .desktop file")
            with open(tmpdir / "Throne.desktop", "w") as file:
                file.write(f"""[Desktop Entry]
Name=Throne
Comment=Qt based cross-platform GUI proxy configuration manager (backend: sing-box)
Exec={APPDIR}/Throne -appdata
Icon={APPDIR}/Throne.png
Terminal=false
Type=Application
Categories=Network;Application;
""")

            with open(tmpdir / "Version", "w") as file:
                file.write(version)
            install(tmpdir / "Version", APPDIR / "Version", 0o644)

            y = message(scr, y, f"Installing binaries to {APPDIR}")
            for name in ("ThroneCore", "Throne"):
                install(tmpdir / "Throne" / name, APPDIR / name, 0o755)
            usr_src = tmpdir / "Throne/usr"
            for f in usr_src.rglob("*"):
                if f.is_file():
                    rel = f.relative_to(usr_src)
                    install(f, APPDIR / "usr" / rel, 0o644)
            install(tmpdir / "Throne/Throne.png", APPDIR / "Throne.png", 0o644)

            y = message(scr, y, f"Installing .desktop to {DESKTOPDIR}")
            install(tmpdir / "Throne.desktop", DESKTOPDIR / "Throne.desktop", 0o644)

    elif action == "Uninstall":
        if version is None:
            y = message(scr, y, "Throne installation was not found", ok=False)
            scr.addstr(y, 0, "└  Press any key to exit")
            scr.refresh()
            scr.getch()
            exit()

        confirm, y = select(
            scr,
            y,
            "Are you sure you want to uninstall Throne?",
            ["Yes", "No"],
        )

        if confirm == "No":
            scr.addstr(y, 0, "└  Press any key to exit")
            scr.refresh()
            scr.getch()
            exit()

        y = message(scr, y, f"Removing binaries from {APPDIR}")
        remove_path(APPDIR)

        y = message(scr, y, f"Removing .desktop file from {DESKTOPDIR}")
        remove_path(DESKTOPDIR / "Throne.desktop")

        y = message(scr, y, "Uninstallation successfully completed!")
    scr.addstr(y, 0, "└  Press any key to exit")
    scr.refresh()
    scr.getch()


if __name__ == "__main__":
    run(main)
