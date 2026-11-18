#!/usr/bin/env python3
"""
Gzip-compresses dist-embedded/index.html into ../include/web_portal_gz.h
for serving directly from ESP32 PROGMEM at http://192.168.4.1.
"""
import gzip
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
INPUT_HTML = ROOT / "dist-embedded" / "embedded.html"
OUTPUT_HEADER = ROOT.parent / "include" / "web_portal_gz.h"

def main():
    if not INPUT_HTML.exists():
        raise FileNotFoundError(f"Missing {INPUT_HTML}. Run vite build --config vite.embedded.config.ts first.")

    raw = INPUT_HTML.read_bytes()
    gz = gzip.compress(raw, compresslevel=9)

    lines = [
        "#pragma once",
        "#include <stdint.h>",
        "#include <stddef.h>",
        "",
        f"// Auto-generated from webapp/dist-embedded/index.html (raw: {len(raw)} B, gzip: {len(gz)} B)",
        f"#define WEB_PORTAL_GZ_LEN {len(gz)}",
        "static const uint8_t WEB_PORTAL_GZ[] = {",
    ]

    for i in range(0, len(gz), 16):
        chunk = ", ".join(f"0x{b:02x}" for b in gz[i : i + 16])
        lines.append(f"  {chunk},")

    lines.append("};")
    lines.append("")

    OUTPUT_HEADER.write_text("\n".join(lines), encoding="utf-8")
    print(f"[OK] Generated {OUTPUT_HEADER} ({len(gz):,} bytes gzipped from {len(raw):,} bytes)")

if __name__ == "__main__":
    main()
