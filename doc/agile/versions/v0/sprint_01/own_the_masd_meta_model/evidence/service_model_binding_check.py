"""Holds the route, tool and binding properties of compass.serve's operation
models to the rules of the component overview, and prints the routes and the
MCP tools they declare.

Usage: python3 -I service_model_binding_check.py <repository root>
Exits 1 on any FAIL line.
"""
import pathlib
import re
import sys

AREAS = {"documents", "agile", "qa"}
READ_PREFIXES = ("list_", "get_", "read_", "find_", "search_", "lookup_")
METHODS = {"get", "post", "put", "patch", "delete"}
HTTP_IN = {"path", "query", "body"}
IDENTITY = {"tester", "author", "addressed_by", "subject", "actor", "view",
            "caller"}
RESERVED_FIRST_SEGMENTS = {"environments", "version", "catalogue"}
SCALARS = {"std::string", "std::uint32_t", "std::uint64_t", "std::int64_t",
           "std::int32_t", "double", "bool"}
ENUMS = {"scenario_state", "step_outcome", "run_outcome"}
ROUTE_PROPS = ("tool", "area", "entity", "writes", "summary", "http_method",
               "http_path", "caller")
ERROR_CODES = ["invalid", "no_view", "no_subject", "forbidden", "not_found",
               "unknown_environment", "unknown_sprint", "conflict",
               "too_large", "internal"]


def models(root):
    out = {}
    for model in sorted((root / "projects/compass.serve/modeling")
                        .glob("compass.serve.*.org")):
        current = section = None
        for n, line in enumerate(model.read_text().splitlines(), 1):
            if line.startswith("* "):
                current, section = None, line[2:].strip()
            elif line.startswith("** ") and section == "Messages":
                current = line[3:].strip()
                out[current] = {"where": f"{model.name}:{n} {current}",
                                "props": {}, "fields": []}
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


def unwrap(cpp_type):
    kinds = []
    while True:
        m = re.match(r"std::(optional|vector)<(.+)>$", cpp_type)
        if not m:
            return kinds, cpp_type
        kinds.append(m.group(1))
        cpp_type = m.group(2)


def required(field):
    if field["props"].get("http_in") == "path":
        return True
    kinds, base = unwrap(field["props"].get("cpp_type", ""))
    return not (kinds or "default" in field["props"]
                or field["props"].get("required") == "false"
                or base not in SCALARS | ENUMS)


