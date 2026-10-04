"""Writes the README diagrams: Images/<name>.svg (English) and Images/<name>.zh-CN.svg (Chinese).

    python Tools/Images/readme_images.py [output-dir]

Each SVG is self-contained -- system fonts, no scripts, no external references -- draws its own
background, and follows the viewer's light or dark preference through a media query, so it reads
the same on GitHub in either theme. Edit the strings and the layout here, not the SVG files.
"""

import io
import math
import os
import sys
from xml.sax.saxutils import escape

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.normpath(os.path.join(HERE, "..", "..", "Images"))

SANS = ("'Segoe UI', -apple-system, BlinkMacSystemFont, 'Helvetica Neue', Arial, 'PingFang SC', "
        "'Microsoft YaHei', 'Noto Sans CJK SC', sans-serif")
MONO = "ui-monospace, 'Cascadia Code', SFMono-Regular, Consolas, Menlo, 'Liberation Mono', monospace"

ACCENTS = ("teal", "violet", "blue", "amber")

STYLE = """
:root{--bg:#ffffff;--panel:#f6f8fa;--chip:#eaeef2;--line:#d0d7de;--text:#1f2328;--muted:#59636e;
--teal:#0b7f72;--violet:#6e44d8;--blue:#0a64c8;--amber:#a95a00}
@media (prefers-color-scheme:dark){:root{--bg:#0d1117;--panel:#161b22;--chip:#222a35;--line:#30363d;
--text:#e6edf3;--muted:#8d96a0;--teal:#2dd4bf;--violet:#a78bfa;--blue:#58a6ff;--amber:#f2a93b}}
text{font-family:%s;fill:var(--text)}
.mono{font-family:%s}
.muted{fill:var(--muted)}
.b{font-weight:600}
.frame{fill:var(--bg);stroke:var(--line)}
.panel{fill:var(--panel);stroke:var(--line)}
.chip{fill:var(--chip)}
.rule{stroke:var(--line);fill:none}
.wire{stroke:var(--muted);fill:none;stroke-width:1.5}
.head{fill:var(--muted)}
.dash{stroke-dasharray:5 4}
.ink{fill:var(--bg)}
.cm{fill:var(--muted)}
""" % (SANS, MONO)

for _a in ACCENTS:
    STYLE += (".f-%(a)s{fill:var(--%(a)s)}.s-%(a)s{stroke:var(--%(a)s);fill:none;stroke-width:1.5}"
              ".tint-%(a)s{fill:var(--%(a)s);fill-opacity:.11;stroke:var(--%(a)s);stroke-opacity:.55}"
              ".w-%(a)s{stroke:var(--%(a)s);fill:none;stroke-width:1.5}.h-%(a)s{fill:var(--%(a)s)}\n") % {"a": _a}

# Syntax colours for the code cells: keyword, type, string, doc comment.
STYLE += ".kw{fill:var(--violet)}.ty{fill:var(--teal)}.st{fill:var(--amber)}.dc{fill:var(--muted)}\n"


def is_wide(ch):
    return ord(ch) >= 0x2E80


def text_width(s, size, mono=False, bold=False):
    """A deliberately generous estimate; layout leaves slack for font differences."""
    w = 0.0
    for ch in s:
        if is_wide(ch):
            w += size
        elif mono:
            w += size * 0.61
        elif ch in "il.,:;'|!()[]· ":
            w += size * 0.3
        elif ch.isupper() or ch in "mwMW@":
            w += size * 0.66
        else:
            w += size * 0.54
    return w * (1.05 if bold else 1.0)


def tokens(s):
    """Words for Latin text, single characters for CJK, so either wraps."""
    out, cur = [], ""
    for ch in s:
        if is_wide(ch):
            if cur:
                out.append(cur)
                cur = ""
            out.append(ch)
        elif ch == " ":
            cur += ch
            out.append(cur)
            cur = ""
        else:
            cur += ch
    if cur:
        out.append(cur)
    return out


def wrap(s, max_w, size, mono=False):
    lines, cur = [], ""
    for tok in tokens(s):
        if cur and text_width((cur + tok).rstrip(), size, mono) > max_w and tok not in "，。、；：）":
            lines.append(cur.rstrip())
            cur = tok.lstrip()
        else:
            cur += tok
    if cur.strip():
        lines.append(cur.rstrip())
    return lines


