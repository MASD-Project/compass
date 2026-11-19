"""
Tests for the release-notes pipeline: compass release-notes and the two
build/scripts it wraps.

Run with:  python -m pytest projects/compass.legacy/tests/test_release_notes.py -v
Git runs for real in a temporary repository whose origin is a local bare
repository; gh is a stub on PATH that records its arguments, so nothing
reaches GitHub.
"""

import json
import os
import subprocess
import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO_ROOT / "projects" / "compass.legacy" / "src"))
sys.path.insert(0, str(REPO_ROOT / "build" / "scripts"))

import compass_release_notes  # noqa: E402
import generate_release_notes  # noqa: E402

GH_STUB = """#!/bin/sh
echo "$*" >> "$GH_LOG"
if [ "$1 $2" = "release view" ]; then exit "$GH_RELEASE_VIEW_EXIT"; fi
if [ "$1 $2" = "pr list" ]; then echo "[]"; fi
exit 0
"""


def git(cwd, *args):
    return subprocess.run(["git", *args], cwd=cwd, check=True,
                          capture_output=True, text=True).stdout.strip()


@pytest.fixture
def repo(tmp_path, monkeypatch):
    origin = tmp_path / "origin.git"
    git(tmp_path, "init", "-q", "--bare", "-b", "main", str(origin))
    root = tmp_path / "work"
    root.mkdir()
    git(root, "init", "-q", "-b", "main")
    git(root, "config", "user.email", "test@example.com")
    git(root, "config", "user.name", "Test")
    git(root, "config", "tag.gpgSign", "false")
    git(root, "remote", "add", "origin", str(origin))
    (root / "CMakeLists.txt").write_text("project(compass VERSION 0.0.1 LANGUAGES CXX)\n")
    sprint = root / "doc" / "agile" / "versions" / "v0" / "sprint_01"
    sprint.mkdir(parents=True)
    (sprint / "release_notes.md").write_text("Notes.\n")
    git(root, "add", ".")
    git(root, "commit", "-q", "-m", "first")
    git(root, "push", "-q", "origin", "main")

    bin_dir = tmp_path / "bin"
    bin_dir.mkdir()
    gh = bin_dir / "gh"
    gh.write_text(GH_STUB)
    gh.chmod(0o755)
    log = tmp_path / "gh.log"
    log.touch()
    monkeypatch.setenv("PATH", f"{bin_dir}{os.pathsep}{os.environ['PATH']}")
    monkeypatch.setenv("GH_LOG", str(log))
    monkeypatch.setenv("GH_RELEASE_VIEW_EXIT", "1")
    return root, origin, log


def draft(root, *extra):
    return compass_release_notes.run(
        ["draft", "--sprint", "1", "--codename", "Rosa", *extra], root)


def test_draft_reuses_an_existing_tag_and_opens_a_draft_release(repo):
    root, origin, log = repo
    git(root, "tag", "v0.0.1")

    assert draft(root) == 0

    assert git(origin, "tag", "-l", "v0.0.1") == "v0.0.1"
    calls = log.read_text().splitlines()
    notes = root / "doc/agile/versions/v0/sprint_01/release_notes.md"
    assert calls[-1] == (f'release create v0.0.1 --draft --title v0.0.1, "Rosa" '
                         f"--notes-file {notes}")


def test_draft_edits_an_existing_release_without_making_it_draft(repo, monkeypatch):
    root, _, log = repo
    git(root, "tag", "v0.0.1")
    monkeypatch.setenv("GH_RELEASE_VIEW_EXIT", "0")

    assert draft(root) == 0

    last = log.read_text().splitlines()[-1]
    assert last.startswith('release edit v0.0.1 --title v0.0.1, "Rosa"')
    assert "--draft" not in last


def test_draft_refuses_to_create_a_tag_off_main(repo):
    root, origin, log = repo
    git(root, "checkout", "-q", "-b", "feature/x")

    assert draft(root) == 1

    assert git(root, "tag", "-l") == ""
    assert git(origin, "tag", "-l") == ""
    assert log.read_text() == ""


def test_collect_without_a_tag_starts_at_the_first_commit(repo):
    root, _, log = repo
    script = REPO_ROOT / "build" / "scripts" / "collect_release_pr_data.py"

    subprocess.run([sys.executable, str(script)], cwd=root, check=True,
                   capture_output=True)

    summary = json.loads((root / "tmp/release_pr_data/summary.json").read_text())
    first = git(root, "log", "-1", "--format=%aI", "HEAD")
    assert summary["since_tag"] == ""
    assert summary["since_commit_iso"] == first
    assert f"merged:>={first}" in log.read_text()


def sprint_context(sprint_start, sprint_end, stories, tmp_path):
    sprint_dir = tmp_path / "doc" / "agile" / "versions" / "v0" / "sprint_01"
    sprint_dir.mkdir(parents=True)
    sprint = {"title": "Sprint 1", "mission": "", "retrospective": {},
              "start": sprint_start, "end": sprint_end, "stories": stories}
    return generate_release_notes.build_context(sprint, [], {}, sprint_dir, tmp_path)


def story_row(title, start, end, state="DONE"):
    return {"title": title, "state": state, "start": start, "end": end, "theme": ""}


def test_duration_comes_from_the_sprint_not_a_carried_story(tmp_path):
    carried = story_row("Carried", "2026-06-05", "2026-11-02")
    ctx = sprint_context("2026-10-27", "2026-11-03", [carried], tmp_path)
    assert "*Sprint duration*: 2026-10-27 → 2026-11-03" in ctx["time_block"]


def test_duration_falls_back_to_story_dates_while_the_sprint_is_open(tmp_path):
    rows = [story_row("A", "2026-10-28", "2026-11-01"),
            story_row("B", "2026-10-29", "2026-11-05")]
    ctx = sprint_context("2026-10-27", "", rows, tmp_path)
    assert "*Sprint duration*: 2026-10-27 → 2026-11-05" in ctx["time_block"]


@pytest.mark.parametrize("tags,title,section", [
    ({"org", "parser", "data_model"}, "Parse org documents", "Parsing & Data Model"),
    ({"ci", "cdash", "testing"}, "Report builds and tests to CDash", "Build, CI & Release"),
    ({"gates", "checks"}, "Run the gates as codecs", "Gates & Checks"),
    ({"codegen"}, "Port ORE Studio codegen", "Codegen"),
    (set(), "Hotfix: unbreak the nightly", "Hotfixes"),
    (set(), "Something unrelated", "Other"),
])
def test_compass_stories_land_in_compass_sections(tags, title, section):
    assert generate_release_notes.assign_section(tags, title) == section
