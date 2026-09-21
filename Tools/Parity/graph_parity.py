"""Structural parity comparator for DreamShader dump-graph JSON (schema 1).

The 1.x generator is deleted in M4, so its output survives only as frozen dumps
(`Saved/DreamShader/GraphBaseline/v2-6c2e0b6-formal`, `.../v2-6c2e0b6-generate-corpus`). The 2.0 compiler must
reproduce them. A dump numbers its nodes n0, n1, ... in a canonical traversal, so one extra or missing node renumbers
everything after it and a text diff is useless. This tool compares graphs by structure instead: every root (a material
property, a function output, a custom output node) is reduced to a recursive signature of the subgraph that feeds it.

Both sides go through the same normalisations, so a normalisation can only erase a difference of form:

  R  named reroutes are transparent: a NamedRerouteUsage reads what its declaration reads.
  M  channel canonicalisation (PD-1 / D1): a wire is (source node, base value, channel list). Inline pin masks,
     masked engine outputs (VectorParameter `RGB`, TextureSample `R`, ...) and ComponentMask nodes all fold into the
     channel list, and a list that covers the whole base value is dropped.
  N  negative literals (D4): Multiply(constant, Constant(-1)) is the negated constant.
  D  unreachable nodes (D2) never enter a signature.
  S  structurally identical nodes (D3) produce identical signatures, so duplicates merge by construction.
  K  key filters (PD-3, D5, D7): named props are left out of every signature.
  A  material attributes (PD-4): a chain of SetMaterialAttributes nodes over a material, or over an empty
     MakeMaterialAttributes, is one set of attributes over that material.
  G  attribute reads (PD-6): BreakMaterialAttributes over a material the graph wrote reads the value written.
  P  pin prefixes (PD-7): a mask that is exactly the leading components a material attribute (or a known
     custom-output pin) reads is dropped; 1.x lost such masks on those pins, 2.0 keeps them.
  B  splats (PD-5): AppendVector(x, x) over one wire is x; the engine spreads a scalar where a vector is wanted.
  C  constant appends (D4): AppendVector(constant, constant) is the constant vector.
  T  output widths (PD-2): the `type` a dump gives a function output is left out of the metadata comparison.

Usage:
  python graph_parity.py stats <dump-root> [--filter-keys K1,K2]
  python graph_parity.py compare <baseline-root> <candidate-root> [--exclude GLOB ...] [--filter-keys K1,K2]
                               [--report FILE.md] [--json FILE.json] [--max-diffs N]
  python graph_parity.py pair <baseline.graph.json> <candidate.graph.json> [--filter-keys K1,K2]

Exit code: 0 when every compared pair is equal (compare/pair) or always (stats); 1 otherwise; 2 on bad usage.
"""
import argparse
import fnmatch
import io
import json
import os
import re
import sys
from collections import Counter, defaultdict

sys.setrecursionlimit(20000)

LEGACY_PARITY_FILTER_KEYS = ("Code", "IncludeFilePaths", "AdditionalOutputs")

# Engine output layouts (F:/UnrealEngine/UE_Moon/Engine/Source/Runtime/Engine/Private/Materials/MaterialExpressions.cpp).
# A "view" class publishes several outputs that are channel views of ONE value; its input masks are absolute channel
# flags on that full value.
VIEW_OUTPUTS = {
    "VectorParameter": ("rgb", "r", "g", "b", "a", "rgba"),
    "TextureSample": ("rgb", "r", "g", "b", "a", "rgba"),   # and every TextureSample* subclass
    "VertexColor": ("rgb", "r", "g", "b", "a"),
    "LocalPosition": ("rgb", "rg", "b"),
    "WorldPosition": ("rgb", "rg", "b"),
}
VIEW_FULL = {"VectorParameter": "rgba", "TextureSample": "rgba", "VertexColor": "rgba", "LocalPosition": "rgb",
             "WorldPosition": "rgb"}
