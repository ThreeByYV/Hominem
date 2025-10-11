# HOMINEM ENGINE

This codebase began as an exploration into modern 2.5D rendering techniques, inspired by the incredible work done in stylized games that blend 3D technology with hand-drawn aesthetics. The goal is to create a game capable of producing high-quality 2.5D visuals. Big thanks to [The Cherno](https://www.youtube.com/@TheCherno) and [OGLDEV](https://www.youtube.com/@OGLDEV) for the foundational engine architecture that got this project off the ground.

![Image](https://github.com/user-attachments/assets/90d1a63e-7975-4c3c-8df2-7d791c421538)

### Controls
WSAD: movement<br>
Mouse: look around<br>
Space: jump/ascend<br>


### Download via terminal
```
git clone https://github.com/ThreeByYV/Hominem.git
cd Hominem
```
### Build
```
 Double click on GenerateProjects.bat to run the bundled premake build script
```

~~August 9 2025~~ <br>
Got the basic window up and running with ImGui integration. GLFW is handling input nicely and we've got a solid foundation to build the 2.5D renderer on top of. Next up is getting basic mesh rendering working with some simple shaders.
<br>

![Image](https://github.com/user-attachments/assets/59842bbb-3a7f-4fe2-a575-902d9108f098)

~~September 2025~~  
Went a little off script, but that led to getting 3D models into the engine, now thinking of pivoting to make a more 3D styled game with a strong camera system, but characters are mainly moving left to right, rather than all directions. Also started ECS, not sure if that'll continue, rather make a game than a game engine I've realized. Still need dable with shader stuff and experimenting with the toon shading and outline techniques.

<br>

![Image](https://github.com/user-attachments/assets/6a34239f-37dd-42f1-9689-df5dc641d97c)


#### Planned: October 2025
This is where it gets interesting - time to start implementing the 2.5D magic. Cel-shading, custom normal manipulation, and getting those clean outlines working. The goal is to make 3D models look like they were hand-drawn.
#### Planned: November 2025

Advanced 2.5D features and polish. Want to get the lighting looking really stylized and art-directable. Maybe some post-processing effects to really sell the hand-drawn aesthetic. Even possibly using AMD FSR SDK for AI image upscaling



