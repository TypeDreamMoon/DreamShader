#!/usr/bin/env python3
"""Read-only translation status for a plugin's Docs/zh-CN mirror.

Use from any directory: python Docs/translation-status.py [--plugin PATH] [--json]
The script uses only Python's standard library. It never writes files, contacts
an AI service, or imports the documentation website's code.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def read(path: Path) -> str:
    return path.read_text(encoding='utf-8-sig').replace('\r\n', '\n')


def digest(text: str) -> str:
    return hashlib.sha256(text.replace('\r\n', '\n').encode('utf-8')).hexdigest()


def plugin_statuses(plugin: Path) -> list[dict]:
    plugin = plugin.resolve()
    source_root = plugin / 'Docs'
    if not source_root.is_dir():
        raise ValueError(f'Plugin Docs directory does not exist: {source_root}')
    try:
        result = subprocess.run(['git', '-C', str(plugin), 'rev-parse', 'HEAD'],
                                capture_output=True, encoding='utf-8', check=False)
        revision = result.stdout.strip() if result.returncode == 0 else 'unversioned'
    except FileNotFoundError:
        revision = 'unversioned'  # Packaged plugins can be read without Git installed.
    rows = []
    for source in sorted(source_root.rglob('*.md')):
        relative = source.relative_to(source_root)
        if relative.parts[0] in ('zh-CN', 'Website'):
            continue
        if relative.as_posix() in ('DuiLanguage.md', 'FontsAndPackaging.md', 'Migration.md'):
            continue
        if not source.resolve().is_relative_to(source_root.resolve()):
            raise ValueError(f'Source escapes the Docs directory: {relative.as_posix()}')
        translated = source_root / 'zh-CN' / relative
        record_path = source_root / 'zh-CN/.translations' / (relative.as_posix() + '.json')
        for candidate in (translated, record_path):
            if not candidate.resolve().is_relative_to((source_root / 'zh-CN').resolve()):
                raise ValueError(f'Translation path escapes the mirror: {relative.as_posix()}')
        source_hash = digest(read(source))
        record = json.loads(read(record_path)) if record_path.is_file() else {}
        if not isinstance(record, dict):
            raise ValueError(f'Invalid translation record: {record_path}')
        if not translated.is_file():
            status = 'missing'
        elif not record.get('sourceHash'):
            status = 'unreviewed'
        elif record['sourceHash'] != source_hash:
            status = 'outdated'
        else:
            status = 'current'
        rows.append({'id': relative.as_posix(), 'path': 'Docs/' + relative.as_posix(),
                     'status': status, 'hash': source_hash, 'revision': revision})
    return rows


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plugin', type=Path, default=Path(__file__).resolve().parents[1],
                        help='Plugin root; defaults to this script at Docs/translation-status.py')
    parser.add_argument('--json', action='store_true', help='Print all records as JSON')
    parser.add_argument('--all', action='store_true', help='Include current files in text output')
    args = parser.parse_args(argv)
    try:
        rows = plugin_statuses(args.plugin)
    except (OSError, ValueError) as error:
        parser.exit(2, f'error: {error}\n')
    if args.json:
        print(json.dumps(rows, ensure_ascii=False, indent=2))
        return 0
    counts = {status: sum(row['status'] == status for row in rows)
              for status in ('missing', 'unreviewed', 'outdated', 'current')}
    for row in rows:
        if args.all or row['status'] != 'current':
            print(f"{row['status']:10} {row['path']}")
    print('\n' + ', '.join(f'{status}: {count}' for status, count in counts.items())
          + f' / total: {len(rows)}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
