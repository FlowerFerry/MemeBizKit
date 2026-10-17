"""Reading and writing GNU gettext PO files.

PO layout:
  - header: the msgid "" entry carries the metadata (Language, Plural-Forms, ...)
  - entry:  [msgctxt] msgid [msgid_plural] msgstr [msgstr[N]]
  - comment: #. translator comment, #: location, #, flag
  - string:  written as "..." with escapes (\\n, \\t, \\", ...)
"""

import re
from datetime import datetime


class PoEntry:
    """One PO entry."""
    __slots__ = ("ctx", "msgid", "msgid_plural", "msgstr", "msgstr_plural",
                 "comment", "locations")

    def __init__(self, ctx="", msgid="", msgid_plural=None, msgstr="",
                 msgstr_plural=None, comment="", locations=None):
        self.ctx = ctx
        self.msgid = msgid
        self.msgid_plural = msgid_plural
        self.msgstr = msgstr
        self.msgstr_plural = msgstr_plural or {}  # {0: "...", 1: "..."}
        self.comment = comment
        self.locations = locations or []  # [(file, line)]

    @property
    def is_plural(self):
        return self.msgid_plural is not None

    @property
    def is_header(self):
        return self.msgid == "" and self.msgstr != ""


def _escape(s):
    """Escape a string for PO output."""
    s = s.replace("\\", "\\\\")
    s = s.replace('"', '\\"')
    s = s.replace("\n", "\\n")
    s = s.replace("\t", "\\t")
    return s


def _unescape(s):
    """Undo the escaping of a PO string."""
    s = s.replace("\\n", "\n")
    s = s.replace("\\t", "\t")
    s = s.replace('\\"', '"')
    s = s.replace("\\\\", "\\")
    return s


def _format_string(s, indent=0):
    """Format a string the way PO wraps long values."""
    if not s:
        return '""'
    escaped = _escape(s)
    # Short strings stay on one line.
    if len(escaped) < 70 and "\n" not in escaped:
        return f'"{escaped}"'
    # Long strings are split across lines.
    lines = []
    for line in escaped.split("\\n"):
        if line:
            lines.append(f'{" " * indent}"{line}\\n"')
    if not lines:
        return '""'
    # The first line carries no trailing newline.
    lines[0] = lines[0].replace("\\n", "")
    return "\n".join(lines)


def _parse_string_at(lines, start, keyword_len):
    """Parse a `keyword "..."` string. Returns (content, index of the next line).

    start points at the line holding the keyword; keyword_len is the length of
    the keyword plus its separating space. Following lines that begin with a
    quote belong to the same logical string (PO multi-line form).
    """
    if start >= len(lines):
        return "", start

    first = lines[start].strip()
    body = first[keyword_len:]
    parts = []
    if body.startswith('"'):
        content = body[1:-1] if body.endswith('"') and len(body) >= 2 else body[1:]
        parts.append(_unescape(content))
    i = start + 1

    while i < len(lines):
        line = lines[i].strip()
        if not line.startswith('"'):
            break
        content = line[1:-1] if line.endswith('"') and len(line) >= 2 else line[1:]
        parts.append(_unescape(content))
        i += 1

    return "".join(parts), i


