#!/usr/bin/env python3
"""L0 guard: the USART protocol contract (test_plan.md section 4).

The protocol is defined in four places (AGENTS.md section 5). Three of them can be
checked statically, without a compiler:

1. every ``extern const char* CMD_*`` in ``definitions.h`` has a ``checkCommand(CMD_*)``
   branch in ``main.c`` (the dispatch direction);
2. every command/response string in ``definitions.c`` equals the matching constant in
   ``Commands.cs`` / ``Responses.cs`` (names differ only by the ``CMD_``/``RESP_`` prefix);
3. every string from ``definitions.c`` occurs in the protocol table of ``AGENTS.md``
   section 5 (containment, not equality: the Response column there is prose).

The fourth place is the C command dispatch behaviour, covered by L2.

Usage:
    python3 tools/test_protocol_contract.py [--repo-root DIR]

Exit code 0 means every check passed. No dependencies beyond the standard library.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# --- parsing patterns -------------------------------------------------------

# firmware/source/common/definitions.c:  const char* CMD_GO_FORWARD = "GOF";
C_STRING_RE = re.compile(r'\bconst\s+char\s*\*\s*(\w+)\s*=\s*"([^"]*)"\s*;')
# firmware/source/common/definitions.h:  extern const char* CMD_GO_FORWARD;
C_EXTERN_RE = re.compile(r'\bextern\s+const\s+char\s*\*\s*(\w+)\s*;')
# ascom/.../Commands.cs:  public const string GO_FORWARD = "GOF";
CS_STRING_RE = re.compile(r'\bpublic\s+const\s+string\s+(\w+)\s*=\s*"([^"]*)"\s*;')
# AGENTS.md table cells are backticked: `GOF`, `GT<number>`, `OK`
BACKTICK_RE = re.compile(r"`([^`\n]+)`")

PREFIX_RE = re.compile(r"^(?:CMD|RESP)_")


def strip_prefix(name: str) -> str:
    """CMD_GO_FORWARD -> GO_FORWARD, RESP_OK -> OK."""
    return PREFIX_RE.sub("", name)


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


def protocol_section(agents_text: str) -> str:
    """Return the text of the '## 5. USART protocol' section (up to '## 6.')."""
    lines = agents_text.splitlines()
    start = None
    for index, line in enumerate(lines):
        if start is None and line.startswith("## 5."):
            start = index
        elif start is not None and line.startswith("## "):
            return "\n".join(lines[start:index])
    return "\n".join(lines[start:]) if start is not None else ""


def matching_constants(c_name: str, cs_constants: list[tuple[str, str, str]]):
    """Match a C constant to C# ones by normalized name, then by suffix."""
    target = strip_prefix(c_name)
    exact = [entry for entry in cs_constants if entry[0] == target]
    if exact:
        return exact
    return [
        entry
        for entry in cs_constants
        if entry[0].endswith(target) or target.endswith(entry[0])
    ]


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

    definitions_h = root / "firmware/source/common/definitions.h"
    definitions_c = root / "firmware/source/common/definitions.c"
    main_c = root / "firmware/source/main.c"
    # The C# side of the protocol lives in its own ASCOM-free assembly; the driver compiles the
    # same sources (ascom/ASCOM.Altair.Dome.csproj, <Compile Include="..\DomeControl.Protocol\**" />).
    ascom_dir = root / "ascom/DomeControl.Protocol"
    agents_md = root / "AGENTS.md"

    report = Report()
    missing = [p for p in (definitions_h, definitions_c, main_c, agents_md) if not p.is_file()]
    for path in missing:
        report.check(False, f"required file not found: {path}")

    c_constants = C_STRING_RE.findall(read(definitions_c)) if definitions_c.is_file() else []
    c_externs = [name for name in C_EXTERN_RE.findall(read(definitions_h))
                 if name.startswith("CMD_")] if definitions_h.is_file() else []
    main_text = read(main_c) if main_c.is_file() else ""

    cs_constants: list[tuple[str, str, str]] = []  # (name, value, file)
    for cs_file in ("Commands.cs", "Responses.cs"):
        path = ascom_dir / cs_file
        if not path.is_file():
            report.check(False, f"required file not found: {path}")
            continue
        cs_constants.extend(
            (name, value, cs_file) for name, value in CS_STRING_RE.findall(read(path))
        )

    report.check(bool(c_constants), f"no command/response strings parsed from {definitions_c}")
    report.check(bool(cs_constants), f"no constants parsed from {ascom_dir}")

    # 1. Extern <-> definition, and the dispatch branch in main.c.
    defined = {name for name, _ in c_constants}
    for name in c_externs:
        report.check(name in defined, f"{definitions_h.name}: {name} is extern but not defined in {definitions_c.name}")
    for name in sorted(defined):
        if name.startswith("CMD_"):
            report.check(
                name in c_externs,
                f"{definitions_c.name}: {name} is not declared extern in {definitions_h.name}",
            )
    for name in sorted(c_externs):
        report.check(
            re.search(r"\bcheckCommand\s*\(\s*" + re.escape(name) + r"\s*,", main_text) is not None,
            f"{main_c.name}: no checkCommand({name}, ...) branch",
        )

    # 2. firmware <-> C#: names match after the CMD_/RESP_ prefix is stripped, values equal.
    matched_cs: set[tuple[str, str]] = set()
    for name, value in sorted(c_constants):
        candidates = matching_constants(name, cs_constants)
        if not report.check(bool(candidates), f"{name} = \"{value}\" has no counterpart in Commands.cs/Responses.cs"):
            continue
        for cs_name, cs_value, cs_file in candidates:
            matched_cs.add((cs_name, cs_file))
            report.check(
                cs_value == value,
                f'{name} = "{value}" != {cs_file} {cs_name} = "{cs_value}"',
            )
    for cs_name, cs_value, cs_file in sorted(cs_constants):
        report.check(
            (cs_name, cs_file) in matched_cs,
            f'{cs_file}: {cs_name} = "{cs_value}" has no counterpart in definitions.c',
        )

    # 3. Every firmware string must appear in the AGENTS.md section 5 table.
    section = protocol_section(read(agents_md)) if agents_md.is_file() else ""
    report.check(bool(section), f"{agents_md.name}: '## 5. USART protocol' section not found")
    spans = BACKTICK_RE.findall(section)
    for name, value in sorted(c_constants):
        report.check(
            any(value in span for span in spans),
            f'{name} = "{value}" does not occur in the {agents_md.name} section 5 table',
        )

    print(f"protocol contract: {len(c_constants)} firmware strings, {len(cs_constants)} C# constants, "
          f"{len(c_externs)} CMD_ externs, {len(spans)} table spans")
    return report.finish()


if __name__ == "__main__":
    sys.exit(main())