# Classes whose outputs are distinct values, each masked: (width letters per output).
DISTINCT_MASKED_OUTPUTS = {"Bounds": ("rgb", "rgb", "rgb", "rgb")}
# Single-output classes of a known width (only used to recognise an identity mask).
KNOWN_WIDTH = {
    "Constant": "r", "Constant2Vector": "rg", "Constant3Vector": "rgb", "Constant4Vector": "rgba",
    "ScalarParameter": "r", "TextureCoordinate": "rg", "Time": "r", "StaticBool": "r", "StaticBoolParameter": "r",
}
FUNCTION_INPUT_WIDTH = {
    "FunctionInput_Scalar": "r", "FunctionInput_Vector2": "rg", "FunctionInput_Vector3": "rgb",
    "FunctionInput_Vector4": "rgba",
}
TYPE_WIDTH = {"float": "r", "float1": "r", "float2": "rg", "float3": "rgb", "float4": "rgba"}
CUSTOM_OUTPUT_CLASSES = {
    "AbsorptionMediumMaterialOutput", "BentNormalCustomOutput", "ClearCoatNormalCustomOutput", "FirstPersonOutput",
    "MaterialCache", "MoonToonFaceOverlay", "MotionVectorWorldOffsetOutput", "NeuralNetworkInput",
    "RuntimeVirtualTextureOutput", "SingleLayerWaterMaterialOutput", "SubsurfaceMediumMaterialOutput", "TangentOutput",
    "TemporalResponsivenessOutput", "ThinTranslucentMaterialOutput", "ToonMaterialOutput", "VertexInterpolator",
    "VolumetricAdvancedMaterialOutput", "VolumetricCloudEmptySpaceSkippingOutput",
}
TRANSPARENT = {"NamedRerouteUsage", "NamedRerouteDeclaration", "Reroute", "ComponentMask"}
# PD-7: pins that take the leading components they need. 1.x dropped the inline mask of a value on its way into a
# SetMaterialAttributes input and into the pin of an Outputs-block expression (it connected the expression and the
# output index and nothing else), so `m.BaseColor = c4` and `m.BaseColor = c4.rgb` are one wire there; 2.0 keeps the
# mask. A mask that is exactly the leading components the pin reads says nothing the pin does not already do.
ATTRIBUTE_WIDTH = {
    "BaseColor": 3, "EmissiveColor": 3, "Normal": 3, "Tangent": 3, "WorldPositionOffset": 3, "SubsurfaceColor": 3,
    "Metallic": 1, "Specular": 1, "Roughness": 1, "Anisotropy": 1, "Opacity": 1, "OpacityMask": 1, "ClearCoat": 1,
    "ClearCoatRoughness": 1, "AmbientOcclusion": 1, "PixelDepthOffset": 1, "Displacement": 1,
}
CUSTOM_OUTPUT_PIN_WIDTH = {("ThinTranslucentMaterialOutput", 0): 3}
# UMaterialExpressionBreakMaterialAttributes' outputs, in the engine's order, as far as that order is the same in every
# engine this plugin builds against.
BREAK_OUTPUTS = ("BaseColor", "Metallic", "Specular", "Roughness", "Anisotropy", "EmissiveColor", "Opacity", "OpacityMask",
                 "Normal", "Tangent", "WorldPositionOffset", "SubsurfaceColor", "ClearCoat", "ClearCoatRoughness",
                 "AmbientOcclusion", "Refraction")


def drop_pin_prefix(channels, width):
    """None when `channels` is exactly the leading `width` components, which is what the pin reads anyway."""
    return None if (width and channels == CHANNELS[:width]) else channels
CHANNELS = "rgba"
STRUCT_TEXT = re.compile(r"^\((?:[A-Za-z]\w*=[^,()]*,?)+\)$")
STRUCT_PAIR = re.compile(r"([A-Za-z]\w*)=([^,()]*)")


def short_class(path):
    leaf = path.rsplit(".", 1)[-1]
    return leaf[len("MaterialExpression"):] if leaf.startswith("MaterialExpression") else leaf


def view_key(cls):
    if cls.startswith("TextureSample"):
        return "TextureSample"
    return cls if cls in VIEW_OUTPUTS else None


def compose(channels, mask):
    """`mask` letters are positions in the value that `channels` describes (None = the whole base value)."""
    if not mask:
        return channels
    if channels is None:
        return mask
    picked = []
    for letter in mask:
        index = CHANNELS.find(letter)
        if 0 <= index < len(channels):
            picked.append(channels[index])
        else:
            picked.append("?")
    if "?" in picked and all(letter in channels for letter in mask):
        # The 1.x generator left the pin mask of a channel view on a wire that reaches the view through a named reroute:
        # VertexColor's `A` output into a reroute, and `a` again on the usage's wire. The flags of such a mask name
        # channels of the whole value, as they do on a wire that comes straight from the view, so where the letters
        # cannot be positions they are taken as the channels they name.
        return mask
    return "".join(picked)


def canon_number(value):
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return round(float(value), 5)
    return value


