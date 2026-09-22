#!/usr/bin/env python3
"""Render 1-bit 800x480 mockups of the proposed screen (see ../2026-09-22-display-improvement-proposal.md).

Uses the repo's own Roboto and Font Awesome files so glyph sizes match what LVGL would show
(the lv_font_conv "size" and Pillow's truetype size are both the em height in pixels). The
image mode is "1", which makes Pillow render text bilevel and hinted, the way the panel does.

    python3 render.py            # writes quiet.png and alerting.png next to this file

Every string is measured before it is drawn; anything that would cross its column prints a
warning, so the script doubles as a fit check for new wording.
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
TOOLS = HERE.parent.parent / "tools"

W, H = 800, 480
M = 13  # margin, lv_dpx(16) at the default 130 dpi
RIGHT = W - M
BLACK, WHITE = 0, 1

BODY = ImageFont.truetype(str(TOOLS / "Roboto-Regular.ttf"), 36)
TITLE = ImageFont.truetype(str(TOOLS / "Roboto-Regular.ttf"), 32)
ICON = ImageFont.truetype(str(TOOLS / "fa-solid-900.ttf"), 33)
ICON_BIG = ImageFont.truetype(str(TOOLS / "fa-solid-900.ttf"), 46)

CHECK = ""  # check-circle
CROSS = ""  # times-circle
WARN = ""  # exclamation-triangle
PLAY = ""

BODY_ROW = 43
TITLE_ROW = 38
BODY_BASE = 34  # baseline offset inside a body row
TITLE_BASE = 30
GAP = 12  # between an icon and its text

# Platform rows: label column, then two content columns.
PLAT_COL1 = M + 175
PLAT_COL2 = 478

# Safety row: three equal tiles.
SAFE_COLS = [M, M + 258, M + 516, RIGHT]


class Screen:
    def __init__(self) -> None:
        self.im = Image.new("1", (W, H), WHITE)
        self.d = ImageDraw.Draw(self.im)

    def text(self, x: int, base: int, s: str, font=BODY, fill=BLACK, right=False, limit: int | None = None) -> int:
        """Draw text on a baseline; return the x just past it (or before it when right-aligned)."""
        w = int(self.d.textlength(s, font=font))
        if right:
            x -= w
        if limit is not None and x + w > limit:
            print(f"warning: '{s}' ends at {x + w}, past {limit}")
        self.d.text((x, base), s, font=font, fill=fill, anchor="ls")
        return x if right else x + w

    def icon(self, x: int, base: int, glyph: str, font=ICON, fill=BLACK) -> int:
        return self.text(x, base, glyph, font=font, fill=fill)

    def hline(self, y: int) -> None:
        self.d.rectangle((M, y, RIGHT - 1, y + 1), fill=BLACK)

    def vline(self, x: int, y0: int, y1: int) -> None:
        self.d.rectangle((x, y0, x + 1, y1), fill=BLACK)

    def dots(self, x: int, base: int, states: str) -> int:
        """Node dots: 'f' filled (ready), 'o' hollow (NotReady), 'h' half (cordoned)."""
        r = 9
        cy = base - 13
        for s in states:
            box = (x, cy - r, x + 2 * r, cy + r)
            if s == "f":
                self.d.ellipse(box, fill=BLACK)
            elif s == "o":
                self.d.ellipse(box, outline=BLACK, width=3)
            else:
                self.d.ellipse(box, outline=BLACK, width=3)
                self.d.pieslice(box, 90, 270, fill=BLACK)
            x += 2 * r + 8
        return x + 2

    def save(self, name: str) -> None:
        self.im.save(HERE / name)


# Content fragments. Each is drawn at (x, baseline) and told where its column ends.


def icon_text(glyph: str, text: str, prefix: str = ""):
    def draw(s: Screen, x: int, base: int, limit: int) -> None:
        if prefix:
            x = s.text(x, base, prefix, limit=limit) + GAP
        x = s.icon(x, base, glyph)
        if text:
            s.text(x + GAP, base, text, limit=limit)

    return draw


def dots_then(states: str, glyph: str | None, text: str):
    def draw(s: Screen, x: int, base: int, limit: int) -> None:
        x = s.dots(x, base, states) + 8
        if glyph:
            x = s.icon(x, base, glyph) + GAP
        s.text(x, base, text, limit=limit)

    return draw


def plain(text: str):
    def draw(s: Screen, x: int, base: int, limit: int) -> None:
        s.text(x, base, text, limit=limit)

    return draw


def platform_rows(s: Screen, top: int, rows: list) -> int:
    """One full-width row per platform tile: label, then two content columns. Returns bottom y."""
    base = top + BODY_BASE
    for label, col1, col2 in rows:
        s.text(M, base, label, TITLE, limit=PLAT_COL1 - 12)
        col1(s, PLAT_COL1, base, PLAT_COL2 - 12)
        col2(s, PLAT_COL2, base, RIGHT)
        base += BODY_ROW
    return top + len(rows) * BODY_ROW


def safety_row(s: Screen, top: int, tiles: list) -> int:
    """Three tiles: title row and one body row. Returns bottom y."""
    tb = top + TITLE_BASE
    lb = top + TITLE_ROW + BODY_BASE
    for i, (title, content) in enumerate(tiles):
        x = SAFE_COLS[i] + (0 if i == 0 else 14)
        limit = SAFE_COLS[i + 1] - 8
        s.text(x, tb, title, TITLE, limit=limit)
        content(s, x, lb, limit)
    bottom = top + TITLE_ROW + BODY_ROW
    for x in (SAFE_COLS[1], SAFE_COLS[2]):
        s.vline(x, top - 2, bottom)
    return bottom


def jenkins(s: Screen, top: int, right_title: str, lines: list, red: str | None = None) -> None:
    tb = top + TITLE_BASE
    x = s.text(M, tb, "JENKINS", TITLE)
    if red:
        x = s.icon(x + 20, tb, CROSS)
        s.text(x + GAP, tb, red, TITLE)
    s.text(RIGHT, tb, right_title, TITLE, right=True)
    base = top + TITLE_ROW + BODY_BASE
    for glyph, when, name in lines:
        if base > H - M:
            print(f"warning: build line '{name}' does not fit")
            break
        x = s.icon(M, base, glyph)
        x = s.text(x + GAP + 2, base, when)
        s.text(x + 22, base, name, limit=RIGHT)
        base += BODY_ROW


def middle(s: Screen, top: int, platform: list, safety: list) -> int:
    """Platform rows, divider, safety tiles, divider. Returns y below the last divider."""
    bottom = platform_rows(s, top, platform)
    s.hline(bottom + 4)
    bottom = safety_row(s, bottom + 10, safety)
    s.hline(bottom + 4)
    return bottom + 10


def quiet() -> None:
    s = Screen()
    # Alert band: one row, everything fine.
    base = M + 38
    x = s.icon(M, base, CHECK, ICON_BIG)
    s.text(x + 14, base, "Geen alerts")
    x = s.text(RIGHT, base, "Build-Main  3 min", right=True)
    s.icon(x - 33 - GAP, base, PLAY)
    s.hline(M + 52 + 3)

    platform = [
        ("PROXMOX", dots_then("fff", None, "23/25 VMs"), icon_text(CHECK, "04:12", "backup")),
        ("K8S", dots_then("ffff", CHECK, "pods"), plain("42 starts (gem 31)")),
        ("OPSLAG", icon_text(CHECK, "61 %", "ceph"), icon_text(CHECK, "", "openbao")),
    ]
    safety = [
        ("BACKUPS", icon_text(CHECK, "oudste 7 u")),
        ("CERTIFICATEN", icon_text(CHECK, "31 d")),
        ("CRON", icon_text(CHECK, "13 / 13")),
    ]
    bottom = middle(s, M + 52 + 12, platform, safety)
    jenkins(
        s,
        bottom,
        "14 builds vandaag",
        [
            (CHECK, "14:12", "KubeCoder Build-Main #233"),
            (CHECK, "13:55", "DockerImages #1410"),
            (CHECK, "12:40", "CalendarDisplay #412"),
        ],
    )
    s.save("quiet.png")


def alerting() -> None:
    s = Screen()
    # Inverted alert band: one row per alert, white on black. No title row; the band is the
    # message. With more alerts than fit, the last row becomes "+ N meer".
    alerts = [
        (CROSS, "BackupOverdue", "postgres-pas", "3 u"),
        (WARN, "NodeMemoryStalled", "srvk8s3", "40 m"),
    ]
    band_h = len(alerts) * BODY_ROW + 8
    s.d.rectangle((M, M, RIGHT - 1, M + band_h), fill=BLACK)
    base = M + 4 + BODY_BASE
    for glyph, name, label, age in alerts:
        x = s.icon(M + 12, base, glyph, fill=WHITE)
        s.text(x + GAP + 2, base, name, fill=WHITE)
        s.text(M + 420, base, label, fill=WHITE)
        s.text(RIGHT - 12, base, age, fill=WHITE, right=True)
        base += BODY_ROW
    s.hline(M + band_h + 4)

    platform = [
        ("PROXMOX", dots_then("fff", None, "23/25 VMs"), icon_text(CHECK, "04:12", "backup")),
        ("K8S", dots_then("ffof", CROSS, "1 pod"), plain("srvk8s3 NotReady")),
        ("OPSLAG", icon_text(WARN, "84 %", "ceph"), icon_text(CHECK, "", "openbao")),
    ]
    safety = [
        ("BACKUPS", icon_text(CROSS, "postgres 3 d")),
        ("CERTIFICATEN", icon_text(WARN, "9 d secrets")),
        ("CRON", icon_text(CHECK, "13 / 13")),
    ]
    bottom = middle(s, M + band_h + 12, platform, safety)
    jenkins(
        s,
        bottom,
        "9 builds vandaag",
        [
            (CROSS, "12:40", "CalendarDisplay #412"),
            (CHECK, "14:12", "KubeCoder Build-Main #233"),
        ],
        red="1 rood",
    )
    s.save("alerting.png")


if __name__ == "__main__":
    quiet()
    alerting()
    print("wrote", HERE / "quiet.png", "and", HERE / "alerting.png")
