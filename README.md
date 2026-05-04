# அணி நிரப்பி — Ani Nirappi

A real-time physically-based rendering engine built with C++20 and OpenGL 4.5. Features a deferred rendering pipeline with PBR materials, image-based lighting, screen-space effects, and moment shadow mapping.

## Features

- **Deferred Rendering** - G-Buffer with position, normal, albedo, metallic/roughness
- **PBR Materials** - Cook-Torrance BRDF with metallic-roughness workflow
- **Image-Based Lighting** - Spherical harmonics for diffuse, GGX importance sampling for specular
- **Screen-Space Ambient Occlusion (SSAO)** - With bilateral blur
- **Screen-Space Directional Occlusion (SSDO)** - Direct and indirect bounce
- **Moment Shadow Mapping** - Soft shadows with compute shader blur
- **Multi-Format Model Loading** - OBJ, glTF, FBX via Assimp with format-aware material extraction
- **HDR Environment Maps** - Equirectangular panoramas with SH irradiance coefficients
- **Live Parameter Tuning** - Full ImGui control panel for all rendering parameters

## Architecture

```
Main
 |
 +-- Input           Keyboard/mouse handling, ImGui-aware
 +-- Camera          Fly/Orbit/Frozen modes
 |
 +-- Scene           Rendering pipeline orchestrator
      |
      +-- Geometry Pass      Render to G-Buffer (4 MRT)
      +-- Shadow Pass        Moment shadow map + compute blur
      +-- SSAO Pass          Raw AO + bilateral blur
      +-- SSDO Pass          Direct visibility/radiance + indirect bounce + blur
      +-- Lighting Pass      Deferred PBR with IBL + shadows + SSAO/SSDO
      +-- Sky Dome           Equirectangular environment rendering
      +-- ImGui              Debug visualization + parameter controls
```

### Model Loading

The model loading system uses a **strategy pattern** for format-specific material extraction, keeping the loader itself format-agnostic.

```
ModelLoader::load("model.gltf")
  |-- detectFormat()           -> GLTF
  |-- createMaterialStrategy() -> PbrMaterialStrategy
  |-- processNode()            Recursive traversal with accumulated transforms
  |     |-- transform * vertex.position    (node transforms applied)
  |     |-- normalMatrix * vertex.normal   (inverse-transpose for normals)
  |     +-- strategy->extractMaterial()    (format-specific)
  |          strategy->getTextureMappings() (priority-based type lookup)
  +-- returns LoadedModel { meshes, bounding box }
```

| Format | Strategy | Material Model |
|--------|----------|----------------|
| glTF/GLB | `PbrMaterialStrategy` | baseColor, metallic, roughness, IOR |
| FBX | `PbrMaterialStrategy` | Same PBR path |
| OBJ | `PhongMaterialStrategy` | Kd/Ks/Ns converted to metallic-roughness |

Adding a new format requires only a new `MaterialStrategy` subclass and one line in the factory.

### IBL Pipeline

1. **SH Irradiance** - 9 spherical harmonic coefficients computed from HDR panorama for diffuse lighting
2. **Hammersley Sequence** - Low-discrepancy sampling points uploaded via UBO
3. **GGX Importance Sampling** - Half-vector sampling around surface normal with correct energy conservation
4. **Mip-mapped Environment** - LOD selection based on solid angle for specular

### Resource Management

All OpenGL resources follow RAII patterns:
- `Shader` - Program ID with uniform location caching
- `Texture` - Texture ID with move-only ownership, shared via `shared_ptr` for texture caching
- `Mesh` - VAO/VBO/EBO with move semantics
- `FrameBuffer` - FBO/RBO with color texture attachments
- `SkyDome` - VAO/VBO/EBO + sampler

## Source Layout

```
include/
  core/         RenderConfig.h, Logger.h
  renderer/     Scene.h, Shader.h, BlurShader.h, FrameBuffer.h, ShadowMapBlur.h
  geometry/     Mesh.h, Model.h, ModelLoader.h, MaterialStrategy.h
  lighting/     Light.h, DirectionalLight.h, SkyDome.h, SHIrradiance.h, HammersleyRandomGenerator.h
  camera/       Camera.h, Input.h
  material/     Texture.h

src/
  core/         Main.cpp
  renderer/     Scene.cpp, Shader.cpp, BlurShader.cpp, FrameBuffer.cpp, ShadowMapBlur.cpp
  geometry/     Mesh.cpp, Model.cpp, ModelLoader.cpp, MaterialStrategy.cpp
  lighting/     Light.cpp, DirectionalLight.cpp, SkyDome.cpp, SHIrradiance.cpp, HammersleyRandomGenerator.cpp
  camera/       Camera.cpp, Input.cpp
  material/     Texture.cpp
  external/     glad.c, stb_image.cpp

assets/
  shaders/      19 GLSL files (vertex, fragment, compute)
  model/        3D scenes (San Miguel, Sponza)
  textures/     HDR environment maps, floor materials
```

## Shaders

| Pass | Vertex | Fragment/Compute |
|------|--------|-----------------|
| Geometry | `gbuffer.vert` | `gbuffer.frag` |
| Lighting | `postprocess.vert` | `deferred_lighting.frag` |
| Debug | `postprocess.vert` | `deferred_lighting_debug.frag` |
| Shadow | `shadow.vert` | `shadow.frag` |
| Shadow Blur | | `shadowblur.comp` |
| SSAO | `postprocess.vert` | `rawssao.frag` |
| SSAO Blur | `postprocess.vert` | `blurssao.frag` |
| SSDO Direct | `postprocess.vert` | `rawdirectssdo.frag` |
| SSDO Indirect | `postprocess.vert` | `rawindirectssdo.frag` |
| SSDO Blur | `postprocess.vert` | `blurssdo.frag` |
| Sky | `skydome.vert` | `skydome.frag` |

## Dependencies

Managed via [vcpkg](https://vcpkg.io):

| Library | Purpose |
|---------|---------|
| assimp | 3D model loading (OBJ, glTF, FBX) |
| glfw3 | Window management and input |
| glm | Mathematics (vectors, matrices, transforms) |
| openimageio | HDR/EXR texture loading |
| imgui | Immediate-mode UI for parameter tuning |
| plog | Logging |
| fmt | String formatting |

External (vendored in `external/`):
- **GLAD** - OpenGL 4.5 function loader
- **stb_image** - Image loading fallback


## Controls

| Input | Action |
|-------|--------|
| WASD | Move camera (Fly mode) |
| QE | Move up/down |
| Mouse | Look around |
| Scroll | Adjust movement speed (Fly) / zoom (Orbit) |
| Shift | Sprint |
| Tab | Cycle camera mode: Fly -> Orbit -> Frozen |
| P | Log camera position |
| Esc | Exit |

Switch to **Frozen** mode (Tab) to interact with the ImGui panel.
