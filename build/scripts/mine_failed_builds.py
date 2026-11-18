#!/usr/bin/env python3
"""Mine ORE Studio's failed GitHub Actions runs and classify why they failed.

fetch lists every run that concluded failure or startup_failure between
two dates, fetches the jobs of each and the log of each failed job, and
keeps on disk only what classification needs: a trimmed record of each run
and job, and of each log the failing step's tail plus the lines that look
like errors. The other commands read the cache and never the network, so
the patterns can be reworked without refetching.

The fetch is deliberately gentle. Each worker sleeps before every request,
`gh api rate_limit` is read every few requests and the fetch waits for the
reset when the budget is low, and 403 and 429 back off. Every listing, job
list and log is cached, so a rerun fetches only runs it has not seen.

report reduces each failed job to its failing step and first real error,
normalises the error into a reason, matches it against PATTERNS for a
pattern and a verdict, and rewrites the generated blocks of the two
evidence pages, investigation_failed_builds_by_day.org and
investigation_failed_builds_summary.org.

Cache layout:
  runs/<day>.json        the failed runs created on that UTC day
  jobs/<run_id>.json     the jobs of the run's latest attempt, with steps
  logs/<job_id>.txt.gz   the trimmed log of a failed job
  logs/<job_id>.missing  the log could not be fetched (expired, 404)

Usage:
  python3 -I build/scripts/mine_failed_builds.py [--cache DIR] fetch \\
      [--repo OreStudio/OreStudio] [--since 2026-09-19] [--until 2026-11-18] \\
      [--delay 1.0] [--workers 1]
  python3 -I build/scripts/mine_failed_builds.py [--cache DIR] stats
  python3 -I build/scripts/mine_failed_builds.py [--cache DIR] survey \\
      [--unclassified]
  python3 -I build/scripts/mine_failed_builds.py [--cache DIR] report \\
      [--out-dir DIR] [--clone PATH-TO-A-CLONE]

Requires the gh CLI, authenticated, for fetch. Stdlib only.
"""

import argparse
import gzip
import json
import os
import re
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import date, datetime, timedelta, timezone
from pathlib import Path

CONCLUSIONS = ("failure", "startup_failure")
PAGE_SIZE = 100
TAIL_LINES = 400
MAX_MATCHED_LINES = 200
MAX_MATCHED_AFTER = 50
RATE_CHECK_EVERY = 25
RATE_FLOOR = 300
MAX_RETRIES = 6
REQUEST_TIMEOUT = 300

TIMESTAMP = re.compile(r"^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d(\.\d+)?Z ")
ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
ERROR_LINE = re.compile(
    r"##\[error\]|\berror\b|\bfatal\b|FAILED|Traceback|Exception|"
    r"\bfailed\b|undefined reference|No such file|not found|Assertion|"
    r"drift|differ|mismatch",
    re.IGNORECASE)

RUN_FIELDS = ("id", "name", "workflow_id", "event", "head_branch", "head_sha",
              "conclusion", "created_at", "run_started_at", "updated_at",
              "html_url", "run_attempt", "display_title", "path")
JOB_FIELDS = ("id", "run_id", "name", "conclusion", "started_at",
              "completed_at", "html_url", "runner_name", "labels")
STEP_FIELDS = ("number", "name", "conclusion", "started_at", "completed_at")


def log(message):
    stamp = datetime.now().strftime("%H:%M:%S")
    print(f"[{stamp}] {message}", file=sys.stderr, flush=True)


def pick(record, fields):
    return {f: record.get(f) for f in fields}


class GitHub:
    """The one place that talks to GitHub: throttled, budgeted, retried."""

    def __init__(self, repo, delay):
        self.repo = repo
        self.delay = delay
        self.requests = 0
        self.lock = threading.Lock()

    def _count(self):
        with self.lock:
            self.requests += 1
            return self.requests

    def _gh(self, args):
        """Run gh; a call that hangs counts as a failed one and is retried."""
        try:
            return subprocess.run(["gh", *args], capture_output=True,
                                  timeout=REQUEST_TIMEOUT)
        except subprocess.TimeoutExpired:
            return subprocess.CompletedProcess(
                args, 1, b"", f"timed out after {REQUEST_TIMEOUT}s".encode())

    def _wait_for_budget(self):
        result = self._gh(["api", "rate_limit"])
        if result.returncode != 0:
            return
        core = json.loads(result.stdout)["resources"]["core"]
        if core["remaining"] >= RATE_FLOOR:
            return
        pause = max(0, core["reset"] - time.time()) + 10
        log(f"rate budget low ({core['remaining']} left); "
            f"waiting {pause:.0f}s for the reset")
        time.sleep(pause)

    def get(self, endpoint):
        """Return the body of GET /repos/<repo>/<endpoint>, or None on 404/410."""
        for attempt in range(MAX_RETRIES):
            if self._count() % RATE_CHECK_EVERY == 0:
                self._wait_for_budget()
            time.sleep(self.delay)
            result = self._gh(["api", "--allow-escape-sequences",
                               f"repos/{self.repo}/{endpoint}"])
            if result.returncode == 0:
                return result.stdout
            err = result.stderr.decode(errors="replace")
            if "HTTP 404" in err or "HTTP 410" in err:
                return None
            if "HTTP 403" in err or "HTTP 429" in err or "rate limit" in err:
                pause = 60 * 2 ** attempt
                log(f"throttled on {endpoint}; backing off {pause}s")
                time.sleep(pause)
                self._wait_for_budget()
                continue
            pause = 5 * 2 ** attempt
            log(f"error on {endpoint}: {err.strip()[:200]}; retry in {pause}s")
            time.sleep(pause)
        raise RuntimeError(f"giving up on {endpoint}")

    def get_json(self, endpoint):
        body = self.get(endpoint)
        return None if body is None else json.loads(body)


def write_atomic(path, data):
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_bytes(data)
    tmp.replace(path)


def day_is_final(day, fetched_at):
    """A day's listing is final once fetched two days after it ended.

    A run created late on a day can still be running the next morning,
    so a listing taken too soon may miss failures that conclude later.
    """
    settled = datetime.combine(day + timedelta(days=2), datetime.min.time(),
                               tzinfo=timezone.utc)
    return datetime.fromisoformat(fetched_at) >= settled


