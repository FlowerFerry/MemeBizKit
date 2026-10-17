"""Minimal C++ lexer that locates translation calls.

Lexical analysis only -- no full C++ parsing. The goals are:
  1. Skip comment and string bodies so that examples inside comments are not
     mistaken for real calls.
  2. Recognize the "function name followed by parentheses" call shape.
  3. Pull the string literal arguments out of those parentheses.

Performance: the main loop jumps in large blocks with bytes.find to skip
comments and string literals; the per-character loop only runs over short
fragments such as identifiers and punctuation.
"""

import re

# ---------------------------------------------------------------------------
# Token kinds
# ---------------------------------------------------------------------------
IDENT = "ident"
STRING = "str"
CHAR = "chr"
PUNCT = "punct"
NUMBER = "num"

_TOKEN_RE = re.compile(
    rb"[A-Za-z_][A-Za-z0-9_]*"      # identifier
)

_PUNCT3 = (b"<<=", b">>=", b"<=>", b"...")
_PUNCT2 = (b"->", b"::", b"==", b"!=", b"<=", b">=", b"&&", b"||",
           b"++", b"--", b"+=", b"-=", b"*=", b"/=", b"%=", b"&=",
           b"|=", b"^=", b"##", b".*")

_ESCAPES = {
    b"n": b"\n", b"t": b"\t", b"r": b"\r", b"0": b"\0",
    b"\\": b"\\", b"\"": b"\"", b"'": b"'", b"?": b"?",
    b"a": b"\a", b"b": b"\b", b"f": b"\f", b"v": b"\v",
}


class Token:
    __slots__ = ("kind", "raw", "value", "line", "col", "pos")

    def __init__(self, kind, raw, value, line, col, pos):
        self.kind = kind
        self.raw = raw          # raw bytes as they appear in the source
        self.value = value      # decoded text (literal contents for strings)
        self.line = line
        self.col = col
        self.pos = pos

    def is_punct(self, text):
        if self.kind != PUNCT:
            return False
        want = text if isinstance(text, bytes) else text.encode()
        return self.raw == want

    def __repr__(self):
        return "Token(%s,%r,%d:%d)" % (self.kind, self.raw[:20], self.line, self.col)


def _unescape(raw):
    """Decode the escape sequences inside a C literal (quotes excluded)."""
    out = bytearray()
    i = 0
    n = len(raw)
    while i < n:
        c = raw[i:i + 1]
        if c != b"\\":
            out += c
            i += 1
            continue
        i += 1
        if i >= n:
            out += b"\\"
            break
        e = raw[i:i + 1]
        if e in _ESCAPES:
            out += _ESCAPES[e]
            i += 1
        elif e == b"x":
            j = i + 1
            hexd = b""
            while j < n and len(hexd) < 2 and raw[j:j + 1] in b"0123456789abcdefABCDEF":
                hexd += raw[j:j + 1]
                j += 1
            if hexd:
                out.append(int(hexd, 16))
                i = j
            else:
                out += b"x"
                i += 1
        elif e in b"01234567":
            j = i
            octd = b""
            while j < n and len(octd) < 3 and raw[j:j + 1] in b"01234567":
                octd += raw[j:j + 1]
                j += 1
            out.append(int(octd, 8) & 0xFF)
            i = j
        else:
            out += e
            i += 1
    return bytes(out)


