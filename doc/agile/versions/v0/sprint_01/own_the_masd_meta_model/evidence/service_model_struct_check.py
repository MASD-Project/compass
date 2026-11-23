"""Holds compass.serve's operation models to compass.db's hand-written types.

Every message is either the struct of its name, member for member (name,
type and default, in order), or one of the pinned new messages; every struct
is a message or one of the pinned unmodelled structs. A field of an
enumerated type lists the enumerators of its enum class in :values:, in
order. A request's :response: is the return type of a query function that
takes the request's struct, or nothing when that function returns void. Each
pin below must still hold, so a pin goes stale, and fails, when the code it
excuses changes.

Usage:
  python3 -I service_model_struct_check.py <repository root>
  python3 -I service_model_struct_check.py <repository root> --header <hpp>
The second form compares every message of the models with the structs of one
generated header instead, with no pins, and names each type the header uses
but neither declares nor takes from std.
Exits 1 on any FAIL line.
"""
import pathlib
import re
import sys

SPLIT = {"sprint_state_request": "sprint_request",
         "what_needs_testing_request": "sprint_request"}

NEW = {
    "error_response",
    "add_note_response",
    "document_summary", "list_documents_request", "list_documents_response",
    "get_document_request", "get_document_response",
    "backlinks_request", "backlink", "backlinks_response",
    "list_environments_request", "list_environments_response",
    "version_request", "version_response", "sync_request", "sync_response",
    "get_run_request", "step_result", "get_run_response",
    "list_notes_request", "campaign_note", "list_notes_response",
}

UNMODELLED = {
    "view_filter", "ranking", "note_kinds", "environment", "placement",
    "exported_screenshot", "exported_step", "exported_run",
    "exported_scenario", "exported_note", "exported_image", "page_ref",
    "campaign_rows", "exported_file",
    "document_row", "heading_row", "section_row", "link_row", "step_row",
    "scenario_row", "corpus_document",
}

# The alignment design moves the caller out of the six write requests and
# puts the environment first in every path. The structs follow in Serve the
# documents over HTTP.
CHANGED = {
    "start_run_request": {"drops": ["tester"], "adds": []},
    "step_result_request": {"drops": ["tester"], "adds": ["environment"]},
    "finish_run_request": {"drops": ["tester"], "adds": ["environment"]},
    "attach_request": {"drops": ["tester"], "adds": ["environment"]},
    "add_note_request": {"drops": ["tester"], "adds": ["environment"]},
    "address_note_request": {"drops": ["tester"], "adds": ["environment"]},
}

RESPONSE_WRAPS = {"add_note_request": ("std::int64_t", "add_note_response")}


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def structs(text):
    out = {}
    for name, body in re.findall(r"struct (\w+) \{(.*?)\n\};", text, re.S):
        members = []
        for line in body.split(";"):
            line = " ".join(line.split())
            if not line or line.startswith(("using ", "friend ")):
                continue
            m = re.match(r"(.+?) (\w+)(?: = (.+)|\{.*\})?$", line)
            members.append((m.group(2), m.group(1), m.group(3)))
        out[name] = members
    return out


def enums(text):
    return {name: [e.strip() for e in body.split(",") if e.strip()]
            for name, body in re.findall(
                r"enum class (\w+)(?: : [\w:]+)? \{([^}]*)\}", text)}


def query_functions(text):
    """Each query function's return type, keyed by the struct it takes."""
    out = {}
    for ret, args in re.findall(
            r"(?:\[\[nodiscard\]\] )?([\w:<>]+) \w+\(io::connection& c,"
            r"([^;]*?)\);", text):
        for taken in re.findall(r"const types::(\w+)& r\b", args):
            out.setdefault(taken, set()).add(ret.replace("types::", ""))
    return out


def models(root):
    out = {}
    for model in sorted((root / "projects/compass.serve/modeling")
                        .glob("compass.serve.*.org")):
        current = section = None
        for line in model.read_text().splitlines():
            if line.startswith("* "):
                current, section = None, line[2:].strip()
            elif line.startswith("** ") and section == "Messages":
                current = line[3:].strip()
                out[current] = {"model": model.name, "props": {},
                                "fields": []}
            elif line.startswith("*** ") and current:
                out[current]["fields"].append(
                    {"name": line[4:].strip(), "props": {}})
            elif current:
                m = re.match(r":(\w+):\s+(.*)$", line)
                if m and m.group(1) not in ("PROPERTIES", "END"):
                    fields = out[current]["fields"]
                    target = fields[-1] if fields else out[current]
                    target["props"][m.group(1)] = m.group(2).strip()
    return out


def shape(message):
    return [(f["name"], f["props"].get("cpp_type"), f["props"].get("default"))
            for f in message["fields"]]


def inner(cpp_type):
    while True:
        m = re.match(r"std::(?:optional|vector)<(.+)>$", cpp_type or "")
        if not m:
            return cpp_type
        cpp_type = m.group(1)


class Report:
    def __init__(self):
        self.failed = False

    def ok(self, text):
        print(f"ok    {text}")

    def fail(self, text):
        self.failed = True
        print(f"FAIL  {text}")


