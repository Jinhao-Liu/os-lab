#!/usr/bin/env python3
"""Inject Lab2 grading tests into a temporary student-tree copy.

Tests are inserted at function boundaries:
  * pmm_init: function body (after specific function calls)

The injector operates only on the temporary grading copy; the student's
original sources are never modified.
"""

import os
import re
import shutil
import sys


def die(message):
    print("[lab2 inject] ERROR: " + message)
    raise SystemExit(1)


if len(sys.argv) != 2:
    die("Usage: inject.py <temporary grading copy>")

root = os.path.abspath(sys.argv[1])
tools = os.path.dirname(os.path.abspath(__file__))


def read_text(path):
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


def write_text(path, text):
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)


def find_function(text, name):
    """Return (declaration_start, opening_brace, closing_brace_end).

    The brace scan ignores comments, strings, and character literals, so the
    matching brace remains correct in functions containing nested blocks.
    """
    pattern = re.compile(
        r"(?ms)^[ \t]*"
        r"(?:static[ \t]+)?"
        r"[A-Za-z_][A-Za-z0-9_ \t\n\*]*?"
        r"\b" + re.escape(name) +
        r"[ \t\n]*\([^;{}]*\)[ \t\n]*\{"
    )
    matches = list(pattern.finditer(text))
    if len(matches) != 1:
        die("Function location is missing or ambiguous: " + name)

    match = matches[0]
    body_start = text.find("{", match.start(), match.end())
    depth = 0
    in_line_comment = False
    in_block_comment = False
    in_string = False
    in_char = False
    escaped = False

    for i in range(body_start, len(text)):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""

        if in_line_comment:
            if ch == "\n":
                in_line_comment = False
        elif in_block_comment:
            if ch == "*" and nxt == "/":
                in_block_comment = False
        elif in_string:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == '"':
                in_string = False
        elif in_char:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == "'":
                in_char = False
        else:
            if ch == "/" and nxt == "/":
                in_line_comment = True
            elif ch == "/" and nxt == "*":
                in_block_comment = True
            elif ch == '"':
                in_string = True
            elif ch == "'":
                in_char = True
            elif ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    return match.start(), body_start, i + 1

    die("Function is missing its matching closing brace: " + name)


def insert_include(path, header, anchor):
    text = read_text(path)
    pattern = re.compile(
        r"(?m)^(?P<line>[ \t]*#include[ \t]*[<\"]"
        + re.escape(anchor)
        + r"[>\"][ \t]*)$"
    )
    matches = list(pattern.finditer(text))
    if len(matches) != 1:
        die("Include anchor is missing or ambiguous: " + anchor)
    match = matches[0]
    replacement = match.group("line") + "\n#include <" + header + ">"
    write_text(path, text[:match.start()] + replacement + text[match.end():])


def indent_code(code):
    return "\n" + "\n".join("    " + line for line in code.splitlines()) + "\n"


def insert_at_function_end(path, name, code):
    text = read_text(path)
    _, _, body_end = find_function(text, name)
    insertion = indent_code(code)
    write_text(path, text[:body_end - 1] + insertion + text[body_end - 1:])


makefile = os.path.join(root, "Makefile")
pmm = os.path.join(root, "kern", "mm", "pmm.c")
testdir = os.path.join(root, "kern", "test")

# Pre-condition: verify the skeleton is clean
make_text = read_text(makefile)
if "kern/test" in make_text:
    die("Makefile already contains kern/test; the skeleton is polluted")
pmm_text = read_text(pmm)
if "#include <test_lab2.h>" in pmm_text:
    die("pmm.c already includes test_lab2.h; the skeleton was injected")

# Copy test sources into kern/test/
os.makedirs(testdir, exist_ok=True)
for name in ("test_lab2.c", "test_lab2.h"):
    src = os.path.join(tools, name)
    if not os.path.exists(src):
        die("tools/{} not found".format(name))
    shutil.copyfile(src, os.path.join(testdir, name))

# Add kern/test to Makefile
for variable in ("KINCLUDE", "KSRCDIR"):
    match = re.search(
        r"(?m)^" + variable + r"[ \t]+\+=.*(?:\n[ \t]+[^\n]*)*",
        make_text,
    )
    if match is None:
        die("Makefile variable not found: " + variable)
    make_text = make_text[:match.end()] + "\n" + variable + "\t+= kern/test" + make_text[match.end():]
write_text(makefile, make_text)

# Add #include <test_lab2.h> to pmm.c
insert_include(pmm, "test_lab2.h", "pmm.h")

# Inject tests at the end of pmm_init
insert_at_function_end(pmm, "pmm_init", "test_page_init();\ntest_best_fit();")

print("lab2 injection complete")