def tokenize(buf):
    """Split the source into tokens. Comments and preprocessor lines are dropped."""
    toks = []
    i = 0
    n = len(buf)
    line = 1

    while i < n:
        c = buf[i:i + 1]

        # ---- newline / whitespace ----
        if c == b"\n":
            line += 1
            i += 1
            continue
        if c in b" \t\r\f\v":
            i += 1
            continue

        # ---- line continuation: backslash + newline ----
        if c == b"\\" and buf[i + 1:i + 2] == b"\n":
            line += 1
            i += 2
            continue

        # ---- line comment ----
        if c == b"/" and buf[i + 1:i + 2] == b"/":
            j = buf.find(b"\n", i)
            i = n if j < 0 else j
            continue

        # ---- block comment ----
        if c == b"/" and buf[i + 1:i + 2] == b"*":
            j = buf.find(b"*/", i + 2)
            if j < 0:
                line += buf.count(b"\n", i, n)
                i = n
            else:
                line += buf.count(b"\n", i, j)
                i = j + 2
            continue

        # ---- preprocessor line (# to end of line, including continuations) ----
        if c == b"#":
            j = i
            while True:
                k = buf.find(b"\n", j)
                if k < 0:
                    j = n
                    break
                # is the line continued?
                m = k - 1
                if m >= 0 and buf[m:m + 1] == b"\\":
                    j = k + 1
                    continue
                j = k
                break
            line += buf.count(b"\n", i, j)
            i = j
            continue

        # ---- string literal (possibly with a u8/u/U/L prefix) ----
        if c == b'"':
            start_line, start_pos = line, i
            i += 1
            seg_start = i
            while i < n:
                b = buf[i:i + 1]
                if b == b"\\":
                    i += 2
                    continue
                if b == b'"':
                    break
                if b == b"\n":
                    line += 1
                i += 1
            raw_inner = buf[seg_start:i]
            i = min(i + 1, n)
            val = _unescape(raw_inner)
            toks.append(Token(STRING, b'"' + raw_inner + b'"', val, start_line, 0, start_pos))
            continue

        # ---- character literal ----
        if c == b"'":
            start_line, start_pos = line, i
            i += 1
            seg_start = i
            while i < n:
                b = buf[i:i + 1]
                if b == b"\\":
                    i += 2
                    continue
                if b == b"'":
                    break
                if b == b"\n":
                    line += 1
                i += 1
            raw_inner = buf[seg_start:i]
            i = min(i + 1, n)
            toks.append(Token(CHAR, b"'" + raw_inner + b"'", raw_inner, start_line, 0, start_pos))
            continue

        # ---- raw string R"delim(...)delim" ----
        if c in b"uULR" or c == b"R":
            # Handles u8R"..." / uR"..." / UR"..." / LR"..." / R"..."
            prefix_len = 0
            if buf[i:i + 3] == b"u8R":
                prefix_len = 2
            elif buf[i:i + 2] in (b"uR", b"UR", b"LR"):
                prefix_len = 1
            elif c == b"R" and buf[i + 1:i + 2] == b'"':
                prefix_len = 0
            else:
                prefix_len = -1
            if prefix_len >= 0:
                q = i + prefix_len
                if buf[q:q + 1] == b'"':
                    paren = buf.find(b"(", q + 1, q + 18)
                    if paren > 0:
                        delim = buf[q + 1:paren]
                        endmark = b")" + delim + b'"'
                        j = buf.find(endmark, paren + 1)
                        start_line, start_pos = line, i
                        if j < 0:
                            line += buf.count(b"\n", i, n)
                            i = n
                            content = buf[paren + 1:n]
                        else:
                            line += buf.count(b"\n", i, j)
                            content = buf[paren + 1:j]
                            i = j + len(endmark)
                        toks.append(Token(STRING, b"R", content, start_line, 0, start_pos))
                        continue

        # ---- identifier / keyword ----
        if c.isalpha() or c == b"_":
            m = _TOKEN_RE.match(buf, i)
            if m:
                start = i
                start_line = line
                i = m.end()
                # may be a u8/u/U/L prefixed string literal
                if i < n and buf[i:i + 1] == b'"' and buf[start:i] in (b"u8", b"u", b"U", b"L"):
                    continue  # let the next iteration treat it as a string (prefix dropped)
                raw = buf[start:i]
                toks.append(Token(IDENT, raw, raw.decode("utf-8", "surrogateescape"),
                                  start_line, 0, start))
                continue

        # ---- number ----
        if c.isdigit() or (c == b"." and buf[i + 1:i + 2].isdigit()):
            start = i
            start_line = line
            while i < n and (buf[i:i + 1].isalnum() or buf[i:i + 1] in b"._"):
                i += 1
            toks.append(Token(NUMBER, buf[start:i], buf[start:i], start_line, 0, start))
            continue

        # ---- punctuation ----
        for p in _PUNCT3:
            if buf.startswith(p, i):
                toks.append(Token(PUNCT, p, p, line, 0, i))
                i += len(p)
                break
        else:
            matched = False
            for p in _PUNCT2:
                if buf.startswith(p, i):
                    toks.append(Token(PUNCT, p, p, line, 0, i))
                    i += len(p)
                    matched = True
                    break
            if matched:
                continue
            toks.append(Token(PUNCT, c, c, line, 0, i))
            i += 1

    return toks


