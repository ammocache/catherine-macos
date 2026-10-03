# Performance notes (2x resolution, Metal/MoltenVK, M4)

Benchmark: bench.sh, main-menu 3D scene, avg guest FPS 55-80 s after launch.

| Setting (all at draw_resolution_scale 2x) | Avg FPS |
|---|---|
| 1x (game's own cap) | 30.0 |
| 2x default | 20.2 |
| anisotropic_override=0 | 20.2 |
| native_2x_msaa=false | 13.3 (worse) |
| gamma_render_target_as_unorm16=false | 20.2 |
| vulkan_dynamic_rendering=false | 19.9 |
| direct_host_resolve=false | 20.0 |
| vulkan_sparse_shared_memory=false | 19.9 |

Conclusions
- No config knob moves 2x. FPS is quantized by display refresh (20 = 3 vsyncs); reaching 30 needs ~40% less GPU time per frame.
- Per-frame GPU time ~50-55 ms: render passes ~30 ms, compute (EDRAM resolves / scaled texture loads) ~15 ms.
- CPU threads are mostly idle-waiting; the Vulkan backend already pools command buffers/fences and never calls queue/device wait-idle.
- Real fix = fewer/cheaper passes (native renderer without EDRAM emulation, like Skate 3 / Unleashed recomps) - large effort.

## GPU investigation, 2x resolution (Oct 2026, session 2)

Measured with vsync off (the game itself is not capped at 30; 1x runs ~44 fps, GPU-bound). 2x = ~20 fps.

Where the extra ~28 ms/frame at 2x goes (found by experiment, SDK instrumentation kept in
SAVEPOINTS/gpu-experiments-2026-10-03.patch, not in the release):
- ~48-200 render passes per frame; about half are Xenos "ownership transfers" that copy small EDRAM ranges
  between host render-target images, and the game flips a small target between 1x and 2x MSAA many times per frame.
- Skipping the *store* of depth in transfer passes for multisampled destinations: 20 -> 30 fps (upper bound,
  output is wrong). Skipping loads, color transfers, stencil, or the draws themselves: no gain.
  So multisampled depth stores on Metal are the dominant cost, and it is not proportional to image area.
- Safe-ish skips tried: dropping depth transfers that only change the MSAA mode (+1 fps), capping render-target image
  height at 1024 px (no gain). Remaining cost is transfers between MSAA depth images with different layouts, which are
  real data moves and cannot be dropped.
- Config knobs (anisotropic, gamma format, dynamic rendering, direct resolve, sparse memory) change nothing;
  native_2x_msaa=false is worse.

Next real options: (1) reduce multisampled depth transfer cost (batching into the guest pass, compute-based copy,
or avoiding MSAA host images), (2) native renderer without EDRAM emulation (Skate 3 / Unleashed approach).

### Follow-up (idle machine, 3 runs each, 2x, vsync off)
- baseline 20.0 / 19.9 / 19.9; skip all ownership transfers 20.1 / 20.1 / 23.4;
  skip the depth *store* in transfer+clear passes together 26.6 / 30.6 / 30.8;
  skip it in only the transfer passes 20.3 / 20.2; only the clear passes 20.6 / 20.3.
- Neither removing the transfers nor removing the stores in one kind of pass helps; only removing the stores in
  both the transfer and the resolve-clear passes does. That points at the chain of dependent passes on the same
  multisampled depth image (store -> load -> store) rather than bandwidth or draw cost. The structural fix is to
  merge transfers/clears into fewer passes (the upstream TODO at render_target_cache.cpp:~4804), not to drop work.