def list_day(github, cache, day):
    path = cache / "runs" / f"{day.isoformat()}.json"
    if path.exists():
        cached = json.loads(path.read_text())
        if day_is_final(day, cached["fetched_at"]):
            return cached["runs"]
    runs = []
    for conclusion in CONCLUSIONS:
        page = 1
        while True:
            body = github.get_json(
                f"actions/runs?status={conclusion}&created={day.isoformat()}"
                f"&per_page={PAGE_SIZE}&page={page}")
            batch = body["workflow_runs"] if body else []
            runs.extend(pick(r, RUN_FIELDS) for r in batch)
            if len(batch) < PAGE_SIZE:
                break
            page += 1
    fetched_at = datetime.now(timezone.utc).isoformat()
    write_atomic(path, json.dumps({"day": day.isoformat(),
                                   "fetched_at": fetched_at,
                                   "runs": runs}, indent=1).encode())
    return runs


def fetch_jobs(github, cache, run_id):
    path = cache / "jobs" / f"{run_id}.json"
    if path.exists():
        return json.loads(path.read_text())
    jobs = []
    page = 1
    while True:
        body = github.get_json(f"actions/runs/{run_id}/jobs?filter=latest"
                               f"&per_page={PAGE_SIZE}&page={page}")
        batch = body["jobs"] if body else []
        for job in batch:
            record = pick(job, JOB_FIELDS)
            record["steps"] = [pick(s, STEP_FIELDS)
                               for s in job.get("steps") or []]
            jobs.append(record)
        if len(batch) < PAGE_SIZE:
            break
        page += 1
    write_atomic(path, json.dumps(jobs, indent=1).encode())
    return jobs


def trim_log(text):
    """Keep the failing step's tail and the other lines that look like errors.

    The tail ends a few lines after the first ##[error] annotation: GitHub
    writes it inside the first failing step, and the steps that run after
    a failure (cache saves, clean-up) add errors of their own that are
    noise. A log with no annotation keeps its last lines.
    """
    lines = [ANSI.sub("", TIMESTAMP.sub("", line))
             for line in text.splitlines()]
    end = len(lines)
    for i, line in enumerate(lines):
        if "##[error]" in line:
            end = min(len(lines), i + 5)
            break
    start = max(0, end - TAIL_LINES)
    before = [f"{i + 1}: {lines[i]}" for i in range(start)
              if ERROR_LINE.search(lines[i])]
    after = [f"{i + 1}: {lines[i]}" for i in range(end, len(lines))
             if ERROR_LINE.search(lines[i])]
    matched = (before[-(MAX_MATCHED_LINES - MAX_MATCHED_AFTER):]
               + after[:MAX_MATCHED_AFTER])
    out = []
    if matched:
        out.append("=== matched lines outside the tail ===")
        out.extend(matched)
    out.append(f"=== tail: lines {start + 1}..{end} of {len(lines)} ===")
    out.extend(lines[start:end])
    return "\n".join(out) + "\n"


def fetch_log(github, cache, job_id):
    path = cache / "logs" / f"{job_id}.txt.gz"
    missing = cache / "logs" / f"{job_id}.missing"
    if path.exists() or missing.exists():
        return
    try:
        body = github.get(f"actions/jobs/{job_id}/logs")
    except RuntimeError as error:
        missing.write_text(f"{error}\n")
        return
    if body is None:
        missing.write_text("unavailable\n")
        return
    trimmed = trim_log(body.decode(errors="replace"))
    write_atomic(path, gzip.compress(trimmed.encode()))


def days_between(since, until):
    day = since
    while day <= until:
        yield day
        day += timedelta(days=1)


def cache_bytes(cache):
    return sum(p.stat().st_size for p in cache.rglob("*") if p.is_file())


def fetch(args):
    cache = Path(args.cache)
    for sub in ("runs", "jobs", "logs"):
        (cache / sub).mkdir(parents=True, exist_ok=True)
    github = GitHub(args.repo, args.delay)
    days = list(days_between(args.since, args.until))
    runs = []
    for day in days:
        runs.extend(list_day(github, cache, day))
    log(f"{len(runs)} failed runs over {len(days)} days "
        f"({github.requests} requests so far)")

    def fetch_run(run):
        """Fetch one run; a run GitHub keeps refusing is left for a rerun."""
        try:
            for job in fetch_jobs(github, cache, run["id"]):
                if job["conclusion"] == "failure":
                    fetch_log(github, cache, job["id"])
        except RuntimeError as error:
            log(f"run {run['id']} left for a rerun: {error}")

    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = [pool.submit(fetch_run, run) for run in runs]
        for n, future in enumerate(as_completed(futures), 1):
            future.result()
            if n % 50 == 0:
                log(f"{n}/{len(runs)} runs, {github.requests} requests, "
                    f"cache {cache_bytes(cache) / 1e6:.1f} MB")
    log(f"done: {len(runs)} runs, {github.requests} requests, "
        f"cache {cache_bytes(cache) / 1e6:.1f} MB")


def stats(args):
    cache = Path(args.cache)
    runs = [r for p in sorted((cache / "runs").glob("*.json"))
            for r in json.loads(p.read_text())["runs"]]
    job_files = list((cache / "jobs").glob("*.json"))
    jobs = [j for p in job_files for j in json.loads(p.read_text())]
    failed = [j for j in jobs if j["conclusion"] == "failure"]
    logs = list((cache / "logs").glob("*.txt.gz"))
    missing = list((cache / "logs").glob("*.missing"))
    print(f"runs listed:       {len(runs)}")
    print(f"runs with jobs:    {len(job_files)}")
    print(f"jobs:              {len(jobs)}")
    print(f"failed jobs:       {len(failed)}")
    print(f"logs cached:       {len(logs)}")
    print(f"logs unavailable:  {len(missing)}")
    print(f"cache size:        {cache_bytes(cache) / 1e6:.1f} MB")


# Classification. Everything below reads the cache only.

# A line that only points at an error printed earlier.
POINTER = re.compile(
    r"Process completed with exit code|Build completed with failures|"
    r"^FAILED: |ninja: build stopped|^\s*\d+ or more Compiler errors|"
    r"Error\(s\) when building project|^Error: Process completed")
# The warning CTest.cmake prints for the phase that failed.
CTEST_PHASE = re.compile(r"(Build|Tests) failed with error code|"
                         r"Coverage collection encountered errors|"
                         r"Failed to configure|Memcheck")
