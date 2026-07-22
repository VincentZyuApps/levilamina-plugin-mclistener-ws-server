#!/usr/bin/env python3
"""Synchronize the plugin version. Usage: python scripts/bump_version.py [--dry-run] <version>."""

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SEMVER_TOKEN = (
    r"(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)"
    r"(?:-[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?"
    r"(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?"
)
SEMVER = re.compile(rf"^{SEMVER_TOKEN}$")


@dataclass(frozen=True)
class Target:
    path: str
    pattern: str
    required: bool = False


@dataclass(frozen=True)
class PlannedTarget:
    path: Path
    content: str
    matched_count: int
    changed_count: int
    previous_versions: tuple[str, ...]


TARGETS = (
    Target(
        "tooth.json",
        r'(?m)^(\s*"version"\s*:\s*")(?P<version>{version})("\s*,?\s*)$',
        required=True,
    ),
    Target(
        "README.md",
        r'(?m)(github\.com/VincentZyuApps/levilamina-plugin-mclistener-ws-server@)'
        r'(?P<version>{version})(?=\s|`|$)',
    ),
    Target(
        ".github/workflows/build.md",
        r'(?m)^(\s*"version"\s*:\s*")(?P<version>{version})("\s*,?\s*)$',
    ),
    Target(
        "js/mclistener-ws-server-exec-command-relay.js",
        r'(?m)^(\s*const\s+PLUGIN_VERSION\s*=\s*")(?P<version>{version})("\s*;.*)$',
        required=True,
    ),
)


class Style:
    BOLD = "\033[1m"
    GREEN = "\033[92m"
    YELLOW = "\033[93m"
    RED = "\033[91m"
    CYAN = "\033[96m"
    DIM = "\033[2m"
    RESET = "\033[0m"


def read_text(path: Path) -> str:
    with path.open("r", encoding="utf-8", newline="") as file:
        return file.read()


def write_text(path: Path, content: str) -> None:
    with path.open("w", encoding="utf-8", newline="") as file:
        file.write(content)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Synchronize the plugin version across repository files.")
    parser.add_argument("new_version", help="new semantic version, for example 0.6.5-alpha.27+20260723")
    parser.add_argument(
        "-d",
        "--dryrun",
        "--dry-run",
        dest="dry_run",
        action="store_true",
        help="show changes without writing files",
    )
    return parser.parse_args()


def compile_target_pattern(target: Target) -> re.Pattern[str]:
    return re.compile(target.pattern.replace("{version}", SEMVER_TOKEN))


def inspect_target(target: Target, new: str) -> PlannedTarget | None:
    path = ROOT / target.path
    if not path.is_file():
        return None

    content = read_text(path)
    pattern = compile_target_pattern(target)
    matches = list(pattern.finditer(content))
    if not matches:
        return None

    previous_versions = tuple(match.group("version") for match in matches)
    changed_count = sum(version != new for version in previous_versions)

    def replace(match: re.Match[str]) -> str:
        if match.group("version") == new:
            return match.group(0)
        start, end = match.span("version")
        relative_start = start - match.start()
        relative_end = end - match.start()
        return match.group(0)[:relative_start] + new + match.group(0)[relative_end:]

    return PlannedTarget(
        path=path,
        content=pattern.sub(replace, content),
        matched_count=len(matches),
        changed_count=changed_count,
        previous_versions=previous_versions,
    )


def plan_changes(new: str) -> tuple[list[PlannedTarget], list[str], list[str]]:
    plans: list[PlannedTarget] = []
    warnings: list[str] = []
    errors: list[str] = []
    for target in TARGETS:
        plan = inspect_target(target, new)
        if plan is None:
            message = f"{target.path}: version field not found"
            (errors if target.required else warnings).append(message)
            continue
        plans.append(plan)
    return plans, warnings, errors


def main() -> int:
    args = parse_args()
    new = args.new_version.strip()
    if not SEMVER.fullmatch(new):
        print(f"{Style.RED}error:{Style.RESET} invalid semantic version: {new}", file=sys.stderr)
        return 2

    plans, warnings, errors = plan_changes(new)
    print(f"\n{Style.BOLD}Synchronize version -> {new}{Style.RESET}")
    if args.dry_run:
        print(f"{Style.CYAN}Dry run: no files will be written.{Style.RESET}")
    print()

    changed_plans = [plan for plan in plans if plan.changed_count]
    for plan in plans:
        relative_path = plan.path.relative_to(ROOT)
        if plan.changed_count:
            previous = ", ".join(sorted({v for v in plan.previous_versions if v != new}))
            action = "would update" if args.dry_run else "update"
            print(
                f"  {Style.GREEN}{action}{Style.RESET} {relative_path} "
                f"({plan.changed_count}/{plan.matched_count} occurrences: {previous} -> {new})"
            )
        else:
            print(f"  {Style.DIM}ok{Style.RESET} {relative_path} ({plan.matched_count} occurrences)")
    for warning in warnings:
        print(f"  {Style.YELLOW}warning:{Style.RESET} {warning}")
    for error in errors:
        print(f"  {Style.RED}error:{Style.RESET} {error}")

    if errors:
        print(f"\n{Style.RED}No files were written because required targets failed validation.{Style.RESET}")
        return 2
    if not changed_plans:
        print(f"\n{Style.YELLOW}Already synchronized at {new}; nothing to change.{Style.RESET}")
        return 0
    if args.dry_run:
        print(f"\n{Style.DIM}Validated {len(plans)} files; no files were written.{Style.RESET}")
        return 0

    for plan in changed_plans:
        write_text(plan.path, plan.content)
    print(f"\n{Style.GREEN}{Style.BOLD}Updated {len(changed_plans)} files.{Style.RESET}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
