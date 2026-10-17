"""Write PO translations into a SQLite database (schema matches catalog.h)."""

import sqlite3
import os

SCHEMA = """
CREATE TABLE IF NOT EXISTS i18n_language (
    lang         TEXT PRIMARY KEY,
    plural_rule  TEXT NOT NULL DEFAULT ''
) WITHOUT ROWID;

CREATE TABLE IF NOT EXISTS i18n_message (
    lang    TEXT    NOT NULL,
    ctx     TEXT    NOT NULL DEFAULT '',
    msgid   TEXT    NOT NULL,
    form    INTEGER NOT NULL DEFAULT 0,
    msgstr  TEXT    NOT NULL,
    PRIMARY KEY (lang, ctx, msgid, form)
) WITHOUT ROWID;
"""

# Standard gettext plural rules (same families as catalog.h).
BUILTIN_RULES = {
    "one": "nplurals=1; plural=0;",
    "en": "nplurals=2; plural=n != 1;",
    "fr": "nplurals=2; plural=n > 1;",
    "ru": ("nplurals=3; plural=n%10==1 && n%100!=11 ? 0 : "
           "n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2;"),
    "pl": ("nplurals=3; plural=n==1 ? 0 : n%10>=2 && n%10<=4 && "
           "(n%100<10 || n%100>=20) ? 1 : 2;"),
    "cs": "nplurals=3; plural=(n==1) ? 0 : (n>=2 && n<=4) ? 1 : 2;",
    "lt": ("nplurals=3; plural=n%10==1 && n%100!=11 ? 0 : "
           "n%10>=2 && (n%100<10 || n%100>=20) ? 1 : 2;"),
    "lv": "nplurals=3; plural=n%10==1 && n%100!=11 ? 0 : n != 0 ? 1 : 2;",
    "ro": "nplurals=3; plural=n==1 ? 0 : (n==0 || (n%100 > 0 && n%100 < 20)) ? 1 : 2;",
    "sl": ("nplurals=4; plural=n%100==1 ? 0 : n%100==2 ? 1 : "
           "n%100==3 || n%100==4 ? 2 : 3;"),
    "ga": "nplurals=5; plural=n==1 ? 0 : n==2 ? 1 : n<7 ? 2 : n<11 ? 3 : 4;",
    "ar": ("nplurals=6; plural=n==0 ? 0 : n==1 ? 1 : n==2 ? 2 : "
           "n%100>=3 && n%100<=10 ? 3 : n%100>=11 ? 4 : 5;"),
}

# Language code -> built-in family name
_LANG_FAMILY = {
    "zh": "one", "ja": "one", "ko": "one", "vi": "one", "th": "one",
    "id": "one", "ms": "one", "lo": "one", "km": "one", "my": "one", "bo": "one",
    "fr": "fr", "oc": "fr", "ln": "fr", "fil": "fr", "ti": "fr",
    "ru": "ru", "uk": "ru", "be": "ru", "sr": "ru", "hr": "ru", "bs": "ru",
    "pl": "pl",
    "cs": "cs", "sk": "cs",
    "lt": "lt",
    "lv": "lv",
    "ro": "ro",
    "sl": "sl",
    "ga": "ga",
    "ar": "ar",
}


def normalize_lang(code):
    """Normalize a PO Language value (e.g. zh_CN) to BCP 47 style (zh-CN)."""
    return code.replace("_", "-")


def guess_plural_rule(lang):
    """Guess a plural rule text for the given language."""
    base = lang.split("-")[0].lower()
    if base == "pt" and lang.upper().endswith("-BR"):
        return BUILTIN_RULES["fr"]
    fam = _LANG_FAMILY.get(base, "en")
    return BUILTIN_RULES[fam]


def write_entries(db_path, lang, entries, plural_rule="", verbose=False):
    """Write translation entries into the database.

    entries: [(ctx, msgid, {form: msgstr})]
    Returns (written count, skipped count).
    """
    d = os.path.dirname(os.path.abspath(db_path))
    if d and not os.path.isdir(d):
        os.makedirs(d, exist_ok=True)

    conn = sqlite3.connect(db_path)
    try:
        conn.executescript(SCHEMA)

        if not plural_rule:
            plural_rule = guess_plural_rule(lang)

        conn.execute(
            "INSERT OR REPLACE INTO i18n_language (lang, plural_rule) VALUES (?, ?)",
            (lang, plural_rule))

        written = 0
        skipped = 0
        for ctx, msgid, forms in entries:
            # Every form is empty, so the entry is untranslated: skip it.
            non_empty = {k: v for k, v in forms.items() if v}
            if not non_empty:
                skipped += 1
                continue
            for form, text in sorted(non_empty.items()):
                conn.execute(
                    "INSERT OR REPLACE INTO i18n_message "
                    "(lang, ctx, msgid, form, msgstr) VALUES (?, ?, ?, ?, ?)",
                    (lang, ctx, msgid, form, text))
            written += 1

        conn.commit()
        return written, skipped
    finally:
        conn.close()


def read_entries(db_path, lang):
    """Read entries back from the database, for verification."""
    conn = sqlite3.connect(db_path)
    try:
        conn.executescript(SCHEMA)
        rows = conn.execute(
            "SELECT ctx, msgid, form, msgstr FROM i18n_message "
            "WHERE lang = ? ORDER BY ctx, msgid, form", (lang,)).fetchall()
        return rows
    finally:
        conn.close()