# pytest's summary line for a failed test, which names its file.
PYTEST_FAILED = re.compile(r"^FAILED (\S+?\.py)::")
# Error signatures in priority order; the first tier with a hit wins.
TIERS = [
    re.compile(r"out of memory|No space left on device|Killed signal|"
               r"internal compiler error|"
               r"lost communication with the server|"
               r"The operation was canceled|exceeded the maximum execution|"
               r"rate limit|Could not resolve host|Connection (reset|refused|"
               r"timed out)|\b50[234]\b.*(Gateway|Unavailable)|"
               r"runner has received a shutdown signal|has timed out after", re.I),
    re.compile(r"^FAILED \S+::"),
    re.compile(r"^(WARNING|ERROR) \[[A-Z]+_\d+\]"),
    re.compile(r"\berror:|\berror (C|LNK|TS)\d{4}|fatal error|"
               r"undefined reference|undefined symbol|"
               r"LINK : fatal|^ld: (?!warning)|lock file's|"
               r"npm error (code E(?!USAGE)|ERESOLVE)", re.I),
    re.compile(r"The following tests FAILED"),
    re.compile(r"^E\s{2,}\S|^FAILED\b|AssertionError|\w+(Error|Exception): |"
               r"Traceback|\bFAIL(ED)?\b|✗|❌|Build failed:", re.I),
    CTEST_PHASE,
    re.compile(r"CMake Error"),
    re.compile(r"drift|differ|would change|out of date|stale|mismatch|"
               r"not found|missing|violation", re.I),
]
EXCEPTION_LINE = re.compile(r"^\s*[\w.]+(Error|Exception|Exit)\b")
# Lines that are never the error: echoed commands, step markers, warnings
# other than CTest's phase warnings, test timings, progress and app logging.
NOISE = re.compile(r"^\s*(echo|print\(|printf)\b|::error::|"
                   r"^##\[(group|endgroup)|"
                   r"^##\[warning\](?!\s*(Build failed|Tests failed|"
                   r"Coverage collection|Failed to configure|Memcheck))|"
                   r"^\[notice\]|^\d+: \d+\.\d+ s: |"
                   r"Failed distributed compilations|Submit failed, waiting|"
                   r"^\s*\[\d+/\d+\] |\[0x[0-9a-f]+\] \[\w+\] ")

NORMALISE = [
    (re.compile(r"(/home/runner/work|/Users/runner/work|[A-Z]:\\a)"
                r"[\\/][^\s:'\"()]*"), "<path>"),
    (re.compile(r"[A-Za-z]:(\\[\w.@+-]+)+\\?"), "<path>"),
    (re.compile(r"(?<![\w<])(/[\w.@+-]+){2,}/?"), "<path>"),
    (re.compile(r"\b[\w.-]+(/[\w.@+-]+)+"), "<path>"),
    (re.compile(r"\d{4}-\d\d-\d\d[T ]\d\d:\d\d:\d\d(\.\d+)?Z?"), "<time>"),
    (re.compile(r"\b[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-"
                r"[0-9a-f]{12}\b", re.I), "<uuid>"),
    (re.compile(r"\b[0-9a-f]{7,40}\b"), "<sha>"),
    (re.compile(r"(:\d+)+\b"), ""),
    (re.compile(r"\(\d+(,\d+)?\)"), ""),
    (re.compile(r"\b\d+(\.\d+)*\b"), "N"),
    (re.compile(r"\s+"), " "),
]
PATH_RULES = 4
# A log with no ##[error] at all was cut off: the job was stopped mid-step.
STOPPED = "(the log ends mid-step with no error: the job was stopped)"
REASON_WIDTH = 140


def failing_regions(text):
    """The output of the first failing step, nearest part first.

    The tail ends a few lines after the first ##[error]; the step that
    wrote it starts at the last ##[group]Run before it, and its output at
    the ##[endgroup] that closes the echoed script and environment. When
    the step started before the tail, its output is the whole tail, then
    the error lines kept from before the tail. Those come second because
    they may belong to earlier steps that failed without stopping the job.
    """
    head, _, tail = text.partition("=== tail:")
    first = int(re.match(r" lines (\d+)", tail).group(1)) if tail else 1
    tail = tail.splitlines()[1:]
    error = next((i for i, l in enumerate(tail) if "##[error]" in l),
                 len(tail) - 1)
    start = next((i for i in range(error, -1, -1)
                  if tail[i].startswith("##[group]Run ")), None)
    end = next((i for i in range(error + 1, len(tail))
                if tail[i].startswith(("##[group]Run ", "Post job cleanup"))),
               len(tail))
    if start is None:
        numbered = [l.split(": ", 1) for l in head.splitlines()[1:]]
        before = [line for n, line in numbered if int(n) < first]
        return [tail[:end], before]
    output = next((i + 1 for i in range(start, error)
                   if tail[i].startswith("##[endgroup]")), start)
    return [tail[output:end]]


def refine(region, line, following):
    """Replace a line that points at an error by the error it points at."""
    if "Traceback" in line:
        return next((l for l in following if EXCEPTION_LINE.match(l)), line)
    if "C2220" in line:
        return next((l.replace("##[warning]", "") for l in region
                     if re.search(r"warning C\d{4}", l)), line)
    if PYTEST_FAILED.match(line):
        files = [PYTEST_FAILED.match(l).group(1) for l in [line, *following]
                 if PYTEST_FAILED.match(l)]
        names = dict.fromkeys(Path(f).stem for f in files)
        return "pytest failed: " + ", ".join(names)
    if "The following tests FAILED" in line:
        failed = []
        for later in following:
            m = re.match(r"^\s*\d+ - (\S+) \((\w+)\)", later)
            if not m:
                break
            failed.append(f"{m.group(1)} ({m.group(2)})")
        return "tests failed: " + ", ".join(failed) if failed else line
    if line.startswith("CMake Error") and following:
        return f"CMake Error: {following[0].strip()}"
    if re.match(r"^(WARNING|ERROR) \[", line) and following:
        code = re.match(r"^\w+ \[(\w+)\]", line).group(1)
        return f"{code} {following[0].strip()}"
    return line


def first_error(text):
    if "##[error]" not in text:
        return STOPPED
    regions = failing_regions(text)
    for region in regions:
        body = [re.sub(r"##\[(error|warning)\]", "", l).strip()
                for l in region if l.strip() and not NOISE.search(l)]
        body = [l for l in body if l and not POINTER.search(l)]
        # Paths are stripped first: a file named for a failure is not one.
        bare = [without_paths(l) for l in body]
        for tier in TIERS:
            for i, line in enumerate(body):
                if tier.search(bare[i]):
                    return refine(region, line, body[i + 1:]).strip()
    lines = [re.sub(r"##\[(error|warning)\]", "", l).strip()
             for l in regions[0] if l.strip() and not NOISE.search(l)]
    said = [l for l in lines if l and not POINTER.search(l)]
    if said:
        return said[-1]
    return lines[0] if lines else "(no output before the error)"


def without_paths(line):
    for rx, sub in NORMALISE[:PATH_RULES]:
        line = rx.sub(sub, line)
    return line


def normalise(line):
    for rx, sub in NORMALISE:
        line = rx.sub(sub, line)
    return line.strip()[:REASON_WIDTH]


CAUGHT = "already caught"
COULD = "could be caught"
NOT = "not catchable"
UNCLASSIFIED = "unclassified"