class Svg:
    def __init__(self, w, h, title, desc):
        self.w, self.h = w, h
        self.parts = []
        self.title, self.desc = title, desc

    def add(self, s):
        self.parts.append(s)

    def rect(self, x, y, w, h, cls, rx=10, extra=""):
        self.add('<rect x="%g" y="%g" width="%g" height="%g" rx="%g" class="%s"%s/>' % (x, y, w, h, rx, cls, extra))

    def text(self, x, y, s, size=14, cls="", anchor="start", extra=""):
        a = ' text-anchor="%s"' % anchor if anchor != "start" else ""
        c = ' class="%s"' % cls if cls else ""
        self.add('<text x="%g" y="%g" font-size="%g"%s%s%s>%s</text>' % (x, y, size, c, a, extra, escape(s)))

    def spans(self, x, y, segments, size=14, cls="", anchor="start"):
        """segments: [(text, class-or-empty)]"""
        a = ' text-anchor="%s"' % anchor if anchor != "start" else ""
        c = ' class="%s"' % cls if cls else ""
        body = "".join(('<tspan class="%s">%s</tspan>' % (k, escape(t))) if k else escape(t) for t, k in segments)
        self.add('<text x="%g" y="%g" font-size="%g"%s%s xml:space="preserve">%s</text>' % (x, y, size, c, a, body))

    def lines(self, x, y, lines, size=13, lead=None, cls="muted", anchor="start"):
        lead = lead or size * 1.42
        for i, line in enumerate(lines):
            self.text(x, y + i * lead, line, size, cls, anchor)
        return y + len(lines) * lead

    def arrow(self, pts, wire="wire", head="head", size=7.5, radius=10):
        """A polyline through pts with rounded corners and a head on the last point."""
        x2, y2 = pts[-1]
        x1, y1 = pts[-2]
        ang = math.atan2(y2 - y1, x2 - x1)
        end = (x2 - math.cos(ang) * size * 0.7, y2 - math.sin(ang) * size * 0.7)
        pts = list(pts[:-1]) + [end]
        d = "M%g,%g" % pts[0]
        for i in range(1, len(pts) - 1):
            (ax, ay), (bx, by), (cx, cy) = pts[i - 1], pts[i], pts[i + 1]
            l1 = math.hypot(bx - ax, by - ay) or 1
            l2 = math.hypot(cx - bx, cy - by) or 1
            r = min(radius, l1 / 2, l2 / 2)
            p = (bx - (bx - ax) / l1 * r, by - (by - ay) / l1 * r)
            q = (bx + (cx - bx) / l2 * r, by + (cy - by) / l2 * r)
            d += " L%g,%g Q%g,%g %g,%g" % (p[0], p[1], bx, by, q[0], q[1])
        d += " L%g,%g" % pts[-1]
        self.add('<path class="%s" d="%s"/>' % (wire, d))
        s, c = math.sin(ang), math.cos(ang)
        p1 = (x2 - c * size + s * size * 0.55, y2 - s * size - c * size * 0.55)
        p2 = (x2 - c * size - s * size * 0.55, y2 - s * size + c * size * 0.55)
        self.add('<path class="%s" d="M%g,%g L%g,%g L%g,%g Z"/>' % (head, x2, y2, p1[0], p1[1], p2[0], p2[1]))

    def chip(self, x, y, label, size=12, cls="chip", tcls="muted", mono=False, pad=8, h=None, anchor="start"):
        w = text_width(label, size, mono) + pad * 2
        h = h or size + 10
        if anchor == "middle":
            x -= w / 2
        elif anchor == "end":
            x -= w
        self.rect(x, y, w, h, cls, rx=h / 2)
        self.text(x + w / 2, y + h / 2 + size * 0.36, label, size, (tcls + " mono") if mono else tcls, "middle")
        return w

    def badge(self, cx, cy, label, accent):
        self.add('<circle cx="%g" cy="%g" r="11" class="f-%s"/>' % (cx, cy, accent))
        self.text(cx, cy + 4.5, label, 13, "ink b", "middle")

    def heading(self, title, subtitle, sub_size=15):
        self.text(40, 58, title, 26, "b")
        self.lines(40, 88, wrap(subtitle, self.w - 80, sub_size), sub_size, cls="muted")

    def save(self, path):
        head = ('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" role="img" '
                'aria-labelledby="t d">\n<title id="t">%s</title>\n<desc id="d">%s</desc>\n<style>%s</style>\n'
                % (self.w, self.h, self.w, self.h, escape(self.title), escape(self.desc), STYLE))
        frame = '<rect x="0.5" y="0.5" width="%g" height="%g" rx="16" class="frame"/>\n' % (self.w - 1, self.h - 1)
        body = head + frame + "\n".join(self.parts) + "\n</svg>\n"
        with io.open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(body)


