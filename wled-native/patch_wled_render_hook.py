#!/usr/bin/env python3
"""Apply the narrow, fail-closed post-render hook to the pinned WLED source."""
from pathlib import Path
import sys

ANCHOR = "  // Paint overlays in to pixel buffer\n  handleOverlayDraw();"
INCLUDE = '#include "../usermods/opentail_native/ot_wled_render_hook.h"'
CALL = "  opentailNativePostRender(_pixels, totalLen);"

def patch(source: str) -> str:
    if CALL in source:
        if source.count(CALL) != 1:
            raise ValueError("duplicate OpenTail render hook; refusing to patch")
        return source
    if source.count(ANCHOR) != 1:
        raise ValueError(
            "pinned WLED FX_fcn.cpp compositor anchor changed; refusing unsafe patch"
        )
    if INCLUDE not in source:
        include_anchor = '#include "wled.h"'
        if source.count(include_anchor) != 1:
            raise ValueError("cannot locate unique wled.h include")
        source = source.replace(
            include_anchor, include_anchor + "\n" + INCLUDE, 1
        )
    return source.replace(ANCHOR, ANCHOR + "\n" + CALL, 1)

def main() -> int:
    if len(sys.argv) == 2 and sys.argv[1] == "--self-test":
        fixture = '#include "wled.h"\nvoid f() {\n' + ANCHOR + "\n}\n"
        result = patch(fixture)
        assert result.count(CALL) == 1
        assert patch(result) == result
        try:
            patch("void f() {}")
        except ValueError:
            pass
        else:
            raise AssertionError("missing anchor must fail closed")
        print("WLED compositor patch self-test passed")
        return 0
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <pinned-wled-root>", file=sys.stderr)
        return 2
    target = Path(sys.argv[1]) / "wled00" / "FX_fcn.cpp"
    if not target.is_file():
        print(f"ERROR: missing pinned WLED compositor: {target}", file=sys.stderr)
        return 2
    original = target.read_text(encoding="utf-8")
    try:
        updated = patch(original)
    except ValueError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2
    target.write_text(updated, encoding="utf-8")
    print(f"Patched actual WLED frame compositor: {target}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
