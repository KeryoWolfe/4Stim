"""Adds hub scenes to a scene pack (docs/SCENES.md, "Hubs").

OStim lists every navigation of a scene in one list. Packs keep that list
short themselves: each one adds a hub scene, a copy of the base scene it
hooks into (same animation, same actors), which puts one row into the base
scene's list ("origin") and links to the pack's own scenes from there. This is
how OStim's packs do it (FlufyFox's FlufyFoxCOREFmHub.json: "origin":
"OStim2PStandingApartMF", "description": "FlufyFox MF Hub", a "Return" at
priority -1000, "noRandomSelection": true).

A converted pack (an AnimBridge pack, Atomic Lust...) often hooks every one of
its scenes into the base scene instead, so the base scene's list grows by a
row per scene. This tool gives such a pack one hub per base scene it hooks
into:

- the hub: the base scene's actors and animation (copied from the base
  scene's file, its plugin written in), "noRandomSelection", an "origin"
  row into the base scene labelled with the pack's name, and "Return" to it;
- the pack's scenes: their "origin" rows and their links back to the base
  scene point at the hub instead.

    python make-hubs.py PACK.json BASE.json [BASE.json ...] [--name "Atomic Lust"] [--icon ICON] [-o OUT.json]

PACK.json is 4Stim's many-scenes-per-file layout ({"plugin", "scenes":
[...]}). A BASE is a file in that layout or a folder in OStim's (one scene per
file, the file name its ID; "speeds" name their Idles by editor ID). Without -o the pack file is rewritten (a .bak
copy is kept). Running it again on a pack that has hubs changes nothing.
"""

import argparse
import copy
import json
import os
import re
import shutil
import sys

HUB_SUFFIX = "_Hub_"


def load(path):
    with open(path, encoding="utf-8-sig") as f:
        return json.load(f)


def nav_target(nav):
    return nav.get("to", nav.get("destination"))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("pack")
    ap.add_argument("base", nargs="+", help="the files with the scenes the pack hooks into")
    ap.add_argument("--name", help="the hub row's label (default: the pack's modpack)")
    ap.add_argument("--icon", default="", help="the hub row's icon (under Interface\\4Stim\\Icons)")
    ap.add_argument("-o", "--output")
    args = ap.parse_args()

    pack = load(args.pack)
    scenes = pack.get("scenes", [])
    if not scenes:
        sys.exit(f"{args.pack}: no \"scenes\"")

    base = {}  # lowercase id -> (scene, plugin)
    for path in args.base:
        if os.path.isdir(path):
            # OStim's layout: one scene per file, its ID the file name.
            for root, _, files in os.walk(path):
                for name in files:
                    if name.lower().endswith(".json"):
                        scene = load(os.path.join(root, name))
                        if isinstance(scene, dict) and "scenes" not in scene:
                            scene_id = os.path.splitext(name)[0]
                            base[scene_id.lower()] = ({"id": scene_id, **scene}, "")
            continue
        data = load(path)
        for scene in data.get("scenes", []):
            base[scene["id"].lower()] = (scene, data.get("plugin", ""))

    own = {s["id"].lower() for s in scenes}
    modpack = next((s["modpack"] for s in scenes if s.get("modpack")), "")
    label = args.name or modpack
    if not label:
        sys.exit("the pack's scenes have no \"modpack\": name the hub with --name")
    prefix = re.sub(r"[^A-Za-z0-9]+", "", modpack or label)

    # The base scenes the pack's scenes hook into.
    hooked = []
    for scene in scenes:
        if HUB_SUFFIX in scene["id"]:
            continue
        for nav in scene.get("navigations", []):
            origin = nav.get("origin")
            if origin and origin.lower() not in own and origin.lower() not in [h.lower() for h in hooked]:
                hooked.append(origin)

    hubs = {}
    for origin in hooked:
        hub_id = prefix + HUB_SUFFIX + origin
        hubs[origin.lower()] = hub_id
        if hub_id.lower() in own:
            continue  # made by an earlier run
        found = base.get(origin.lower())
        if not found:
            print(f"warning: {origin} isn't in the base files, its scenes keep their own rows", file=sys.stderr)
            del hubs[origin.lower()]
            continue
        source, plugin = found
        hub = copy.deepcopy(source)
        for key in ("navigations", "autoTransitions", "destination", "origin"):
            hub.pop(key, None)
        hub["id"] = hub_id
        hub["name"] = f"{label} Hub"
        hub["modpack"] = modpack
        hub["noRandomSelection"] = True
        # Its idles are the base file's: name that plugin wherever they're given.
        if plugin:
            for actor in hub.get("actors", []):
                actor.setdefault("plugin", plugin)
            for speed in hub.get("speeds", []):
                speed.setdefault("plugin", plugin)
        row = {"origin": origin, "label": label}
        if args.icon:
            row["icon"] = args.icon
        hub["navigations"] = [row, {"to": origin, "label": "Return", "icon": "4Stim/symbols/return", "priority": -1000}]
        scenes.insert(0, hub)
        print(f"hub {hub_id} for {origin}")

    # The pack's scenes go through the hubs.
    moved = 0
    for scene in scenes:
        if HUB_SUFFIX in scene["id"]:
            continue
        for nav in scene.get("navigations", []):
            origin = nav.get("origin")
            if origin and origin.lower() in hubs:
                nav["origin"] = hubs[origin.lower()]
                moved += 1
                # Inside the hub, a leading "(Pack) " says nothing.
                for key in ("label", "description"):
                    text = nav.get(key, "")
                    if text.lower().startswith(f"({label.lower()}) "):
                        nav[key] = text[len(label) + 3:]
            target = nav_target(nav)
            if target and not origin and target.lower() in hubs:
                nav["to" if "to" in nav else "destination"] = hubs[target.lower()]

    out = args.output or args.pack
    if not args.output:
        shutil.copyfile(args.pack, args.pack + ".bak")
    with open(out, "w", encoding="utf-8") as f:
        json.dump(pack, f, indent="\t", ensure_ascii=False)
        f.write("\n")
    print(f"{moved} row(s) moved into {len(hubs)} hub(s), written to {out}")


if __name__ == "__main__":
    main()
