#!/usr/bin/env python3
"""Exact ESP32-S3 ELF audit for the GPIO edge ISR's IRAM/DRAM residency.

Run inside the ESP-IDF toolchain container after the exact-head build:
  python3 tests/check_input_edge_isr_iram.py build/opentail_core_alpha.elf

This is a link/disassembly gate, not a substitute for physical ISR stress tests.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: check_input_edge_isr_iram.py path/to/opentail_core_alpha.elf")
elf = Path(sys.argv[1])
if not elf.is_file() or elf.stat().st_size == 0:
    raise SystemExit(f"missing or empty ELF: {elf}")

objdump = (shutil.which("xtensa-esp-elf-objdump")
           or shutil.which("xtensa-esp32s3-elf-objdump"))
if not objdump:
    raise SystemExit("ESP32-S3 Xtensa objdump is not available in PATH")

def run(*args):
    return subprocess.run([objdump, *args, str(elf)], check=True,
                          text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE).stdout

symbol_table = run("-t")
symbols = {}
for line in symbol_table.splitlines():
    fields = line.split()
    if len(fields) >= 6:
        section, name = fields[-3], fields[-1]
        if name and not name.startswith("."):
            symbols[name] = section

def require_section(name, prefixes):
    section = symbols.get(name)
    if section is None:
        raise SystemExit(f"missing linked symbol: {name}")
    if not any(section.startswith(prefix) for prefix in prefixes):
        raise SystemExit(f"{name} is in {section}, expected one of {prefixes}")
    print(f"PASS section {name}: {section}")

# The ISR's instruction stream must execute from instruction RAM. Every mutable
# value it touches directly must reside in data RAM, not flash-backed rodata.
require_section("input_edge_isr", (".iram0.text", ".iram1.text"))
dram_prefixes = (".dram0.", ".iram0.data", ".rtc.data", ".rtc.bss")
for name in (
    "input_pin_isr",
    "input_edge_epoch",
    "turn_edge_events",
    "turn_edge_head",
    "turn_edge_tail",
    "turn_edge_overflow",
    "turn_edge_writer_active",
    "input_raw_edges",
    "input_asserted_edges",
    "input_deasserted_edges",
    "input_raw_level_mask",
    "turn_edge_high_water",
    "turn_edge_overflow_count",
):
    require_section(name, dram_prefixes)

# Decode the actual linked ISR, not only its C attributes. All direct call
# targets must also be IRAM/RTC-resident. Indirect calls are rejected because
# their runtime target cannot be proven from this bounded static audit.
disassembly = run("-dr", "--disassemble=input_edge_isr")
if "<input_edge_isr>" not in disassembly:
    raise SystemExit("objdump did not emit input_edge_isr disassembly")
if re.search(r"\bcallx(?:0|4|8|12)?\b", disassembly):
    raise SystemExit("input_edge_isr contains an indirect call; target residency is unproven")

targets = re.findall(
    r"\bcall(?:0|4|8|12)\s+[^\n]*?<([^>]+)>", disassembly
)
for raw_target in targets:
    target = raw_target.split("+", 1)[0].strip()
    section = symbols.get(target)
    if section is None:
        raise SystemExit(f"ISR direct call target is not in the linked symbol table: {target}")
    if not section.startswith((".iram0.", ".iram1.", ".rtc.text")):
        raise SystemExit(f"ISR direct call reaches non-IRAM code: {target} in {section}")
    print(f"PASS ISR call target {target}: {section}")

# Preserve the exact linked evidence beside the ELF so independent review
# can inspect the full symbol table and actual ISR instruction stream.
evidence_dir = elf.parent / "isr-evidence"
evidence_dir.mkdir(parents=True, exist_ok=True)
version = subprocess.run([objdump, "--version"], check=True, text=True,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT).stdout
(evidence_dir / "input_edge_isr.symbols.txt").write_text(symbol_table)
(evidence_dir / "input_edge_isr.dis").write_text(disassembly)
audit_lines = [
    f"ELF: {elf}",
    f"GITHUB_SHA: {os.environ.get('GITHUB_SHA', 'not-provided')}",
    f"OBJDUMP: {objdump}",
    version.strip(),
    "PASS: input_edge_isr linked in IRAM",
    "PASS: ISR mutable data in DRAM",
    "PASS: direct call targets linked in IRAM",
    "CALL TARGETS:",
]
audit_lines.extend(f"  {target.split('+', 1)[0].strip()}: {symbols[target.split('+', 1)[0].strip()]}"
                   for target in targets)
(evidence_dir / "input_edge_isr.audit.txt").write_text("\\n".join(audit_lines) + "\\n")
print(f"Saved linked ISR evidence: {evidence_dir}")
print("PASS: input_edge_isr linked in IRAM; ISR data in DRAM; direct calls IRAM-safe")
