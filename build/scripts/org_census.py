#!/usr/bin/env python3
r"""Count org-mode constructs across a corpus.

Walks a directory, reads every .org file outside the top-level build/,
external/ and vcpkg/ directories and outside virtual environments at any
depth, and counts the elements and objects of the Org
Syntax specification: headings, planning lines, drawers, keywords, blocks,
tables, lists, footnotes, links, emphasis, timestamps and the rest. The
counts rank what the compass.org tables must hold and size them.

The scan is line-based, not a parser. Where it is approximate:

- Elements are classified one line at a time with a small context stack.
  A block or drawer with no closing line before the next heading is text,
  as it is in org-element, and is reported under "unclosed".
- Lists are separated by indentation and by two blank lines only; an item
  body is not parsed for nesting beyond the indent of its bullet.
- Objects are matched with regular expressions over one line. An emphasis
  span or link that wraps a line is missed. Code and verbatim spans are
  removed before other objects are counted, then link targets, so nesting
  is seen in that order only.
- Emphasis uses the org-emphasis-regexp borders but not the newline limit.
- An angle link is counted for any <scheme:...>, registered or not, so C++
  text such as <std::string> outside a block counts as a link.
- A subscript or superscript is counted wherever the syntax matches; whether
  the export honours it depends on #+options: ^, which is counted apart.
- A dollar LaTeX fragment is counted only in the strict $x$ form of the
  specification; prose with two dollar amounts on one line can match.
- Entities, emphasis and scripts inside a LaTeX fragment are not counted,
  because the fragment is one opaque object. Entities written with the
  \_<spaces> form are not counted at all.
- A section is counted for every heading, and for every document start,
  that is followed by at least one non-blank line before the next heading.
- Names are the strings the tables intern: keyword, affiliated keyword and
  property names, heading tags, TODO keywords, block types and languages,
  and link types. "names" sums the distinct names of each document.
- A breakdown whose count is zero is still printed, so an absent construct is
  visible as absent.
- Paragraphs are runs of text lines, split by any other element. The text
  on an item's bullet line is one paragraph, as it is in org-element.

Contents of src, example, export and comment blocks are opaque and are not
scanned. Contents of quote, center, verse and special blocks are.

Usage: python3 build/scripts/org_census.py <root> [--json] [--top N]
"""
import argparse
import collections
import json
import os
import re
import sys

EXCLUDED_AT_ROOT = {"build", "external", "vcpkg"}
EXCLUDED_ANYWHERE = {"venv", ".venv", ".git", "node_modules"}

VERBATIM_BLOCKS = {"src", "example", "export", "comment"}

PARSED_KEYWORDS = {"title", "date", "author", "caption"}

AFFILIATED = {"caption", "name", "results", "header", "headers", "plot",
              "label", "data", "resname", "source", "srcname", "tblname"}