def canon_value(value):
    if isinstance(value, (int, float, bool)) or value is None:
        return canon_number(value)
    if isinstance(value, str):
        text = value.strip()
        if STRUCT_TEXT.match(text):
            pairs = []
            for key, raw in STRUCT_PAIR.findall(text):
                try:
                    pairs.append((key, round(float(raw), 5)))
                except ValueError:
                    pairs.append((key, raw.strip()))
            return ("struct",) + tuple(pairs)
        return value
    if isinstance(value, list):
        return ("list",) + tuple(canon_value(v) for v in value)
    if isinstance(value, dict):
        return ("dict",) + tuple(sorted((k, canon_value(v)) for k, v in value.items()))
    return repr(value)


class Graph:
    """One dump, normalised lazily."""

    def __init__(self, data, path, filter_keys, callee_widths=None):
        self.data = data
        self.path = path
        self.kind = data.get("kind", "?")
        self.filter_keys = set(filter_keys)
        self.nodes = {n["id"]: n for n in data.get("nodes", [])}
        self.cls = {nid: short_class(n["class"]) for nid, n in self.nodes.items()}
        self.declarations = {}
        for nid, n in self.nodes.items():
            if self.cls[nid] == "NamedRerouteDeclaration":
                self.declarations[str(n.get("props", {}).get("Name", ""))] = nid
        self.callee_widths = callee_widths or {}
        self.sig_cache = {}
        self.in_progress = set()
        self.stats = Counter()
        self.class_stats = defaultdict(Counter)

    # ---------------------------------------------------------------------------------------------------------- wires
    def output_width(self, nid, output):
        cls = self.cls[nid]
        props = self.nodes[nid].get("props", {})
        if cls in KNOWN_WIDTH and output == 0:
            return KNOWN_WIDTH[cls]
        if cls == "FunctionInput" and output == 0:
            return FUNCTION_INPUT_WIDTH.get(str(props.get("InputType", "")))
        if cls == "MaterialFunctionCall":
            widths = self.callee_widths.get(str(props.get("MaterialFunction", "")))
            if widths and 0 <= output < len(widths):
                return widths[output]
        return None

    def resolve(self, edge, depth=0):
        """(source node id or None, base, channels) for one input edge, through every transparent node.

        Memoised per edge object, so every counter in `stats` counts a wire once however often it is walked.
        """
        cache = self.__dict__.setdefault("resolve_cache", {})
        if "raw_masks_counted" not in self.__dict__:
            self.raw_masks_counted = True
            self.stats["M: inline masks in the dump"] = self.count_raw_masks()
        hit = cache.get(id(edge))
        if hit is not None:
            return hit
        result = self.resolve_uncached(edge, depth)
        cache[id(edge)] = result
        return result

    def count_raw_masks(self):
        count = sum(1 for n in self.nodes.values() for e in n.get("inputs", []) if e.get("mask"))
        properties = self.data.get("properties")
        if isinstance(properties, dict):
            count += sum(1 for e in properties.values() if isinstance(e, dict) and e.get("mask"))
        return count

    def resolve_uncached(self, edge, depth):
        if depth > 256:
            return (None, "cycle", None)
        nid = edge.get("from")
        if nid not in self.nodes:
            return (None, "dangling", None)
        cls = self.cls[nid]
        output = int(edge.get("output") or 0)
        mask = edge.get("mask") or None
        node = self.nodes[nid]
        if cls in ("NamedRerouteUsage", "NamedRerouteDeclaration", "Reroute"):
            if cls == "NamedRerouteUsage":
                decl = self.declarations.get(str(node.get("reroute", {}).get("declaration", "")))
                inputs = self.nodes[decl]["inputs"] if decl else []
            else:
                inputs = node.get("inputs", [])
            if not inputs:
                return (None, "unconnected-reroute", None)
            self.stats["R: reroute hops"] += 1
            source, base, channels = self.resolve(inputs[0], depth + 1)
            return self.finish(source, base, compose(channels, mask))
        if cls == "BreakMaterialAttributes" and 0 <= output < len(BREAK_OUTPUTS):
            # G (PD-6): an attribute read back from a material this graph wrote is the value written. 1.x built
            # Break(Set(..., Roughness = R)) and read the Break's Roughness; 2.0 hands R on.
            inputs = node.get("inputs", [])
            written = self.find_attribute_edge(inputs[0], BREAK_OUTPUTS[output]) if inputs else None
            if written is not None:
                self.stats["G: attribute read folded"] += 1
                source, base, channels = self.resolve(written, depth + 1)
                return self.finish(source, base, compose(channels, mask))
        if cls == "ComponentMask":
            inputs = node.get("inputs", [])
            if not inputs:
                return (None, "unconnected-mask", None)
            props = node.get("props", {})
            flags = "".join(c for c in CHANNELS if props.get(c.upper()))
            self.stats["M: ComponentMask folded"] += 1
            source, base, channels = self.resolve(inputs[0], depth + 1)
            return self.finish(source, base, compose(compose(channels, flags), mask))
        view = view_key(cls)
        if view:
            outputs = VIEW_OUTPUTS[view]
            channels = mask if mask else (outputs[output] if 0 <= output < len(outputs) else None)
            if mask:
                self.stats["M: inline mask"] += 1
            return self.finish(nid, "full", channels, VIEW_FULL[view])
        if cls in DISTINCT_MASKED_OUTPUTS:
            widths = DISTINCT_MASKED_OUTPUTS[cls]
            full = widths[output] if 0 <= output < len(widths) else None
            if mask:
                self.stats["M: inline mask"] += 1
            return self.finish(nid, output, mask if mask else full, full)
        if mask:
            self.stats["M: inline mask"] += 1
        return self.finish(nid, output, mask, self.output_width(nid, output))

    def finish(self, source, base, channels, full=None):
        if source is not None and full is None:
            if base == "full":
                full = VIEW_FULL.get(view_key(self.cls[source]) or "", None)
            elif isinstance(base, int):
                full = self.output_width(source, base)
                cls = self.cls[source]
                if cls in DISTINCT_MASKED_OUTPUTS:
                    widths = DISTINCT_MASKED_OUTPUTS[cls]
                    full = widths[base] if 0 <= base < len(widths) else None
        if channels is not None and ((full is not None and channels == full) or (full is None and channels == "rgba")):
            self.stats["M: identity dropped"] += 1
            channels = None
        return (source, base, channels)

    # ------------------------------------------------------------------------------------------------------ constants
    def constant_value(self, nid):
        cls = self.cls[nid]
        props = self.nodes[nid].get("props", {})
        if cls == "Constant":
            return (float(props.get("R", 0)),)
        if cls == "Constant2Vector":
            return (float(props.get("R", 0)), float(props.get("G", 0)))
        if cls in ("Constant3Vector", "Constant4Vector"):
            parsed = canon_value(props.get("Constant", ""))
            values = {k: v for k, v in parsed[1:]} if isinstance(parsed, tuple) else {}
            letters = "RGB" if cls == "Constant3Vector" else "RGBA"
            return tuple(float(values.get(c, 0.0)) for c in letters)
        return None

    def folded_negation(self, nid):
        """Multiply(constant, Constant(-1)) -> the negated constant tuple, else None."""
        if self.cls[nid] != "Multiply":
            return None
        inputs = sorted(self.nodes[nid].get("inputs", []), key=lambda e: e.get("index", 0))
        if len(inputs) != 2:
            return None
        resolved = [self.resolve(e) for e in inputs]
        if any(r[0] is None or r[2] is not None for r in resolved):
            return None
        values = [self.constant_value(r[0]) for r in resolved]
        if values[0] is None or values[1] is None:
            return None
        for minus, other in ((values[1], values[0]), (values[0], values[1])):
            if minus == (-1.0,):
                return tuple(-v for v in other)
        return None

    # ----------------------------------------------------------------------------------------------------- signatures
    def node_sig(self, nid):
        if nid in self.sig_cache:
            return self.sig_cache[nid]
        if nid in self.in_progress:
            return ("cycle", self.cls[nid])
        self.in_progress.add(nid)
        folded = self.folded_negation(nid)
        if folded is not None:
            self.stats["N: negative literal folded"] += 1
            sig = ("const", tuple(round(v, 5) for v in folded))
        elif self.cls[nid] in ("Constant", "Constant2Vector", "Constant3Vector", "Constant4Vector"):
            sig = ("const", tuple(round(v, 5) for v in self.constant_value(nid)))
        elif self.cls[nid] in ("SetMaterialAttributes", "MakeMaterialAttributes"):
            sig = self.material_attributes_sig(nid)
        else:
            node = self.nodes[nid]
            props = tuple(sorted((k, canon_value(v)) for k, v in node.get("props", {}).items()
                                 if k not in self.filter_keys))
            inputs = []
            for edge in sorted(node.get("inputs", []), key=lambda e: e.get("index", 0)):
                source, base, channels = self.resolve(edge)
                channels = drop_pin_prefix(channels, CUSTOM_OUTPUT_PIN_WIDTH.get((self.cls[nid], edge.get("index", 0))))
                child = self.node_sig(source) if source is not None else ("missing", base)
                inputs.append((edge.get("index", 0), edge.get("name", ""), base, channels, child))
            sig = (self.cls[nid], props, tuple(inputs))
            # C (D4): an AppendVector of constants is the constant vector. 1.x built the zero of a `float3 v;` by
            # appending one Constant node to itself; 2.0 writes the Constant3Vector.
            if self.cls[nid] == "AppendVector" and len(inputs) == 2                     and all(item[4][0] == "const" and item[3] is None for item in inputs)                     and len(inputs[0][4][1]) + len(inputs[1][4][1]) <= 4:
                self.stats["C: constant append folded"] += 1
                sig = ("const", tuple(inputs[0][4][1]) + tuple(inputs[1][4][1]))
            # B (PD-5): a splat. 1.x built `vec3(s)` by appending the scalar to itself; 2.0 hands the scalar on and lets
            # the engine spread it, as the engine does for every scalar that meets a vector. An AppendVector of one wire
            # with itself -- or with a splat of that same wire -- is that wire.
            elif self.cls[nid] == "AppendVector" and len(inputs) == 2:
                first, second = inputs[0], inputs[1]
                if first[2:5] == second[2:5] and first[3] is None:
                    self.stats["B: splat folded"] += 1
                    sig = first[4]
        self.in_progress.discard(nid)
        self.sig_cache[nid] = sig
        return sig

    def find_attribute_edge(self, edge, name):
        """The edge that writes attribute `name` into the material `edge` carries, or None: the outermost Set (or the
        Make) of the chain that has it. None as soon as the chain leaves Set/Make nodes -- what is read then is whatever
        came in."""
        hops = 0
        while edge is not None and hops < 512:
            hops += 1
            nid = edge.get("from")
            if nid not in self.nodes or (edge.get("mask") or None) is not None:
                return None
            cls = self.cls[nid]
            if cls in ("NamedRerouteUsage", "NamedRerouteDeclaration", "Reroute"):
                if cls == "NamedRerouteUsage":
                    decl = self.declarations.get(str(self.nodes[nid].get("reroute", {}).get("declaration", "")))
                    inputs = self.nodes[decl]["inputs"] if decl else []
                else:
                    inputs = self.nodes[nid].get("inputs", [])
                edge = inputs[0] if inputs else None
                continue
            if cls not in ("SetMaterialAttributes", "MakeMaterialAttributes"):
                return None
            next_edge = None
            for candidate in self.nodes[nid].get("inputs", []):
                if cls == "SetMaterialAttributes" and int(candidate.get("index", 0)) == 0:
                    next_edge = candidate
                elif str(candidate.get("name", "")) == name:
                    return candidate
            edge = next_edge
        return None

    def material_attributes_sig(self, nid):
        """A (PD-4): the attributes written over a material are one set.

        1.x made one SetMaterialAttributes node per `m.X = v;` and chained them over the material they started from --
        an empty MakeMaterialAttributes for an output nothing was assigned to. 2.0 keeps the writes in a map and makes one
        node when the material has to become one. Both are the same material: the later write of an attribute wins, and
        attributes nobody wrote come from the base. The signature is the base (None for an empty Make) and the attributes
        by name.
        """
        attributes = {}
        base = None
        current = nid
        hops = 0
        while current is not None and hops < 512:
            hops += 1
            node = self.nodes[current]
            is_set = self.cls[current] == "SetMaterialAttributes"
            next_node = None
            for edge in sorted(node.get("inputs", []), key=lambda e: e.get("index", 0)):
                is_base = is_set and int(edge.get("index", 0)) == 0
                if is_base:
                    source, wire_base, channels = self.resolve(edge)
                    if source is not None and channels is None and wire_base == 0                             and self.cls[source] in ("SetMaterialAttributes", "MakeMaterialAttributes"):
                        next_node = source
                    else:
                        base = (wire_base, channels, self.node_sig(source) if source is not None else ("missing", wire_base))
                    continue
                name = str(edge.get("name", ""))
                if name not in attributes:
                    source, wire_base, channels = self.resolve(edge)
                    channels = drop_pin_prefix(channels, ATTRIBUTE_WIDTH.get(name))
                    child = self.node_sig(source) if source is not None else ("missing", wire_base)
                    attributes[name] = (name, name, wire_base, channels, child)
            if current != nid:
                self.stats["A: material attribute nodes merged"] += 1
            current = next_node
        inputs = []
        if base is not None:
            inputs.append(("", "MaterialAttributes", base[0], base[1], base[2]))
        inputs.extend(attributes[name] for name in sorted(attributes))
        return ("MaterialAttributes", (), tuple(inputs))

    def edge_sig(self, edge):
        source, base, channels = self.resolve(edge)
        return (base, channels, self.node_sig(source) if source is not None else ("missing", base))

    def roots(self):
        """{root key: signature}."""
        result = {}
        properties = self.data.get("properties")
        if isinstance(properties, dict):
            for key, edge in sorted(properties.items()):
                if isinstance(edge, dict) and "from" in edge:
                    result["property " + key] = self.edge_sig(edge)
        custom = defaultdict(list)
        for nid in self.nodes:
            cls = self.cls[nid]
            if cls == "FunctionOutput":
                name = str(self.nodes[nid].get("props", {}).get("OutputName", nid))
                result["output " + name] = self.node_sig(nid)
            elif cls in CUSTOM_OUTPUT_CLASSES:
                custom[cls].append(self.node_sig(nid))
        for cls, sigs in custom.items():
            for index, sig in enumerate(sorted(sigs, key=repr)):
                result["custom %s #%d" % (cls, index)] = sig
        return result

    def metadata(self):
        filtered = lambda value: canon_value(value)
        meta = {
            "kind": self.kind,
            "backend": self.data.get("backend"),
            "settings": filtered(self.data.get("settings", {})),
        }
        for key in ("inputs", "outputs", "instance"):
            if key in self.data:
                value = self.data[key]
                if key == "outputs" and isinstance(value, list):
                    # PD-2: an output's dumped `type` is inferred from what drives it, and that walk does not see an
                    # inline pin mask, so a 1.x dump says float4 where the function returns a float3. What the output
                    # carries is compared where it can be trusted: in the root signature of that output.
                    value = [{k: v for k, v in item.items() if k != "type"} if isinstance(item, dict) else item for item in value]
                meta[key] = filtered(value)
        return meta

    # ---------------------------------------------------------------------------------------------------------- stats
    def reachability_stats(self):
        reached = set()

        def visit_sig(sig):
            return sig  # signatures are values; reachability is computed on ids below

        stack = []
        properties = self.data.get("properties")
        if isinstance(properties, dict):
            for edge in properties.values():
                if isinstance(edge, dict) and "from" in edge:
                    stack.append(edge)
        for nid in self.nodes:
            if self.cls[nid] == "FunctionOutput" or self.cls[nid] in CUSTOM_OUTPUT_CLASSES:
                reached.add(nid)
                stack.extend(self.nodes[nid].get("inputs", []))
        seen_edges = 0
        while stack:
            edge = stack.pop()
            seen_edges += 1
            if seen_edges > 200000:
                break
            source, _, _ = self.resolve(edge)
            if source is None or source in reached:
                continue
            reached.add(source)
            stack.extend(self.nodes[source].get("inputs", []))
        dead = Counter()
        for nid in self.nodes:
            if nid not in reached and self.cls[nid] not in TRANSPARENT:
                dead[self.cls[nid]] += 1
        live_sigs = Counter(self.node_sig(nid) for nid in reached if self.cls[nid] not in TRANSPARENT)
        duplicates = Counter()
        for nid in reached:
            if self.cls[nid] in TRANSPARENT:
                continue
            count = live_sigs[self.node_sig(nid)]
            if count > 1:
                duplicates[self.cls[nid]] += 1
        # every duplicate group of size k counts k-1 removable nodes
        removable = Counter()
        groups = defaultdict(list)
        for nid in reached:
            if self.cls[nid] not in TRANSPARENT:
                groups[self.node_sig(nid)].append(nid)
        for sig, members in groups.items():
            if len(members) > 1:
                removable[self.cls[members[0]]] += len(members) - 1
        return dead, removable