def check(msgs, fail):
    routes = []
    spread = {unwrap(f["props"]["cpp_type"])[1]
              for m in msgs.values() for f in m["fields"]
              if f["props"].get("http_in") == "query"
              and unwrap(f["props"].get("cpp_type", ""))[1] in msgs}
    for name, m in msgs.items():
        p, where = m["props"], m["where"]
        routed = "http_method" in p
        for f in m["fields"]:
            fp, at = f["props"], f"{where}.{f['name']}"
            kinds, base = unwrap(fp.get("cpp_type", ""))
            if base not in SCALARS | ENUMS and base not in msgs:
                fail(f"{at}: unknown type {fp.get('cpp_type')}")
            if base in ENUMS and "values" not in fp:
                fail(f"{at}: enumerated type without :values:")
            if routed and fp.get("http_in") not in HTTP_IN:
                fail(f"{at}: :http_in: {fp.get('http_in')} is not one of "
                     f"{sorted(HTTP_IN)}")
            if not routed:
                for k in ("http_in", "http_name", "required", "encoding"):
                    if k in fp and not (k == "http_name" and name in spread):
                        fail(f"{at}: :{k}: on a message with no route")
            if routed and f["name"] in IDENTITY:
                fail(f"{at}: a request names the caller; it comes from the "
                     "headers")
            if "required" in fp and (fp["required"] != "false"
                                     or fp.get("http_in") != "query"
                                     or kinds or "default" in fp):
                fail(f"{at}: :required: false belongs only on a plain query "
                     "field with no default")
            if "encoding" in fp and (fp["encoding"] != "base64"
                                     or fp.get("cpp_type") != "std::string"
                                     or fp.get("http_in") != "body"):
                fail(f"{at}: :encoding: base64 belongs only on a "
                     "std::string body field")
            if routed and f["name"] == "bytes" and "encoding" not in fp:
                fail(f"{at}: a field named bytes holds a file and states "
                     ":encoding: base64")
            if fp.get("http_in") == "path" and (kinds or base not in SCALARS):
                fail(f"{at}: a path field must be a scalar")
            if fp.get("http_in") == "query" and base in msgs and not kinds:
                for member in msgs[base]["fields"]:
                    mk, mb = unwrap(member["props"].get("cpp_type", ""))
                    pair = [g["props"].get("cpp_type")
                            for g in msgs.get(mb, {"fields": []})["fields"]]
                    if mb in msgs and not (mk == ["vector"] and
                                           pair == ["std::string"] * 2):
                        fail(f"{at}.{member['name']}: a spread query member "
                             "is a scalar or a vector of name:value pairs")
        if not routed:
            for k in ROUTE_PROPS + ("destructive", "idempotent",
                                    "max_body_size"):
                if k in p:
                    fail(f"{where}: :{k}: on a message with no route")
            continue

        for k in ROUTE_PROPS:
            if k not in p:
                fail(f"{where}: no :{k}:")
        method, path, tool = p.get("http_method"), p.get("http_path", ""), \
            p.get("tool", "")
        writes = p.get("writes")
        fields = {k: [f for f in m["fields"] if f["props"].get("http_in") == k]
                  for k in HTTP_IN}
        if method not in METHODS:
            fail(f"{where}: :http_method: {method}")
        if p.get("area") not in AREAS:
            fail(f"{where}: area {p.get('area')} is not one of {sorted(AREAS)}")
        if "response" in p and p["response"] not in msgs:
            fail(f"{where}: :response: {p['response']} is not a message")
        if not path.startswith("/v1/"):
            fail(f"{where}: path {path} is not under /v1/")
        holes = re.findall(r"\{(\w+)\}", path)
        if sorted(holes) != sorted(f["name"] for f in fields["path"]):
            fail(f"{where}: placeholders {holes} vs path fields "
                 f"{[f['name'] for f in fields['path']]}")
        first = path.split("/")[2] if path.count("/") >= 2 else ""
        if first != "{environment}" and first not in RESERVED_FIRST_SEGMENTS:
            fail(f"{where}: first segment {first} is neither {{environment}} "
                 "nor a reserved name")
        if method == "get":
            if fields["body"]:
                fail(f"{where}: GET with body fields")
            if writes != "false":
                fail(f"{where}: GET that writes")
        elif writes != "true":
            fail(f"{where}: {method} that does not write")
        reads_by_name = tool.startswith(READ_PREFIXES) and "_or_" not in tool
        if writes == "false" and not reads_by_name:
            fail(f"{where}: read {tool} has no read prefix {READ_PREFIXES}")
        if writes == "true" and reads_by_name:
            fail(f"{where}: write {tool} is named as a read")
        expected_caller = "subject" if writes == "true" else "view"
        if p.get("caller") != expected_caller:
            fail(f"{where}: :caller: {p.get('caller')}, want "
                 f"{expected_caller}")
        hints = {}
        for k in ("destructive", "idempotent"):
            if writes == "true" and p.get(k) not in ("true", "false"):
                fail(f"{where}: a write states :{k}: true or false")
            if writes == "false" and k in p:
                fail(f"{where}: :{k}: on a read")
            hints[k] = p.get(k, "-")
        if method == "put" and p.get("idempotent") != "true":
            fail(f"{where}: a PUT is idempotent")
        if "max_body_size" in p and (not fields["body"]
                                     or not p["max_body_size"].isdigit()):
            fail(f"{where}: :max_body_size: belongs on a route with a body, "
                 "in bytes")
        if any("encoding" in f["props"] for f in fields["body"]) \
                and "max_body_size" not in p:
            fail(f"{where}: an encoded body needs :max_body_size:")
        routes.append({
            "where": where, "tool": tool, "method": method, "path": path,
            "area": p.get("area"), "entity": p.get("entity"),
            "writes": writes, "hints": hints,
            "no_body": method != "get" and not fields["body"],
            "body": [f["name"] for f in fields["body"]],
            "query": [f["name"] for f in fields["query"]],
            "required": [f["name"] for f in m["fields"] if required(f)],
            "response": p.get("response", "204"),
        })

    error = msgs.get("error_response")
    if not error:
        fail("no error_response message")
    else:
        got = {f["name"]: f["props"] for f in error["fields"]}
        if list(got) != ["code", "message"]:
            fail(f"error_response members {list(got)}, want code, message")
        elif got["code"].get("values", "").split() != ERROR_CODES:
            fail(f"error_response.code :values: {got['code'].get('values')}"
                 f", want {' '.join(ERROR_CODES)}")

    for key, label in (("tool", "tool"), (("method", "path"), "route")):
        seen = {}
        for r in routes:
            k = tuple(r[x] for x in key) if isinstance(key, tuple) else r[key]
            if k in seen:
                fail(f"{label} {k} shared by {seen[k]} and {r['where']}")
            seen[k] = r["where"]
    return routes


def main(root):
    failed = []

    def fail(text):
        failed.append(text)

    msgs = models(pathlib.Path(root))
    routes = check(msgs, fail)
    for r in routes:
        hints = ("readOnly" if r["writes"] == "false" else
                 f"destructive={r['hints']['destructive']} "
                 f"idempotent={r['hints']['idempotent']}")
        print(f"{r['tool']:22} {r['method']:5} {r['path']:50} "
              f"{r['area']:9} {r['entity']:11} {hints}")
        print(f"{'':22} body={r['body'] or '-'} query={r['query'] or '-'} "
              f"required={r['required'] or '-'} "
              f"takes_no_body={str(r['no_body']).lower()} "
              f"response={r['response']}")
    print(f"{len(msgs)} messages, {len(routes)} routes and tools, "
          f"{len(failed)} failures")
    for text in failed:
        print(f"FAIL  {text}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
