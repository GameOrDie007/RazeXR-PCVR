"""
RazeXR PCVR's controls picture: the Meta Quest 3's controllers, drawn as our own
illustrations, with every button labelled with its job. The jobs are read from
the engine's real default bindings, so the picture cannot drift from the game.

    python tools\\controls-picture.py [out folder] [--logo <png>]

Writes one 1920 x 1440 PNG (default docs\\controls):
    Controls - RazeXR PCVR - Meta Quest 3.png    Quest, and the Valve Index (its
                                                 two differences are on the page)

and prints every binding it drew from. Two halves make a control:

  - which key each controller button sends. That is the VR input layer's own
    code (vr/RazeXR/VrInputDefault.cpp) and is listed in BUTTONS below; every
    key named there is checked against that file, so a key it stops sending
    fails the run instead of drawing a stale label.
  - what each key does: the bindings, read from wadsrc/static/engine
    (commonbinds.txt and defbinds.txt) and the Duke games' own defbinds.txt.

A few controls are the VR layer's and not a binding at all - the sticks'
walking and turning, the wheels, the grips, the menu button - and are written
here.

The page is laid out as HTML/SVG and photographed by a headless Edge - no
window opens. Fonts are in tools\\fonts (Press Start 2P and Chakra Petch, both
OFL). The controllers are drawn from the product's published photos, by hand,
as simple illustrations; no photo or render of Meta's or Valve's is used.
"""
import base64, html, os, re, subprocess, sys, tempfile, time

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
args = [a for a in sys.argv[1:]]
LOGO = os.path.join(os.path.dirname(REPO), "NOLFVR-private", "art", "gameordie-logo.png")
if "--logo" in args:
    i = args.index("--logo"); LOGO = args[i + 1]; del args[i:i + 2]
PORT_TITLE = "RAZEXR"
PORT_NAME = "RazeXR PCVR"
OUT = args[0] if args else os.path.join(REPO, "docs", "controls")
EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
W, H = 1920, 1440
ENGINE = os.path.join(REPO, "wadsrc", "static", "engine")
DUKE = os.path.join(REPO, "wadsrc", "static", "filter", "duke", "engine")
VRINPUT = os.path.join(REPO, "vr", "RazeXR", "VrInputDefault.cpp")

# ---- the bindings -----------------------------------------------------------------

# Controller button -> the key the VR layer sends for it (right-handed, Touch).
BUTTONS = {
    "R trigger":        "KEY_MOUSE1",
    "A":                "KEY_SPACE",
    "B":                "KEY_PAD_A",
    "R thumbrest":      "KEY_PAD_B",
    "R stick up":       "KEY_MWHEELUP",
    "R stick down":     "KEY_MWHEELDOWN",
    "L trigger":        "KEY_PAD_LTRIGGER",
    "X":                "KEY_PAD_LTHUMB",
    "Y":                "KEY_TAB",
    "L stick click":    "KEY_PAD_RTHUMB",
    "L grip":           "KEY_LSHIFT",
    # with the right grip held
    "grip + R trigger": "KEY_PAD_RSHOULDER",
    "grip + A":         "KEY_ENTER",
    "grip + B":         "KEY_BACKSPACE",
    "grip + stick up":  "KEY_PAD_DPAD_LEFT",
    "grip + stick down": "KEY_PAD_DPAD_RIGHT",
}

# The engine's key names (c_bind.cpp) for the KEY_ constants above.
KEYNAME = {
    "KEY_MOUSE1": "mouse1", "KEY_SPACE": "space", "KEY_PAD_A": "pad_a", "KEY_PAD_B": "pad_b",
    "KEY_MWHEELUP": "mwheelup", "KEY_MWHEELDOWN": "mwheeldown", "KEY_PAD_LTRIGGER": "ltrigger",
    "KEY_PAD_LTHUMB": "lthumb", "KEY_TAB": "tab", "KEY_PAD_RTHUMB": "rthumb", "KEY_LSHIFT": "shift",
    "KEY_PAD_RSHOULDER": "rshoulder", "KEY_ENTER": "enter", "KEY_BACKSPACE": "backspace",
    "KEY_PAD_DPAD_LEFT": "dpadleft", "KEY_PAD_DPAD_RIGHT": "dpadright",
}

