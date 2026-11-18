#!/usr/bin/env python3
"""Mine a repository's failed GitHub Actions runs into a cache.

Lists every run that concluded failure or startup_failure between two
dates, fetches the jobs of each and the log of each failed job, and keeps
on disk only what classification needs: a trimmed record of each run and
job, and of each log the failing tail plus the lines that look like
errors. Classification reads the cache and never the network, so it can
be iterated on without refetching.

The fetch is deliberately gentle. It sleeps between requests, reads
`gh api rate_limit` every few requests and waits for the reset when the
budget is low, and backs off on 403 and 429. Every listing, job list and
log is cached, so a rerun fetches only runs it has not seen.

Cache layout:
  runs/<day>.json     the failed runs created on that UTC day
  jobs/<run_id>.json  the jobs of the run's latest attempt, with steps
  logs/<job_id>.txt.gz  the trimmed log of a failed job
  logs/<job_id>.missing  the log could not be fetched (expired, 404)

Usage:
  python3 -I build/scripts/mine_failed_builds.py fetch \\
      [--repo OreStudio/OreStudio] [--since 2026-09-19] [--until 2026-11-18] \\
      [--cache DIR] [--delay 1.0] [--workers 1]
  python3 -I build/scripts/mine_failed_builds.py stats [--cache DIR]

Requires the gh CLI, authenticated. Stdlib only.
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
from concurrent.futures import ThreadPoolExecutor
from datetime import date, datetime, timedelta, timezone
from pathlib import Path

CONCLUSIONS = ("failure", "startup_failure")
PAGE_SIZE = 100
TAIL_LINES = 400
MAX_MATCHED_LINES = 200
RATE_CHECK_EVERY = 25
RATE_FLOOR = 300
MAX_RETRIES = 6

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
        return subprocess.run(["gh", *args], capture_output=True)

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
    matched = [f"{i + 1}: {line}" for i, line in enumerate(lines)
               if not start <= i < end and ERROR_LINE.search(line)]
    out = []
    if matched:
        out.append("=== matched lines outside the tail ===")
        out.extend(matched[:MAX_MATCHED_LINES])
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
        for job in fetch_jobs(github, cache, run["id"]):
            if job["conclusion"] == "failure":
                fetch_log(github, cache, job["id"])

    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        for n, _ in enumerate(pool.map(fetch_run, runs), 1):
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
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
