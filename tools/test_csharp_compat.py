#!/usr/bin/env python3
"""L0 guard: the two C# invariants that nothing else here can see.

1. **The C# stage must build offline.** The ASCOM driver is Windows-only and is never built
   here, so ``ascom/DomeControl.Protocol`` plus its tests are the only C# an agent can check.
   That is only worth having if ``dotnet build`` works with no network and no writable home
   directory: a sandbox denies ``~/.nuget/packages`` and ``~/.local/share/NuGet``, so any
   package, and any framework whose reference assemblies are not in the SDK, breaks the stage
   on a fresh clone. A ``netstandard2.0`` SDK-style project is exactly that case -- it carries
   an implicit ``PackageReference`` on ``NETStandard.Library``.

2. **The protocol sources stay usable from .NET Framework 4.7.2.** The driver compiles them
   into itself rather than referencing the built assembly, so the target framework of the
   library says nothing about the driver: a net8.0-only API here compiles cleanly on Linux and
   breaks the Visual Studio build, which no check on this host runs. ``LangVersion`` is the
   compiler's half of the guarantee; the rest is this list of common offenders. It is a list,
   not a proof -- a Windows build is still the real check (AGENTS.md section 8.1).

Usage:
    python3 tools/test_csharp_compat.py [--repo-root DIR]

Exit code 0 means every check passed. No dependencies beyond the standard library.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# --- the SDK-style projects that must restore without a network --------------------

LIBRARY_PROJECT = "ascom/DomeControl.Protocol/DomeControl.Protocol.csproj"
TESTS_PROJECT = "ascom/DomeControl.Protocol.Tests/DomeControl.Protocol.Tests.csproj"
SDK_PROJECTS = (LIBRARY_PROJECT, TESTS_PROJECT)

DRIVER_PROJECT = "ascom/ASCOM.Altair.Dome/ASCOM.Altair.Dome.csproj"
# How the driver's sources reach the library: a glob, so there is no assembly to reference.
DRIVER_GLOB_RE = re.compile(r"<Compile\s+Include=\"\.\.\\DomeControl\.Protocol\\\*\*\\\*\.cs\"")
DRIVER_REFERENCE_RE = re.compile(r"<ProjectReference\b[^>]*DomeControl\.Protocol", re.DOTALL)

PACKAGE_REFERENCE_RE = re.compile(r"<PackageReference\b")
TARGET_FRAMEWORK_RE = re.compile(r"<TargetFramework>([^<]+)</TargetFramework>")
LANG_VERSION_RE = re.compile(r"<LangVersion>([^<]+)</LangVersion>")
NUGET_AUDIT_RE = re.compile(r"<NuGetAudit>([^<]+)</NuGetAudit>")

# Frameworks whose reference assemblies ship inside the .NET SDK. A netstandardX.Y project is
# absent on purpose: its NETStandard.Library reference assemblies are a downloaded package.
OFFLINE_FRAMEWORKS = ("net8.0",)
# The driver is an old-style csproj, so this is the level it can compile.
DRIVER_LANG_VERSION = "7.3"

# --- APIs that exist in net8.0 and not in .NET Framework 4.7.2 ----------------------
#
# Each entry: (pattern, what to write instead). Only members that a plausible edit to this
# library would reach for; the list is not exhaustive and does not claim to be.

FORBIDDEN_APIS = (
    (
        re.compile(r"\.Contains\(\s*'"),
        "string.Contains(char) does not exist in .NET Framework 4.7.2; use "
        "\".Contains(\\\"c\\\")\" (a collection's Contains(char) is fine, check which one this is)",
    ),
    (
        re.compile(r"\.(?:StartsWith|EndsWith)\(\s*'"),
        "string.StartsWith(char)/EndsWith(char) do not exist in .NET Framework 4.7.2; pass a "
        "string instead",
    ),
    (
        re.compile(r"\.Split\(\s*'[^']*'\s*,"),
        "string.Split(char, StringSplitOptions) does not exist in .NET Framework 4.7.2",
    ),
    (
        re.compile(r"\.Join\(\s*'"),
        "string.Join(char, ...) does not exist in .NET Framework 4.7.2; pass a string separator",
    ),
    (
        re.compile(r"\.(?:Contains|Replace)\([^)]*StringComparison"),
        "the StringComparison overload of Contains/Replace does not exist in .NET Framework "
        "4.7.2",
    ),
    (re.compile(r"\bMath\.Clamp\s*\("), "Math.Clamp does not exist in .NET Framework 4.7.2"),
    (re.compile(r"\bMathF\."), "MathF does not exist in .NET Framework 4.7.2"),
    (re.compile(r"\bHashCode\."), "System.HashCode does not exist in .NET Framework 4.7.2"),
    (
        re.compile(r"\bConvert\.ToHexString\s*\("),
        "Convert.ToHexString does not exist in .NET Framework 4.7.2",
    ),
    (
        re.compile(r"\b(?:ReadOnly)?Span<|\b(?:ReadOnly)?Memory<"),
        "Span/Memory do not exist in .NET Framework 4.7.2, and this project has no packages",
    ),
    (re.compile(r"\bstackalloc\b"), "stackalloc of Span does not exist in .NET Framework 4.7.2"),
    (
        re.compile(r"\bAs(?:Span|Memory)\s*\("),
        "AsSpan/AsMemory do not exist in .NET Framework 4.7.2",
    ),
    (
        re.compile(r"\bIAsyncEnumerable<|\bIAsyncDisposable\b"),
        "IAsyncEnumerable/IAsyncDisposable do not exist in .NET Framework 4.7.2",
    ),
)

# Lines whose matches are comments or literals, not calls.
CODE_LINE_RE = re.compile(r"^\s*(?://|/\*|\*|#)")


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


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def line_of(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def check_offline_restore(report: Report, root: Path) -> int:
    """1. Both SDK-style projects restore with no network and no writable home directory."""
    for relative in SDK_PROJECTS:
        path = root / relative
        if not report.check(path.is_file(), f"required project not found: {relative}"):
            continue
        text = read(path)

        for match in PACKAGE_REFERENCE_RE.finditer(text):
            report.check(
                False,
                f"{relative}:{line_of(text, match.start())}: a PackageReference cannot be "
                "restored here (no writable ~/.nuget/packages); the C# stage must stay "
                "package-free -- see the csproj comment",
            )

        frameworks = TARGET_FRAMEWORK_RE.findall(text)
        if report.check(
            len(frameworks) == 1,
            f"{relative}: expected exactly one <TargetFramework>, found {len(frameworks)}",
        ):
            framework = frameworks[0]
            report.check(
                framework in OFFLINE_FRAMEWORKS,
                f"{relative}: <TargetFramework>{framework}</TargetFramework> needs reference "
                f"assemblies from a package (netstandardX.Y does); use one of "
                f"{', '.join(OFFLINE_FRAMEWORKS)}, which the SDK carries",
            )

        audits = NUGET_AUDIT_RE.findall(text)
        report.check(
            any(value.strip().lower() == "false" for value in audits),
            f"{relative}: <NuGetAudit>false</NuGetAudit> is required; the audit writes to "
            "~/.local/share/NuGet/http-cache, and TreatWarningsAsErrors turns the sandbox "
            "denial into error NU1900",
        )
    return len(SDK_PROJECTS)


def check_driver_uses_sources(report: Report, root: Path) -> None:
    """2. The driver still compiles the sources, so nothing references the built assembly."""
    path = root / DRIVER_PROJECT
    if not report.check(path.is_file(), f"required project not found: {DRIVER_PROJECT}"):
        return
    text = read(path)
    report.check(
        DRIVER_GLOB_RE.search(text) is not None,
        f"{DRIVER_PROJECT}: the <Compile Include=\"..\\DomeControl.Protocol\\**\\*.cs\" /> glob "
        "is gone; the driver must build from the sources, not from a net8.0 assembly",
    )
    for match in DRIVER_REFERENCE_RE.finditer(text):
        report.check(
            False,
            f"{DRIVER_PROJECT}:{line_of(text, match.start())}: a ProjectReference would make a "
            "net8.0 assembly a build input of a net472 project",
        )


def check_language_level(report: Report, root: Path) -> None:
    """3. C# 7.3, the level the old-style driver project compiles."""
    path = root / LIBRARY_PROJECT
    if not report.check(path.is_file(), f"required project not found: {LIBRARY_PROJECT}"):
        return
    versions = LANG_VERSION_RE.findall(read(path))
    if report.check(
        len(versions) == 1,
        f"{LIBRARY_PROJECT}: expected exactly one <LangVersion>, found {len(versions)}",
    ):
        report.check(
            versions[0] == DRIVER_LANG_VERSION,
            f"{LIBRARY_PROJECT}: <LangVersion>{versions[0]}</LangVersion>, but the driver is an "
            f"old-style project and needs {DRIVER_LANG_VERSION}",
        )


