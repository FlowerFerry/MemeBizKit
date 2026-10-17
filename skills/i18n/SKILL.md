# i18n Translation System

Complete internationalization (i18n) solution for C++ projects: a header-only C++17
library plus Python tooling that drives the translation workflow.

## Components

### 1. C++ library - `include/mmbkpp/i18n/catalog.h`

Header-only C++17 runtime translation library.

Features:
- Two modes: `fast_catalog` (fully in memory) and `lite_catalog` (queries SQLite directly)
- Thread-safe lookups
- Plural forms with built-in families plus a gettext expression interpreter
- Contexts for disambiguating identical source strings
- Zero-copy returns for long translations (MemeString takeover)

Basic use:

```cpp
#include <mmbkpp/i18n/catalog.h>
using namespace mmbkpp::i18n;

// Lite mode: read straight from a SQLite file.
auto cat = lite_catalog::open("locales/translations.db");
cat->set_default_language(make_tag("zh-CN"));

cat->tr("Open");                              // "open" in Chinese
cat->tr_ctx("menu", "File");                  // context disambiguates
cat->tr_n("%d file", "%d files", 5);          // plural form selection
cat->tr_ctx_n("dlg", "%d item", "%d items", 3);
```

Fast mode (build once at startup, then read-only and lock-free):

```cpp
fast_catalog_builder b;
b.set_source_language(make_tag("en"));
b.set_plural_rule(make_tag("ru"), "@ru");           // optional explicit rule
b.add(make_tag("zh-CN"), "", "Open", "打开");
b.add_plural(make_tag("ru"), "", "%d file", { "%d файл", "%d файла", "%d файлов" });
mgpp::err e = b.load_from_sqlite("locales/translations.db");  // optional bulk load
auto fast = b.build();
```

Four translation interfaces (all have tag-less overloads that use the catalog's
default language):

| Function | Purpose |
|---|---|
| `tr(msgid)` | plain text |
| `tr_ctx(ctx, msgid)` | text with context |
| `tr_n(msgid, msgid_plural, n)` | plural |
| `tr_ctx_n(ctx, msgid, msgid_plural, n)` | plural with context |

Language tags are always built with `make_tag("zh-CN")`. Passing a bare string
literal where a tag is expected is a compile error by design, because the implicit
`simple_tag` constructor would silently truncate it.

### 2. Python tool - `tools/i18n/i18n_tool.py`

Drives the workflow: extract strings from sources, hand PO files to translators,
load the translations back into SQLite.

All four sub-commands, with English output:

```bash
# 1. Scan sources, write a PO template
python tools/i18n/i18n_tool.py extract --config tools/i18n/i18n.json

# 2. Verify a translated file
python tools/i18n/i18n_tool.py check --po locales/zh_CN.po

# 3. Load it into the database
python tools/i18n/i18n_tool.py import --po locales/zh_CN.po --db locales/translations.db

# 4. Inspect the database
python tools/i18n/i18n_tool.py stats --db locales/translations.db
```

CLI options worth knowing:

- `extract --sources DIR... --functions NAME...` override the config file
- `extract -o FILE` choose the output path
- `import --lang CODE` override the PO `Language` header
- `--config` points at the JSON configuration

## Configuration file

`tools/i18n/i18n.json`:

```json
{
  "source_lang": "en",
  "functions": ["tr", "tr_ctx", "tr_n", "tr_ctx_n"],
  "sources": ["include", "src"],
  "exclude": ["**/deps/**", "**/thirdparty/**", "**/*.moc", "**/build/**", "**/out/**"],
  "extensions": [".h", ".hpp", ".hxx", ".c", ".cc", ".cpp", ".cxx", ".inl"],
  "output": "locales/messages.pot",
  "po": { "project": "MemeBizKit", "charset": "UTF-8", "translator_comments": "zh" },
  "db": { "path": "locales/translations.db" }
}
```

Command line arguments override the config file; the config file overrides the
built-in defaults. So `functions` and `sources` can be changed per invocation
without editing anything.

## Database schema

Identical to what `catalog.h` expects, so the tool and the runtime agree:

```sql
CREATE TABLE i18n_language (
    lang         TEXT PRIMARY KEY,
    plural_rule  TEXT NOT NULL DEFAULT ''
) WITHOUT ROWID;

CREATE TABLE i18n_message (
    lang    TEXT    NOT NULL,
    ctx     TEXT    NOT NULL DEFAULT '',
    msgid   TEXT    NOT NULL,
    form    INTEGER NOT NULL DEFAULT 0,
    msgstr  TEXT    NOT NULL,
    PRIMARY KEY (lang, ctx, msgid, form)
) WITHOUT ROWID;
```

`i18n_message.lang` must be byte-identical to `i18n_language.lang`; the tool writes
the normalized tag (`zh-CN`) to both, and `catalog.h` preserves the raw string it
reads from the database when binding queries.

## What the extractor recognizes