ENTITIES = set("""
Agrave agrave Aacute aacute Acirc acirc Amacr amacr Atilde atilde Auml auml
Aring AA aring AElig aelig Ccedil ccedil Egrave egrave Eacute eacute Ecirc
ecirc Euml euml Igrave igrave Iacute iacute Idot inodot Icirc icirc Iuml iuml
Ntilde ntilde Ograve ograve Oacute oacute Ocirc ocirc Otilde otilde Ouml ouml
Oslash oslash OElig oelig Scaron scaron szlig Ugrave ugrave Uacute uacute
Ucirc ucirc Uuml uuml Yacute yacute Yuml yuml fnof real image weierp ell
imath jmath Alpha alpha Beta beta Gamma gamma Delta delta Epsilon epsilon
varepsilon Zeta zeta Eta eta Theta theta thetasym vartheta Iota iota Kappa
kappa Lambda lambda Mu mu nu Nu Xi xi Omicron omicron Pi pi Rho rho Sigma
sigma sigmaf varsigma Tau Upsilon upsih upsilon Phi phi varphi Chi chi acutex
Psi psi tau Omega omega piv varpi partial alefsym aleph gimel beth dalet ETH
eth THORN thorn dots cdots hellip middot iexcl iquest shy ndash mdash quot
acute ldquo rdquo bdquo lsquo rsquo sbquo laquo raquo lsaquo rsaquo circ vert
vbar brvbar S sect P para amp lt gt tilde slash plus under equal asciicirc
dagger dag Dagger ddag nbsp ensp emsp thinsp curren cent pound yen euro EUR
dollar USD copy reg trade minus pm plusmn times frasl colon div frac12 frac14
frac34 permil sup1 sup2 sup3 radic sum prod micro macr deg prime Prime infin
infty prop propto not neg land wedge lor vee cap cup smile frown int therefore
there4 because sim cong simeq asymp approx ne neq equiv triangleq le leq ge
geq lessgtr lesseqgtr ll Ll lll gg Gg ggg prec preceq preccurlyeq succ succeq
succcurlyeq sub subset sup supset nsub sube nsup supe setminus forall exist
exists nexist nexists empty emptyset isin in notin ni nabla ang angle perp
parallel sdot cdot lceil rceil lfloor rfloor lang rang langle rangle hbar mho
larr leftarrow gets lArr Leftarrow uarr uparrow uArr Uparrow rarr to
rightarrow rArr Rightarrow darr downarrow dArr Downarrow harr leftrightarrow
hArr Leftrightarrow crarr hookleftarrow arccos arcsin arctan arg cos cosh cot
coth csc det dim exp gcd hom inf ker lg lim liminf limsup ln log max min Pr
sec sin sinh tan tanh bull bullet star lowast ast odot oplus otimes check
checkmark ordf ordm cedil oline uml zwnj zwj lrm rlm smiley blacksmile sad
frowny clubs clubsuit spades spadesuit hearts heartsuit diams diamondsuit
diamond Diamond loz
""".split())

HEADING = re.compile(r"^(\*+) (.*)$")
HEADING_TAGS = re.compile(r"[ \t]+(:[\w@#%:]+:)[ \t]*$")
PRIORITY = re.compile(r"^\[#([A-Z0-9])\] ?")
PLANNING = re.compile(r"\b(SCHEDULED|DEADLINE|CLOSED):")
CLOCK = re.compile(r"^[ \t]*CLOCK:")
DRAWER_OPEN = re.compile(r"^[ \t]*:([\w-]+):[ \t]*$")
DRAWER_END = re.compile(r"^[ \t]*:END:[ \t]*$", re.I)
NODE_PROPERTY = re.compile(r"^[ \t]*:([^ \t:]+?)\+?:(?:[ \t]|$)")
BLOCK_BEGIN = re.compile(r"^[ \t]*#\+begin_(\S+)(?:[ \t]+(\S+))?", re.I)
BLOCK_END = re.compile(r"^[ \t]*#\+end_(\S+)", re.I)
DYNAMIC_BEGIN = re.compile(r"^[ \t]*#\+begin:(?:[ \t]+(\S+))?", re.I)
DYNAMIC_END = re.compile(r"^[ \t]*#\+end:?[ \t]*$", re.I)
BABEL_CALL = re.compile(r"^[ \t]*#\+call:", re.I)
KEYWORD = re.compile(r"^[ \t]*#\+([^ \t:\[]+)(\[[^\]]*\])?:(?:[ \t]|$)(.*)$")
COMMENT = re.compile(r"^[ \t]*#(?: |$)")
FIXED_WIDTH = re.compile(r"^[ \t]*:(?: |$)")
RULE = re.compile(r"^[ \t]*-{5,}[ \t]*$")
TABLE_ROW = re.compile(r"^[ \t]*\|")
TABLE_RULE = re.compile(r"^[ \t]*\|-")
TABLE_EL = re.compile(r"^[ \t]*\+-")
ITEM = re.compile(
    r"^([ \t]*)(?:([-+])|(\*)|((?:\d+|[A-Za-z])[.)]))(?:[ \t]+|$)(.*)$")
