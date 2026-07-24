# Texture Credits

Both textures are from [ambientCG](https://ambientcg.com), released under the **CC0 1.0 Universal
(Public Domain)** license -- free for any use, including commercial, no attribution required.
Credited here anyway as good practice.

- `knob_metal.jpg` -- derived from [Metal009](https://ambientcg.com/view?id=Metal009) ("Brushed
  Bumpy Metal Scratches Silver Steel"). Since a PBR colour/albedo map is deliberately flat on its
  own (real shine normally comes from roughness/normal maps under live lighting, which a static 2D
  UI texture doesn't have), the shine is instead baked directly into the pixels offline: a soft
  radial highlight + sharper glint hotspot + opposite-corner vignette, composited on top of the
  brushed-steel photo via Pillow (see the generating script, kept alongside this project's scratch
  files). Baking it into the texture -- rather than drawing it as a separate gradient/shape at
  render time -- is what makes it read as a genuine photographed highlight instead of a flat UI
  overlay. The renderer applies one consistent orientation to every knob (no per-knob rotation),
  matching how a real panel has a single physical light source.
- `chassis_metal.jpg` -- derived from [Metal038](https://ambientcg.com/view?id=Metal038) ("Scratched
  Steel"), downscaled with a contrast boost so the fine scratch detail stays visible once darkened
  further at render time.
