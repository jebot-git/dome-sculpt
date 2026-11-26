# SPDX-License-Identifier: GPL-3.0-or-later
"""Identify ELF mappings belonging to the actual native test executable."""
import os
from pathlib import Path


def elf_mappings(pid, executable):
    proc = Path("/proc") / str(pid)
    expected = Path(executable).stat()
    try:
        # A freshly forked child can still have the Python wrapper's address
        # space. Only sample maps bracketed by the expected native executable.
        if not os.path.samestat((proc / "exe").stat(), expected):
            return set()
        lines = (proc / "maps").read_text().splitlines()
        if not os.path.samestat((proc / "exe").stat(), expected):
            return set()
    except FileNotFoundError:
        return set()

    result = set()
    for line in lines:
        fields = line.split(None, 5)
        if len(fields) < 6 or not fields[5].startswith("/"):
            continue
        path = Path(fields[5])
        try:
            with path.open("rb") as stream:
                if stream.read(4) == b"\x7fELF":
                    result.add(str(path.resolve()))
        except FileNotFoundError:
            continue
    return result if str(Path(executable).resolve()) in result else set()
