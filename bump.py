#!/usr/bin/env python3
"""Bump version across all files. Usage: python bump.py <new_version>"""

import json
import re
import sys
import os

ROOT = os.path.dirname(os.path.abspath(__file__))

# ── file: match_mode ────────────────────────────────────
#   "exact" → content.replace(old_ver, new_ver)  (tooth.json)
#   "regex" → re.sub(r'{base_ver}[-+\w.]*', new_ver, content)  (others)
FILES = {
    "tooth.json": "exact",
    "README.md": "regex",
    ".github/workflows/build.md": "regex",
}
TOOTH = os.path.join(ROOT, "tooth.json")


# ── Style helpers ───────────────────────────────────────
class S:
    BOLD = "\033[1m"
    GREEN = "\033[92m"
    YELLOW = "\033[93m"
    RED = "\033[91m"
    CYAN = "\033[96m"
    MAGENTA = "\033[95m"
    DIM = "\033[2m"
    RESET = "\033[0m"


def ok(msg):
    print(f"  {S.GREEN}✅{S.RESET} {msg}")


def warn(msg):
    print(f"  {S.YELLOW}⚠️ {S.RESET} {msg}")


def err(msg):
    print(f"  {S.RED}❌{S.RESET} {msg}")


def info(msg):
    print(f"  {S.CYAN}ℹ️ {S.RESET} {S.DIM}{msg}{S.RESET}")


# ── Helpers ─────────────────────────────────────────────
def banner():
    print()
    print(f"  {S.BOLD}╔══════════════════════════════════════╗{S.RESET}")
    print(
        f"  {S.BOLD}║       {S.MAGENTA}📦  Version Bumper{S.RESET}{S.BOLD}             ║{S.RESET}"
    )
    print(f"  {S.BOLD}╚══════════════════════════════════════╝{S.RESET}")
    print()


def footer(old_ver, new_ver):
    print(f"  ---------------------------------------------")
    ok(f"{S.BOLD}Done: {old_ver} → {new_ver}{S.RESET}")
    print()


def extract_base(ver):
    m = re.match(r"(\d+\.\d+\.\d+)", ver)
    return m.group(1) if m else None


def build_regex(base):
    escaped = base.replace(".", r"\.")
    return re.compile(rf"{escaped}[-+\w.]*")


def replace_regex(content, old_ver, new_ver):
    base = extract_base(old_ver)
    if not base:
        return content
    return build_regex(base).sub(new_ver, content)


def find_old_values(content, old_ver):
    base = extract_base(old_ver)
    if not base:
        return []
    return list(dict.fromkeys(build_regex(base).findall(content)))


# ── Main ────────────────────────────────────────────────
def main():
    if len(sys.argv) != 2 or sys.argv[1] in ("-h", "--help"):
        banner()
        print(
            f"  {S.BOLD}Usage:{S.RESET} python {sys.argv[0]} {S.CYAN}<new_version>{S.RESET}"
        )
        print(f"  {S.BOLD}e.g.:{S.RESET}  python {sys.argv[0]} x.y.z-beta.w")
        print()
        sys.exit(1)

    banner()

    new_ver = sys.argv[1]

    with open(TOOTH, "r", encoding="utf-8") as f:
        tooth = json.load(f)
    old_ver = tooth["version"]

    if old_ver == new_ver:
        warn(f"Already at {old_ver}, nothing to do.")
        print()
        return

    info(f"Old version: {old_ver}")
    info(f"New version: {new_ver}")
    print(f"  ---------------------------------------------")
    print()

    total_replaced = 0

    for rel_path, mode in FILES.items():
        path = os.path.join(ROOT, rel_path)
        if not os.path.exists(path):
            warn(f"{rel_path} — file not found, skipping")
            continue

        with open(path, "r", encoding="utf-8") as f:
            content = f.read()

        old_vals = []
        count = 0

        if mode == "exact":
            count = content.count(old_ver)
            if count:
                content = content.replace(old_ver, new_ver)
        else:
            old_vals = find_old_values(content, old_ver)
            count = len(old_vals)
            if count:
                content = replace_regex(content, old_ver, new_ver)

        print(f"  {S.BOLD}📄 {rel_path}{S.RESET}")

        if count == 0:
            warn("no version strings found, skipped")
        else:
            with open(path, "w", encoding="utf-8") as f:
                f.write(content)
            total_replaced += count
            if old_vals:
                ok_msg = f"replaced {count} occurrence(s): {', '.join(old_vals)}"
            else:
                ok_msg = f"replaced {count} occurrence(s)"
            ok(ok_msg)

        print()

    footer(old_ver, new_ver)


if __name__ == "__main__":
    main()