# ---------------------------------------------------------------------------------------------------------- loading
def load_json(path):
    with io.open(path, encoding="utf-8") as handle:
        return json.load(handle)


def collect(root, excludes):
    files = {}
    for dirpath, _, names in os.walk(root):
        for name in names:
            if not name.endswith(".graph.json"):
                continue
            full = os.path.join(dirpath, name)
            rel = os.path.relpath(full, root).replace("\\", "/")
            if any(fnmatch.fnmatch(rel, pattern) or fnmatch.fnmatch(name, pattern) for pattern in excludes):
                continue
            files[rel] = full
    return files


def callee_width_table(files):
    table = {}
    for full in files.values():
        try:
            data = load_json(full)
        except (OSError, ValueError):
            continue
        if data.get("kind") in ("MaterialFunction", "MaterialLayer", "MaterialLayerBlend") and data.get("asset"):
            table[data["asset"]] = [TYPE_WIDTH.get(str(o.get("type", "")).lower()) for o in data.get("outputs", [])]
    return table


# ------------------------------------------------------------------------------------------------------- comparison
def first_divergence(a, b, trail, out, limit=12):
    if len(out) >= limit or a == b:
        return
    # A root is a wire, (output, mask, node): say what differs on the wire, then go on into the node.
    if isinstance(a, tuple) and isinstance(b, tuple) and len(a) == 3 and len(b) == 3             and not isinstance(a[0], str) and not isinstance(b[0], str)             and isinstance(a[2], tuple) and isinstance(b[2], tuple):
        if a[0:2] != b[0:2]:
            out.append("%s: wire %s/%s != %s/%s" % (" > ".join(trail) or "root", a[0], a[1], b[0], b[1]))
        first_divergence(a[2], b[2], trail, out, limit)
        return
    if not (isinstance(a, tuple) and isinstance(b, tuple)) or len(a) != 3 or len(b) != 3 or \
            not isinstance(a[0], str) or not isinstance(b[0], str) or a[0] in ("const", "missing", "cycle") or \
            b[0] in ("const", "missing", "cycle"):
        out.append("%s: %s  !=  %s" % (" > ".join(trail) or "root", short(a), short(b)))
        return
    if a[0] != b[0]:
        out.append("%s: class %s != %s" % (" > ".join(trail), a[0], b[0]))
        return
    pa, pb = dict(a[1]), dict(b[1])
    for key in sorted(set(pa) | set(pb)):
        if pa.get(key) != pb.get(key):
            out.append("%s [%s]: prop %s: %s != %s" % (" > ".join(trail), a[0], key, short(pa.get(key)), short(pb.get(key))))
    ia = {(i[0]): i for i in a[2]}
    ib = {(i[0]): i for i in b[2]}
    for index in sorted(set(ia) | set(ib)):
        ea, eb = ia.get(index), ib.get(index)
        if ea is None or eb is None:
            out.append("%s [%s]: input %s only on %s" % (" > ".join(trail), a[0], index, "baseline" if eb is None else "candidate"))
            continue
        if ea[2:4] != eb[2:4]:
            out.append("%s [%s]: input %s (%s) wire %s/%s != %s/%s" % (
                " > ".join(trail), a[0], index, ea[1], ea[2], ea[3], eb[2], eb[3]))
        if ea[4] != eb[4]:
            first_divergence(ea[4], eb[4], trail + ["%s.%s" % (a[0], ea[1] or index)], out, limit)


