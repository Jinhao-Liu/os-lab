#!/usr/bin/env python3
"""Inject Lab3 grading tests into a temporary student-tree copy.

Tests are inserted after specific statements in kern_init:
  * After idt_init() (second call): inject exception/context test
  * After clock_init(): inject timer_is_armed test  
  * After intr_enable(): inject wait_for_timer test

The injector operates only on the temporary grading copy; the student's
original sources are never modified.
"""

import os
import re
import shutil
import sys


def die(message):
    print("[lab3 inject] ERROR: " + message)
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


def insert_after_statement(path, function_name, statement_pattern, code, occurrence=-1):
    """Insert code after a specific statement within a function body.
    
    Args:
        path: File path
        function_name: Name of the function containing the statement
        statement_pattern: The statement to search for (e.g., "idt_init();")
        code: Code to insert (without trailing semicolon or newline)
        occurrence: Which occurrence to use (0-based, -1 for last, default -1)
    """
    text = read_text(path)
    _, body_start, body_end = find_function(text, function_name)
    
    # Search for the statement within the function body
    function_body = text[body_start:body_end]
    
    # Find all occurrences of the statement
    # Use a pattern that matches the statement possibly followed by a comment
    pattern = re.escape(statement_pattern) + r"(?:[ \t]*//[^\n]*)?"
    matches = list(re.finditer(pattern, function_body))
    
    if len(matches) == 0:
        die("Statement '{}' not found in {}()".format(statement_pattern, function_name))
    
    if occurrence == -1:
        occurrence = len(matches) - 1
    
    if occurrence >= len(matches):
        die("Occurrence {} of '{}' not found in {}() (only {} occurrences)".format(
            occurrence, statement_pattern, function_name, len(matches)))
    
    match = matches[occurrence]
    statement_end_in_body = match.end()
    
    # Find the end of the line
    end_of_line = function_body.find("\n", statement_end_in_body)
    if end_of_line == -1:
        end_of_line = len(function_body)
    
    # Calculate position in full text
    insert_pos = body_start + end_of_line + 1
    
    # Get the indentation of the statement line
    line_start = function_body.rfind("\n", 0, match.start())
    if line_start == -1:
        line_start = 0
    else:
        line_start += 1
    
    indent = ""
    for ch in function_body[line_start:match.start()]:
        if ch in " \t":
            indent += ch
        else:
            break
    
    # Insert the code with proper indentation
    insertion = indent + code + ";\n"
    write_text(path, text[:insert_pos] + insertion + text[insert_pos:])


makefile = os.path.join(root, "Makefile")
init_c = os.path.join(root, "kern", "init", "init.c")
testdir = os.path.join(root, "kern", "test")

# Pre-condition: verify the skeleton is clean
make_text = read_text(makefile)
init_text = read_text(init_c)
for marker in ("test_lab3.h", "test_lab3_exception_context", "test_lab3_wait_for_timer"):
    if marker in make_text or marker in init_text:
        die("The skeleton already contains injected Lab 3 test markers")
if os.path.exists(testdir):
    die("The skeleton already contains kern/test; it must be clean")

# Copy test sources into kern/test/
os.makedirs(testdir)
for filename in ("test_lab3.c", "test_lab3.h", "test_lab3_context.S"):
    src = os.path.join(tools, filename)
    if not os.path.exists(src):
        die("tools/{} not found".format(filename))
    shutil.copyfile(src, os.path.join(testdir, filename))

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

# Add #include <test_lab3.h> to init.c
insert_include(init_c, "test_lab3.h", "dtb.h")

# Inject exception/context test after the second idt_init()
insert_after_statement(init_c, "kern_init", "idt_init();", "test_lab3_exception_context()", occurrence=-1)

# Inject timer_is_armed test after clock_init()
insert_after_statement(init_c, "kern_init", "clock_init();", "test_lab3_timer_is_armed()")

# Inject wait_for_timer test after intr_enable()
insert_after_statement(init_c, "kern_init", "intr_enable();", "test_lab3_wait_for_timer()")

print("lab3 injection complete")
