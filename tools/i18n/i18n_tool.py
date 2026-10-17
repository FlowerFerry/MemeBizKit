#!/usr/bin/env python3
"""i18n translation tool: extract translatable strings from C++ sources,
export them as PO files, and import translated PO files into SQLite databases.

Usage:
    i18n_tool.py extract [--config CONFIG] [-o OUTPUT] [--sources DIR...] [--functions FUNC...]
    i18n_tool.py import --po FILE --db FILE [--lang CODE]
    i18n_tool.py check --po FILE [--pot FILE]
    i18n_tool.py stats --db FILE [--lang CODE]
"""

import argparse
import json
import os
import sys
from pathlib import Path
from datetime import datetime

# Add current directory to path for module imports
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from cpp_lexer import scan_file, ExtractStats
from po_file import PoEntry, read_po, write_po, get_header
from db_writer import write_entries, read_entries, normalize_lang, guess_plural_rule


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

DEFAULT_CONFIG = {
    "source_lang": "en",
    "functions": ["tr", "tr_ctx", "tr_n", "tr_ctx_n"],
    "sources": ["."],
    "exclude": ["**/deps/**", "**/thirdparty/**", "**/*.moc", "**/build/**"],
    "extensions": [".h", ".hpp", ".hxx", ".c", ".cc", ".cpp", ".cxx", ".inl"],
    "po": {
        "project": "Project",
        "charset": "UTF-8",
        "translator_comments": "zh"
    },
    "db": {
        "path": "translations.db"
    }
}


def load_config(config_path):
    """Load configuration file and merge with defaults."""
    config = DEFAULT_CONFIG.copy()
    if config_path and os.path.exists(config_path):
        with open(config_path, "r", encoding="utf-8") as f:
            user_config = json.load(f)
            # Deep merge
            for key, value in user_config.items():
                if key in config and isinstance(config[key], dict) and isinstance(value, dict):
                    config[key].update(value)
                else:
                    config[key] = value
    return config


def collect_files(config, base_dir="."):
    """Collect source files to scan."""
    import fnmatch

    sources = config.get("sources", ["."])
    exclude = config.get("exclude", [])
    extensions = set(config.get("extensions", [".h", ".hpp", ".c", ".cpp"]))

    files = []
    for src in sources:
        src_path = os.path.join(base_dir, src)
        if os.path.isfile(src_path):
            files.append(src_path)
        elif os.path.isdir(src_path):
            for root, dirs, filenames in os.walk(src_path):
                # Apply exclusion rules
                rel_root = os.path.relpath(root, base_dir)
                skip = False
                for pattern in exclude:
                    if fnmatch.fnmatch(rel_root, pattern):
                        skip = True
                        break
                if skip:
                    continue

                for fname in filenames:
                    ext = os.path.splitext(fname)[1].lower()
                    if ext in extensions:
                        full_path = os.path.join(root, fname)
                        files.append(full_path)

    return files


# ---------------------------------------------------------------------------
# extract command
# ---------------------------------------------------------------------------

def cmd_extract(args):
    """Scan source files and generate .pot template."""
    config = load_config(args.config)

    # Command line arguments override config
    if args.sources:
        config["sources"] = args.sources
    if args.functions:
        config["functions"] = args.functions
    if args.output:
        output_path = args.output
    else:
        output_path = config.get("output", "messages.pot")

    base_dir = args.base_dir or "."
    files = collect_files(config, base_dir)

    if not files:
        print("Error: no files found to scan", file=sys.stderr)
        return 1

    print(f"Scanning {len(files)} file(s)...")

    entries = {}  # key -> Entry
    stats = ExtractStats()

    for fpath in files:
        relpath = os.path.relpath(fpath, base_dir).replace("\\", "/")
        funcs = set(config["functions"])
        scan_file(fpath, relpath, funcs, entries, stats)

    # Generate PO entries
    po_entries = []
    for entry in entries.values():
        ctx = entry.ctx
        msgid = entry.msgid.decode("utf-8") if isinstance(entry.msgid, bytes) else entry.msgid
        msgid_plural = entry.msgid_plural.decode("utf-8") if entry.msgid_plural and isinstance(entry.msgid_plural, bytes) else entry.msgid_plural

        msgstr_plural = {}
        if msgid_plural:
            # Plural forms: default 2 (English rule)
            msgstr_plural = {0: "", 1: ""}

        po_entries.append(PoEntry(
            ctx=ctx,
            msgid=msgid,
            msgid_plural=msgid_plural,
            msgstr_plural=msgstr_plural,
            locations=entry.refs
        ))

    # Write PO file
    project = config.get("po", {}).get("project", "Project")
    write_po(output_path, po_entries,
             source_lang=config["source_lang"],
             target_lang="",
             project=project)

    print(f"OK: extracted {len(po_entries)} translatable string(s)")
    print(f"  - files scanned: {stats.files_scanned}")
    print(f"  - files skipped: {stats.files_skipped}")
    print(f"  - calls found: {stats.calls}")
    if stats.dynamic:
        print(f"  - dynamic calls (not extracted): {len(stats.dynamic)}")
    print(f"  - output: {output_path}")

    return 0


# ---------------------------------------------------------------------------
# import command
# ---------------------------------------------------------------------------