# ---------------------------------------------------------------------------
# Argument splitting
# ---------------------------------------------------------------------------

def _split_args(toks, open_idx):
    """toks[open_idx] is '('. Returns (list of argument token lists, index of ')')."""
    args = []
    cur = []
    depth = 0
    i = open_idx
    n = len(toks)
    while i < n:
        t = toks[i]
        if t.kind == PUNCT:
            r = t.raw
            if r in (b"(", b"[", b"{"):
                depth += 1
                if depth == 1:
                    i += 1
                    continue
            elif r in (b")", b"]", b"}"):
                depth -= 1
                if depth == 0:
                    args.append(cur)
                    return [a for a in args if a], i
            elif r == b"," and depth == 1:
                args.append(cur)
                cur = []
                i += 1
                continue
        cur.append(t)
        i += 1
    return None, -1


def _literal_of(seg):
    """Reduce one argument to its literal content; None if not a pure literal.

    Adjacent literals are concatenated: "a" "b"  -> "ab"
    """
    if not seg:
        return None
    out = bytearray()
    saw_str = False
    for t in seg:
        if t.kind == STRING:
            out += t.value
            saw_str = True
        else:
            return None
    return bytes(out) if saw_str else None


def _is_tag_arg(seg):
    """Return True when the argument looks like a locale tag (make_tag(...))."""
    if not seg:
        return False
    if seg[0].kind != IDENT:
        return False
    name = seg[0].value
    # make_tag("zh-CN") / T("zh-CN") / tag_t::parse_from(...)
    if len(seg) >= 2 and seg[1].is_punct(b"("):
        return True
    # A bare tag variable
    if name in ("tag_t",):
        return True
    return False


_DECL_HINTS = (b"&", b"*", b"::")


def _looks_like_declaration(seg):
    """Tell a declaration parameter from a run-time variable.

    A declaration parameter looks like `const memepp::string_view& _msgid` and
    carries const, references or qualified names; a run-time variable is usually
    a bare identifier such as `dynamic_id`.
    """
    for t in seg:
        if t.kind == PUNCT and t.raw in _DECL_HINTS:
            return True
        if t.kind == IDENT and t.value in ("const", "volatile", "struct", "class"):
            return True
    return False


# ---------------------------------------------------------------------------
# Extraction
# ---------------------------------------------------------------------------

class Entry:
    """One translatable entry."""
    __slots__ = ("ctx", "msgid", "msgid_plural", "refs")

    def __init__(self, ctx, msgid, msgid_plural):
        self.ctx = ctx                    # str (empty means no context)
        self.msgid = msgid                # bytes
        self.msgid_plural = msgid_plural  # bytes or None
        self.refs = []                    # [(relpath, line)]

    def key(self):
        return (self.ctx, self.msgid, self.msgid_plural)

    def add_ref(self, path, line):
        if (path, line) not in self.refs:
            self.refs.append((path, line))