def check_against_types(root, msgs, report):
    types_dir = root / "projects/compass.db/include/compass.db"
    text = "\n".join(strip_comments(h.read_text())
                     for h in sorted((types_dir / "types").glob("*.hpp")))
    cpp, cpp_enums = structs(text), enums(text)
    functions = query_functions("\n".join(
        strip_comments(h.read_text())
        for h in sorted((types_dir / "query").glob("*.hpp"))))

    used = set()
    for name, msg in msgs.items():
        for f in msg["fields"]:
            enum = inner(f["props"].get("cpp_type"))
            if enum in cpp_enums:
                values = f["props"].get("values", "").split()
                if values != cpp_enums[enum]:
                    report.fail(f"{name}.{f['name']}: :values: {values} vs "
                                f"enum class {enum} {cpp_enums[enum]}")
        struct = SPLIT.get(name, name)
        if struct not in cpp:
            if name in NEW:
                report.ok(f"new      {name}")
            else:
                report.fail(f"{name}: no struct {struct} and not pinned new")
            continue
        used.add(struct)
        if name in NEW:
            report.fail(f"{name}: pinned new but struct {struct} exists")
        model, theirs = shape(msg), cpp[struct]
        change = CHANGED.get(name, {"drops": [], "adds": []})
        for field in change["drops"]:
            if field not in [m[0] for m in theirs]:
                report.fail(f"{name}: pin drops {field}, which {struct} "
                            "no longer has")
            if field in [m[0] for m in model]:
                report.fail(f"{name}: pin drops {field}, which the model "
                            "states")
        for field in change["adds"]:
            if field in [m[0] for m in theirs]:
                report.fail(f"{name}: pin adds {field}, which {struct} "
                            "already has")
            if field not in [m[0] for m in model]:
                report.fail(f"{name}: pin adds {field}, which the model "
                            "lacks")
        kept = [m for m in theirs if m[0] not in change["drops"]]
        mine = [m for m in model if m[0] not in change["adds"]]
        if mine == kept:
            pinned = "".join(f" -{d}" for d in change["drops"]) + \
                "".join(f" +{a}" for a in change["adds"])
            report.ok(f"matches  {name} = {struct} ({len(model)} members"
                      f"{pinned})")
        else:
            report.fail(f"{name} differs from {struct}\n"
                        f"      model: {model}\n      c++:   {theirs}")

        if "http_method" in msg["props"] and struct in functions:
            returns = functions[struct]
            response = msg["props"].get("response")
            wrap = RESPONSE_WRAPS.get(name)
            if wrap and wrap[0] in returns and response == wrap[1]:
                report.ok(f"response {name}: {response} wraps {wrap[0]}")
            elif response is None and "void" in returns:
                report.ok(f"response {name}: none, the function is void")
            elif response in returns:
                report.ok(f"response {name}: {response}")
            else:
                report.fail(f"{name}: :response: {response}, but the query "
                            f"functions taking {struct} return {returns}")

    for name in CHANGED:
        if name not in msgs:
            report.fail(f"pin for {name}, which is not a message")
    for name in NEW:
        if name not in msgs:
            report.fail(f"{name}: pinned new but not a message")
    for struct in sorted(set(cpp) - used):
        if struct not in UNMODELLED:
            report.fail(f"struct {struct} has no message and is not pinned")
    for struct in sorted(UNMODELLED - set(cpp)):
        report.fail(f"pinned unmodelled struct {struct} does not exist")
    for struct in sorted(UNMODELLED & used):
        report.fail(f"pinned unmodelled struct {struct} has a message")


def check_against_header(header, msgs, report):
    text = strip_comments(header.read_text())
    generated = structs(text)
    declared = set(generated) | set(enums(text))
    names = [n for n, m in msgs.items()
             if header.name.endswith(
                 m["model"].removeprefix("compass.serve.").removesuffix(".org")
                 + "_protocol.hpp")]
    for name in names:
        if name not in generated:
            report.fail(f"{name}: not in {header.name}")
        elif shape(msgs[name]) == generated[name]:
            report.ok(f"matches  {name} ({len(generated[name])} members)")
        else:
            report.fail(f"{name} differs\n      model: {shape(msgs[name])}"
                        f"\n      hpp:   {generated[name]}")
    for name in sorted(set(generated) - set(names)):
        report.fail(f"{header.name} has struct {name}, which is no message")
    undeclared = sorted({inner(t) for members in generated.values()
                         for _, t, _ in members}
                        - declared
                        - {t for t in (inner(t) for members in
                                       generated.values()
                                       for _, t, _ in members)
                           if t.startswith("std::")
                           or t in ("bool", "double", "float", "int")})
    for t in undeclared:
        print(f"note  {header.name} uses {t} and does not declare it")


def main(argv):
    root = pathlib.Path(argv[1])
    msgs = models(root)
    report = Report()
    if len(argv) == 4 and argv[2] == "--header":
        check_against_header(pathlib.Path(argv[3]), msgs, report)
    else:
        check_against_types(root, msgs, report)
    return 1 if report.failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
