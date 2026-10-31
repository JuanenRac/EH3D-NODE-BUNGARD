#!/usr/bin/env python3
"""EH3D-NODE-BUNGARD - compresses the web panel for embedding in the firmware.
Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.

    pack_panel.py PANEL_DIR OUT_DIR

Writes index.html.gz, app.js.gz (text.js and app.js joined), style.css.gz and langs.js.gz (the words of the other languages, from panel/locales). The compression is reproducible (no timestamp), so the same panel always gives the same bytes.
"""
import gzip
import json
import pathlib
import sys

ASSETS = ("index.html", "app.js", "style.css")


def pack(sources: list, target: pathlib.Path) -> None:
    data = b"\n".join(source.read_bytes() for source in sources)
    with target.open("wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, compresslevel=9, mtime=0) as packed:
            packed.write(data)


def pack_languages(panel: pathlib.Path, out: pathlib.Path) -> None:
    """langs.js: the words of the languages beyond the first eight, from panel/locales/CODE.tsv (Key<TAB>text per line), as `window.X = {code: {key: text}}`."""
    languages = {}
    folder = panel / "locales"
    for source in sorted(folder.glob("*.tsv")) if folder.is_dir() else []:
        words = {}
        for line in source.read_text(encoding="utf-8").splitlines():
            if line.startswith("#") or "\t" not in line:
                continue
            key, text = line.split("\t", 1)
            if text.strip():
                words[key.strip()] = text.replace("\\n", "\n")
        languages[source.stem] = words
    script = "window.X=" + json.dumps(languages, ensure_ascii=False, separators=(",", ":")) + ";\n"
    with (out / "langs.js.gz").open("wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, compresslevel=9, mtime=0) as packed:
            packed.write(script.encode("utf-8"))


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    panel, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    for name in ASSETS:
        # the texts (text.js) are joined in front of the pages (app.js): one script for the browser
        sources = [panel / "text.js", panel / name] if name == "app.js" else [panel / name]
        for source in sources:
            if not source.is_file():
                print(f"missing panel file: {source}", file=sys.stderr)
                return 1
        pack(sources, out / (name + ".gz"))
    pack_languages(panel, out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