ITEM_COUNTER = re.compile(r"^\[@(?:\d+|[A-Za-z])\][ \t]*")
ITEM_CHECKBOX = re.compile(r"^\[([ X-])\](?:[ \t]|$)")
ITEM_TAG = re.compile(r"[ \t]::(?:[ \t]|$)")
FOOTNOTE_DEFINITION = re.compile(r"^\[fn:([\w-]+)\]")
LATEX_ENVIRONMENT = re.compile(r"^[ \t]*\\begin\{([A-Za-z0-9*]+)\}")
DIARY_SEXP = re.compile(r"^%%\(")

CODE_SPAN = re.compile(
    r"(?:^|(?<=[-\s('\"{]))([=~])(?=\S)(.+?)(?<=\S)\1(?=[-\s.,:;!?'\")}\[\\]|$)")
EMPHASIS = re.compile(
    r"(?:^|(?<=[-\s('\"{]))([*/_+])(?=\S)(.+?)(?<=\S)\1(?=[-\s.,:;!?'\")}\[\\]|$)")
BRACKET_LINK = re.compile(r"\[\[((?:[^\]\\]|\\.)+)\](?:\[((?:[^\]]|\][^\]])+)\])?\]")
ANGLE_LINK = re.compile(r"<([A-Za-z][\w+.-]*):[^>\s][^>]*>")
PLAIN_LINK = re.compile(
    r"(?<![\w\[<])(https?|ftp|file|mailto|id|doi|news|shell|elisp|info|help"
    r"|proj):[^\s<>\]()]+")
LINK_SCHEME = re.compile(r"^([A-Za-z][\w+.-]*):")
FOOTNOTE_REFERENCE = re.compile(r"\[fn:([\w-]*)(:)?")
INLINE_SRC = re.compile(r"(?<![\w])src_([^\s\[{]+)(?:\[[^\]]*\])?\{")
INLINE_CALL = re.compile(r"(?<![\w])call_([^\s\[(]+)(?:\[[^\]]*\])?\(")
TIMESTAMP = re.compile(
    r"([<\[])(\d{4}-\d{2}-\d{2})([^\]>\n]*)[>\]](--[<\[]\d{4}-\d{2}-\d{2}"
    r"[^\]>\n]*[>\]])?")
TIMESTAMP_DIARY = re.compile(r"<%%\(")
REPEATER = re.compile(r"[.+]?\+\d+[hdwmy]")
MACRO = re.compile(r"\{\{\{([A-Za-z][\w-]*)(?:\((.*?)\))?\}\}\}")
EXPORT_SNIPPET = re.compile(r"@@([\w-]+):.*?@@")
LATEX_PAREN = re.compile(r"\\\(.*?\\\)")
LATEX_BRACKET = re.compile(r"\\\[.*?\\\]")
LATEX_DOLLARS = re.compile(r"\$\$.+?\$\$")
LATEX_DOLLAR = re.compile(
    r"(?:^|(?<=[^$\w]))\$(?:[^\s$,.;]|[^\s$,.;][^$\n]*?[^\s$,.])\$"
    r"(?=[-\s.,;:!?'\")]|$)")
ENTITY = re.compile(r"\\([A-Za-z]+[0-9]*)(\{\})?(?![A-Za-z])")
STATISTICS_COOKIE = re.compile(r"\[(?:\d*/\d*|\d*%)\]")
RADIO_TARGET = re.compile(r"<<<[^<>\n]+>>>")
TARGET = re.compile(r"(?<!<)<<[^<>\n]+>>(?!>)")
LINE_BREAK = re.compile(r"\\\\[ \t]*$")
SCRIPT = re.compile(r"(?<=\S)([_^])(\{[^{}\n]*\}|\*|[+-]?[A-Za-z0-9]+)")
CITATION = re.compile(r"\[cite(?:/[^:\]]*)?:[^\]]+\]")