# --------------------------------------------------------------------------------------------------
# workflow-overview: source file -> compiler -> what Unreal gets, and the diagnostics coming back.

WORKFLOW = {
    "en": {
        "title": "From a source file to a material",
        "subtitle": "Save a file, and DreamShader rebuilds everything it describes. The source is what you edit; "
                    "the assets are build output.",
        "desc": "Source files under DShader/ are compiled on save by one compiler with two front ends into "
                "materials, functions, instances and Custom Pass pipelines; diagnostics come back with a line and a code.",
        "cols": ("YOU WRITE", "ONE COMPILER", "UNREAL GETS"),
        "tags": ("2.0", "instance", "2.0", "UE 5.8", "header", "1.x"),
        "foot": ("Sources go in version control;", "the assets can always be rebuilt."),
        "save": "save",
        "debounce": "0.25 s",
        "parser": ("2.0 parser", "legacy front end"),
        "stages": (("bind", "names · types · calls"), ("lower", "to a graph IR"),
                   ("passes", "fold · dedupe · prune"), ("validate", "before any asset"),
                   ("emit", "assets · HLSL · targets")),
        "products": (
            ("Material — ThinCustom, the default", "In memory (Ephemeral) until a cook or Materialize writes it to disk."),
            ("Functions · layers · Graph materials", "Saved as .uasset on every successful build; layer blends too."),
            ("Material instances", "From a .dsi, checked against the parent material's parameters."),
            ("Custom Pass pipeline · UE 5.8", "A UDreamPassPipeline, plus a render target per exported buffer."),
        ),
        "diag": "file(line,col): DSHnnnn message",
        "diag_where": "shown in the Material Content Browser, VS Code and Rider",
    },
    "zh": {
        "title": "从源文件到材质",
        "subtitle": "保存文件，DreamShader 就重建它描述的一切。你编辑的是源文件，资源只是生成结果。",
        "desc": "DShader/ 下的源文件在保存时由同一个编译器（两个前端）编译成材质、函数、实例和 Custom Pass 管线；诊断带着行号和代码返回。",
        "cols": ("你写的", "一个编译器", "Unreal 得到的"),
        "tags": ("2.0", "实例", "2.0", "UE 5.8", "头文件", "1.x"),
        "foot": ("源文件纳入版本控制；", "资源随时可以重新生成。"),
        "save": "保存",
        "debounce": "0.25 秒",
        "parser": ("2.0 解析器", "旧版前端"),
        "stages": (("绑定", "名称 · 类型 · 调用"), ("降低", "为图 IR"),
                   ("优化", "折叠 · 去重 · 剪枝"), ("校验", "先于任何资源"),
                   ("生成", "资源 · HLSL · 渲染目标")),
        "products": (
            ("材质 — ThinCustom（默认）", "只在内存里（Ephemeral），烘焙或 Materialize 时才写入磁盘。"),
            ("函数 · 层 · Graph 材质", "每次成功生成都保存为 .uasset，层混合也一样。"),
            ("材质实例", "来自 .dsi，按父材质的参数逐项检查。"),
            ("Custom Pass 管线 · UE 5.8", "一个 UDreamPassPipeline，每个导出的缓冲再加一张渲染目标。"),
        ),
        "diag": "file(line,col): DSHnnnn message",
        "diag_where": "显示在 Material Content Browser、VS Code 和 Rider 中",
    },
}

FILES = (  # (directory, stem, extension, accent)
    ("Materials/", "M_Panel", ".dss", "teal"),
    ("Materials/", "MI_Red", ".dsi", "blue"),
    ("Functions/", "MF_Noise", ".dss", "teal"),
    ("Passes/", "CP_Outline", ".dsp", "amber"),
    ("Shared/", "Common", ".dsh", None),
    ("Legacy/", "M_Old", ".dsm", "violet"),
)