def cmd_import(args):
    """Read PO file and write to SQLite database."""
    if not os.path.exists(args.po):
        print(f"Error: PO file not found: {args.po}", file=sys.stderr)
        return 1

    po_entries = read_po(args.po)
    if not po_entries:
        print("Error: PO file is empty", file=sys.stderr)
        return 1

    # Get language from PO header
    header = get_header(po_entries)
    lang = args.lang or header.get("Language", "")
    if not lang:
        print("Error: cannot determine target language (PO file missing Language field, or use --lang)", file=sys.stderr)
        return 1

    lang = normalize_lang(lang)

    # Get plural rule from PO header
    plural_rule = header.get("Plural-Forms", "")
    if not plural_rule:
        plural_rule = guess_plural_rule(lang)

    # Convert PO entries to database format
    db_entries = []
    for e in po_entries:
        if e.is_header:
            continue
        if not e.msgid:
            continue

        forms = {}
        if e.is_plural:
            forms = e.msgstr_plural
        else:
            forms = {0: e.msgstr}

        db_entries.append((e.ctx, e.msgid, forms))

    # Write to database
    db_path = args.db
    written, skipped = write_entries(db_path, lang, db_entries, plural_rule, verbose=args.verbose)

    print(f"OK: imported {lang}")
    print(f"  - entries written: {written}")
    print(f"  - entries skipped: {skipped}")
    print(f"  - database: {db_path}")

    return 0


# ---------------------------------------------------------------------------
# check command
# ---------------------------------------------------------------------------

def cmd_check(args):
    """Check translation completeness of PO file."""
    if not os.path.exists(args.po):
        print(f"Error: PO file not found: {args.po}", file=sys.stderr)
        return 1

    po_entries = read_po(args.po)
    if not po_entries:
        print("Error: PO file is empty", file=sys.stderr)
        return 1

    header = get_header(po_entries)
    lang = header.get("Language", "unknown")

    total = 0
    translated = 0
    untranslated = 0
    fuzzy = 0

    for e in po_entries:
        if e.is_header:
            continue
        total += 1

        if e.is_plural:
            # Plural forms: all msgstr[N] must have values
            if e.msgstr_plural and all(v for v in e.msgstr_plural.values()):
                translated += 1
            else:
                untranslated += 1
        else:
            if e.msgstr:
                translated += 1
            else:
                untranslated += 1

        # Check fuzzy flag
        if "fuzzy" in e.comment:
            fuzzy += 1

    print(f"Translation check: {lang}")
    print(f"  - total entries: {total}")
    print(f"  - translated: {translated}")
    print(f"  - untranslated: {untranslated}")
    if fuzzy:
        print(f"  - fuzzy: {fuzzy}")

    if untranslated > 0:
        print(f"\nWarning: {untranslated} untranslated entry(ies)")
        return 1
    else:
        print("\nOK: translation is complete")
        return 0


# ---------------------------------------------------------------------------
# stats command
# ---------------------------------------------------------------------------

def cmd_stats(args):
    """Display database statistics."""
    if not os.path.exists(args.db):
        print(f"Error: database not found: {args.db}", file=sys.stderr)
        return 1

    import sqlite3
    conn = sqlite3.connect(args.db)

    # Query language list
    langs = conn.execute("SELECT lang FROM i18n_language ORDER BY lang").fetchall()

    if not langs:
        print("Database is empty")
        conn.close()
        return 0

    print(f"Database: {args.db}")
    print(f"Languages: {len(langs)}\n")

    for (lang,) in langs:
        if args.lang and lang != args.lang:
            continue

        # Query entry count for this language
        count = conn.execute(
            "SELECT COUNT(*) FROM (SELECT DISTINCT ctx, msgid FROM i18n_message WHERE lang = ?)",
            (lang,)
        ).fetchone()[0]

        # Query plural rule
        rule = conn.execute(
            "SELECT plural_rule FROM i18n_language WHERE lang = ?",
            (lang,)
        ).fetchone()
        rule = rule[0] if rule else ""

        print(f"  {lang}: {count} translation(s)")
        if rule:
            print(f"    plural rule: {rule[:60]}...")

    conn.close()
    return 0


# ---------------------------------------------------------------------------
# Main program
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="i18n translation tool: extract translatable strings from C++ sources, export as PO files, import into SQLite databases"
    )
    subparsers = parser.add_subparsers(dest="command", help="sub-command")

    # extract
    p_extract = subparsers.add_parser("extract", help="scan source files and generate .pot template")
    p_extract.add_argument("--config", help="configuration file path (JSON)")
    p_extract.add_argument("-o", "--output", help="output file path (default: messages.pot)")
    p_extract.add_argument("--sources", nargs="+", help="source file directories (override config)")
    p_extract.add_argument("--functions", nargs="+", help="translation function names (override config)")
    p_extract.add_argument("--base-dir", help="base directory (for computing relative paths)")

    # import
    p_import = subparsers.add_parser("import", help="read PO file and write to SQLite database")
    p_import.add_argument("--po", required=True, help="PO file path")
    p_import.add_argument("--db", required=True, help="database path")
    p_import.add_argument("--lang", help="target language (override Language field in PO file)")
    p_import.add_argument("-v", "--verbose", action="store_true", help="verbose output")

    # check
    p_check = subparsers.add_parser("check", help="check translation completeness of PO file")
    p_check.add_argument("--po", required=True, help="PO file path")

    # stats
    p_stats = subparsers.add_parser("stats", help="display database statistics")
    p_stats.add_argument("--db", required=True, help="database path")
    p_stats.add_argument("--lang", help="show statistics for specified language only")

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        return 1

    if args.command == "extract":
        return cmd_extract(args)
    elif args.command == "import":
        return cmd_import(args)
    elif args.command == "check":
        return cmd_check(args)
    elif args.command == "stats":
        return cmd_stats(args)
    else:
        parser.print_help()
        return 1


if __name__ == "__main__":
    sys.exit(main())