class Pattern:
    """A named kind of failure and compass's verdict on it.

    workflow, step and error are regexes over the run's workflow name, the
    failing step's name and the first error; a pattern matches when every
    one it sets matches. detail names the codec (already caught), the data
    a check would read (could be caught) or the reason (not catchable).
    home says, for a catchable pattern, whether the check needs ORE Studio
    knowledge in compass's code or can be stated as project data.
    """

    def __init__(self, name, verdict, detail, workflow=None, step=None,
                 error=None, home="", guess=False):
        self.name = name
        self.verdict = verdict
        self.detail = detail
        self.home = home
        self.guess = guess
        self.rx = [(field, re.compile(rx, re.I))
                   for field, rx in (("workflow", workflow), ("step", step),
                                     ("error", error)) if rx]

    def matches(self, failure):
        return all(rx.search(failure[field]) for field, rx in self.rx)

    @property
    def label(self):
        return f"{self.verdict} (guess)" if self.guess else self.verdict


INFRA = "runner or GitHub infrastructure; nothing in the tree predicts it"
TEST = "behaviour only running the code shows"
COMPILER = "a compiler diagnostic; compass reads C++ facts but does not type-check"
DATA = "project data"
ORE = "ORE code"
P = Pattern
# First match wins, so specific patterns precede general ones. Gate steps are
# matched by name, since a gate's step names the rule it enforces.
PATTERNS = [
    # Infrastructure, network and limits.
    P("runner out of memory", NOT, "the compiler ran out of memory or "
      "crashed on the runner", error=r"out of memory|Allocation failed|"
      r"Killed signal|internal compiler error|heap space|C1060"),
    P("runner shut down or lost", NOT, INFRA,
      error=r"shutdown signal|lost communication|The operation was canceled"),
    P("disk full", NOT, INFRA, error=r"No space left on device|"
      r"ld: write\(\) failed"),
    P("network or service error", NOT, "a network fetch or remote service "
      "failed", error=r"Could not resolve host|Connection (reset|refused|"
      r"timed out)|\b50[234]\b|Cache service responded|rate limit|"
      r"TLS handshake|Failed to (download|fetch|connect)|Unable to connect|"
      r"Could not connect|ECONNRESET|ETIMEDOUT|EAI_AGAIN|resolve host|"
      r"archive contents unavailable|Download timed out|"
      r"Failed to get ID Token|"
      r"\bForbidden\b"),
    P("job timed out", NOT, "the job hit its time limit",
      error=r"job timed_out|exceeded the maximum execution|"
      r"has timed out after"),
    P("job stopped mid-step", NOT, "the log stops with no error; the job "
      "hit its time limit or lost its runner", error=r"log ends mid-step",
      guess=True),
    P("build cache tool failed", NOT, "sccache failed on the runner",
      error=r"sccache: encountered fatal error"),
    P("bot action failed", NOT, "a third-party bot (Claude Code review, "
      "Dependabot) failed; it does not build the tree",
      step=r"Run Claude Code|Run Dependabot"),
    P("checkout of a vanished commit", NOT, "the commit a run was asked "
      "to check out no longer exists", error=r"could not be found",
      step=r"checkout", guess=True),
    P("job cancelled", NOT, "a sibling job failed or a newer run "
      "superseded it", error=r"job cancelled"),
    P("coverage collection error", NOT, "gcov could not match coverage "
      "data to sources; a tooling fault", error=r"Coverage collection"),
    P("log unavailable", UNCLASSIFIED, "the job's log could not be fetched",
      error=r"\(log unavailable\)"),
    P("run started no job", COULD, "workflow files: a run that starts no "
      "job has an invalid workflow or a job condition no event meets",
      error=r"with no job", home=DATA, guess=True),

    # ORE Studio's code generator and its gates. Every one of these is a
    # compass codec today, so a compass check before the push catches it.
    P("generated output drifted from its model", CAUGHT,
      "codegen and component_files (diff mode)",
      step=r"generated output matches the models|Sweep every catalogue "
      r"component|component drift"),
    P("ER diagram stale", CAUGHT, "er_diagram", step=r"ER diagram"),
    P("SQL not re-runnable or malformed", CAUGHT, "sql_hygiene",
      step=r"unterminated comments|SQL hygiene"),
    P("protocol header without TypeScript twin", CAUGHT, "protocol_twins",
      step=r"TypeScript twin"),
    P("model contradicts its profile", CAUGHT, "model_rules",
      step=r"models agree with the profiles"),
    P("pending-change model not derived", CAUGHT, "pending_models",
      step=r"pending-change model"),
    P("handler permission not seeded", CAUGHT, "handler_permissions",
      step=r"handler permission"),
    P("open read not allow-listed", CAUGHT, "open_reads",
      step=r"open read"),
    P("shell menu name collision", CAUGHT, "shell_menus",
      step=r"shell menu"),
    P("HTTP route without auth position", CAUGHT, "route_auth",
      step=r"HTTP route"),
    P("recipe inventory stale", CAUGHT, "recipe_inventories",
      step=r"recipe inventory"),
    P("accepted exception malformed", CAUGHT, "registry_exceptions",
      step=r"accepted exception"),
    P("test case inside a conditional block", CAUGHT,
      "test_case_reachability", step=r"conditional block"),
    P("shell script differs from its recipe", CAUGHT, "tangle_check",
      step=r"scripts match their recipe|Tangle shell"),
    P("template differs from its org source", CAUGHT, "tangle_check",
      step=r"templates match their org", guess=True),
    P("core links another component's core", CAUGHT, "core_link_edges",
      step=r"core links another"),
    P("entity table written outside its repository", CAUGHT,
      "entity_writes", workflow=r"Entity-table writes"),
    P("event registrar not composed", CAUGHT, "event_registrars",
      workflow=r"Event registrar composition"),
    P("history registrar not composed", CAUGHT, "history_registrars",
      workflow=r"History-provider registrar"),
    P("shell command unit not registered", CAUGHT,
      "shell_command_composition", workflow=r"Shell command composition"),
    P("shell recipe without provenance", CAUGHT, "shell_recipe_provenance",
      workflow=r"Shell recipe provenance"),
    P("RLS policy widens to the system tenant", CAUGHT, "rls_widenings",
      workflow=r"RLS system tenant"),
    P("stop type outside the platform", CAUGHT, "portable_stop",
      workflow=r"Portable stop types"),
    P("populate reference unresolved", CAUGHT, "populate_refs",
      workflow=r"Populate reference drift"),
    P("Boost header without a vcpkg port", COULD, "C++ facts and "
      "vcpkg.json: every Boost header a source includes has its port "
      "declared", step=r"Boost header has a vcpkg port",
      home=f"{DATA} (the header-to-port map)"),
    P("version out of step", COULD, "package manifests and the product "
      "version: every manifest carries the product version",
      error=r"version is .*product version", home=DATA),
    P("exported type not compiled by its library", CAUGHT,
      "exported_types (uncompiled_exported_type)",
      step=r"exported type is compiled"),
    P("npm lock file out of step", COULD, "package manifests: every "
      "package.json range is satisfied by its lock file",
      error=r"lock file's", home=DATA),
    P("pasted block names a missing symbol", COULD, "org models and C++ "
      "sources: every symbol a pasted block names exists",
      step=r"pasted block", home=DATA),
    # The codegen suite is pytest. Many of its tests are tree checks, so a
    # failure is classified by the first entry below whose test failed.
    P("suite: handler permission not seeded", CAUGHT, "handler_permissions",
      step=r"codegen suite", error=r"\btest_handler_permissions"),
    P("suite: event registrar not composed", CAUGHT, "event_registrars",
      step=r"codegen suite",
      error=r"\btest_check_event_registrar_composition"),
    P("suite: history registrar not composed", CAUGHT, "history_registrars",
      step=r"codegen suite",
      error=r"\btest_check_history_registrar_composition"),
    P("suite: shell command unit not registered", CAUGHT,
      "shell_command_composition", step=r"codegen suite",
      error=r"\btest_check_shell_command_composition"),
    P("suite: SQL file unreachable", CAUGHT, "sql_reachability",
      step=r"codegen suite", error=r"\btest_sql_reachability"),
    P("suite: subject outside its protocol", CAUGHT, "subject_conformance",
      step=r"codegen suite", error=r"\btest_check_subject_conformance"),
    P("suite: translation unit over budget", CAUGHT,
      "translation_unit_budget", step=r"codegen suite",
      error=r"\btest_translation_unit_budget"),
    P("suite: test case in a conditional block", CAUGHT,
      "test_case_reachability", step=r"codegen suite",
      error=r"\btest_test_case_reachability"),
    P("suite: workflow triggers on pull requests", CAUGHT, "no_pr_triggers",
      step=r"codegen suite", error=r"\btest_check_no_pr_triggers"),
    P("suite: stop type outside the platform", CAUGHT, "portable_stop",
      step=r"codegen suite", error=r"\btest_portable_stop_types"),
    P("suite: open read not allow-listed", CAUGHT, "open_reads",
      step=r"codegen suite", error=r"\btest_open_reads"),
    P("suite: trade type routed twice", CAUGHT, "trade_type_coverage",
      step=r"codegen suite", error=r"\btest_trade_type_catalogue",
      guess=True),
    P("suite: domain header includes", COULD, "org models and generated "
      "C++: a generated header's declared standard includes are the ones "
      "its members use", step=r"codegen suite",
      error=r"\btest_domain_includes",
      home=f"{DATA} (the type-to-header map)"),
    P("suite: SQL identifier over 63 bytes", COULD, "org models and SQL "
      "facts: every identifier an entity derives fits PostgreSQL's 63 bytes",
      step=r"codegen suite", error=r"\btest_sql_name_base", home=DATA),
    P("suite: provisioning step words disagree", COULD, "C++ and "
      "TypeScript facts: the step labels the server declares are the "
      "sentences the screen catalogue translates", step=r"codegen suite",
      error=r"\btest_provision_step_words", home=ORE),
    P("suite: asset-class catalogue inconsistent", COULD, "org tables: "
      "the asset-class catalogue states each mapping once",
      step=r"codegen suite", error=r"\btest_asset_class_catalogue",
      home=ORE, guess=True),
    P("suite: TypeScript protocol disagrees with C++", COULD, "org models, "
      "C++ and TypeScript facts: the derived TypeScript messages are the "
      "C++ protocol's", step=r"codegen suite",
      error=r"\btest_entity_typescript", home=DATA, guess=True),
    P("codegen tool test failed", NOT, "a test of the generator itself; it "
      "tests the tool, not the tree", step=r"codegen suite",
      error=r"pytest failed"),
    P("codegen suite failed, test unknown", UNCLASSIFIED, "the suite failed "
      "but the log kept no FAILED line naming the test",
      step=r"codegen suite"),

    # Documentation gates.
    P("structure note not linked back", CAUGHT, "lint (missing_backlink)",
      step=r"structure notes"),
    P("page lists a non-pattern under Patterns used", CAUGHT,
      "lint (pattern_not_a_pattern)", error=r"not a service architecture "
      r"pattern"),
    P("filetag malformed", CAUGHT, "lint (bad_filetag)",
      error=r"filetags: \d+ violation"),
    P("prototype missing from its inventory", CAUGHT,
      "prototypes (unlisted)", step=r"prototype is in the inventory"),
    P("link target begins with a colon", CAUGHT, "org_links (colon_link)",
      step=r"link target begins with a colon"),
    P("dangling or malformed id link", CAUGHT,
      "lint (dangling_link, malformed_link)",
      error=r"(dangling|malformed) id link"),
    P("skill type tables stale", CAUGHT, "skill_type_tables",
      step=r"type tables are current"),
    P("skill catalogue stale", CAUGHT, "skill_catalogue",
      step=r"catalogue is current"),
    P("site id link does not resolve", CAUGHT,
      "lint (dangling_link, malformed_link)",
      error=r'Unable to resolve link: "[0-9A-Fa-f]{8}-[0-9A-Fa-f-]{20,}"'),
    P("site link target begins with a colon", CAUGHT,
      "org_links (colon_link)", error=r'Unable to resolve link: ":'),
    P("site link does not resolve", COULD, "org documents: every link "
      "target resolves the way the site export resolves it (fuzzy, file "
      "and custom links, such as a C++ attribute in double square brackets; id "
      "links are lint's)",
      error=r"Unable to resolve link", home=DATA),

    # The ER parser's validations, which the build workflows run first.
    P("SQL file not wired into create.sql", CAUGHT, "sql_reachability",
      error=r"^WIRE_001"),
    P("SQL table without row-level security", CAUGHT, "rls_coverage",
      error=r"^RLS_00[12]"),
    P("SQL table naming", COULD, "SQL facts: table names against the "
      "component prefixes", error=r"^NAM(E|ING)_\d+",
      home=f"{DATA} (the component prefixes)"),
    P("SQL table missing from drop scripts", COULD, "SQL facts: every "
      "created table is dropped", error=r"^DROP_\d+", home=DATA),
    P("SQL schema convention", COULD, "SQL facts: temporal, tenant and "
      "data-quality column conventions",
      error=r"^(TEMPORAL|TENANT|DQ|RLS)_\d+",
      home=f"{ORE} (ORE's temporal and tenancy conventions)"),

    # Database set-up before the tests.
    P("SQL unterminated block comment", CAUGHT,
      "sql_hygiene (unterminated_block_comment)",
      error=r"unterminated /\* comment"),
    P("SQL foreign key without unique target", COULD, "SQL facts: a "
      "foreign key references a primary or unique key",
      error=r"no unique constraint matching given keys", home=DATA),
    P("SQL syntax error", COULD, "SQL facts: the scripts parse as "
      "PostgreSQL with psql variables", error=r"syntax error at or near",
      home=DATA, guess=True),
    P("database role missing in CI", NOT, "the CI database lacks a role "
      "the scripts expect; environment set-up, not the tree",
      error=r"Account not found", guess=True),

    # C++ and TypeScript compilation.
    P("C++20 stop type outside the platform", CAUGHT, "portable_stop",
      error=r"'(stop_token|jthread|stop_source|stop_callback)'"),
    P("storage macro on an enum", CAUGHT,
      "exported_types (visibility_macro_on_enum)", error=r"__dllimport__' "
      r"attribute only applies", guess=True),
    P("standard library feature a platform lacks", COULD, "C++ facts: no "
      "use of a standard facility on a list of ones some platform's library "
      "lacks or deprecates (atomic shared_ptr, floating from_chars, getenv, "
      "sscanf, formatting std::quoted)",
      error=r"_Atomic cannot be applied|is_trivially_copyable<std::shared_ptr|"
      r"deleted function 'from_chars'|'(getenv|sscanf)' is deprecated|"
      r"formatter<std::__detail::_Quoted_string",
      home=f"{DATA} (the list of facilities)", guess=True),
    P("SQL include names a missing file", CAUGHT,
      "sql_reachability (missing_include)",
      error=r"^psql:.*No such file or directory"),
    P("include not found", COULD, "C++ facts and CMake: every #include "
      "resolves to a file in the tree or a declared dependency",
      error=r"file not found|Cannot open include file|No such file or "
      r"directory", home=f"{DATA} (the include roots)"),
    P("SQL object used before it is created", COULD, "SQL facts: a create "
      "script names only functions and tables a script before it creates",
      error=r"(function \w+\(.*\)|relation \"\w+\") does not exist",
      home=DATA),
    P("populate row violates its table's checks", COULD, "SQL facts: "
      "every populate row meets its table's check constraints and the "
      "validations its insert trigger applies",
      error=r"Invalid \w+: value cannot be|violates check constraint",
      home=f"{DATA} for check constraints; {ORE} for trigger validations",
      guess=True),
    P("packaging failed", NOT, "cpack failed after a successful build",
      error=r"CPack Error", guess=True),
    P("MSVC reflection depth limit", COULD, "C++ facts: the member count of "
      "every struct or variant reflect-cpp reflects, against a ceiling",
      error=r"C1202", home=f"{DATA} (the ceiling)", guess=True),
    P("undefined symbol at link", COULD, "CMake and C++ facts: a library "
      "that uses a symbol links the target that defines it, and an "
      "exported class is compiled into its library",
      error=r"undefined (symbol|reference)|error LNK2019|LNK2001",
      home=DATA, guess=True),
    P("missing standard include", COULD, "C++ facts: a file that names a "
      "standard type includes its header",
      error=r"no member named '\w+' in namespace 'std'|"
      r"'std::\w+': too few template arguments|is not a member of 'std'|"
      r"no template named '\w+' in namespace 'std'", home=DATA,
      guess=True),
    P("MSVC warning treated as error", NOT, COMPILER + "; MSVC's warnings "
      "differ from clang's and gcc's", error=r"warning C\d{4}"),
    P("TypeScript type error", NOT, "a tsc diagnostic; compass does not "
      "type-check TypeScript", error=r"error TS\d+"),
    P("TypeScript format", COULD, "hand-written TypeScript: prettier "
      "--check over the changed files, as compass's codegen already runs "
      "prettier over generated ones", step=r"Format check",
      home=f"{DATA} (the formatter, its style file and the globs)"),

    # Compile errors and tests, after the specific compile patterns above.
    # A failing test prints ERROR: from the database, so a compile error is
    # recognised by its location format, not by the word.
    P("C++ compile error", NOT, COMPILER,
      error=r":\d+(:\d+)?: (fatal )?error:|\(\d+(,\d+)?\): (fatal )?error|"
      r"error C\d{4}|\binclude\\\w+: error"),
    P("CTest test failed", NOT, TEST, step=r"CTest workflow|Run the tests",
      error=r"FAILED:|failed:|tests failed|unexpected exception|"
      r"REQUIRE|CHECK\(|The following tests FAILED|\*\*\*Failed|"
      r"\d+ failed\)"),
    P("agent contract out of step", COULD, "C++ and TypeScript facts: the "
      "backend contract the agent records matches the backend source",
      error=r"Backend contract source changed", home=ORE, guess=True),
    P("agent test failed", NOT, TEST, workflow=r"^Agent ",
      error=r"Subtest|not ok|AssertionError"),
    P("TypeScript test failed", NOT, TEST,
      error=r"npm error Lifecycle script .test. failed"),
    P("TypeScript test failed", NOT, TEST, workflow=r"TypeScript",
      step=r"tests"),
    P("other test failed", NOT, TEST, step=r"Host tests|Unit tests",
      guess=True),
    P("service failed to start", NOT, TEST + "; the stack must run",
      step=r"Start the services"),

]

