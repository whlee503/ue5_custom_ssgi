# UE5 Custom SSGI with RDG Pipeline and Profiling-Based Optimization

A custom Screen Space Global Illumination (SSGI) prototype implemented as a **UE5 plugin inside a runnable host project** by directly extending the post-process rendering pipeline with **SceneViewExtension + RDG**.

This project focuses not only on implementing real-time GI, but also on **profiling bottlenecks and optimizing the pipeline for practical performance**.

> **Key result:** Implemented custom SSGI system on UE5. Reduced the total pipeline cost from **1.85 ms to 0.46 ms** (~75.13% optimization) while preserving visual quality.

---

## Verification Status

This repository is not just a code snapshot of the plugin source.

It includes the **host UE5 project (`Test4.uproject`)**, plugin source, content, and demo assets, and it was **successfully verified by cloning into a separate directory and rebuilding the project from the cloned repository**.

That means the repository can be reviewed in two ways:

- **Code review:** inspect the plugin source, shader code, and project structure
- **Executable review:** clone the repository, open the UE5 project, rebuild, and run the demo map

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
- preserving history across frames
- profiling each pass with **NVIDIA Nsight**
- redesigning the slowest parts of the pipeline

This repository is the technical implementation behind the portfolio project submitted for a graphics programming role.

<p align="center">
  <img src="Docs/final_result.png" width="80%" alt="Final result">
</p>

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
- Extended the prototype from an SSR-style starting point toward indirect diffuse/specular GI

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

This allowed me to build an engine-level rendering feature as an external plugin while keeping the implementation inside a normal UE5 project.

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

---

## Profiling and Optimization

The most important part of this project was not just making GI work, but making it **measurably faster**.

### Bottlenecks found
Using **NVIDIA Nsight**, I found that the main performance bottlenecks were:
- the ray marching pass
- the blur / denoising pass

<p align="center">
  <img src="Docs/perf_before.png" width="80%" alt="performance Before">
</p>

### Optimization work
- Converted brute-force **3D ray marching** into a more efficient **2.5D DDA-based traversal**
- Replaced expensive 2D blur with **separable Gaussian blur**
- Reduced unnecessary per-step computation inside the inner traversal loop
- Restructured the pipeline to better fit UE5's RDG workflow

### Result
- **Before optimization:** 1.85 ms
- **After optimization:** 0.46 ms
- **Improvement:** ~75.13%

<p align="center">
  <img src="Docs/nsight_profile.png" width="48%" alt="Nsight profiling">
  <img src="Docs/final_result.png" width="48%" alt="Optimization result">
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
Test4.uproject
Config/
Content/
  NewMap1.umap
  ...
Docs/
  final_result.png
  nsight_profile.png
Plugins/
  CustomSSGI/
    CustomSSGI.uplugin
    Shaders/
      CustomSSGI.usf
    Source/
      CustomSSGI/
        CustomSSGI.Build.cs
        Public/
          CustomSSGI.h
          CustomSSGIViewExtension.h
        Private/
          CustomSSGI.cpp
          CustomSSGIViewExtension.cpp
Source/
  Test4/
  Test4.Target.cs
  Test4Editor.Target.cs
README.md
```

---

## Key Files

### `Plugins/CustomSSGI/Source/CustomSSGI/CustomSSGI.Build.cs`
Configures module dependencies and renderer access.
- `Renderer`
- `RenderCore`
- `RHI`
- renderer private include path

### `Plugins/CustomSSGI/Source/CustomSSGI/Private/CustomSSGI.cpp`
Plugin startup/shutdown logic.
- shader directory mapping
- post-engine-init registration
- `FSceneViewExtension` creation

### `Plugins/CustomSSGI/Source/CustomSSGI/Public/CustomSSGIViewExtension.h`
Defines the custom view extension.
- lifecycle hooks
- post-process subscription
- diffuse/specular history buffers

### `Plugins/CustomSSGI/Source/CustomSSGI/Private/CustomSSGIViewExtension.cpp`
Core rendering implementation.
- custom post-process callback
- RDG pass creation
- render target allocation
- fullscreen pass execution
- history extraction

### `Plugins/CustomSSGI/Shaders/CustomSSGI.usf`
Shader implementation for:
- ray marching
- denoising
- temporal accumulation
- composition

---

## Build / Run

### Tested Environment
- **Engine Version:** UE 5.7.4
- **OS:** Windows
- **IDE:** Visual Studio 2022

### How to Run
1. Clone this repository
2. Make sure Git LFS is installed so `.uasset` and `.umap` files are pulled correctly
3. Open `Test4.uproject`
4. If prompted, rebuild the project modules
5. Launch the Unreal Editor
6. Open the demo map (`NewMap1`)
7. Run in PIE / standalone and inspect the custom SSGI result

### Clone / Rebuild Verification
This workflow was tested by:
- cloning the repository into a separate directory
- rebuilding the cloned UE5 project
- launching the project successfully
- confirming that the custom SSGI plugin works as intended in the cloned build

---

## Reproducibility Note

This repository is organized as an **executable UE5 demo project**, not just as a plugin-only code dump.

Because the repository includes the host project, plugin source, content, and demo map, reviewers can verify the work at multiple levels:
- source-level architecture and implementation
- shader integration and renderer access
- project buildability
- runtime behavior inside the demo scene

Minor setup adjustments may still be required if the reviewer uses a different UE5 minor version.

---

## Limitations

This is a **graphics programming prototype**, not a production replacement for UE5 Lumen.

Current limitations include:
- screen-space techniques cannot recover information outside the visible frame
- temporal methods depend on stable reprojection/history quality
- quality/performance trade-offs still depend on scene conditions
- production use would require more robustness, validation, scalability work, and artist-facing controls

---

## What I Learned

This project taught me that graphics programming is not just about producing an interesting image.

It is about:
- designing a rendering feature inside a complex engine architecture
- understanding how data moves across passes and frames
- using profiling tools to locate real bottlenecks
- controlling the trade-off between quality and performance with measurable data

The most valuable part of this work was moving from **“effect implementation”** to **“engine-level rendering architecture + profiling-driven optimization.”**

---

## Contact

- GitHub: https://github.com/whlee503
- Email: whlee503@ajou.ac.kr
