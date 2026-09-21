"""Phase-2 parity sweep for M4: build the four DShader roots and the Legacy corpus through the 2.0 compiler, dump them,
compare the dumps with the frozen 1.x captures, and leave every repository exactly as it was.

    python Tools/Parity/run_parity_sweep.py [--project <DevTest dir>] [--skip-compile] [--keep-assets]
                                            [--legacy-corpus <dir>] [--out <dir>]

What it does, in order:

 1. Refuses when an editor or commandlet process is running (a commandlet run with the editor open fights it for
    packages).
 2. Snapshots the four Content trees (size and mtime of every file), hashes the DShader sources, records the git status
    of the plugin repositories, and backs up every product asset (matched by source base name and by the asset leaves of
    the formal baseline) plus every dirty file of those repositories. The DevTest `DShader` folder is not in git, so it is
    backed up whole.
 3. `dsc.ps1 compile -All -Force`, then `dsc.ps1 dump-graph -All -Force -Out <out>/candidate/roots`.
 4. `dsc.ps1 dump-graph <fixture> -Out <out>/candidate/legacy-raw` for every `.dsm`/`.dsf` fixture of the Legacy corpus
    that the 1.x capture covers.
 5. Restores everything the runs wrote or removed (backup copy, `git checkout` of a file that was clean, deletion of a file
    that did not exist before) and verifies git status and file sizes against the snapshot. Nothing is restored with
    --keep-assets (for a second, --skip-compile run).
 6. Runs `graph_parity.py compare` for the roots against `Saved/DreamShader/GraphBaseline/v2-6c2e0b6-formal` (excluding
    `MF_OutlineWidth`, a hand-edited asset) and for the Legacy corpus against `.../v2-6c2e0b6-generate-corpus` (paired by
    file name, because the fixtures moved from `Tests/Corpus/Generate` to `Tests/Corpus/Legacy/Compile`). Markdown reports land in
    `<out>`.

Exit code: 0 when both comparisons are equal, 1 when either differs, 2 when the sweep refused to start.
Backups are never deleted; they stay under `<out>/backup`.
"""
import argparse
import hashlib
import io
import json
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
PLUGIN = os.path.dirname(os.path.dirname(HERE))


def say(log, *parts):
    line = " ".join(str(p) for p in parts)
    print(line)
    log.write(line + "\n")
    log.flush()


def editor_running():
    out = subprocess.run(["tasklist"], capture_output=True).stdout.decode("utf-8", "replace").lower()
    return "unrealeditor.exe" in out or "unrealeditor-cmd.exe" in out


def file_stats(root):
    result = {}
    if not os.path.isdir(root):
        return result
    for dirpath, _, files in os.walk(root):
        for name in files:
            path = os.path.join(dirpath, name)
            try:
                st = os.stat(path)
            except OSError:
                continue
            result[path] = (st.st_size, st.st_mtime_ns)
    return result


def file_hashes(root):
    result = {}
    if not os.path.isdir(root):
        return result
    for dirpath, _, files in os.walk(root):
        for name in files:
            path = os.path.join(dirpath, name)
            result[path] = hashlib.sha1(io.open(path, "rb").read()).hexdigest()
    return result


def git_status(repo):
    run = subprocess.run(["git", "-C", repo, "status", "--porcelain=v1", "-uall"], capture_output=True)
    return sorted(line for line in run.stdout.decode("utf-8", "replace").splitlines() if line.strip())


