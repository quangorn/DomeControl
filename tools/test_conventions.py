#!/usr/bin/env python3
"""L0 guard: the AGENTS.md section 8 conventions (test_plan.md section 4).

Grep guards over the firmware, cheap enough to run anywhere:

1. no ``sprintf`` family calls -- the project formats with ``printInt``/``parseInt``
   (AGENTS.md section 4, commit "removed sprintf usage");
2. no ``usartPrint*`` call before ``sei()`` inside ``main()`` -- the transmit path
   blocks while waiting for the transmit interrupt, so it hangs with interrupts off
   (AGENTS.md section 8.5);
3. every module under ``firmware/source/`` declares an ``<module>Init*()`` function
   and that function is called from ``main()`` (AGENTS.md section 4).

Usage:
    python3 tools/test_conventions.py [--repo-root DIR]

Exit code 0 means every check passed. No dependencies beyond the standard library.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

SPRINTF_RE = re.compile(r"\b(?:sprintf|snprintf|vsprintf|vsnprintf)\b")
USART_PRINT_RE = re.compile(r"\busartPrint\w*\s*\(")
SEI_RE = re.compile(r"\bsei\s*\(\s*\)")
MAIN_SIGNATURE_RE = re.compile(r"\bint\s+main\s*\([^;{]*\)")

MODULES_TO_SKIP = {"common"}
SOURCE_SUFFIXES = (".c", ".h")


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


class Report:
    """Tiny pass/fail collector; the script prints failures and exits non-zero."""

    def __init__(self) -> None:
        self.checks = 0
        self.failures: list[str] = []

    def check(self, condition: bool, message: str) -> bool:
        self.checks += 1
        if not condition:
            self.failures.append(message)
        return condition

    def finish(self) -> int:
        for failure in self.failures:
            print(f"[FAIL] {failure}")
        if self.failures:
            print(f"\n{len(self.failures)} of {self.checks} checks failed")
            return 1
        print(f"all {self.checks} checks passed")
        return 0


def line_of(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def function_body(text: str, signature_re: re.Pattern[str]):
    """Return (body, body_offset) for the first brace-balanced match, or (None, None)."""
    match = signature_re.search(text)
    if match is None:
        return None, None
    opening = text.find("{", match.end())
    if opening < 0:
        return None, None
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[opening:index + 1], opening
    return None, None


def check_no_sprintf(report: Report, root: Path) -> int:
    """1. No sprintf family in the firmware."""
    sources = sorted((root / "firmware/source").rglob("*"))
    sources = [p for p in sources if p.is_file() and p.suffix in SOURCE_SUFFIXES]
    report.check(bool(sources), f"no firmware sources found under {root / 'firmware/source'}")
    for path in sources:
        text = read(path)
        for match in SPRINTF_RE.finditer(text):
            report.check(
                False,
                f"{path.relative_to(root)}:{line_of(text, match.start())}: "
                f"'{match.group(0)}' is banned in the firmware",
            )
    return len(sources)


def check_usart_print_order(report: Report, main_c: Path) -> None:
    """2. No usartPrint* before sei() inside main()."""
    text = read(main_c)
    body, offset = function_body(text, MAIN_SIGNATURE_RE)
    if not report.check(body is not None, f"{main_c.name}: main() not found"):
        return
    assert body is not None and offset is not None

    sei = SEI_RE.search(body)
    if not report.check(sei is not None, f"{main_c.name}: no sei() call inside main()"):
        return
    assert sei is not None

    for match in USART_PRINT_RE.finditer(body):
        if match.start() < sei.start():
            report.check(
                False,
                f"{main_c.name}:{line_of(text, offset + match.start())}: "
                f"'{match.group(0)[:-1]}' is called before sei() in main(); "
                "usartPrint* blocks with interrupts disabled (AGENTS.md 8.5)",
            )


def check_module_inits(report: Report, source_dir: Path, main_text: str) -> int:
    """3. Every module declares <module>Init*() and main() calls it."""
    modules = sorted(p for p in source_dir.iterdir()
                     if p.is_dir() and p.name not in MODULES_TO_SKIP)
    report.check(bool(modules), f"no module directories found under {source_dir}")
    for module in modules:
        header = module / f"{module.name}.h"
        if not report.check(header.is_file(), f"{module.name}: {header.name} not found"):
            continue
        init_re = re.compile(r"\bvoid\s+(" + re.escape(module.name) + r"Init\w*)\s*\(")
        inits = init_re.findall(read(header))
        if not report.check(
            bool(inits),
            f"{module.name}: {header.name} declares no {module.name}Init*() function",
        ):
            continue
        for name in sorted(set(inits)):
            report.check(
                re.search(r"\b" + re.escape(name) + r"\s*\(", main_text) is not None,
                f"main.c: {module.name} module init {name}() is never called",
            )
    return len(modules)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
        help="repository root (default: the parent of this script's directory)",
    )
    args = parser.parse_args()
    root: Path = args.repo_root

    source_dir = root / "firmware/source"
    main_c = source_dir / "main.c"
    report = Report()

    if not report.check(main_c.is_file(), f"required file not found: {main_c}"):
        return report.finish()
    main_text = read(main_c)

    sources = check_no_sprintf(report, root)
    check_usart_print_order(report, main_c)
    modules = check_module_inits(report, source_dir, main_text)

    print(f"conventions: {sources} firmware sources, {modules} modules, main.c guard")
    return report.finish()


if __name__ == "__main__":
    sys.exit(main())