# HOMINEM ENGINE

This codebase began as an exploration into modern 2.5D rendering techniques, inspired by the incredible work done in stylized games that blend 3D technology with hand-drawn aesthetics. The goal is to create a game capable of producing high-quality 2.5D visuals. Big thanks to TheCherno for the foundational engine architecture that got this project off the ground.

![Image](https://github.com/user-attachments/assets/59842bbb-3a7f-4fe2-a575-902d9108f098)

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

#### August 9 2025 
 Got the basic window up and running with ImGui integration. GLFW is handling input nicely and we've got a solid foundation to build the 2.5D renderer on top of. Next up is getting basic mesh rendering working with some simple shaders.

![Image](https://github.com/user-attachments/assets/59842bbb-3a7f-4fe2-a575-902d9108f098)

#### Planned: September 2025
Going to focus on getting the core 3D rendering pipeline solid before diving into the stylized stuff. Need mesh loading, basic lighting, and a proper camera system. Once that's stable we can start experimenting with the toon shading and outline techniques.
#### Planned: October 2025
This is where it gets interesting - time to start implementing the 2.5D magic. Cel-shading, custom normal manipulation, and getting those clean outlines working. The goal is to make 3D models look like they were hand-drawn.
#### Planned: November 2025
Advanced 2.5D features and polish. Want to get the lighting looking really stylized and art-directable. Maybe some post-processing effects to really sell the hand-drawn aesthetic.