def short(value, width=140):
    text = repr(value)
    return text if len(text) <= width else text[:width] + "..."


def compare_pair(base_path, cand_path, filter_keys, base_widths, cand_widths):
    base = Graph(load_json(base_path), base_path, filter_keys, base_widths)
    cand = Graph(load_json(cand_path), cand_path, filter_keys, cand_widths)
    problems = []
    bm, cm = base.metadata(), cand.metadata()
    for key in sorted(set(bm) | set(cm)):
        if bm.get(key) != cm.get(key):
            problems.append("metadata %s: %s != %s" % (key, short(bm.get(key), 300), short(cm.get(key), 300)))
    br, cr = base.roots(), cand.roots()
    for key in sorted(set(br) | set(cr)):
        if key not in cr:
            problems.append("root %s: only in baseline" % key)
        elif key not in br:
            problems.append("root %s: only in candidate" % key)
        elif br[key] != cr[key]:
            details = []
            first_divergence(br[key], cr[key], [key], details)
            problems.append("root %s differs:\n      " % key + "\n      ".join(details or ["(structure differs)"]))
    return problems


def cmd_compare(args):
    base_files = collect(args.baseline, args.exclude)
    cand_files = collect(args.candidate, args.exclude)
    base_widths = callee_width_table(base_files)
    cand_widths = callee_width_table(cand_files)
    # PD-2: the width a 1.x dump gives a function output is inferred through inline masks it cannot see (float4 for a
    # function that returns a float3). Whether a mask on a wire from that output is the identity has to be answered
    # the same way on both sides, so both go by the candidate's word for an asset it has dumped too.
    base_widths = dict(base_widths, **cand_widths)
    filter_keys = parse_keys(args.filter_keys)
    results = {}
    for rel in sorted(set(base_files) | set(cand_files)):
        if rel not in cand_files:
            results[rel] = ["missing in candidate"]
        elif rel not in base_files:
            results[rel] = ["missing in baseline"]
        else:
            results[rel] = compare_pair(base_files[rel], cand_files[rel], filter_keys, base_widths, cand_widths)
    # Registered deltas the normalisations cannot absorb (--allow): every detail line of a file has to match one of the
    # patterns registered for it, or the file is different. What was let through is listed, never hidden.
    allowed = {}
    registered = load_json(args.allow) if args.allow else {}
    for rel, problems in list(results.items()):
        entry = next((value for key, value in registered.items() if fnmatch.fnmatch(rel.replace("\\", "/"), key)), None)
        if not problems or not entry:
            continue
        patterns = [re.compile(pattern) for pattern in entry.get("lines", [])]
        details = [line.strip() for problem in problems for line in problem.split("\n")[1:]] or list(problems)
        if details and all(any(pattern.search(line) for pattern in patterns) for line in details):
            allowed[rel] = (entry.get("delta", "?"), problems)
            results[rel] = []
    equal = [r for r, p in results.items() if not p]
    different = {r: p for r, p in results.items() if p}
    lines = ["# Graph parity", "",
             "- baseline: `%s`" % args.baseline, "- candidate: `%s`" % args.candidate,
             "- filter keys: %s" % (", ".join(sorted(filter_keys)) or "none"),
             "- excluded: %s" % (", ".join(args.exclude) or "none"), "",
             "| Result | Count |", "| :-- | --: |",
             "| equal | %d |" % len(equal), "| of those, equal under a registered delta | %d |" % len(allowed),
             "| different or missing | %d |" % len(different), ""]
    for rel, (delta, problems) in sorted(allowed.items()):
        lines.append("## %s  (registered delta %s)" % (rel, delta))
        lines.extend("- " + p for p in problems)
        lines.append("")
    for rel, problems in sorted(different.items())[: args.max_diffs]:
        lines.append("## %s" % rel)
        lines.extend("- " + p for p in problems)
        lines.append("")
    report = "\n".join(lines)
    print(report)
    if args.report:
        io.open(args.report, "w", encoding="utf-8", newline="\n").write(report)
    if args.json:
        json.dump({"equal": equal, "different": different}, io.open(args.json, "w", encoding="utf-8"), indent=1)
    return 0 if not different else 1


