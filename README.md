# ECSGameEngine
Custom C++20 ECS game engine with SDL3 + OpenGL, ImGui editor/diagnostics, Assimp/stb asset pipeline, and an experimental GPU path tracer (OpenGL compute + A-Trous denoising).
This repository is primarily for code review and technical discussion. It is not maintained as a turnkey “clone-and-build” distribution.

## What’s here
- ECS runtime with system-driven update loop.
- SDL3 window/input + OpenGL context management.
- ImGui editor dockspace (scene view, console, profiling/diagnostics).
- Diagnostics overlay and timing instrumentation (CPU scopes + GPU timers).
- Asset pipeline (model + material import, texture loading/caching).
- Raster renderer with material/texture binding.
- Experimental path tracer (OpenGL 4.3+ compute):
  - Progressive accumulation.
  - Optional A-Trous denoising.
  - Tonemapping and debug views.

## Notes on dependencies
Third-party libraries and binaries are not the focus of this repo. Expect SDL3 / ImGui / GLAD / stb / Assimp (and any other externals) to be provided separately in your environment or solution setup.

## Project state
- Working real-time path-tracing. (1 to 8 spp/frame, with ~200fps to ~40fps on staging system).
- Actively evolving. Some code is intentionally “engine-in-progress” rather than polished library code.
- File organization is currently flat; planned refactor will separate core/platform/graphics/editor/diagnostics more cleanly.

## Roadmap (near-term)
- Repository cleanup: consistent folder/module boundaries, naming, and build configuration.
- Path tracer feature parity with the raster material pipeline where applicable (map-driven materials, tangent-space normals, better BSDF parameterization).
- Full scene graph transforms + instancing support across both raster and path tracing paths.
- Lighting improvements (emissive meshes, analytic lights, environment maps).
- Optional OptiX backend behind a build flag; long-term: neural denoising/ML hooks (TensorRT/ONNX) where it fits.

## License
No license is provided.