class Census:
    def __init__(self):
        self.count = collections.Counter()
        self.heading_depth = collections.Counter()
        self.todo = collections.Counter()
        self.priority = collections.Counter()
        self.tags = collections.Counter()
        self.planning = collections.Counter()
        self.drawer = collections.Counter()
        self.property = collections.Counter()
        self.keyword = collections.Counter()
        self.affiliated = collections.Counter()
        self.option = collections.Counter()
        self.block = collections.Counter()
        self.src_language = collections.Counter()
        self.dynamic = collections.Counter()
        self.list_kind = collections.Counter()
        self.item_kind = collections.Counter()
        self.checkbox = collections.Counter()
        self.link = collections.Counter()
        self.link_form = collections.Counter()
        self.emphasis = collections.Counter()
        self.timestamp = collections.Counter()
        self.macro = collections.Counter()
        self.snippet = collections.Counter()
        self.latex = collections.Counter()
        self.latex_environment = collections.Counter()
        self.entity = collections.Counter()
        self.script = collections.Counter()
        self.footnote = collections.Counter()
        self.inline_src = collections.Counter()
        self.maxima = collections.Counter()
        self.names = set()

    def peak(self, key, value):
        if value > self.maxima[key]:
            self.maxima[key] = value


def todo_keywords(lines):
    """The TODO vocabulary of a file: its #+todo: lines, or org's default."""
    words = set()
    for line in lines:
        m = re.match(r"^[ \t]*#\+(?:todo|seq_todo|typ_todo):(.*)$", line, re.I)
        if m:
            for word in m.group(1).split():
                if word != "|":
                    words.add(re.sub(r"\(.*\)$", "", word))
    return words or {"TODO", "DONE"}


def scan_objects(c, text):
    """Count the objects on one line of parsed text."""
    def strip_code(m):
        c.emphasis[m.group(1)] += 1
        return " " * len(m.group(0))
    text = CODE_SPAN.sub(strip_code, text)

    def strip_link(m):
        target = m.group(1)
        scheme = LINK_SCHEME.match(target)
        if scheme:
            kind = scheme.group(1).lower()
        elif target.startswith("#"):
            kind = "(custom-id)"
        elif target.startswith("*"):
            kind = "(heading)"
        elif target.startswith("("):
            kind = "(coderef)"
        elif target.startswith(("./", "../", "/", "~/")):
            kind = "(path)"
        else:
            kind = "(fuzzy)"
        c.link[kind] += 1
        c.names.add(kind)
        c.link_form["bracket, described" if m.group(2) else "bracket, bare"] += 1
        description = m.group(2) or ""
        return description + " " * (len(m.group(0)) - len(description))
    text = BRACKET_LINK.sub(strip_link, text)

    def strip_angle(m):
        c.link[m.group(1).lower()] += 1
        c.names.add(m.group(1).lower())
        c.link_form["angle"] += 1
        return " " * len(m.group(0))
    text = ANGLE_LINK.sub(strip_angle, text)

    def strip_plain(m):
        c.link[m.group(1).lower()] += 1
        c.names.add(m.group(1).lower())
        c.link_form["plain"] += 1
        return " " * len(m.group(0))
    text = PLAIN_LINK.sub(strip_plain, text)

    for name, pattern in ((r"\(...\)", LATEX_PAREN),
            (r"\[...\]", LATEX_BRACKET), ("$$...$$", LATEX_DOLLARS),
            ("$...$", LATEX_DOLLAR)):
        text, found = pattern.subn(lambda m: " " * len(m.group(0)), text)
        c.latex[name] += found

    for m in EMPHASIS.finditer(text):
        c.emphasis[m.group(1)] += 1

    for m in FOOTNOTE_REFERENCE.finditer(text):
        if m.group(2):
            c.footnote["inline" if m.group(1) else "anonymous"] += 1
        else:
            c.footnote["reference"] += 1
    for m in INLINE_SRC.finditer(text):
        c.inline_src[m.group(1)] += 1
    c.count["inline babel calls"] += len(INLINE_CALL.findall(text))

    for m in TIMESTAMP.finditer(text):
        kind = "active" if m.group(1) == "<" else "inactive"
        if m.group(4):
            kind += " range"
        c.timestamp[kind] += 1
        if REPEATER.search(m.group(3)):
            c.timestamp["with repeater or delay"] += 1
        if re.search(r"\d{1,2}:\d{2}", m.group(3)):
            c.timestamp["with time"] += 1
    c.timestamp["diary"] += len(TIMESTAMP_DIARY.findall(text))

    for m in MACRO.finditer(text):
        c.macro[m.group(1)] += 1
    for m in EXPORT_SNIPPET.finditer(text):
        c.snippet[m.group(1)] += 1

    for m in ENTITY.finditer(text):
        if m.group(1) in ENTITIES:
            c.entity[m.group(1)] += 1
    c.count["statistics cookies"] += len(STATISTICS_COOKIE.findall(text))
    c.count["radio targets"] += len(RADIO_TARGET.findall(text))
    c.count["targets"] += len(TARGET.findall(text))
    c.count["line breaks"] += len(LINE_BREAK.findall(text))
    c.count["citations"] += len(CITATION.findall(text))
    for m in SCRIPT.finditer(text):
        kind = "subscript" if m.group(1) == "_" else "superscript"
        c.script[kind + (", braced" if m.group(2).startswith("{") else "")] += 1