FALLBACK = Pattern("unclassified", UNCLASSIFIED, "")


def classify(failure):
    return next((p for p in PATTERNS if p.matches(failure)), FALLBACK)


def job_minutes(job):
    start, end = job.get("started_at"), job.get("completed_at")
    if not start or not end:
        return 0.0
    delta = datetime.fromisoformat(end) - datetime.fromisoformat(start)
    return max(0.0, delta.total_seconds() / 60)


def load_failures(cache):
    """One record per failed job, or per run when no job failed."""
    failures = []
    run_count = 0
    job_count = 0
    for path in sorted((cache / "runs").glob("*.json")):
        for run in json.loads(path.read_text())["runs"]:
            jobs_path = cache / "jobs" / f"{run['id']}.json"
            if not jobs_path.exists():
                continue
            run_count += 1
            jobs = json.loads(jobs_path.read_text())
            job_count += len(jobs)
            failed = [j for j in jobs if j["conclusion"] == "failure"]
            if not failed:
                failed = [j for j in jobs
                          if j["conclusion"] not in ("success", "skipped")]
            for job in failed or [None]:
                failures.append(describe(cache, run, job))
    return failures, run_count, job_count


def describe(cache, run, job):
    record = {"run": run, "job": job, "day": run["created_at"][:10],
              "workflow": run["name"], "minutes": 0.0}
    if job is None:
        record.update(job_name="(no job ran)", step="(none)",
                      error=f"run concluded {run['conclusion']} with no job",
                      job_url=run["html_url"])
    else:
        step = next((s["name"] for s in job["steps"]
                     if s["conclusion"] == "failure"), None)
        if step is None:
            step = next((s["name"] for s in job["steps"]
                         if s["conclusion"] not in ("success", "skipped")),
                        "(no step)")
        record.update(job_name=job["name"], step=step,
                      minutes=job_minutes(job), job_url=job["html_url"])
        log_path = cache / "logs" / f"{job['id']}.txt.gz"
        if job["conclusion"] != "failure":
            record["error"] = f"job {job['conclusion']}"
        elif log_path.exists():
            record["error"] = first_error(
                gzip.decompress(log_path.read_bytes()).decode())
        else:
            record["error"] = "(log unavailable)"
    record["reason"] = normalise(record["error"])
    record["pattern"] = classify(record)
    return record