def workflow(lang):
    L = WORKFLOW[lang]
    caps = lang == "en"
    s = Svg(1200, 604, L["title"], L["desc"])
    s.heading(L["title"], L["subtitle"])

    top, height = 150, 352
    cols = ((40, "teal"), (435, "violet"), (830, "amber"))
    for i, (x, accent) in enumerate(cols):
        s.badge(x + 11, 128, str(i + 1), accent)
        s.text(x + 30, 133, L["cols"][i], 13, "b muted", extra=' letter-spacing="1.2"' if caps else "")
        s.rect(x, top, 330, height, "panel", rx=12)

    # 1 -- the source tree
    x = 40
    s.add('<path class="s-teal" d="M60,172 h7 l3,3 h11 v12 h-21 z"/>')
    s.text(90, 186, "DShader/", 15, "mono b")
    s.add('<line x1="56" y1="200" x2="354" y2="200" class="rule"/>')
    for i, (folder, stem, ext, accent) in enumerate(FILES):
        y = 230 + i * 36
        s.add('<circle cx="64" cy="%g" r="4" class="%s"/>' % (y - 5, "f-" + accent if accent else "head"))
        s.spans(78, y, [(folder, "muted"), (stem, ""), (ext, "f-" + accent if accent else "muted")], 14, "mono")
        s.chip(352, y - 16, L["tags"][i], 11, anchor="end", h=21)
    s.add('<line x1="56" y1="440" x2="354" y2="440" class="rule"/>')
    s.lines(60, 466, L["foot"], 13)

    # save ->
    s.arrow([(374, 300), (431, 300)])
    s.text(402.5, 290, L["save"], 12.5, "b", "middle")
    s.text(402.5, 320, L["debounce"], 11.5, "muted", "middle")

    # 2 -- one compiler, two front ends
    for j, (cx, accent, exts) in enumerate(((451, "teal", ".dss .dsi .dsp"), (604, "violet", ".dsm .dsf"))):
        s.rect(cx, 168, 145, 56, "tint-" + accent, rx=8)
        s.text(cx + 12, 191, L["parser"][j], 13, "b")
        s.text(cx + 12, 211, exts, 12, "mono muted")
    s.add('<path class="wire" d="M523.5,224 V234 Q523.5,240 529.5,240 H670.5 Q676.5,240 676.5,234 V224"/>')
    s.arrow([(600, 240), (600, 262)])
    for k, (name, detail) in enumerate(L["stages"]):
        y = 264 + k * 46
        last = k == len(L["stages"]) - 1
        s.rect(455, y, 290, 34, "tint-violet" if last else "chip", rx=17)
        s.spans(600, y + 22, [(name, "b"), ("   " + detail, "muted")], 13.5, anchor="middle")
        if not last:
            s.arrow([(600, y + 34), (600, y + 46)], size=5.5)

    # -> 3 -- the products
    s.arrow([(769, 300), (826, 300)])
    for k, (title, detail) in enumerate(L["products"]):
        y = 166 + k * 82
        accent = ("teal", "violet", "blue", "amber")[k]
        s.rect(846, y, 298, 74, "tint-" + accent, rx=8, extra=' stroke-dasharray="5 4"' if k == 0 else "")
        s.text(860, y + 24, title, 14, "b")
        s.lines(860, y + 45, wrap(detail, 270, 12.5)[:2], 12.5, 18)

    # diagnostics, back to the source
    s.arrow([(600, 502), (600, 558), (205, 558), (205, 506)], wire="wire dash")
    s.chip(402.5, 547, L["diag"], 12, cls="panel", tcls="", mono=True, anchor="middle", h=22)
    s.text(618, 563, L["diag_where"], 13, "muted")
    return s


# --------------------------------------------------------------------------------------------------
# language-model: the 1.x spelling and the 2.0 spelling of each thing the compiler builds.

def K(t):
    return (t, "kw")


def T(t):
    return (t, "ty")


def S(t):
    return (t, "st")


def D(t):
    return (t, "dc")


def P(t):
    return (t, "")