def run_dsc(project, log, label, args, logdir):
    dsc = os.path.join(PLUGIN, ".skill", "dsc.ps1")
    say(log, "\n==== %s: dsc.ps1 %s" % (label, " ".join(args)))
    started = time.time()
    with io.open(os.path.join(logdir, label + ".log"), "wb") as handle:
        code = subprocess.run(["pwsh", "-NoProfile", "-File", dsc] + args, cwd=project,
                              stdout=handle, stderr=subprocess.STDOUT).returncode
    say(log, "%s: exit %d in %.0fs" % (label, code, time.time() - started))
    return code


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--project", default=os.path.dirname(os.path.dirname(PLUGIN)))
    parser.add_argument("--out", default=None)
    parser.add_argument("--legacy-corpus", default=os.path.join(PLUGIN, "Tests", "Corpus", "Legacy", "Compile"))
    parser.add_argument("--skip-compile", action="store_true")
    parser.add_argument("--keep-assets", action="store_true")
    args = parser.parse_args(argv)

    project = os.path.abspath(args.project)
    plugins = os.path.join(project, "Plugins")
    baselines = os.path.join(project, "Saved", "DreamShader", "GraphBaseline")
    formal = os.path.join(baselines, "v2-6c2e0b6-formal")
    generate = os.path.join(baselines, "v2-6c2e0b6-generate-corpus")
    stamp = time.strftime("%Y%m%d-%H%M%S")
    out = os.path.abspath(args.out or os.path.join(project, "Saved", "DreamShader", "ParitySweep", stamp))
    backup_root = os.path.join(out, "backup")
    os.makedirs(backup_root, exist_ok=True)
    log = io.open(os.path.join(out, "sweep.log"), "w", encoding="utf-8")

    if editor_running():
        say(log, "an editor or commandlet process is running - the sweep did not start")
        return 2
    for required in (formal, generate):
        if not os.path.isdir(required):
            say(log, "missing baseline:", required)
            return 2

    content_roots = {
        "Game": os.path.join(project, "Content"),
        "DreamDynamicWorld": os.path.join(plugins, "DreamDynamicWorld", "Content"),
        "DreamGUI": os.path.join(plugins, "DreamGUI", "Content"),
        "MoonToon": os.path.join(plugins, "MoonToon", "Content"),
    }
    source_roots = {
        "Game": os.path.join(project, "DShader"),
        "DreamDynamicWorld": os.path.join(plugins, "DreamDynamicWorld", "DShader"),
        "DreamGUI": os.path.join(plugins, "DreamGUI", "DShader"),
        "MoonToon": os.path.join(plugins, "MoonToon", "DShader"),
    }
    repos = {name: os.path.join(plugins, name) for name in ("DreamDynamicWorld", "DreamGUI", "MoonToon", "DreamShader")}

    # ------------------------------------------------------------------------------------------ 2. snapshot and backup
    pre_content = {key: file_stats(root) for key, root in content_roots.items()}
    pre_sources = {key: file_hashes(root) for key, root in source_roots.items()}
    pre_git = {key: git_status(repo) for key, repo in repos.items()}
    names = set()
    for root in source_roots.values():
        for dirpath, _, files in os.walk(root):
            names.update(os.path.splitext(f)[0] for f in files if f.endswith((".dsm", ".dsf", ".dss", ".dsi")))
    for dirpath, _, files in os.walk(formal):
        for f in files:
            if f.endswith(".graph.json"):
                asset = json.load(io.open(os.path.join(dirpath, f), encoding="utf-8")).get("asset", "")
                if asset:
                    names.add(asset.rsplit(".", 1)[-1])

    def backup_path(path):
        return os.path.join(backup_root, os.path.splitdrive(path)[1].lstrip("\\/"))

    backed = set()

    def backup(path):
        target = backup_path(path)
        os.makedirs(os.path.dirname(target), exist_ok=True)
        shutil.copy2(path, target)
        backed.add(path)

    for files in pre_content.values():
        for path in files:
            if path.endswith((".uasset", ".umap")) and os.path.splitext(os.path.basename(path))[0] in names:
                backup(path)
    for key, repo in repos.items():
        for line in pre_git[key]:
            path = os.path.join(repo, line[3:].strip().strip('"').replace("/", os.sep))
            if os.path.isfile(path) and path not in backed:
                backup(path)
    if os.path.isdir(source_roots["Game"]):
        shutil.copytree(source_roots["Game"], os.path.join(out, "backup-DevTest-DShader"))
    say(log, "sweep dir:", out, "| files backed up:", len(backed))

    # ------------------------------------------------------------------------------------------------ 3-4. build, dump
    candidate_roots = os.path.join(out, "candidate", "roots")
    candidate_legacy_raw = os.path.join(out, "candidate", "legacy-raw")
    codes = {}
    if not args.skip_compile:
        codes["compile"] = run_dsc(project, log, "compile", ["compile", "-All", "-Force"], out)
    codes["dump-roots"] = run_dsc(project, log, "dump-roots", ["dump-graph", "-All", "-Force", "-Out", candidate_roots], out)
    captured = set()
    for dirpath, _, files in os.walk(generate):
        captured.update(f.split(".graph.json")[0].split(".")[0] for f in files if f.endswith(".graph.json"))
    fixtures = []
    for dirpath, _, files in os.walk(args.legacy_corpus):
        for f in files:
            if f.endswith((".dsm", ".dsf")) and os.path.splitext(f)[0] in captured:
                fixtures.append(os.path.join(dirpath, f))
    for fixture in sorted(fixtures):
        label = "dump-legacy-" + os.path.splitext(os.path.basename(fixture))[0]
        codes[label] = run_dsc(project, log, label, ["dump-graph", fixture, "-Out", candidate_legacy_raw], out)
    say(log, "legacy fixtures dumped:", len(fixtures), "of", len(captured), "captured")

    # ------------------------------------------------------------------------------------------------------- 5. restore
    while editor_running():
        time.sleep(2)
    if not args.keep_assets:
        restored = deleted = checked_out = 0
        unrestorable = []
        for key, root in content_roots.items():
            before = pre_content[key]
            after = file_stats(root)
            for path, st in after.items():
                if before.get(path) == st:
                    continue
                if path in before:
                    if path in backed:
                        shutil.copy2(backup_path(path), path)
                        restored += 1
                        continue
                    repo_key = next((k for k, r in repos.items()
                                     if os.path.normcase(path).startswith(os.path.normcase(r) + os.sep)), None)
                    if repo_key:
                        rel = os.path.relpath(path, repos[repo_key]).replace("\\", "/")
                        if not any(line[3:].strip().strip('"') == rel for line in pre_git[repo_key]):
                            subprocess.run(["git", "-C", repos[repo_key], "checkout", "--", rel], capture_output=True)
                            checked_out += 1
                            continue
                    unrestorable.append(path)
                else:
                    os.remove(path)
                    deleted += 1
            for path in before:
                if path not in after:
                    if path in backed:
                        shutil.copy2(backup_path(path), path)
                        restored += 1
                    else:
                        unrestorable.append(path + " (removed by the run)")
        for key, root in source_roots.items():
            now = file_hashes(root)
            for path in set(now) | set(pre_sources[key]):
                if now.get(path) == pre_sources[key].get(path):
                    continue
                if key == "Game":
                    saved = os.path.join(out, "backup-DevTest-DShader", os.path.relpath(path, root))
                    if os.path.isfile(saved):
                        shutil.copy2(saved, path)
                    elif os.path.isfile(path):
                        os.remove(path)
                else:
                    subprocess.run(["git", "-C", repos[key], "checkout", "--",
                                    os.path.relpath(path, repos[key]).replace("\\", "/")], capture_output=True)
                say(log, "source changed by the run (reverted):", path)
        say(log, "\nrestored %d, checked out %d, deleted %d" % (restored, checked_out, deleted))
        for path in unrestorable:
            say(log, "  UNRESTORABLE", path)
        same = True
        for key, repo in repos.items():
            if git_status(repo) != pre_git[key]:
                same = False
                say(log, "  git status differs in", key)
        say(log, "git status identical to before:", same)

    # ------------------------------------------------------------------------------------------------------ 6. compare
    flat_base = os.path.join(out, "baseline", "legacy-flat")
    flat_cand = os.path.join(out, "candidate", "legacy-flat")
    for source, target in ((generate, flat_base), (candidate_legacy_raw, flat_cand)):
        os.makedirs(target, exist_ok=True)
        for dirpath, _, files in os.walk(source):
            for f in files:
                if f.endswith(".graph.json"):
                    shutil.copy2(os.path.join(dirpath, f), os.path.join(target, f))
    comparator = os.path.join(HERE, "graph_parity.py")
    registered = os.path.join(HERE, "registered-deltas.json")
    roots_code = subprocess.run([sys.executable, comparator, "compare", formal, candidate_roots, "--exclude", "*MF_OutlineWidth*",
                                 "--allow", registered,
                                 "--report", os.path.join(out, "parity-roots.md")], capture_output=True).returncode
    legacy_code = subprocess.run([sys.executable, comparator, "compare", flat_base, flat_cand, "--allow", registered,
                                  "--report", os.path.join(out, "parity-legacy.md")], capture_output=True).returncode
    say(log, "\ndsc exit codes:", codes)
    say(log, "roots parity: %s (parity-roots.md) | legacy parity: %s (parity-legacy.md)" % (
        "equal" if roots_code == 0 else "DIFFERENT", "equal" if legacy_code == 0 else "DIFFERENT"))
    return 0 if roots_code == 0 and legacy_code == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