# A binding's command -> the words the README's controls section uses.
WORDS = {
    "+fire": "Fire", "+alt_fire": "Alt fire", "+jump": "Jump", "+open": "Open / use",
    "+crouch": "Crouch", "togglemap": "Toggle map", "weapalt": "Alt weapon",
    "weapprev": "Previous weapon", "weapnext": "Next weapon", "invuse": "Use the item",
    "turnaround": "Turn around", "invprev": "Previous item", "invnext": "Next item",
    "+quick_kick": "Quick kick", "+run": "Run",
}

def read_binds(path):
    """{key name: command} from a bind file (mapbind lines are the automap's)."""
    out = {}
    for line in open(path, encoding="latin-1"):
        line = line.split("//")[0].strip()
        m = re.match(r'^(\S+)\s+"?([^"]+?)"?\s*$', line)
        if m and m.group(1).lower() != "mapbind":
            out[m.group(1).lower()] = m.group(2).strip()
    return out

def binds():
    """{button: (command, game note)}, every BUTTONS key checked in the VR layer."""
    src = open(VRINPUT, encoding="latin-1").read()
    for b, key in BUTTONS.items():
        assert re.search(r"\b%s\b" % key, src), "%s no longer sends %s - update BUTTONS" % (b, key)
    common = read_binds(os.path.join(ENGINE, "defbinds.txt"))
    common.update(read_binds(os.path.join(ENGINE, "commonbinds.txt")))
    duke = read_binds(os.path.join(DUKE, "defbinds.txt"))
    out = {}
    for b, key in BUTTONS.items():
        name = KEYNAME[key]
        if name in common:
            out[b] = (common[name], "")
        elif name in duke:
            out[b] = (duke[name], "Duke games")
        else:
            out[b] = ("", "")
    return out

def job(b, binding, lower=False):
    cmd, note = binding.get(b, ("", ""))
    if not cmd: return "-"
    words = WORDS.get(cmd.lower(), cmd)
    if lower: words = words[0].lower() + words[1:]
    return words + (" (%s)" % note if note else "")

# ---- the house look (game-or-die-brand-theme: Jazz Cup on dark) ---------------

TURQ, PURPLE, ORANGE, GREEN, WHITE = "#03C1CB", "#3E1E84", "#FF7518", "#00FF6E", "#FFFFFF"

def font_url(name):
    return "file:///" + os.path.join(REPO, "tools", "fonts", name).replace("\\", "/").replace(" ", "%20")

def data_url(path):
    """A PNG as a data URL, or None when it is not there."""
    if not os.path.exists(path): return None
    return "data:image/png;base64," + base64.b64encode(open(path, "rb").read()).decode()

# ---- the controllers: our own drawings, face up (front) and from the side ----
#
# Each front view is drawn round the centre of its face, right hand; the left
# hand is the mirror image. Buttons are named so the callouts can find them.

LETTERS = []   # (x, y, size, text) in face coordinates, for the controller being built

def quest_front(left):
    """Meta Quest 3 (Touch Plus): white handle, dark round face, no ring.
    Returns (svg, {button: (x, y, r)}, letters) in face coordinates, right hand."""
    b = {}
    s = []
    del LETTERS[:]
    s.append('<path d="M 40 60 C 64 130 70 210 62 270 C 56 312 8 318 -6 290 C -18 230 -30 150 -60 90 Z" fill="url(#qhandle)" stroke="#9c9ca6" stroke-width="3"/>')
    s.append('<ellipse cx="0" cy="0" rx="158" ry="140" fill="#d9d9df" stroke="#9c9ca6" stroke-width="3"/>')
    s.append('<ellipse cx="0" cy="-4" rx="146" ry="128" fill="url(#qface)"/>')
    s.append(stick(62, -18, 44, "#2b2b30")); b["stick"] = (62, -18, 44)
    s.append(button(-58, -40, 27, "Y" if left else "B", "#2d2d33")); b["B"] = (-58, -40, 27)
    s.append(button(-18, 40, 27, "X" if left else "A", "#2d2d33")); b["A"] = (-18, 40, 27)
    s.append(button(70, 72, 17, "&#x2261;" if left else "&#x221E;", "#2d2d33", 17)); b["system"] = (70, 72, 17)
    # The thumbrest: the flat pad between the buttons and the stick, low down.
    s.append('<ellipse cx="-92" cy="58" rx="26" ry="16" fill="#323238" stroke="#5c5c66" stroke-width="2" transform="rotate(-30 -92 58)"/>')
    b["thumbrest"] = (-92, 58, 16)
    return s, b, list(LETTERS)

