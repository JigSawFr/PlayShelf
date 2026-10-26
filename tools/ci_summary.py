#!/usr/bin/env python3
"""The job summary of the clang-tidy workflow (.github/workflows/clang-tidy.yml),
as Markdown on stdout, appended to $GITHUB_STEP_SUMMARY.

  tidy-files the files clang-tidy checks: the changed sources the desktop
             build compiles, on stdout
  tidy       the findings of that run, by check, from run-clang-tidy's output

Standard library only; `--help` on a command lists its options.
"""
import argparse
import collections
import json
import os
import re
import sys

OK, FAIL, SKIP, WARN = "✅", "❌", "⏭️", "⚠️"


# ---------------------------------------------------------------- formatting

def cell(s):
    return str(s).replace("|", "\\|").replace("\n", " ")


def table(headers, rows, align=None):
    """A GitHub table. align: one letter per column, l / r / c (default l)."""
    align = align or "l" * len(headers)
    sep = {"l": "---", "r": "---:", "c": ":---:"}
    lines = ["| " + " | ".join(cell(h) for h in headers) + " |",
             "| " + " | ".join(sep[a] for a in align) + " |"]
    for r in rows:
        lines.append("| " + " | ".join(cell(c) for c in r) + " |")
    return "\n".join(lines)


def details(summary, body, open_=False):
    return (f"<details{' open' if open_ else ''}><summary>{summary}</summary>\n\n{body}\n\n</details>")


def code(s):
    return "`" + str(s).replace("`", "'") + "`"


def read_text(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            return f.read()
    except OSError:
        return None


def read_json(path):
    text = read_text(path) if path else None
    if text is None:
        return None
    try:
        return json.loads(text)
    except ValueError:
        return None



# ---------------------------------------------------------------- clang-tidy

SOURCE_EXT = (".c", ".cpp")
HEADER_EXT = {".h": ".c", ".hpp": ".cpp"}


def cmd_tidy_files(a):
    """The files run-clang-tidy is given: of the changed files (one a line),
    the sources under source/ and the source file of a changed header (the
    same stem, .c for .h, .cpp for .hpp), those the compile commands hold,
    as they name them (an absolute path, which run-clang-tidy matches)."""
    db = read_json(a.compile_commands) or []
    compiled = {os.path.normpath(e["file"]) for e in db if "file" in e}
    wanted = set()
    for line in (read_text(a.changed) or "").splitlines():
        path = line.strip()
        if not path.startswith("source/"):
            continue
        stem, ext = os.path.splitext(path)
        if ext in SOURCE_EXT:
            wanted.add(path)
        elif ext in HEADER_EXT:
            for src_ext in (HEADER_EXT[ext],) + SOURCE_EXT:
                if os.path.exists(stem + src_ext):
                    wanted.add(stem + src_ext)
                    break
    return "\n".join(sorted(f for f in compiled if os.path.relpath(f) in wanted))


FINDING = re.compile(r"^(?P<file>[^\s:][^:]*?):(?P<line>\d+):(?P<col>\d+): (?P<kind>warning|error): (?P<text>.*?)"
                     r"(?: \[(?P<checks>[\w.,-]+)\])?$")


def cmd_tidy(a):
    """run-clang-tidy's output as a report: the findings by check, each
    once (a header's is printed once per file that includes it), and the
    list of them, folded."""
    log = read_text(a.log) or ""
    root = os.getcwd() + os.sep
    findings, errors = {}, []
    for line in log.splitlines():
        m = FINDING.match(line)
        if not m:
            continue
        path = m["file"]
        if path.startswith(root):
            path = path[len(root):]
        if "extern/" in path or "library/" in path:   # borealis' own: the matcher leaves them out too
            continue
        checks = m["checks"] or ""
        key = (path, int(m["line"]), int(m["col"]), checks, m["text"])
        if m["kind"] == "error" and not checks:
            errors.append(key)   # clang-tidy could not read the file (a compile error)
        else:
            findings[key] = checks.split(",")[0] if checks else "clang-diagnostic"
    checked = [l for l in (read_text(a.files) or "").splitlines() if l.strip()]
    changed = [l.strip() for l in (read_text(a.changed) or "").splitlines() if l.strip().startswith("source/")]
    left_out = sorted(f for f in changed if os.path.splitext(f)[1] in SOURCE_EXT
                      and not any(c.endswith(os.sep + f) or c == f for c in checked))
    by_check = collections.Counter(findings.values())
    files_hit = len({k[0] for k in findings})
    n = len(findings)
    if errors:
        verdict = f"{FAIL} clang-tidy could not read {len({e[0] for e in errors})} file(s)"
    elif not checked:
        verdict = f"{SKIP} nothing to check"
    elif n:
        verdict = (f"{WARN} {n} finding{'s' if n != 1 else ''} in {files_hit} file{'s' if files_hit != 1 else ''}, "
                   f"{len(checked)} checked")
    else:
        verdict = f"{OK} no finding in {len(checked)} file{'s' if len(checked) != 1 else ''}"
    what = "bugprone, cert, performance" + (" and the static analyzer" if a.analyzer == "1" else "")
    heading = f"## 🧹 clang-tidy — {verdict} · {what}" + (f" · clang-tidy {a.version}" if a.version else "")
    out = [heading, ""]
    if left_out:
        out.append(f"{SKIP} Changed but not compiled by the desktop build (the Switch's alone), not checked: "
                   + ", ".join(code(f) for f in left_out))
        out.append("")
    if errors:
        out.append(table(["File", "Error"], [[code(f"{e[0]}:{e[1]}"), e[4][:200]] for e in errors[:20]]))
        out.append("")
    if n:
        rows = [[c, code(name), len({k[0] for k, v in findings.items() if v == name})]
                for name, c in by_check.most_common()]
        out.append(table(["Findings", "Check", "Files"], rows, "rlr"))
        out.append("")
        lines = [f"{k[0]}:{k[1]}:{k[2]}: {k[4]} [{k[3]}]" for k in sorted(findings)]
        shown = lines[: a.max_lines]
        body = "```\n" + "\n".join(shown) + "\n```"
        if len(lines) > len(shown):
            body += f"\n\n… and {len(lines) - len(shown)} more: the `clang-tidy-<run>` artifact has them all."
        out.append(details(f"Every finding ({n})", body))
        out.append("")
    out.append("A finding is a claim to check, not a failure: this job never fails on one. "
               "The checks and why each left-out one is: `.clang-tidy`.")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0], formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="command", required=True)

    p = sub.add_parser("tidy-files", help="the files clang-tidy checks: the changed sources the desktop build compiles")
    p.add_argument("changed", help="the changed files, one a line (a pull request's, or every file of source/)")
    p.add_argument("compile_commands", help="build-tidy/compile_commands.json")
    p.set_defaults(func=cmd_tidy_files)

    p = sub.add_parser("tidy", help="the findings of run-clang-tidy, by check")
    p.add_argument("log", help="run-clang-tidy's output")
    p.add_argument("--files", default="files.txt", help="the files it was given")
    p.add_argument("--changed", default="changed.txt", help="the changed files it was chosen from")
    p.add_argument("--analyzer", default="0", help="1 when the static analyzer's checks ran")
    p.add_argument("--version", default="")
    p.add_argument("--max-lines", type=int, default=400, help="findings listed in the folded block")
    p.set_defaults(func=cmd_tidy)

    a = ap.parse_args()
    print(a.func(a))
    return 0


if __name__ == "__main__":
    sys.exit(main())