ROWS = (
    # (1.x segments or None, (en, zh) product, 2.0 segments, tag or None)
    ([K("Shader"), P("(Name = "), S('"…"'), P(") { … }")], ("Material", "材质"),
     [K("export void "), P("M("), K("inout "), T("material"), P(" m)")], None),
    ([K("ShaderFunction"), P("(Name = "), S('"…"'), P(") { … }")], ("Material Function", "材质函数"),
     [K("export "), T("float3"), P(" F("), T("float2"), P(" UV) { … }")], None),
    ([K("ShaderLayer"), P("(Name = "), S('"…"'), P(") { … }")], ("Material Layer", "材质层"),
     [D("/// @layer  "), K("export void "), P("L("), K("inout "), T("material"), P(" m)")], None),
    ([K("ShaderLayerBlend"), P("(Name = "), S('"…"'), P(") { … }")], ("Layer Blend", "层混合"),
     [D("/// @layerblend  "), K("export void "), P("B(…)")], None),
    (None, ("Material Instance", "材质实例"),
     [K("#pragma "), P("instance(Parent = "), S('"/Game/M_Panel"'), P(")")], ".dsi"),
    (None, ("Custom Pass pipeline", "Custom Pass 管线"),
     [K("#pragma "), P("pipeline  "), K("buffer"), P(" …  "), K("pass"), P(" …")], ".dsp · UE 5.8"),
    "INSIDE",
    ([K("Properties"), P(" = { "), T("float4"), P(" Tint = …; }")], ("parameter nodes", "参数节点"),
     [D("/// @group Look  "), K("uniform "), T("float4"), P(" Tint = …;")], None),
    ([K("Settings"), P(" = { ShadingModel = "), S('"Unlit"'), P("; }")], ("material settings", "材质设置"),
     [K("#pragma "), P("material(ShadingModel = Unlit)")], None),
    ([K("Function"), P(" F(…)  /  "), K("GraphFunction"), P(" G(…)")], ("a Custom HLSL node", "Custom HLSL 节点"),
     [D("/// @custom  "), T("float"), P(" F(…) { … }")], None),
    ([K("VirtualFunction"), P("(Name = "), S('"…"'), P(")")], ("a call to an existing asset", "调用已有的函数资源"),
     [D("/// @asset /Game/MF_X  "), K("extern "), T("float"), P(" MF_X(…);")], None),
    (None, ("nodes, inlined per call", "节点，在调用处内联"),
     [T("float"), P(" Wave("), T("float"), P(" X) { … }")], "helper"),
)

LANGMODEL = {
    "en": {
        "title": "Two spellings, one compiler",
        "subtitle": "Every 1.x block has a 2.0 spelling that builds the same thing, and 2.0 adds instances and "
                    "Custom Pass pipelines. Both front ends feed one pipeline, so both get the same checks.",
        "desc": "A table pairing each 1.x DreamShaderLang block with its 2.0 spelling and the Unreal asset or "
                "graph element both of them build.",
        "legacy": "1.x  ·  .dsm .dsf .dsh",
        "modern": "2.0  ·  .dss .dsi .dsp",
        "builds": "builds",
        "groups": ("ASSETS", "INSIDE A GRAPH"),
        "none": "no 1.x form",
        "helper": "helper",
        "foot": (("dsc migrate", "rewrites 1.x sources as .dss, and proves each rewrite builds the same graph before writing it"),
                 ("dsc decompile", "writes the right-hand column from an existing asset")),
    },
    "zh": {
        "title": "两种写法，同一个编译器",
        "subtitle": "每个 1.x 块都有一个生成同样结果的 2.0 写法，2.0 还多了材质实例和 Custom Pass 管线。"
                    "两个前端接入同一条流水线，检查完全一样。",
        "desc": "对照表：每个 1.x DreamShaderLang 块、它的 2.0 写法，以及两者生成的 Unreal 资源或图元素。",
        "legacy": "1.x  ·  .dsm .dsf .dsh",
        "modern": "2.0  ·  .dss .dsi .dsp",
        "builds": "生成",
        "groups": ("生成的资源", "图内部"),
        "none": "1.x 没有对应写法",
        "helper": "辅助函数",
        "foot": (("dsc migrate", "把 1.x 源文件改写为 .dss，写入前先证明改写结果生成同一张图"),
                 ("dsc decompile", "从现有资源反向写出右栏的 2.0 源文件")),
    },
}


