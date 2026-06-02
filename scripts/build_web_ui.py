#!/usr/bin/env python3
"""
build_web_ui.py — Minify web UI source files and pack into web_ui.h

Reads source files from firmware/esp32_inverter_bridge/web_ui/:
  - web_ui_main.html
  - web_ui_config.html
  - web_ui.css
  - web_ui.js

Outputs: firmware/esp32_inverter_bridge/web_ui/web_ui.h

Run after any changes to the web UI source files:
  python scripts/build_web_ui.py

The output file is what gets compiled into firmware and served over Ethernet.
Keeping it as small as possible reduces ENC28J60 transfer time.

Dependencies (install once):
  uv pip install rjsmin rcssmin
"""

import re
import sys
from pathlib import Path

try:
    import rjsmin
    import rcssmin
except ImportError:
    print("ERROR: Missing minification libraries. Run:  uv pip install rjsmin rcssmin", file=sys.stderr)
    sys.exit(1)

REPO_ROOT = Path(__file__).parent.parent
WEB_UI_DIR = REPO_ROOT / "firmware" / "esp32_inverter_bridge" / "web_ui"
OUTPUT_H = WEB_UI_DIR / "web_ui.h"


# ---------------------------------------------------------------------------
# Minifiers
# ---------------------------------------------------------------------------

def minify_css(css: str) -> str:
    """Minify CSS using rcssmin (handles strings/comments safely)."""
    return rcssmin.cssmin(css).strip()


def minify_js(js: str) -> str:
    """Minify JS using rjsmin (handles strings/regex/comments safely)."""
    return rjsmin.jsmin(js).strip()


def minify_html(html: str) -> str:
    """Strip HTML comments, collapse whitespace between tags, minify inline scripts."""
    # Remove HTML comments
    html = re.sub(r'<!--.*?-->', '', html, flags=re.DOTALL)

    # Minify inline <script> blocks with rjsmin
    def minify_script_block(m):
        return f'<script>{rjsmin.jsmin(m.group(1))}</script>'
    html = re.sub(r'<script>(.*?)</script>', minify_script_block, html, flags=re.DOTALL)

    # Collapse whitespace between tags
    html = re.sub(r'>\s+<', '><', html)
    # Strip leading/trailing whitespace per line then join
    html = ''.join(line.strip() for line in html.splitlines())
    # Collapse any remaining multi-space runs
    html = re.sub(r'  +', ' ', html)
    return html.strip()


# ---------------------------------------------------------------------------
# C header generation
# ---------------------------------------------------------------------------

def escape_for_raw_literal(content: str, delimiter: str) -> tuple[str, str]:
    """
    Choose a raw string delimiter that doesn't conflict with the content.
    Returns (open_delimiter, close_delimiter).
    """
    # R"DELIM(...)DELIM" — find a delimiter not in content
    for d in ['', 'X', 'XX', 'RAW', 'HTML', 'CSS', 'JS']:
        close = f'){d}"'
        if close not in content:
            return f'R"{d}(', close
    raise ValueError("Cannot find safe raw literal delimiter")


def build_blob_entry(var_name: str, content: str, lang_tag: str) -> str:
    """Generate a PROGMEM static array entry for a content blob."""
    open_delim, close_delim = escape_for_raw_literal(content, lang_tag)
    lines = [
        f'static const char {var_name}[] PROGMEM = {open_delim}{content}{close_delim};',
        f'static const size_t {var_name}_len = sizeof({var_name}) - 1;',
        '',
    ]
    return '\n'.join(lines)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    # Read source files
    main_html_src   = (WEB_UI_DIR / "web_ui_main.html").read_text(encoding='utf-8')
    config_html_src = (WEB_UI_DIR / "web_ui_config.html").read_text(encoding='utf-8')
    css_src         = (WEB_UI_DIR / "web_ui.css").read_text(encoding='utf-8')
    js_src          = (WEB_UI_DIR / "web_ui.js").read_text(encoding='utf-8')

    # Minify
    css_min         = minify_css(css_src)
    js_min          = minify_js(js_src)
    main_html_min   = minify_html(main_html_src)
    config_html_min = minify_html(config_html_src)

    # Report sizes
    def sz(original: str, minified: str, label: str):
        orig_bytes = len(original.encode('utf-8'))
        mini_bytes = len(minified.encode('utf-8'))
        saving_pct = (1 - mini_bytes / orig_bytes) * 100 if orig_bytes else 0
        print(f"  {label:<22} {orig_bytes:>6} bytes  ->  {mini_bytes:>6} bytes  ({saving_pct:.0f}% saved)")

    print("Web UI asset sizes:")
    sz(main_html_src,   main_html_min,   "web_ui_main.html")
    sz(config_html_src, config_html_min, "web_ui_config.html")
    sz(css_src,         css_min,         "web_ui.css")
    sz(js_src,          js_min,          "web_ui.js")
    total_src = sum(len(s.encode()) for s in [main_html_src, config_html_src, css_src, js_src])
    total_min = sum(len(s.encode()) for s in [main_html_min, config_html_min, css_min, js_min])
    print(f"  {'TOTAL':<22} {total_src:>6} bytes  ->  {total_min:>6} bytes  ({(1-total_min/total_src)*100:.0f}% saved)")

    # Generate header
    header_lines = [
        '#ifndef WEB_UI_H',
        '#define WEB_UI_H',
        '',
        '#include <pgmspace.h>',
        '',
        '// Auto-generated by scripts/build_web_ui.py — do not edit by hand.',
        '// Edit source files in firmware/esp32_inverter_bridge/web_ui/ then re-run the script.',
        '// Compacted asset blobs (PROGMEM). Include only in api.cpp.',
        '',
        build_blob_entry('web_ui_main_html',   main_html_min,   'HTML'),
        build_blob_entry('web_ui_config_html', config_html_min, 'HTML'),
        build_blob_entry('web_ui_css',         css_min,         'CSS'),
        build_blob_entry('web_ui_js',          js_min,          'JS'),
        '#endif // WEB_UI_H',
    ]

    output = '\n'.join(header_lines)
    OUTPUT_H.write_text(output, encoding='utf-8')
    print(f"\nWrote {OUTPUT_H}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
