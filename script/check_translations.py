#!/usr/bin/env python3
r"""Checks res/translations/*.ts against the strings in the code.

Runs lupdate over src, include and 3rdparty into temporary copies of the .ts files and compares each copy with its
original per (context, source, comment):
  missing  in the code, but absent or unfinished in the .ts: an error for zh_CN and ru_RU, only counted for fa_IR;
  stale    in the .ts, but no longer in the code: an error in every language.
Stale entries listed in script/translations_allowlist.txt are not reported: one "context | source" line each, \n for a
line break, for strings translated through a variable that lupdate cannot see.

usage: python3 -I script/check_translations.py [--lupdate PATH]
"""
import argparse
import re
import shutil
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TS_DIR = ROOT / "res" / "translations"
ALLOWLIST = ROOT / "script" / "translations_allowlist.txt"
SOURCES = ["src", "include", "3rdparty"]
# Whether a missing entry is an error, per language.
LANGUAGES = {"zh_CN": True, "ru_RU": True, "fa_IR": False}
CPP_SUFFIXES = {".c", ".c++", ".cc", ".cpp", ".cxx", ".ch", ".h", ".h++", ".hh", ".hpp", ".hxx"}
# A class or struct whose tr() comes from Q_DECLARE_TR_FUNCTIONS(context), the macro ahead of any nested brace.
TR_FUNCTIONS = re.compile(r"\b(?:class|struct)\s+(?:\w+\s+)*?(\w+)\s*(?:final\s*)?(?::[^;{]*)?\{[^{}]*?"
                          r"\bQ_DECLARE_TR_FUNCTIONS\s*\(\s*([\w:]+)\s*\)")


def tr_function_contexts():
    contexts = {}
    for base in SOURCES:
        for path in (ROOT / base).rglob("*"):
            if path.suffix in CPP_SUFFIXES:
                text = path.read_text(encoding="utf-8", errors="replace")
                if "Q_DECLARE_TR_FUNCTIONS" in text:
                    contexts.update(TR_FUNCTIONS.findall(text))
    return contexts


def read_ts(path):
    """Maps (context, source, comment) to the translation's type ("" when finished) and whether it has any text."""
    messages = {}
    for context in ET.parse(path).getroot().iter("context"):
        name = context.findtext("name")
        for message in context.iter("message"):
            translation = message.find("translation")
            key = (name, message.findtext("source") or "", message.findtext("comment") or "")
            if translation is None:
                messages[key] = ("unfinished", False)
            else:
                messages[key] = (translation.get("type", ""), bool("".join(translation.itertext())))
    return messages


def read_allowlist():
    if not ALLOWLIST.exists():
        return set()
    entries = set()
    for line in ALLOWLIST.read_text(encoding="utf-8").splitlines():
        if line.strip() and not line.startswith("#"):
            context, source = line.split(" | ", 1)
            entries.add((context, source.replace("\\n", "\n")))
    return entries


def describe(key):
    context, source, comment = key
    return f"{context} | {source}".replace("\n", "\\n") + (f"  (comment: {comment})" if comment else "")


def main():
    parser = argparse.ArgumentParser(description="Checks the .ts files against the strings in the code.")
    parser.add_argument("--lupdate", default=shutil.which("lupdate"), help="the lupdate binary (default: from PATH)")
    lupdate = parser.parse_args().lupdate
    if not lupdate:
        sys.exit("check_translations: no lupdate on PATH, pass --lupdate")
    sys.stdout.reconfigure(encoding="utf-8")

    with tempfile.TemporaryDirectory() as tmp:
        copies = {language: shutil.copy(TS_DIR / f"{language}.ts", tmp) for language in LANGUAGES}
        # Like the CMake lupdate target, the repo root as include path: without it lupdate cannot see the declarations
        # in headers, and files the strings of namespaced classes under their bare class names.
        command = [lupdate, "-silent", "-locations", "none", "-I", ROOT.as_posix(), *SOURCES, "-ts", *copies.values()]
        started = time.monotonic()
        result = subprocess.run(command, cwd=ROOT, capture_output=True, encoding="utf-8", errors="replace")
        print(f"lupdate took {time.monotonic() - started:.1f} s")
        for line in (result.stdout + result.stderr).splitlines():
            print("lupdate:", line.replace(ROOT.as_posix() + "/", ""))
        if result.returncode != 0:
            sys.exit(f"check_translations: lupdate exited with {result.returncode}")
        extracted = {language: read_ts(path) for language, path in copies.items()}

    aliases = tr_function_contexts()

    def runtime_context(context):
        # lupdate files the X::tr of `cond ? QCoreApplication::translate(...) : X::tr(...)` under QCoreApplication::X.
        context = context.removeprefix("QCoreApplication::")
        return aliases.get(context.rsplit("::", 1)[-1], context)

    allowed = read_allowlist()
    errors = 0
    for language, missing_is_error in LANGUAGES.items():
        original = read_ts(TS_DIR / f"{language}.ts")
        in_code = {(runtime_context(context), source, comment)
                   for (context, source, comment), (kind, _) in extracted[language].items()
                   if kind not in ("vanished", "obsolete")}
        missing = sorted(key for key in in_code if original.get(key) != ("", True))
        stale = sorted(key for key in original if key not in in_code and key[:2] not in allowed)
        print(f"\n{language}: {len(missing)} missing{'' if missing_is_error else ' (not an error)'}, {len(stale)} stale")
        if missing_is_error:
            for key in missing:
                print("  missing ", describe(key))
            errors += len(missing)
        for key in stale:
            print("  stale   ", describe(key))
        errors += len(stale)

    if errors:
        print(f"\ncheck_translations: {errors} error(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