def langmodel(lang):
    L = LANGMODEL[lang]
    zh = lang == "zh"
    s = Svg(1200, 850, L["title"], L["desc"])
    s.heading(L["title"], L["subtitle"])

    LX, LW, CX, CW, RX, RW = 40, 420, 490, 220, 740, 420
    s.chip(LX, 134, L["legacy"], 13, cls="tint-violet", tcls="b", h=26, pad=12)
    s.chip(RX + RW, 134, L["modern"], 13, cls="tint-teal", tcls="b", h=26, pad=12, anchor="end")
    s.text(CX + CW / 2, 152, L["builds"], 13, "muted", "middle")

    y = 200
    group = 0
    s.text(LX, y - 12, L["groups"][0], 12, "b muted", extra="" if zh else ' letter-spacing="1.3"')
    for row in ROWS:
        if row == "INSIDE":
            group = 1
            y += 26
            s.text(LX, y - 12, L["groups"][1], 12, "b muted", extra="" if zh else ' letter-spacing="1.3"')
            continue
        legacy, product, modern, tag = row
        mid = y + 20
        if legacy:
            s.rect(LX, y, LW, 40, "panel", rx=8)
            s.spans(LX + 16, y + 25, legacy, 13, "mono")
            s.arrow([(LX + LW + 2, mid), (CX - 3, mid)], wire="w-violet", head="h-violet", size=6.5)
        else:
            s.rect(LX, y, LW, 40, "rule dash", rx=8)
            s.text(LX + 16, y + 25, L["none"], 13, "muted")
        accent = "blue" if group == 0 else "amber"
        s.rect(CX, y, CW, 40, "tint-" + accent, rx=20)
        s.text(CX + CW / 2, y + 25, product[1] if zh else product[0], 13.5, "b", "middle")
        s.rect(RX, y, RW, 40, "panel", rx=8)
        s.spans(RX + 16, y + 25, modern, 13, "mono")
        if tag:
            s.chip(RX + RW - 10, y + 9, L["helper"] if tag == "helper" else tag, 11, anchor="end", h=22,
                   mono=tag != "helper")
        s.arrow([(RX - 2, mid), (CX + CW + 3, mid)], wire="w-teal", head="h-teal", size=6.5)
        y += 48

    y += 18
    s.add('<line x1="40" y1="%g" x2="1160" y2="%g" class="rule"/>' % (y, y))
    for k, (cmd, what) in enumerate(L["foot"]):
        yy = y + 34 + k * 30
        w = s.chip(LX, yy - 16, cmd, 12.5, cls="chip", tcls="b", mono=True, h=22)
        s.text(LX + w + 12, yy, what, 13.5, "muted")
    s.h = int(yy + 30)
    return s


# --------------------------------------------------------------------------------------------------
# editor-tools: the editor, the code editors on the bridge, and the headless commandlet.

TOOLS = {
    "en": {
        "title": "Where DreamShader meets your tools",
        "subtitle": "The editor builds and previews. Code editors talk to it over the bridge. CI and agents run "
                    "the same compiler with no editor at all.",
        "desc": "Code editors connect to the Unreal Editor through the DreamShader bridge; a headless commandlet "
                "runs the same compiler for CI and agents; all three work on the DShader/ sources.",
        "left": ("Code editors", "VS Code · Rider"),
        "vscode": ("VS Code", "highlighting, completion, hover, go to definition, references, diagnostics, material preview"),
        "rider": ("Rider", "grammar and PSI, completion, navigation, diagnostics, semantic tokens, inlay hints"),
        "center": ("Unreal Editor", "with DreamShader"),
        "items": (("Material Content Browser", "status, diagnostics, provenance, preview"),
                  ("Source watcher", "save → rebuild what it affects"),
                  ("Decompiler", "an asset → .dss · .dsi · .dsp"),
                  ("Material Editor", "a node → Open Source Line")),
        "right": ("Headless", "no editor, no bridge"),
        "ci": ("CI", "a non-zero exit fails the gate"),
        "agents": ("AI agents", "the .skill/ driver and skills"),
        "requests": "requests",
        "diagnostics": "diagnostics",
        "links": ("edits", "watches · builds", "builds"),
        "sources": "the sources — .dss .dsi .dsp .dsh .dsm .dsf",
    },
    "zh": {
        "title": "DreamShader 与你的工具",
        "subtitle": "编辑器负责生成和预览；代码编辑器通过 bridge 与它通信；CI 和 AI 代理不开编辑器，运行同一个编译器。",
        "desc": "代码编辑器通过 DreamShader bridge 连接 Unreal 编辑器；无界面的 commandlet 为 CI 和 AI 代理运行同一个编译器；三者都基于 DShader/ 源文件工作。",
        "left": ("代码编辑器", "VS Code · Rider"),
        "vscode": ("VS Code", "语法高亮、补全、悬停、跳转定义、查找引用、诊断、材质预览"),
        "rider": ("Rider", "语法与 PSI 解析、补全、导航、诊断、语义高亮、内联提示"),
        "center": ("Unreal 编辑器", "装有 DreamShader"),
        "items": (("Material Content Browser", "状态、诊断、来源追踪、预览"),
                  ("源文件监视", "保存 → 重新生成受影响的资源"),
                  ("反编译器", "资源 → .dss · .dsi · .dsp"),
                  ("材质编辑器", "节点 → Open Source Line")),
        "right": ("无界面", "不需要编辑器和 bridge"),
        "ci": ("CI", "退出码非零即判定失败"),
        "agents": ("AI 代理", ".skill/ 里的驱动脚本和 skill"),
        "requests": "请求",
        "diagnostics": "诊断",
        "links": ("编辑", "监视 · 生成", "生成"),
        "sources": "源文件 — .dss .dsi .dsp .dsh .dsm .dsf",
    },
}

