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