def scan_file(c, text):
    lines = text.split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    todo = todo_keywords(lines)
    c.count["documents"] += 1
    c.count["bytes"] += len(text.encode("utf-8", "surrogateescape"))
    c.count["lines"] += len(lines)
    c.peak("lines in one document", len(lines))
    c.peak("bytes in one document", len(text.encode("utf-8", "surrogateescape")))

    local = collections.Counter()
    c.names = set()
    section_open = False
    block = None
    drawer = None
    in_table = None
    table_rows = 0
    table_columns = 0
    in_paragraph = False
    in_fixed = False
    in_comment = False
    list_indents = []
    blanks = 0
    after_heading = False
    after_planning = False

    def end_table():
        nonlocal in_table, table_rows, table_columns
        if in_table:
            c.peak("rows in one table", table_rows)
            c.peak("columns in one table", table_columns)
        in_table = None
        table_rows = 0
        table_columns = 0

    def end_runs(keep=None):
        nonlocal in_paragraph, in_fixed, in_comment
        if keep != "paragraph":
            in_paragraph = False
        if keep != "fixed":
            in_fixed = False
        if keep != "comment":
            in_comment = False
        if keep != "table":
            end_table()

    def closes(start, pattern, name=None):
        for later in lines[start + 1:]:
            if HEADING.match(later):
                return False
            m = pattern.match(later)
            if m and (name is None or m.group(1).lower() == name):
                return True
        return False

    for n, line in enumerate(lines):
        c.peak("bytes in one line", len(line))

        if block is not None:
            end = BLOCK_END.match(line)
            if end and end.group(1).lower() == block:
                block = None
            else:
                c.count["lines inside opaque blocks"] += 1
            continue

        if drawer is not None:
            if DRAWER_END.match(line):
                drawer = None
            elif drawer == "PROPERTIES":
                m = NODE_PROPERTY.match(line)
                if m:
                    c.count["node properties"] += 1
                    local["node properties"] += 1
                    c.property[m.group(1).upper()] += 1
                    c.names.add(m.group(1).upper())
            else:
                c.count["lines inside other drawers"] += 1
                if CLOCK.match(line):
                    c.count["clock lines"] += 1
                elif line.strip():
                    scan_objects(c, line)
            continue

        if not line.strip():
            blanks += 1
            c.count["blank lines"] += 1
            end_runs()
            if blanks >= 2:
                list_indents.clear()
            after_heading = False
            after_planning = False
            continue
        was_blank = blanks > 0
        blanks = 0

        m = HEADING.match(line)
        if m:
            end_runs()
            list_indents.clear()
            section_open = False
            depth = len(m.group(1))
            title = m.group(2)
            c.count["headings"] += 1
            local["headings"] += 1
            c.heading_depth[depth] += 1
            c.peak("heading depth", depth)
            if depth >= 15:
                c.count["inlinetask-depth headings"] += 1
            word = title.split(" ", 1)[0]
            if word in todo:
                c.todo[word] += 1
                c.names.add(word)
                c.count["headings with a TODO keyword"] += 1
                title = title[len(word):].lstrip()
            p = PRIORITY.match(title)
            if p:
                c.priority[p.group(1)] += 1
                c.count["headings with a priority"] += 1
                title = title[p.end():]
            if title.startswith("COMMENT"):
                c.count["COMMENT headings"] += 1
            t = HEADING_TAGS.search(title)
            if t:
                c.count["headings with tags"] += 1
                for tag in t.group(1).strip(":").split(":"):
                    c.tags[tag] += 1
                    c.names.add(tag)
                    c.count["heading tags"] += 1
                title = title[:t.start()]
            scan_objects(c, title)
            after_heading = True
            after_planning = False
            continue

        if not section_open:
            section_open = True
            c.count["sections"] += 1

        if after_heading and PLANNING.search(line):
            c.count["planning lines"] += 1
            for word in PLANNING.findall(line):
                c.planning[word] += 1
            scan_objects(c, line)
            after_heading = False
            after_planning = True
            continue

        d = DRAWER_OPEN.match(line)
        if d and d.group(1).upper() != "END" and not closes(n, DRAWER_END):
            c.count["unclosed drawers"] += 1
            d = None
        if d and d.group(1).upper() != "END":
            end_runs()
            name = d.group(1).upper()
            drawer = name
            if name == "PROPERTIES":
                c.count["property drawers"] += 1
                if not local["headings"]:
                    c.count["property drawers, document level"] += 1
            else:
                c.count["other drawers"] += 1
                c.drawer[name] += 1
            after_heading = False
            continue
        after_heading = False
        after_planning = False

        if DYNAMIC_BEGIN.match(line):
            end_runs()
            c.count["dynamic blocks"] += 1
            c.dynamic[(DYNAMIC_BEGIN.match(line).group(1) or "").lower()] += 1
            continue
        if DYNAMIC_END.match(line):
            end_runs()
            continue

        b = BLOCK_BEGIN.match(line)
        if b and not closes(n, BLOCK_END, b.group(1).lower()):
            c.count["unclosed blocks"] += 1
            b = None
        if b:
            end_runs()
            kind = b.group(1).lower()
            c.block[kind] += 1
            c.count["blocks"] += 1
            local["blocks"] += 1
            if kind == "src":
                c.src_language[(b.group(2) or "(none)").lower()] += 1
                c.names.add((b.group(2) or "").lower())
            c.names.add(kind)
            if kind in VERBATIM_BLOCKS:
                block = kind
            continue
        if BLOCK_END.match(line):
            end_runs()
            continue

        if BABEL_CALL.match(line):
            end_runs()
            c.count["babel calls"] += 1
            continue

        k = KEYWORD.match(line)
        if k:
            name = k.group(1).lower()
            c.names.add(name)
            if name in PARSED_KEYWORDS:
                scan_objects(c, k.group(3))
            if name in AFFILIATED or name.startswith("attr_"):
                c.affiliated[name] += 1
                c.count["affiliated keywords"] += 1
            else:
                end_runs()
                c.keyword[name] += 1
                c.count["keywords"] += 1
                local["keywords"] += 1
                if name == "tblfm":
                    c.count["table formulas"] += 1
                if name == "options":
                    for option in k.group(3).split():
                        c.option[option] += 1
            continue

        if COMMENT.match(line):
            if not in_comment:
                end_runs()
                c.count["comments"] += 1
                in_comment = True
            c.count["comment lines"] += 1
            continue

        if FIXED_WIDTH.match(line):
            if not in_fixed:
                end_runs()
                c.count["fixed width areas"] += 1
                in_fixed = True
            c.count["fixed width lines"] += 1
            continue

        if RULE.match(line):
            end_runs()
            c.count["horizontal rules"] += 1
            continue

        if TABLE_ROW.match(line):
            if in_table != "org":
                end_runs()
                in_table = "org"
                c.count["tables"] += 1
                local["tables"] += 1
            table_rows += 1
            local["table rows"] += 1
            if TABLE_RULE.match(line):
                c.count["table rule rows"] += 1
            else:
                c.count["table rows"] += 1
                cells = line.strip().strip("|").split("|")
                table_columns = max(table_columns, len(cells))
                c.count["table cells"] += len(cells)
                local["table cells"] += len(cells)
                for cell in cells:
                    if cell.strip():
                        scan_objects(c, cell)
            continue

        if TABLE_EL.match(line):
            if in_table != "table.el":
                end_runs()
                in_table = "table.el"
                c.count["table.el tables"] += 1
            continue
        if in_table == "table.el" and line.lstrip().startswith("|"):
            continue

        if LATEX_ENVIRONMENT.match(line):
            end_runs()
            c.count["LaTeX environments"] += 1
            c.latex_environment[LATEX_ENVIRONMENT.match(line).group(1)] += 1
            continue

        f = FOOTNOTE_DEFINITION.match(line)
        if f:
            end_runs()
            list_indents.clear()
            c.count["footnote definitions"] += 1
            scan_objects(c, line[f.end():])
            in_paragraph = True
            continue

        if DIARY_SEXP.match(line):
            end_runs()
            c.count["diary sexps"] += 1
            continue

        i = ITEM.match(line)
        if i and not (i.group(3) and not i.group(1)):
            end_runs()
            indent = len(i.group(1).expandtabs())
            while list_indents and list_indents[-1][0] > indent:
                list_indents.pop()
            body = i.group(5)
            counter = ITEM_COUNTER.match(body)
            if counter:
                c.count["items with a counter"] += 1
                body = body[counter.end():]
            box = ITEM_CHECKBOX.match(body)
            if box:
                c.checkbox["[" + box.group(1) + "]"] += 1
                body = body[box.end():]
            if i.group(4):
                kind = "ordered"
            elif ITEM_TAG.search(body):
                kind = "descriptive"
            else:
                kind = "unordered"
            if not list_indents or list_indents[-1][0] < indent:
                list_indents.append((indent, kind))
                c.count["plain lists"] += 1
                c.list_kind[kind] += 1
                c.peak("list nesting", len(list_indents))
                if len(list_indents) > 1:
                    c.count["nested plain lists"] += 1
            c.count["items"] += 1
            local["items"] += 1
            c.item_kind[kind] += 1
            scan_objects(c, body)
            if body.strip():
                c.count["paragraphs"] += 1
                local["paragraphs"] += 1
            in_paragraph = True
            continue

        if list_indents:
            indent = len(line) - len(line.lstrip())
            while list_indents and list_indents[-1][0] >= indent:
                list_indents.pop()

        if not in_paragraph:
            end_runs()
            c.count["paragraphs"] += 1
            local["paragraphs"] += 1
            in_paragraph = True
        end_runs(keep="paragraph")
        c.count["paragraph lines"] += 1
        scan_objects(c, line)

    end_table()
    c.count["names"] += len(c.names)
    c.count["name bytes"] += sum(len(name) for name in c.names)
    c.peak("names in one document", len(c.names))
    for key, value in local.items():
        c.peak(key + " in one document", value)
    elements = sum(local[k] for k in ("headings", "blocks", "keywords",
        "tables", "table rows", "items", "paragraphs"))
    c.peak("elements in one document (approx)", elements)


