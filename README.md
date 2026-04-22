# UE5 Custom SSGI with RDG Pipeline and Profiling-Based Optimization

A custom Screen Space Global Illumination (SSGI) prototype implemented as a UE5 plugin by directly extending the post-process rendering pipeline with **SceneViewExtension + RDG**.  
This project focuses not only on implementing GI, but also on **profiling bottlenecks and optimizing the pipeline for real-time performance**.

> **Key result:** Reduced the total pipeline cost from **1.85 ms to 0.46 ms** (~75% optimization) while preserving visual quality.

---

## Demo

- Portfolio: [PDF link]
- Demo Video: [YouTube link]

<p align="center">
  <img src="assets/overview.png" width="80%" alt="Overview image">
</p>

---

## Overview

This project started from a simple question:

**Can I inject a custom real-time GI system into UE5's post-process pipeline, and then optimize it enough to become a practical high-performance alternative for selected use cases?**

To answer that, I built a custom SSGI pipeline from scratch inside UE5 by:

- registering a custom `FSceneViewExtension`
- hooking into the post-process stage with **RDG**
- reconstructing scene information from **G-Buffer**
- implementing screen-space ray marching
- adding denoising and temporal accumulation
- profiling each pass with **NVIDIA Nsight**
- redesigning the slowest parts of the pipeline

This repository is the technical implementation behind the portfolio project submitted for a graphics programming role.

---

## What I Implemented

### 1. UE5 rendering pipeline bridge
- Built a custom plugin module that accesses UE5 renderer internals
- Added shader source directory mapping for plugin shaders
- Registered a custom `FSceneViewExtension` after engine initialization
- Subscribed to the post-process pipeline before DOF

### 2. Multi-pass RDG architecture
- Implemented an RDG-based multi-pass pipeline instead of a single fullscreen shader
- Structured the pipeline as:
  1. **Ray Marching Pass**
  2. **Denoise X Pass**
  3. **Denoise Y + Temporal Accumulation Pass**
- Managed intermediate textures and final outputs through RDG
- Split **diffuse** and **specular** signals into separate render targets for better temporal behavior

### 3. G-Buffer based screen-space GI
- Read scene textures through UE5 scene texture parameters
- Reconstructed depth / normal / scene color information
- Implemented screen-space ray marching and hit testing
- Extended the prototype from simple SSR-style behavior toward indirect diffuse/specular GI

### 4. Stateful temporal pipeline
- Preserved previous-frame results using `IPooledRenderTarget`
- Built ping-pong style history handling for diffuse and specular buffers
- Combined current-frame and history data to stabilize 1-SPP rendering

### 5. Profiling-based optimization
- Measured GPU cost with **NVIDIA Nsight**
- Identified major bottlenecks in:
  - ray marching pass
  - geometry-aware blur pass
- Replaced expensive parts of the pipeline with more efficient designs:
  - **3D ray marching → 2.5D DDA**
  - **2D blur → separable Gaussian blur**

---

## Technical Highlights

### SceneViewExtension + RDG integration
Instead of modifying UE5 engine source directly, I used `FSceneViewExtensionBase` as a safe integration point and injected custom rendering passes into the engine's post-process flow.  
This allowed me to build an engine-level rendering feature as an external plugin.

### History buffer management
A temporary RDG texture is reset every frame, so temporal accumulation requires persistent external resources.  
To solve that, I used `IPooledRenderTarget` to preserve diffuse/specular history buffers across frames and extracted the current outputs back into history at the end of the pass chain.

### Diffuse / specular separation
Diffuse and specular components have very different temporal and spatial characteristics.  
I separated them into different render targets and histories so that each signal could be filtered and accumulated more appropriately.

### Low-discrepancy temporal sampling
To improve convergence and temporal stability, I experimented with low-discrepancy sampling ideas such as **Halton sequence** instead of relying only on naïve per-frame random noise.

---

## Pipeline Structure

```text
SceneViewExtension registration
    ↓
SubscribeToPostProcessingPass(BeforeDOF)
    ↓
Pass 1: CustomSSGI_Pass1_RayMarch
    - read scene textures
    - compute noisy diffuse/specular GI
    ↓
Pass 2: CustomSSGI_Pass2_Denoise_X
    - horizontal denoising
    ↓
Pass 3: CustomSSGI_Pass2_Denoise_Y & Temporal
    - vertical denoising
    - temporal accumulation with history
    - final composition
    ↓
QueueTextureExtraction
    - save diffuse/specular history for next frame
```