class ExtractStats:
    def __init__(self):
        self.files_scanned = 0
        self.files_skipped = 0
        self.calls = 0
        self.dynamic = []          # [(relpath, line, snippet)] calls not statically extractable
        self.warnings = []


def _decode(b):
    try:
        return b.decode("utf-8")
    except UnicodeDecodeError:
        return b.decode("utf-8", "replace")


def extract_from_tokens(toks, relpath, funcs, entries, stats):
    """Find translation calls in a token stream."""
    n = len(toks)
    i = 0
    while i < n:
        t = toks[i]
        if t.kind != IDENT or t.value not in funcs:
            i += 1
            continue

        # The next token decides whether this is really a call site: it must be
        # "(". Declarations such as `int tr;` fail that test immediately, and a
        # declaration with a parameter list is caught by the "no literal"
        # filter below.
        if i + 1 >= n or not toks[i + 1].is_punct(b"("):
            i += 1
            continue

        args, close_idx = _split_args(toks, i + 1)
        if args is None or close_idx < 0:
            i += 1
            continue

        # Rule out definitions and declarations: no string literal at all in the
        # argument list means this cannot be a translation call.
        has_literal = any(_literal_of(a) is not None for a in args)
        if not has_literal:
            # ...unless it is a declaration (const / & / :: present)
            is_decl = False
            for arg_idx in range(len(args)):
                if _looks_like_declaration(args[arg_idx]):
                    is_decl = True
                    break
            
            if not is_decl and args:
                # Not a declaration, so it is a run-time (dynamic) call: record it.
                snippet = _decode(b"".join(x.raw for x in toks[i:close_idx + 1]))[:120]
                stats.dynamic.append((relpath, t.line, snippet))
            
            i = close_idx + 1
            continue

        fname = t.value
        has_tag = len(args) >= 2 and _is_tag_arg(args[0])
        base = 1 if has_tag else 0

        def lit(k):
            if k < len(args):
                return _literal_of(args[k])
            return None

        ctx = b""
        msgid = None
        plural = None

        if fname.endswith("_ctx_n"):
            ctx = lit(base) or b""
            msgid = lit(base + 1)
            plural = lit(base + 2)
        elif fname.endswith("_ctx"):
            ctx = lit(base) or b""
            msgid = lit(base + 1)
        elif fname.endswith("_n"):
            msgid = lit(base)
            plural = lit(base + 1)
        else:
            msgid = lit(base)

        if msgid is None:
            # There is a literal but not in the msgid slot (for example the tag is
            # a literal and msgid is a variable). Skip declarations, record the rest.
            is_decl = False
            for arg_idx in range(len(args)):
                if _looks_like_declaration(args[arg_idx]):
                    is_decl = True
                    break
            
            if not is_decl:
                # Not a declaration, so it is a run-time (dynamic) call: record it.
                snippet = _decode(b"".join(x.raw for x in toks[i:close_idx + 1]))[:120]
                stats.dynamic.append((relpath, t.line, _decode(snippet)))
            
            i = close_idx + 1
            continue

        stats.calls += 1
        key = (_decode(ctx), msgid, plural)
        e = entries.get(key)
        if e is None:
            e = Entry(_decode(ctx), msgid, plural)
            entries[key] = e
        e.add_ref(relpath, t.line)

        i = close_idx + 1


def scan_file(path, relpath, funcs, entries, stats, decoy_check=True):
    """Scan one file. Returns False when it was skipped for containing no target name."""
    with open(path, "rb") as f:
        buf = f.read()

    # Quick pre-filter: skip the whole lexical pass when no target name occurs.
    if decoy_check:
        hit = False
        for fn in funcs:
            if fn.encode() in buf:
                hit = True
                break
        if not hit:
            stats.files_skipped += 1
            return False

    toks = tokenize(buf)
    extract_from_tokens(toks, relpath, funcs, entries, stats)
    stats.files_scanned += 1
    return True
