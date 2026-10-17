# Refit tools

`build_strapon.cpp` refits OStim's Calyps strap-on (Skyrim SE, 3BA BodySlide
project) to Fallout 4's True Wasteland Body as a BodySlide project
(`Optional/4Stim Equip Objects/Tools/BodySlide/`). Built against
[nifly](https://github.com/ousnius/nifly), Outfit Studio's NIF library:

```
g++ -std=c++20 -O2 -I nifly/include -I nifly/external build_strapon.cpp nifly/build/src/libnifly.a -o build_strapon
build_strapon <out folder> "<OStim>/CalienteTools/BodySlide/ShapeData/OStim 3BA/OStim Calyps Strap-On.nif" \
  "<TWB>/Tools/BodySlide/ShapeData/True Wasteland Body/True Wasteland Body.nif" "<ZeX>/meshes/actors/character/characterassets/skeleton.nif" \
  "<CBBE>/Tools/BodySlide/ShapeData/CBBE/CBBEBody.osd" "<TWB>/Tools/BodySlide/ShapeData/True Wasteland Body/The Wasteland Body.osd" \
  sliders.tsv "4Stim OStim Strap-On (TWB)"
```

What it does: moves the strap-on from Skyrim's to Fallout 4's skin space
(fitted to TWB's hips: scale 0.98, 2 forward, 119 down; 0.44 units mean
distance), gives the harness the weights of the nearest TWB vertex, weights
the dildo along ZeX's Penis_00-05 chain, writes every TWB slider
(`sliders.tsv`, from TWB's slider set) conformed from the nearest TWB vertex,
and the `.osp` slider set (output `meshes\4Stim\EquipObjects\OStimStrapOn`).
`tools/make-equip-plugin.py` writes the plugin with its armor.