# Reporting. The two pages are hand-written around generated blocks; the
# report rewrites only what sits between a block's markers.

EVIDENCE = Path("doc/agile/versions/v0/sprint_01/catch_llm_error_patterns/"
                "evidence")
BY_DAY_PAGE = "investigation_failed_builds_by_day.org"
SUMMARY_PAGE = "investigation_failed_builds_summary.org"
SCRIPT = "build/scripts/mine_failed_builds.py"
LLM_TRAILER = re.compile(r"^Co-Authored-By:.*(Claude|anthropic|DeepSeek|GPT|"
                         r"openai|Gemini|Codex|Copilot|Qwen)", re.I | re.M)
VERDICT_ORDER = (CAUGHT, COULD, NOT, UNCLASSIFIED)


class Commits:
    """Reads head commits from a local clone of the mined repository."""

    def __init__(self, clone):
        self.clone = clone
        self.seen = {}

    def __call__(self, sha):
        if sha not in self.seen:
            self.seen[sha] = self._read(sha)
        return self.seen[sha]

    def _git(self, *args):
        result = subprocess.run(["git", "-C", self.clone, *args],
                                capture_output=True, text=True)
        return result.stdout if result.returncode == 0 else None

    def _read(self, sha):
        """The head commit; a merge is judged by the commits it merged."""
        if not self.clone:
            return None
        out = self._git("log", "-1", "--format=%P%x00%s%x00%B", sha)
        if out is None:
            return None
        parents, subject, body = out.split("\0", 2)
        parents = parents.split()
        bodies = [body]
        if len(parents) == 2:
            merged = self._git("log", "--format=%B%x1e",
                               f"{parents[0]}..{parents[1]}")
            bodies = (merged or "").split("\x1e")
        return {"sha": sha, "subject": subject, "merge": len(parents) == 2,
                "llm": any(LLM_TRAILER.search(b) for b in bodies)}