<p align="center">
  <img src="assets/final_result.png" width="80%" alt="Final result">
</p>

---

## Profiling and Optimization

The most important part of this project was not just making GI work, but making it **measurably faster**.

### Bottlenecks found
Using **NVIDIA Nsight**, I found that the main performance bottlenecks were:
- the ray marching pass
- the blur / denoising pass

### Optimization work
- Converted brute-force **3D ray marching** into a more efficient **2.5D DDA-based traversal**
- Replaced expensive 2D blur with **separable Gaussian blur**
- Reduced unnecessary per-step computation inside the inner ray traversal loop
- Restructured the pipeline to better fit UE5's RDG workflow

### Result
- **Before optimization:** 1.85 ms
- **After optimization:** 0.46 ms
- **Improvement:** ~75.13%

<p align="center">
  <img src="assets/nsight_profile.png" width="48%" alt="Nsight profiling">
  <img src="assets/optimization.png" width="48%" alt="Optimization result">
</p>

---

## Tech Stack

- **Language:** C++
- **Engine:** Unreal Engine 5
- **Graphics Framework:** RDG (Render Dependency Graph)
- **Engine Hook:** `FSceneViewExtension`
- **Shader Language:** HLSL / `.usf`
- **Profiling / Debugging:** NVIDIA Nsight
- **Concepts:** G-Buffer reconstruction, screen-space ray marching, temporal accumulation, denoising, low-discrepancy sampling

---

## Repository Structure

```text
CustomSSGI.Build.cs
CustomSSGI.h
CustomSSGI.cpp
CustomSSGIViewExtension.h
CustomSSGIViewExtension.cpp
Shaders/
  CustomSSGI.usf
assets/
  overview.png
  final_result.png
  nsight_profile.png
  optimization.png
README.md
```

---

## Key Files

### `CustomSSGI.Build.cs`
Configures module dependencies and renderer access.
- `Renderer`
- `RenderCore`
- `RHI`
- renderer private include path

### `CustomSSGI.cpp`
Plugin startup/shutdown logic.
- shader directory mapping
- post-engine-init registration
- `FSceneViewExtension` creation

### `CustomSSGIViewExtension.h`
Defines the custom view extension.
- lifecycle hooks
- post-process subscription
- diffuse/specular history buffers

### `CustomSSGIViewExtension.cpp`
Core rendering implementation.
- custom post-process callback
- RDG pass creation
- render target allocation
- fullscreen pass execution
- history extraction

### `Shaders/CustomSSGI.usf`
Shader implementation for:
- ray marching
- denoising
- temporal accumulation
- composition

---

## Build / Run Notes

- Tested as a UE5 plugin project
- Requires proper plugin shader directory mapping
- Requires renderer module access in `.Build.cs`
- The implementation depends on UE5 rendering internals, so minor engine-version differences may require small adjustments

> Fill this section with your exact tested UE version and project setup steps.

Example:
1. Copy the plugin into the project's `Plugins/` directory
2. Regenerate project files
3. Build the project in Visual Studio
4. Launch the project and enable the plugin
5. Run in PIE / standalone and inspect the post-process result

---

## Limitations

This is a **graphics programming prototype**, not a full production replacement for UE5 Lumen.

Current limitations include:
- screen-space techniques cannot recover information outside the visible frame
- temporal methods depend on stable reprojection/history quality
- quality/performance trade-offs still depend on scene conditions
- production use would need more robustness, validation, and artist-facing controls

---

## What I Learned

This project taught me that graphics programming is not just about producing a visually interesting image.

It is about:
- designing a rendering feature inside a complex engine architecture
- understanding how data moves across passes and frames
- using profiling tools to locate real bottlenecks
- controlling the trade-off between quality and performance with measurable data

The most valuable part of this work was moving from **“effect implementation”** to **“engine-level rendering architecture + profiling-driven optimization.”**

---

## Related Materials

- Portfolio PDF: [link]
- Demo video: [link]
- Other rendering projects: [link to path tracer or ISM repo]

---

## Contact

- GitHub: [your profile link]
- Email: [your email]