def check_net472_apis(report: Report, root: Path) -> int:
    """4. No net8.0-only API in the sources the driver compiles."""
    source_dir = root / "ascom/DomeControl.Protocol"
    # The driver globs the same tree, so mirror it: bin/ and obj/ hold build output, and the
    # AssemblyInfo.cs the SDK generates there is not a source anyone edits.
    sources = sorted(
        p
        for p in source_dir.rglob("*.cs")
        if p.is_file() and not {"bin", "obj"} & set(p.relative_to(source_dir).parts)
    )
    report.check(bool(sources), f"no C# sources found under {source_dir}")
    for path in sources:
        text = read(path)
        relative = path.relative_to(root)
        for line_number, line in enumerate(text.splitlines(), start=1):
            if CODE_LINE_RE.match(line):
                continue
            for pattern, advice in FORBIDDEN_APIS:
                match = pattern.search(line)
                if match is not None:
                    report.check(
                        False,
                        f"{relative}:{line_number}: '{match.group(0).strip()}' -- {advice}",
                    )
    return len(sources)


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
    report = Report()

    projects = check_offline_restore(report, root)
    check_driver_uses_sources(report, root)
    check_language_level(report, root)
    sources = check_net472_apis(report, root)

    print(f"csharp compat: {projects} SDK projects, {sources} protocol sources, "
          f"{len(FORBIDDEN_APIS)} guarded APIs")
    return report.finish()


if __name__ == "__main__":
    sys.exit(main())