def quest_side():
    """Quest 3 controller from its thumb side, front to the right."""
    return ['<path d="M -120 -70 C -60 -120 90 -110 140 -60 C 150 -40 130 -20 100 -18 C 60 -16 40 0 30 40 '
            'C 10 120 -10 190 -40 230 C -60 250 -95 240 -92 205 C -85 130 -90 60 -110 10 C -125 -20 -140 -50 -120 -70 Z" '
            'fill="url(#qhandle)" stroke="#9c9ca6" stroke-width="3"/>',
            '<path d="M -120 -70 C -60 -120 90 -110 140 -60 C 120 -86 -40 -104 -120 -70 Z" fill="#3a3a42"/>',
            '<path d="M 70 -16 C 96 -8 104 20 92 48 C 84 62 70 58 66 44 C 62 24 60 6 56 -10 Z" fill="#2d2d33"/>',   # trigger
            '<ellipse cx="-40" cy="80" rx="22" ry="44" fill="#3a3a42" transform="rotate(-12 -40 80)"/>'], \
           {"trigger": (86, 22), "grip": (-40, 80)}

def stick(x, y, r, well):
    return ('<circle cx="%g" cy="%g" r="%g" fill="%s" stroke="#55555e" stroke-width="2"/>'
            '<circle cx="%g" cy="%g" r="%g" fill="url(#cap)"/>'
            '<circle cx="%g" cy="%g" r="%g" fill="none" stroke="#5c5c66" stroke-width="2"/>') % (
        x, y, r, well, x, y, r * 0.66, x, y, r * 0.42)

def button(x, y, r, text, fill, size=None):
    if text: LETTERS.append((x, y, size or int(r * 1.1), text))
    return ('<circle cx="%g" cy="%g" r="%g" fill="%s" stroke="#5c5c66" stroke-width="2"/>'
            '<circle cx="%g" cy="%g" r="%g" fill="url(#shine)"/>') % (x, y, r, fill, x, y - r * 0.2, r * 0.8)

DEFS = """
<defs>
  <linearGradient id="qhandle" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#c9c9d1"/><stop offset="0.45" stop-color="#f4f4f8"/><stop offset="1" stop-color="#b9b9c3"/></linearGradient>
  <radialGradient id="qface" cx="0.45" cy="0.35" r="0.7"><stop offset="0" stop-color="#55555e"/><stop offset="1" stop-color="#26262c"/></radialGradient>
  <radialGradient id="cap" cx="0.4" cy="0.35" r="0.7"><stop offset="0" stop-color="#6a6a74"/><stop offset="1" stop-color="#1c1c21"/></radialGradient>
  <radialGradient id="shine" cx="0.5" cy="0.2" r="0.6"><stop offset="0" stop-color="#ffffff" stop-opacity="0.22"/><stop offset="1" stop-color="#ffffff" stop-opacity="0"/></radialGradient>
  <pattern id="grid" width="24" height="24" patternUnits="userSpaceOnUse"><path d="M 24 0 L 0 0 0 24" fill="none" stroke="#3E1E84" stroke-opacity="0.35" stroke-width="1"/></pattern>
  <linearGradient id="brand" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#3E1E84"/><stop offset="0.5" stop-color="#FF7518"/><stop offset="1" stop-color="#03C1CB"/></linearGradient>
  <linearGradient id="logo" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#4A1CF0"/><stop offset="0.55" stop-color="#9A3AA0"/><stop offset="1" stop-color="#FF5A14"/></linearGradient>
</defs>"""

# ---- the page ------------------------------------------------------------------