def cell(text):
    return str(text).replace("|", "\\vert{}").replace("\n", " ").strip()


def prose(text):
    """Log text in a table: no bracket link, so it cannot dangle."""
    return text.replace("[[", "[ [").replace("]]", "] ]")


def table(header, rows):
    out = ["| " + " | ".join(header) + " |",
           "|" + "+".join("-" * (len(h) + 2) for h in header) + "|"]
    out.extend("| " + " | ".join(cell(c) for c in row) + " |" for row in rows)
    return out


def run_link(run):
    return f"[[{run['html_url']}][{run['id']}]]"


def commit_cell(info, sha):
    if info is None:
        return f"={sha[:10]}= (not in the local clone)"
    if info["merge"]:
        llm = ("merges LLM co-authored commits" if info["llm"]
               else "merges no LLM-trailered commit")
    else:
        llm = "LLM co-authored" if info["llm"] else "no LLM trailer"
    return f"={sha[:10]}= {prose(info['subject'][:70])} ({llm})"


def replace_block(path, name, lines):
    begin = f"# BEGIN generated {name} ({SCRIPT})"
    end = f"# END generated {name}"
    text = path.read_text()
    pattern = re.compile(re.escape(begin) + r"\n.*?" + re.escape(end), re.S)
    if not pattern.search(text):
        sys.exit(f"{path}: no '{begin}' block")
    body = "\n".join([begin, *lines, end])
    path.write_text(pattern.sub(lambda _: body, text))


def by_day_lines(failures):
    """One table per day. Failures of one workflow, step, reason and
    pattern share a row, which links every run they came from."""
    days = {}
    for f in failures:
        key = (f["workflow"], f["run"]["event"], f["step"], f["reason"],
               f["pattern"].name)
        days.setdefault(f["day"], {}).setdefault(key, []).append(f)
    lines = []
    for day in sorted(days):
        groups = days[day]
        runs = {f["run"]["id"] for g in groups.values() for f in g}
        minutes = sum(f["minutes"] for g in groups.values() for f in g)
        lines += ["", f"** {day}", "",
                  f"{len(runs)} failed runs, {minutes:.0f} failed-job minutes.",
                  ""]
        body = []
        for (workflow, event, step, reason, _), group in groups.items():
            first = group[0]
            links = ", ".join(run_link(r) for r in
                              {f["run"]["id"]: f["run"] for f in group}.values())
            jobs = first["job_name"] if len(group) == 1 else (
                f"{first['job_name']} and {len(group) - 1} more")
            body.append([links, workflow, event, prose(jobs), prose(step),
                         prose(reason), first["pattern"].name,
                         first["pattern"].label])
        lines += table(["Runs", "Workflow", "Event", "Job", "Step", "Reason",
                        "Pattern", "Verdict"], body)
    return lines


def pattern_stats(failures):
    stats = {}
    for f in failures:
        s = stats.setdefault(f["pattern"].name, {
            "pattern": f["pattern"], "runs": set(), "jobs": 0, "minutes": 0.0,
            "heads": set(), "days": set(), "first": f})
        s["runs"].add(f["run"]["id"])
        s["jobs"] += 1
        s["minutes"] += f["minutes"]
        s["heads"].add(f["run"]["head_sha"])
        s["days"].add(f["day"])
        if example_key(f) < example_key(s["first"]):
            s["first"] = f
    return stats


def example_key(failure):
    """The earliest pull-request run first: its head is the branch's own
    commit, while a push or schedule run's head is whatever main was."""
    run = failure["run"]
    return (run["event"] != "pull_request", run["created_at"])