def org_files(root):
    for directory, names, files in os.walk(root):
        excluded = set(EXCLUDED_ANYWHERE)
        if os.path.samefile(directory, root):
            excluded |= EXCLUDED_AT_ROOT
        names[:] = sorted(n for n in names if n not in excluded)
        for name in sorted(files):
            if name.endswith(".org"):
                yield os.path.join(directory, name)


def section(title, counter, top, out):
    total = sum(counter.values())
    out.append(f"\n{title} (total {total:,}, distinct {len(counter):,})")
    for key, value in counter.most_common(top):
        out.append(f"  {str(key):<34} {value:>10,}")
    if len(counter) > top:
        rest = sum(v for _, v in counter.most_common()[top:])
        out.append(f"  {'(other)':<34} {rest:>10,}")


def report(c, top):
    for key in ("planning lines", "dynamic blocks", "babel calls",
            "inline babel calls", "table.el tables", "footnote definitions",
            "radio targets", "targets", "statistics cookies", "citations",
            "horizontal rules", "comments", "fixed width areas",
            "LaTeX environments", "other drawers", "diary sexps",
            "clock lines", "line breaks"):
        c.count.setdefault(key, 0)
    out = ["Totals"]
    for key in sorted(c.count):
        out.append(f"  {key:<34} {c.count[key]:>10,}")
    out.append("\nMaxima")
    for key in sorted(c.maxima):
        out.append(f"  {key:<34} {c.maxima[key]:>10,}")
    depth = collections.Counter(
        {f"level {k}": v for k, v in c.heading_depth.items()})
    for title, counter in (
            ("Headings by depth", depth),
            ("TODO keywords on headings", c.todo),
            ("Priorities", c.priority),
            ("Heading tags", c.tags),
            ("Planning keywords", c.planning),
            ("Drawers other than PROPERTIES", c.drawer),
            ("Node property names", c.property),
            ("Keywords by name", c.keyword),
            ("Affiliated keywords by name", c.affiliated),
            ("#+options: settings", c.option),
            ("Blocks by kind", c.block),
            ("Source block languages", c.src_language),
            ("Dynamic blocks by name", c.dynamic),
            ("Plain lists by kind", c.list_kind),
            ("Items by kind", c.item_kind),
            ("Checkboxes", c.checkbox),
            ("Footnote references", c.footnote),
            ("Links by type", c.link),
            ("Links by form", c.link_form),
            ("Emphasis by marker", c.emphasis),
            ("Inline source by language", c.inline_src),
            ("Timestamps", c.timestamp),
            ("Macros by name", c.macro),
            ("Export snippets by backend", c.snippet),
            ("LaTeX fragments", c.latex),
            ("LaTeX environments", c.latex_environment),
            ("Entities", c.entity),
            ("Subscripts and superscripts", c.script)):
        section(title, +counter, top, out)
    return "\n".join(out)


def as_json(c):
    return json.dumps({name: dict(value) for name, value in vars(c).items()
        if name != "names"},
        indent=2, sort_keys=True, default=str)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("root", help="directory to scan")
    parser.add_argument("--json", action="store_true",
        help="print every counter as JSON")
    parser.add_argument("--top", type=int, default=20,
        help="rows shown per breakdown (default 20)")
    args = parser.parse_args()

    if not os.path.isdir(args.root):
        sys.exit(f"org_census: {args.root} is not a directory")
    census = Census()
    for path in org_files(args.root):
        with open(path, encoding="utf-8", errors="surrogateescape") as source:
            scan_file(census, source.read())
    if not census.count["documents"]:
        sys.exit(f"org_census: no .org files under {args.root}")
    print(as_json(census) if args.json else report(census, args.top))


if __name__ == "__main__":
    main()