class Page:
    def __init__(self, device, games):
        self.device, self.games = device, games
        self.art = []          # controller drawings, in page space
        self.calls = []        # (column, anchor (x, y), key, job, glyph, order)

    def place(self, shapes, cx, cy, scale, mirror, letters=()):
        """Draws a controller at (cx, cy); returns a function from face to page
        coordinates. The letters on its buttons are drawn unmirrored on top."""
        sx = -scale if mirror else scale
        self.art.append('<g transform="translate(%g %g) scale(%g %g)">%s</g>' % (cx, cy, sx, scale, "".join(shapes)))
        f = lambda p: (cx + p[0] * sx, cy + p[1] * scale)
        for x, y, size, text in letters:
            px, py = f((x, y))
            self.art.append('<text x="%g" y="%g" font-family="Chakra Petch" font-weight="700" font-size="%g" fill="#e8e8f0" '
                            'text-anchor="middle" dominant-baseline="central">%s</text>' % (px, py + 1, size * scale, text))
        return f

    def call(self, column, anchor, key, what, glyph="", order=None):
        """A callout; boxes in a column go top to bottom by their button's
        height, or by order where leaders would otherwise cross."""
        self.calls.append((column, anchor, key, what, glyph, anchor[1] if order is None else order))

    # The columns: (left x, right x, top y, bottom y, side the leader leaves from)
    COLS = {
        "L-out": (40, 420, 470, 1010, "right"), "L-in": (790, 1130, 470, 700, "left"),
        "R-in": (790, 1130, 720, 1010, "right"), "R-out": (1500, 1880, 470, 1010, "left"),
        "S-L": (250, 640, 1060, 1236, "right"), "S-R": (1280, 1670, 1060, 1236, "left"),
    }

    def callouts(self):
        boxes, leaders = [], []
        for name, (x0, x1, y0, y1, edge) in self.COLS.items():
            items = sorted([c for c in self.calls if c[0] == name], key=lambda c: c[5])
            if not items: continue
            bh, gap = 74, 14
            ys, prev = [], y0 - 1e9
            for c in items:                               # level with the button, as room allows
                y = max(c[1][1] - bh / 2, y0, prev + bh + gap); ys.append(y); prev = y
            for n in range(len(ys) - 1, -1, -1):          # then pulled up from the bottom
                ys[n] = min(ys[n], (y1 - bh) if n == len(ys) - 1 else ys[n + 1] - bh - gap)
            for (col, anchor, key, what, glyph, _), y in zip(items, ys):
                ex = x1 if edge == "right" else x0
                ey = y + bh / 2
                leaders.append('<polyline points="%g,%g %g,%g %g,%g" fill="none" stroke="%s" stroke-width="3"/>'
                               '<circle cx="%g" cy="%g" r="6" fill="%s"/><circle cx="%g" cy="%g" r="7" fill="%s" stroke="#120826" stroke-width="2"/>'
                               % (ex, ey, ex + (22 if edge == "right" else -22), ey, anchor[0], anchor[1], TURQ,
                                  ex, ey, TURQ, anchor[0], anchor[1], ORANGE))
                boxes.append(self.box(x0, y, x1 - x0, bh, key, what, glyph))
        return "".join(leaders) + "".join(boxes)

    @staticmethod
    def box(x, y, w, h, key, what, glyph):
        g = ""
        tx = x + 18
        if glyph:
            g = ('<circle cx="%g" cy="%g" r="22" fill="#120826" stroke="%s" stroke-width="2"/>'
                 '<text x="%g" y="%g" font-family="Chakra Petch" font-weight="700" font-size="22" fill="%s" text-anchor="middle" dominant-baseline="central">%s</text>'
                 % (x + 36, y + h / 2, TURQ, x + 36, y + h / 2 + 1, WHITE, glyph))
            tx = x + 70
        return ('<rect x="%g" y="%g" width="%g" height="%g" rx="6" fill="#1b0d3d" fill-opacity="0.92" stroke="%s" stroke-width="2.5"/>%s'
                '<text x="%g" y="%g" font-family="Chakra Petch" font-weight="700" font-size="25" fill="%s">%s</text>'
                '<text x="%g" y="%g" font-family="Chakra Petch" font-weight="500" font-size="23" fill="%s"%s>%s</text>'
                % (x, y, w, h, TURQ, g, tx, y + 31, ORANGE, html.escape(key), tx, y + 60, WHITE,
                   # a line too long for its box is squeezed to fit (about 11.3 px a character)
                   ' textLength="%g" lengthAdjust="spacingAndGlyphs"' % (x + w - 14 - tx) if len(what) * 11.3 > x + w - 14 - tx else "",
                   html.escape(what)))

    def render(self, context, path):
        logo = data_url(LOGO)
        head = ['<rect x="0" y="0" width="%d" height="400" fill="#0b0518"/>' % W]
        tx = 60
        if logo:
            head.append('<image href="%s" x="40" y="22" height="356" width="260" preserveAspectRatio="xMidYMid meet"/>' % logo)
            tx = 330
        # The port's title in the logo's own style: purple to orange, a
        # turquoise shadow, a black edge. (No splash for this port to cut it from.)
        head.append('<text x="%d" y="218" font-family="Pixel" font-size="84" fill="%s">%s</text>' % (tx + 8, TURQ, PORT_TITLE))
        head.append('<text x="%d" y="210" font-family="Pixel" font-size="84" fill="url(#logo)" stroke="#000" stroke-width="3" paint-order="stroke">%s</text>' % (tx, PORT_TITLE))
        head.append('<text x="%d" y="300" font-family="Pixel" font-size="30" fill="%s">CONTROLLER MAP</text>' % (tx + 4, TURQ))
        head.append('<text x="%d" y="352" font-family="Pixel" font-size="18" fill="%s">A GAME OR DIE VR PORT</text>' % (tx + 4, ORANGE))
        head.append('<text x="1880" y="300" text-anchor="end" font-family="Chakra Petch" font-weight="700" font-size="60" fill="%s" letter-spacing="3">%s</text>' % (TURQ, html.escape(self.device.upper())))
        head.append('<text x="1880" y="352" text-anchor="end" font-family="Chakra Petch" font-weight="500" font-size="28" fill="%s">%s</text>' % (GREEN, html.escape(self.games)))
        head.append('<rect x="0" y="398" width="%d" height="6" fill="url(#brand)"/>' % W)

        tabs = []
        for x, text in ((215, "LEFT CONTROLLER"), (1705, "RIGHT CONTROLLER")):
            tabs.append('<rect x="%g" y="424" width="330" height="40" rx="4" fill="#120826" stroke="%s" stroke-width="2.5"/>'
                        '<text x="%g" y="444" text-anchor="middle" dominant-baseline="central" font-family="Pixel" font-size="16" fill="%s">%s</text>'
                        % (x - 165, TURQ, x, TURQ, text))
        tabs.append('<text x="960" y="1052" text-anchor="middle" font-family="Pixel" font-size="16" fill="%s">SIDE VIEW</text>' % TURQ)

        top = 1252
        ctx = ['<rect x="40" y="%d" width="1840" height="%d" rx="8" fill="#1b0d3d" stroke="%s" stroke-width="2.5"/>' % (top, H - 16 - top, ORANGE),
               '<text x="64" y="%d" font-family="Pixel" font-size="15" fill="%s">ALSO</text>' % (top + 36, ORANGE)]
        for n, line in enumerate(context):
            ctx.append('<text x="200" y="%d" font-family="Chakra Petch" font-weight="500" font-size="24" fill="%s">%s</text>' % (top + 38 + n * 38, WHITE, html.escape(line)))

        svg = ('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">%s'
               '<defs><linearGradient id="bg" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#140a2e"/><stop offset="1" stop-color="#0a0516"/></linearGradient></defs>'
               '<rect width="%d" height="%d" fill="url(#bg)"/><rect y="404" width="%d" height="%d" fill="url(#grid)"/>%s%s%s%s%s</svg>'
               % (W, H, W, H, DEFS, W, H, W, H - 404, "".join(head), "".join(tabs), "".join(self.art), self.callouts(), "".join(ctx)))
        page = ('<!doctype html><html><head><meta charset="utf-8"><style>'
                '@font-face{font-family:"Pixel";src:url("%s");}'
                '@font-face{font-family:"Chakra Petch";font-weight:500;src:url("%s");}'
                '@font-face{font-family:"Chakra Petch";font-weight:700;src:url("%s");}'
                'html,body{margin:0;padding:0;background:#0a0516;overflow:hidden}</style></head><body>%s</body></html>'
                % (font_url("PressStart2P-Regular.ttf"), font_url("ChakraPetch-Medium.ttf"), font_url("ChakraPetch-Bold.ttf"), svg))
        tmp = os.path.join(tempfile.gettempdir(), "razexr-controls-%d.html" % os.getpid())
        open(tmp, "w", encoding="utf-8").write(page)
        # Its own throwaway profile, so a running Edge is not handed the job.
        profile = tempfile.mkdtemp(prefix="razexr-controls-edge-")
        path = os.path.abspath(path)
        if os.path.exists(path): os.remove(path)
        subprocess.run([EDGE, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--allow-file-access-from-files",
                        "--user-data-dir=" + profile, "--no-first-run",
                        "--virtual-time-budget=3000", "--window-size=%d,%d" % (W, H), "--screenshot=" + path,
                        "file:///" + tmp.replace("\\", "/")], check=True, capture_output=True, timeout=120)
        # Edge's launcher returns before the page is photographed: wait for the
        # picture to arrive and stop growing.
        last = -1
        for _ in range(120):
            size = os.path.getsize(path) if os.path.exists(path) else -1
            if size > 0 and size == last: break
            last = size
            time.sleep(0.5)
        else:
            raise SystemExit("Edge wrote no picture: " + path)