def ranking_lines(stats):
    ordered = sorted(stats.values(),
                     key=lambda s: (-len(s["runs"]), -s["minutes"]))
    rows = [[s["pattern"].name, s["pattern"].label, len(s["runs"]), s["jobs"],
             f"{s['minutes']:.0f}", len(s["heads"]), len(s["days"]),
             s["pattern"].detail] for s in ordered]
    lines = ["", "Ranked by failed runs.", ""]
    lines += table(["Pattern", "Verdict", "Runs", "Jobs", "Minutes", "Heads",
                    "Days", "Codec, data or reason"], rows)
    by_minutes = sorted(stats.values(), key=lambda s: -s["minutes"])[:15]
    lines += ["", "The fifteen costliest, ranked by failed-job minutes.", ""]
    lines += table(["Pattern", "Verdict", "Minutes", "Runs"],
                   [[s["pattern"].name, s["pattern"].label,
                     f"{s['minutes']:.0f}", len(s["runs"])]
                    for s in by_minutes])
    return lines


def verdict_lines(failures, stats):
    rows = []
    for verdict in VERDICT_ORDER:
        group = [s for s in stats.values() if s["pattern"].verdict == verdict]
        rows.append([verdict, len(group),
                     len({r for s in group for r in s["runs"]}),
                     sum(s["jobs"] for s in group),
                     f"{sum(s['minutes'] for s in group):.0f}"])
    return [""] + table(["Verdict", "Patterns", "Runs", "Jobs", "Minutes"],
                        rows)


def candidate_lines(stats, commits):
    """The catchable patterns, with the run and head commit they began at."""
    catchable = sorted((s for s in stats.values()
                        if s["pattern"].verdict in (COULD, CAUGHT)),
                       key=lambda s: (s["pattern"].verdict != COULD,
                                      -len(s["runs"])))
    rows = []
    for s in catchable:
        first = s["first"]["run"]
        event = "" if first["event"] == "pull_request" else f" ({first['event']})"
        infos = [commits(h) for h in s["heads"]]
        known = [i for i in infos if i is not None]
        llm = sum(i["llm"] for i in known)
        rows.append([s["pattern"].name, s["pattern"].label, len(s["runs"]),
                     run_link(first) + event,
                     commit_cell(commits(first["head_sha"]), first["head_sha"]),
                     f"{llm} of {len(known)}", s["pattern"].detail,
                     s["pattern"].home or "-"])
    return [""] + table(["Pattern", "Verdict", "Runs", "Example run",
                         "Its head commit", "LLM heads", "Codec or data",
                         "ORE code or project data"], rows)


def unclassified_lines(failures):
    counts = {}
    for f in failures:
        if f["pattern"] is FALLBACK:
            key = (f["workflow"], f["step"], f["reason"])
            counts.setdefault(key, []).append(f)
    rows = [[len(v), wf, prose(step), prose(reason), run_link(v[0]["run"])]
            for (wf, step, reason), v in sorted(counts.items(),
                                                key=lambda kv: -len(kv[1]))]
    if not rows:
        return ["", "Every failure matched a pattern."]
    return [""] + table(["Jobs", "Workflow", "Step", "Reason", "Example"],
                        rows)


def provenance_lines(cache, failures, runs, jobs):
    days = sorted(p.stem for p in (cache / "runs").glob("*.json"))
    listed = sum(len(json.loads(p.read_text())["runs"])
                 for p in (cache / "runs").glob("*.json"))
    logs = len(list((cache / "logs").glob("*.txt.gz")))
    missing = len(list((cache / "logs").glob("*.missing")))
    rows = [["Days listed", f"{days[0]} to {days[-1]} ({len(days)})"],
            ["Failed runs listed", listed],
            ["Runs whose jobs were fetched", runs],
            ["Jobs in those runs", jobs],
            ["Failures classified (failed jobs, or runs with none)",
             len(failures)],
            ["Logs cached", logs],
            ["Logs unavailable", missing],
            ["Cache size", f"{cache_bytes(cache) / 1e6:.1f} MB"]]
    return [""] + table(["What", "Count"], rows)


def report(args):
    cache = Path(args.cache)
    out = Path(args.out_dir)
    failures, runs, jobs = load_failures(cache)
    stats = pattern_stats(failures)
    commits = Commits(args.clone)
    replace_block(out / BY_DAY_PAGE, "failures_by_day", by_day_lines(failures))
    summary = out / SUMMARY_PAGE
    replace_block(summary, "provenance",
                  provenance_lines(cache, failures, runs, jobs))
    replace_block(summary, "verdicts", verdict_lines(failures, stats))
    replace_block(summary, "ranking", ranking_lines(stats))
    replace_block(summary, "candidates", candidate_lines(stats, commits))
    replace_block(summary, "unclassified", unclassified_lines(failures))
    log(f"{len(failures)} failures in {len(stats)} patterns written to {out}")


def survey(args):
    failures, runs, jobs = load_failures(Path(args.cache))
    counts = {}
    for f in failures:
        if args.unclassified and f["pattern"] is not FALLBACK:
            continue
        key = (f["pattern"].name, f["workflow"], f["step"][:50], f["reason"])
        counts[key] = counts.get(key, 0) + 1
    print(f"{runs} runs, {jobs} jobs, {len(failures)} failures")
    for (pattern, wf, step, reason), n in sorted(counts.items(),
                                                 key=lambda kv: -kv[1]):
        print(f"{n:4} | {pattern} | {wf} | {step} | {reason}")


def parse_day(text):
    return date.fromisoformat(text)


def main():
    today = datetime.now(timezone.utc).date()
    default_cache = Path(os.environ.get("TMPDIR", "/tmp")) / "mine_failed_builds"
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--cache", default=str(default_cache),
                        help="cache directory (default: %(default)s)")
    sub = parser.add_subparsers(dest="command", required=True)
    f = sub.add_parser("fetch", help="list failed runs and fetch jobs and logs")
    f.add_argument("--repo", default="OreStudio/OreStudio")
    f.add_argument("--since", type=parse_day,
                   default=today - timedelta(days=60))
    f.add_argument("--until", type=parse_day, default=today)
    f.add_argument("--delay", type=float, default=1.0,
                   help="seconds each worker sleeps before a request")
    f.add_argument("--workers", type=int, default=1,
                   help="concurrent fetchers; keep it small (default: 1)")
    f.set_defaults(func=fetch)
    s = sub.add_parser("stats", help="count what the cache holds")
    s.set_defaults(func=stats)
    v = sub.add_parser("survey", help="print failures grouped by reason")
    v.add_argument("--unclassified", action="store_true",
                   help="only the failures no pattern matches")
    v.set_defaults(func=survey)
    r = sub.add_parser("report", help="write the generated blocks of the "
                       "by-day and summary pages")
    r.add_argument("--out-dir", default=str(EVIDENCE))
    r.add_argument("--clone", default="",
                   help="a local clone of the repository, to read head "
                        "commits from")
    r.set_defaults(func=report)
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
