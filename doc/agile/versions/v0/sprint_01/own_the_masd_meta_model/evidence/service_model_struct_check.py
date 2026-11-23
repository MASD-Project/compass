"""Compares each message of compass.serve's operation models with the
hand-written struct of the same name in compass.db's types: member names in
order, types and defaults. A request the model splits from one struct is
compared with that struct.

Usage: python3 -I service_model_struct_check.py <repository root>
Exits 1 when a message and its struct differ.
"""
import pathlib
import re
import sys

SPLIT = {"sprint_state_request": "sprint_request",
         "what_needs_testing_request": "sprint_request"}


def structs(header):
    text = re.sub(r"/\*.*?\*/", "", header.read_text(), flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    out = {}
    for name, body in re.findall(r"struct (\w+) \{(.*?)\n\};", text, re.S):
        members = []
        for line in body.split(";"):
            line = " ".join(line.split())
            if not line:
                continue
            m = re.match(r"(.+?) (\w+)(?: = (.+)|\{.*\})?$", line)
            members.append((m.group(2), m.group(1), m.group(3)))
        out[name] = members
    return out


def messages(model):
    out, current, field = {}, None, None
    for line in model.read_text().splitlines():
        if line.startswith("* "):
            current = None
        elif line.startswith("** "):
            current = line[3:].strip()
            out[current] = []
        elif line.startswith("*** ") and current:
            field = [line[4:].strip(), None, None]
            out[current].append(field)
        elif current and out[current] and line.startswith(":cpp_type:"):
            out[current][-1][1] = line.split(None, 1)[1].strip()
        elif current and out[current] and line.startswith(":default:"):
            out[current][-1][2] = line.split(None, 1)[1].strip()
    return {k: [tuple(f) for f in v] for k, v in out.items()}


def main(root):
    root = pathlib.Path(root)
    cpp = {}
    for h in sorted((root / "projects/compass.db/include/compass.db/types")
                    .glob("*.hpp")):
        cpp.update(structs(h))
    failed = False
    for model in sorted((root / "projects/compass.serve/modeling")
                        .glob("compass.serve.*.org")):
        for name, fields in messages(model).items():
            struct = SPLIT.get(name, name)
            if struct not in cpp:
                print(f"new      {model.name} {name}")
                continue
            if fields == cpp[struct]:
                print(f"matches  {model.name} {name} = {struct} "
                      f"({len(fields)} members)")
            else:
                failed = True
                print(f"DIFFERS  {model.name} {name} vs {struct}")
                print(f"  model: {fields}")
                print(f"  c++:   {cpp[struct]}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