Recognized (a real call: the name is followed by `(`, and the argument list holds
at least one string literal):

```cpp
tr("Open");
cat->tr("Save");
mmbkpp::i18n::tr("Close");
tr_ctx("menu", "File");
tr_n("%d file", "%d files", n);
tr_ctx_n("dialog", "%d item", "%d items", n);
cat->tr(make_tag("zh-CN"), "Tagged");            // explicit tag, msgid shifts right
tr("Hello " "World");                            // adjacent literals concatenate
tr(R"(raw string)");                             // raw string literals
```

Deliberately ignored:

- Anything inside `//`, `/* */`, or a string literal body
- Preprocessor lines (`#define`, `#include`, and their continuations)
- Declarations and definitions - detected because their parameter list carries no
  string literal, or because it carries `const` / `&` / `::`
- Bare identifiers not followed by `(`
- Members of other classes: the scan is name-based, so `obj.substr(...)` never
  matches because `substr` is not in the function list

Reported separately as "dynamic calls" (found but not extractable, because the
msgid is a runtime value):

```cpp
memepp::string_view id = "Dynamic";
tr(id);        // -> listed under "dynamic calls" with file:line
```

The extractor is lexical, not a full C++ parser. It understands comments, string
and character literals (including raw strings and `u8`/`u`/`U`/`L` prefixes),
identifiers, numbers and punctuation, and it matches calls on that token stream.
That is enough to avoid the classic false positives while staying fast.

## PO file conventions

The exported template carries standard gettext headers plus one extension:

```po
msgid ""
msgstr ""
"Project-Id-Version: MemeBizKit\n"
"Content-Type: text/plain; charset=UTF-8\n"
"Language: \n"
"Plural-Forms: \n"
"X-Source-Language: en\n"
```

- `Language` - target language, filled in by the translator (`zh_CN`, `ru`, ...)
- `Plural-Forms` - the gettext plural rule, stored verbatim into
  `i18n_language.plural_rule`, so `catalog.h` needs no separate rule source
- `X-Source-Language` - source language. PO has no standard field for this, so the
  tool writes an `X-` extension; standard editing tools preserve it untouched.

Entries carry source locations so translators can see where a string is used:

```po
#: include/app/mainwindow.h:42
msgctxt "menu"
msgid "File"
msgstr ""

#: include/app/document.h:128
msgid "%d file"
msgid_plural "%d files"
msgstr[0] ""
msgstr[1] ""
```

Empty `msgstr` means untranslated; `import` skips those entries rather than writing
empty rows.

## Full workflow

```bash
# 1. Extract
python tools/i18n/i18n_tool.py extract --config tools/i18n/i18n.json

# 2. Hand the .pot to translators; they produce per-language .po files
cp locales/messages.pot locales/zh_CN.po
#    translator fills in msgstr / msgstr[N], sets Language and Plural-Forms

# 3. Import each language
python tools/i18n/i18n_tool.py import --po locales/zh_CN.po --db locales/translations.db

# 4. Check what is still missing
python tools/i18n/i18n_tool.py check --po locales/zh_CN.po

# 5. Look at the database
python tools/i18n/i18n_tool.py stats --db locales/translations.db
```

## Troubleshooting

`error: no files found to scan`
: `sources` points at nothing, or `exclude` filters everything out. Run with
  `--sources <dir>` to test a single directory.

`dynamic calls (not extracted)`
: The tool found the call but the msgid is a runtime value. Replace the variable
  with a literal, or keep it and accept that it needs manual translation.

`error: cannot determine target language`
: The PO file has an empty `Language` header. Fill it in, or pass `--lang zh-CN`.

`WARN: N untranslated entry(ies)`
: `msgstr` (or some `msgstr[N]`) is still empty. That is expected while translation
  is in progress; `import` skips those entries.

Import writes with `INSERT OR REPLACE`, so re-importing an updated file is safe and
idempotent.

## Performance notes

The extractor pre-filters: if a file contains none of the target function names as
a substring, the whole lexical pass is skipped. Files that do contain them are read
once into memory and tokenized with `bytes.find()` jumps over comments and literals,
so the per-character loop only runs over short spans.

## Requirements

- C++ library: C++17, plus SQLite3 headers when `lite_catalog` is enabled
  (default; set `MMBKPP_I18N_ENABLE_SQLITE` to 0 to compile the library without SQLite)
- Python tool: Python 3.7+, standard library only (no pip installs)

## Layout

```
include/mmbkpp/i18n/catalog.h      C++ header-only library
tools/i18n/i18n_tool.py            CLI: extract / import / check / stats
tools/i18n/cpp_lexer.py            C++ lexer and call recognition
tools/i18n/po_file.py              PO reader / writer
tools/i18n/db_writer.py            SQLite writer plus plural-rule guessing
tools/i18n/i18n.json               configuration
tools/i18n/examples/               fixture source + sample PO/db
```

## Licensing

Part of the MemeBizKit project; see the repository license.