def cmd_pair(args):
    filter_keys = parse_keys(args.filter_keys)
    problems = compare_pair(args.baseline, args.candidate, filter_keys, {}, {})
    print("equal" if not problems else "\n".join(problems))
    return 0 if not problems else 1


def cmd_stats(args):
    files = collect(args.root, args.exclude)
    widths = callee_width_table(files)
    filter_keys = parse_keys(args.filter_keys)
    totals = Counter()
    dead_total, dup_total = Counter(), Counter()
    per_file_dead = Counter()
    for rel, full in sorted(files.items()):
        graph = Graph(load_json(full), full, filter_keys, widths)
        graph.roots()
        dead, removable = graph.reachability_stats()
        totals.update(graph.stats)
        dead_total.update(dead)
        dup_total.update(removable)
        if dead:
            per_file_dead[rel] = sum(dead.values())
    print("files:", len(files))
    for key in sorted(totals):
        print("  %-34s %d" % (key, totals[key]))
    print("  %-34s %d  %s" % ("D: unreachable nodes", sum(dead_total.values()), dict(dead_total.most_common())))
    print("  %-34s %d files" % ("   ... in", len(per_file_dead)))
    print("  %-34s %d  %s" % ("S: removable duplicates", sum(dup_total.values()), dict(dup_total.most_common())))
    return 0