# Front views: left face at (560, 660), right at (1360, 660). Side views below.
LF, RF, FS = (560, 660), (1360, 660), 0.95
LS, RS, SS = (830, 1140), (1090, 1140), 0.5

def quest_picture(b):
    p = Page("Meta Quest 3", "Valve Index and other VR controllers work the same way")
    lshapes, bb, lletters = quest_front(True)
    lf = p.place(lshapes, LF[0], LF[1], FS, True, lletters)
    rshapes, bb, rletters = quest_front(False)
    rf = p.place(rshapes, RF[0], RF[1], FS, False, rletters)
    side, sb = quest_side()
    ls = p.place(side, LS[0], LS[1], SS, True)
    rs = p.place(side, RS[0], RS[1], SS, False)
    edge = lambda f, k, dx, dy=0: (f((bb[k][0], bb[k][1]))[0] + dx, f((bb[k][0], bb[k][1]))[1] + dy)

    # Left controller
    p.call("L-out", edge(lf, "stick", -44), "Left stick", "Walk. Click: " + job("L stick click", b, True), "L")
    p.call("L-out", edge(lf, "system", -17), "Menu", "Pause menu", "&#x2261;")
    p.call("L-in", lf((bb["B"][0], bb["B"][1] - 27)), "Y", job("Y", b), "Y")
    p.call("L-in", edge(lf, "A", 27), "X", job("X", b) + " (hold)", "X")
    # Right controller
    p.call("R-out", rf((bb["stick"][0], bb["stick"][1] - 44)), "Right stick", "Turn. Hold the click: weapon wheel", "R", order=0)
    p.call("R-out", edge(rf, "stick", 44), "Right stick up / down",
           "%s / %s" % (job("R stick up", b), job("R stick down", b, True)), "&#x2195;", order=1)
    p.call("R-out", edge(rf, "system", 17), "Meta button", "The headset's own menu", "&#x221E;", order=2)
    # In this order the three leaders do not cross: the thumbrest sits left of A.
    p.call("R-in", edge(rf, "B", -27), "B", job("B", b), "B", order=0)
    p.call("R-in", edge(rf, "thumbrest", -26), "Thumbrest (touch)", job("R thumbrest", b), order=1)
    p.call("R-in", edge(rf, "A", -27), "A", job("A", b), "A", order=2)
    # Side views
    p.call("S-L", ls(sb["trigger"]), "Left trigger", job("L trigger", b))
    p.call("S-L", ls(sb["grip"]), "Left grip", job("L grip", b) + " (hold)")
    p.call("S-R", rs(sb["trigger"]), "Right trigger", job("R trigger", b))
    p.call("S-R", rs(sb["grip"]), "Right grip (hold)", "The second set, below")

    grip = "Right grip held.   A: %s.   B: %s.   Trigger: %s.   Stick up / down: %s / %s.   Click: item wheel." % (
        job("grip + A", b, True), job("grip + B", b, True), job("grip + R trigger", b, True),
        job("grip + stick up", b, True).replace(" item", ""), job("grip + stick down", b, True))
    return p, [grip,
               "Both grips together: recenter.   Both palms up together, a second: the Game Tome.   In a menu, hold the right grip to point.",
               "Pause menu: click the left stick to hide it for photos.   Valve Index: left trackpad press is the menu, right is the kick.",
               "Change any button: VR Options > Controls.   VR Options is on the main menu and the pause menu."]

def main():
    b = binds()
    print("bindings, from the VR layer's keys and wadsrc/static:")
    for k, key in BUTTONS.items():
        cmd, note = b[k]
        print("   %-18s %-20s %-16s %s" % (k, key, cmd or "-", note))
    if not os.path.exists(LOGO): print("NOTE: no logo at %s - the header has none" % LOGO)
    os.makedirs(OUT, exist_ok=True)
    for name, (page, ctx) in {
        "Controls - %s - Meta Quest 3.png" % PORT_NAME: quest_picture(b),
    }.items():
        path = os.path.join(OUT, name)
        page.render(ctx, path)
        print("wrote", path)

main()