VERBS = ("compile", "check", "decompile", "migrate", "fmt", "list-generated", "pass-registry", "dump-ir")


def tools(lang):
    L = TOOLS[lang]
    s = Svg(1200, 600, L["title"], L["desc"])
    s.heading(L["title"], L["subtitle"])

    top, h = 136, 318
    cards = ((40, "blue", L["left"]), (450, "violet", L["center"]), (860, "teal", L["right"]))
    for x, accent, (name, sub) in cards:
        s.rect(x, top, 300, h, "panel", rx=12)
        s.add('<rect x="%g" y="%g" width="4" height="34" rx="2" class="f-%s"/>' % (x + 18, top + 20, accent))
        s.text(x + 32, top + 36, name, 17, "b")
        s.text(x + 32, top + 55, sub, 12.5, "mono muted" if x == 860 and lang == "en" else "muted")
        s.add('<line x1="%g" y1="%g" x2="%g" y2="%g" class="rule"/>' % (x + 16, top + 72, x + 284, top + 72))

    # code editors
    y = top + 100
    for title, detail in (L["vscode"], L["rider"]):
        s.text(60, y, title, 14.5, "b")
        y = s.lines(60, y + 22, wrap(detail, 262, 12.5), 12.5, 18) + 16
    s.lines(60, top + h - 36, ("ws://127.0.0.1:17864", "Saved/DreamShader/Bridge/"), 11.5, 18, "mono muted")

    # the editor
    for k, (title, detail) in enumerate(L["items"]):
        y = top + 100 + k * 54
        s.add('<circle cx="476" cy="%g" r="4" class="f-violet"/>' % (y - 5))
        s.text(490, y, title, 14, "b")
        s.text(490, y + 20, detail, 12.5, "muted")

    # headless
    x, y = 880, top + 90
    for verb in VERBS:
        w = text_width(verb, 12, True) + 16
        if x + w > 1140:
            x, y = 880, y + 30
        s.chip(x, y, verb, 12, mono=True, h=22)
        x += w + 6
    y += 56
    for title, detail in (L["ci"], L["agents"]):
        s.text(880, y, title, 14, "b")
        s.text(880, y + 20, detail, 12.5, "muted")
        y += 50

    # the bridge between the code editors and the editor
    s.arrow([(343, 268), (446, 268)])
    s.arrow([(446, 312), (343, 312)])
    s.chip(394.5, 279, "bridge", 12, cls="chip", tcls="b", h=22, anchor="middle")
    s.text(394.5, 258, L["requests"], 11.5, "muted", "middle")
    s.text(394.5, 333, L["diagnostics"], 11.5, "muted", "middle")

    # all three on the sources
    bar = 506
    s.rect(40, bar, 1120, 50, "tint-teal", rx=12)
    s.spans(60, bar + 31, [("DShader/", "b mono"), ("   " + L["sources"], "muted")], 14)
    s.chip(1144, bar + 14, "Packages/@scope/name", 11.5, mono=True, anchor="end", h=22)
    for (cx, label) in zip((190, 600, 1010), L["links"]):
        s.arrow([(cx, top + h + 2), (cx, bar - 3)], size=6.5)
        s.text(cx + 10, top + h + 30, label, 12, "muted")
    return s


def main():
    os.makedirs(OUT, exist_ok=True)
    for name, build in (("workflow-overview", workflow), ("language-model", langmodel), ("editor-tools", tools)):
        for lang, suffix in (("en", ""), ("zh", ".zh-CN")):
            path = os.path.join(OUT, name + suffix + ".svg")
            build(lang).save(path)
            print("wrote", os.path.relpath(path))


if __name__ == "__main__":
    main()