def read_po(path):
    """Read a PO file and return the list of PoEntry."""
    with open(path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    entries = []
    i = 0
    n = len(lines)

    while i < n:
        line = lines[i].strip()

        # Skip blank lines.
        if not line:
            i += 1
            continue

        # Collect the comment block that precedes the entry.
        comment_lines = []
        locations = []
        while i < n and lines[i].strip().startswith("#"):
            cline = lines[i].strip()
            if cline.startswith("#:"):
                # Source location comment (#:).
                locs = cline[2:].strip().split()
                for loc in locs:
                    if ":" in loc:
                        parts = loc.rsplit(":", 1)
                        if len(parts) == 2:
                            try:
                                locations.append((parts[0], int(parts[1])))
                            except ValueError:
                                pass
            elif cline.startswith("#."):
                comment_lines.append(cline[2:].strip())
            i += 1

        comment = "\n".join(comment_lines)

        # Parse msgctxt / msgid / msgid_plural / msgstr.
        ctx = ""
        msgid = ""
        msgid_plural = None
        msgstr = ""
        msgstr_plural = {}

        while i < n:
            line = lines[i].strip()
            if not line or line.startswith("#"):
                break

            if line.startswith("msgctxt "):
                ctx, i = _parse_string_at(lines, i, len("msgctxt "))
                continue

            if line.startswith("msgid_plural "):
                msgid_plural, i = _parse_string_at(lines, i, len("msgid_plural "))
                continue

            if line.startswith("msgid "):
                msgid, i = _parse_string_at(lines, i, len("msgid "))
                continue

            if line.startswith("msgstr "):
                msgstr, i = _parse_string_at(lines, i, len("msgstr "))
                continue

            # msgstr[N]
            m = re.match(r'msgstr\[(\d+)\]\s+"', line)
            if m:
                idx = int(m.group(1))
                val, i = _parse_string_at(lines, i, line.index('"'))
                msgstr_plural[idx] = val
                continue

            i += 1

        if msgid or msgstr:
            entries.append(PoEntry(
                ctx=ctx,
                msgid=msgid,
                msgid_plural=msgid_plural,
                msgstr=msgstr,
                msgstr_plural=msgstr_plural,
                comment=comment,
                locations=locations
            ))

    return entries


def write_po(path, entries, source_lang="en", target_lang="", project=""):
    """Write a PO file."""
    now = datetime.now().strftime("%Y-%m-%d %H:%M")

    with open(path, "w", encoding="utf-8") as f:
        # Header entry.
        f.write(f'# Translation file for {project}\n')
        f.write(f'# Copyright (C) {datetime.now().year}\n')
        f.write(f'#\n')
        f.write(f'msgid ""\n')
        f.write(f'msgstr ""\n')
        f.write(f'"Project-Id-Version: {project}\\n"\n')
        f.write(f'"Report-Msgid-Bugs-To: \\n"\n')
        f.write(f'"POT-Creation-Date: {now}\\n"\n')
        f.write(f'"PO-Revision-Date: {now}\\n"\n')
        f.write(f'"Last-Translator: \\n"\n')
        f.write(f'"Language-Team: {target_lang}\\n"\n')
        f.write(f'"Language: {target_lang}\\n"\n')
        f.write(f'"MIME-Version: 1.0\\n"\n')
        f.write(f'"Content-Type: text/plain; charset=UTF-8\\n"\n')
        f.write(f'"Content-Transfer-Encoding: 8bit\\n"\n')
        f.write(f'"X-Source-Language: {source_lang}\\n"\n')
        f.write(f'\n')

        # Entries.
        for e in entries:
            # Source location comment.
            if e.locations:
                locs = " ".join(f"{path}:{line}" for path, line in e.locations[:10])
                f.write(f"#: {locs}\n")

            # Translator comment.
            if e.comment:
                for cline in e.comment.split("\n"):
                    f.write(f"#. {cline}\n")

            # msgctxt
            if e.ctx:
                f.write(f'msgctxt "{_escape(e.ctx)}"\n')

            # msgid
            f.write(f'msgid "{_escape(e.msgid)}"\n')

            # msgid_plural
            if e.msgid_plural:
                f.write(f'msgid_plural "{_escape(e.msgid_plural)}"\n')
                # msgstr[N]
                for idx in sorted(e.msgstr_plural.keys()):
                    f.write(f'msgstr[{idx}] "{_escape(e.msgstr_plural[idx])}"\n')
            else:
                # msgstr
                f.write(f'msgstr "{_escape(e.msgstr)}"\n')

            f.write("\n")


def get_header(entries):
    """Extract the metadata dictionary from the header entry."""
    for e in entries:
        if e.is_header:
            meta = {}
            for line in e.msgstr.split("\n"):
                if ":" in line:
                    key, _, val = line.partition(":")
                    meta[key.strip()] = val.strip()
            return meta
    return {}
