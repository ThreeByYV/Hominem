# HOMINEM ENGINE

This codebase began as an exploration into modern 2.5D rendering techniques, inspired by the incredible work done in stylized games that blend 3D technology with hand-drawn aesthetics. The goal is to create a game capable of producing high-quality 2.5D visuals. Big thanks to [The Cherno](https://www.youtube.com/@TheCherno) and [OGLDEV](https://www.youtube.com/@OGLDEV) for the foundational engine architecture that got this project off the ground.

![Image](https://github.com/user-attachments/assets/874258f1-304c-4821-afdf-13155816bf12)

### Controls
A/D: move left/right<br>
ESC: back to menu<br>
F: toggle fullscreen<br>
TAB: toggle debug UI<br>
R: hot-reload config (debug)<br>


### Download via terminal
This repo uses [Git LFS](https://git-lfs.com) for large assets (models, textures, audio). Make sure it is installed before cloning or assets will appear as empty pointer files.
```
git lfs install
git clone https://github.com/ThreeByYV/Hominem.git
cd Hominem
```
### Build
Two build systems are supported:

**Premake** (Windows, quickest)
```
Double-click scripts/Win-GenProjects.bat to generate Visual Studio solution files
```

**CMake**
```
cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug
```

~~August 9 2025~~ <br>
Got the basic window up and running with ImGui integration. GLFW is handling input nicely and we've got a solid foundation to build the 2.5D renderer on top of. Next up is getting basic mesh rendering working with some simple shaders.
<br>

![Image](https://github.com/user-attachments/assets/59842bbb-3a7f-4fe2-a575-902d9108f098)

~~September 2025~~  
Went a little off script, but that led to getting 3D models into the engine, now thinking of pivoting to make a more 3D styled game with a strong camera system, but characters are mainly moving left to right, rather than all directions. Also started ECS, not sure if that'll continue, rather make a game than a game engine I've realized. Still need dable with shader stuff and experimenting with the toon shading and outline techniques.

<br>

![Image](https://github.com/user-attachments/assets/6a34239f-37dd-42f1-9689-df5dc641d97c)


~~Planned: October 2025~~
This is where it gets interesting - time to start implementing the 2.5D magic. Cel-shading, custom normal manipulation, and getting those clean outlines working. The goal is to make 3D models look like they were hand-drawn.
~~Planned: November 2025~~

Advanced 2.5D features and polish. Want to get the lighting looking really stylized and art-directable. Maybe some post-processing effects to really sell the hand-drawn aesthetic. Even possibly using AMD FSR SDK for AI image upscaling

~~May 2026~~  
A lot has come together on the `feature/create-core-systems` branch. The renderer moved to a dedicated render thread with a double-buffered RenderFrame, HDR framebuffers, a multi-pass render graph, PBR lighting with Cook-Torrance BRDF, bloom, auto-exposure tone mapping, and a point light pipeline with ImGui editing and JSON persistence. The physics integration (Box2D) stabilised with a proper side-scroller camera system. Bloom and HDR still need polish but the foundation is solid and progress has been made!

Moving forward the goal is a fixed-camera 3D game using the engine's rendering capabilities to their fullest.

![Image](https://github.com/user-attachments/assets/1158a6c1-e56c-4ce1-9adc-8a48825e49e7)

~~July 2026~~  
This is definitely the most difficult part of the rendering engine to date. Working from real ray tracing (via `VK_KHR_ray_query`) and a few papers and talks, we now have Dynamic Diffuse Global Illumination (DDGI). Next, of course, we need to integrate it with the rest of the scene, but these images serve as a good reminder of why GI is so important.

<table>
<tr>
<th width="50%">Before (direct lighting only, flat ambient fill)</th>
<th width="50%">After (DDGI)</th>
</tr>
<tr>
<td width="50%"><img width="100%" src="https://github.com/user-attachments/assets/56ff4537-2ff7-4cb7-ac95-bcf7a7f04f8c"></td>
<td width="50%"><img width="100%" src="https://github.com/user-attachments/assets/06d75858-256a-43a6-aea2-fc8c1635dcef"></td>
</tr>
</table>

~~September 2026~~  
Temporal antialiasing. Toon shading makes aliasing unusually obvious: the hard bands between light steps drop a high-contrast edge right across surfaces that would otherwise be smooth, and every one of those edges crawls when the camera moves. So TAA jitters the projection each frame and accumulates the result, reprojecting the history through a velocity buffer so moving objects resolve too, not just the camera. A neighbourhood clamp in YCoCg keeps the ghosting down, and the history gets dropped on camera cuts so a hard cut does not smear into the next shot.

The resolve is heavily inspired by Playdead's TAA from INSIDE, which fits a fixed-camera side-on game like this one almost exactly. The YCoCg clipping, the velocity dilation from the closest depth, and the luminance-based history feedback all follow Lasse Jon Fuglsang Pedersen's talk [Temporal Reprojection Anti-Aliasing in INSIDE](https://www.gdcvault.com/play/1022970/Temporal-Reprojection-Anti-Aliasing-in) (GDC 2016) and Playdead's open-source [reference implementation](https://github.com/playdeadgames/temporal) (MIT).

<table>
<tr>
<th width="50%">Before (no AA, aliasing along the toon bands)</th>
<th width="50%">After (TAA)</th>
</tr>
<tr>
<td width="50%"><img width="100%" src="https://github.com/user-attachments/assets/79562633-d4fe-4483-83cd-fbec434beb40"></td>
<td width="50%"><img width="100%" src="https://github.com/user-attachments/assets/74f64ded-828e-4382-8b5e-bb2d70c02e36"></td>
</tr>
</table>