def parse_keys(text):
    if text is None:
        return set(LEGACY_PARITY_FILTER_KEYS)
    return {k.strip() for k in text.split(",") if k.strip()}


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="command")
    p_stats = sub.add_parser("stats")
    p_stats.add_argument("root")
    p_stats.add_argument("--exclude", action="append", default=[])
    p_stats.add_argument("--filter-keys", default=None, help="comma list; default: %s; '' for none" % ",".join(LEGACY_PARITY_FILTER_KEYS))
    p_cmp = sub.add_parser("compare")
    p_cmp.add_argument("baseline")
    p_cmp.add_argument("candidate")
    p_cmp.add_argument("--exclude", action="append", default=[])
    p_cmp.add_argument("--filter-keys", default=None)
    p_cmp.add_argument("--report")
    p_cmp.add_argument("--json")
    p_cmp.add_argument("--max-diffs", type=int, default=200)
    p_cmp.add_argument("--allow", default=None, help="registered-deltas.json: {file glob: {delta, lines: [regex]}}")
    p_pair = sub.add_parser("pair")
    p_pair.add_argument("baseline")
    p_pair.add_argument("candidate")
    p_pair.add_argument("--filter-keys", default=None)
    args = parser.parse_args(argv)
    if args.command == "stats":
        return cmd_stats(args)
    if args.command == "compare":
        return cmd_compare(args)
    if args.command == "pair":
        return cmd_pair(args)
    parser.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
