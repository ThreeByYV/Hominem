#type vertex

#version 460 core

layout(location = 0) in vec3 a_Pos;
layout(location = 1) in vec3 a_Normal;   // currently unused, but for lighting
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;	
uniform mat4 u_Model; // model transform 

out vec2 v_TexCoord;
out vec3 v_WorldPos;

void main()
{
    // compute world position
    vec4 worldPos = u_Model * vec4(a_Pos, 1.0); //moves object from model space -> world space
    gl_Position = u_ViewProjection * worldPos; // multiplies world-space by camera viewProjection -> clip space for OpenGL to rasterize

    v_TexCoord = a_TexCoord;
    v_WorldPos = worldPos.xyz;
}


#type fragment

#version 460 core

in vec2 v_TexCoord;
in vec3 v_WorldPos;

uniform vec3 gCameraWorldPos;
uniform sampler2D u_Texture;

//Fog 
uniform float gFogStart; //min distance for fog boundary
uniform float gFogEnd;   //max distance for fog boundary
uniform vec3 gFogColor;  //zero is when the fog is disabled and vice-versa

out vec4 FragColor;

float CalcLinearFogFactor() 
{
    // calcs the dist from the camera to the pixel in the world
    // this is length of the vector between the two,this is easiest way w/ world cam pos
    float cameraToPixelDist = length(v_WorldPos - gCameraWorldPos);

    float fogRange = gFogEnd - gFogStart;
    // guard against divide-by-zero
    if (fogRange <= 0.0001) {
        return 1.0; // treat as no fog falloff (full scene color)
    }

    float fogDist = gFogEnd - cameraToPixelDist;
    float fogFactor = fogDist / fogRange;

    return clamp(fogFactor, 0.0, 1.0);
}

float CalcFogFactor()
{
    float fogFactor = CalcLinearFogFactor();
    return fogFactor;
}

void main()
{
    // no lighting stuff for now
    vec4 texColor = texture(u_Texture, v_TexCoord); // modern glsl doesn't use texture2D

    // If gFogColor is zero-vector, treat as fog disabled
    bool fogEnabled = any(greaterThan(gFogColor, vec3(0.0)));

    if (fogEnabled)
    {
        float fogFactor = CalcFogFactor();

        // performs linear lerp between fog color and pixel color
        vec4 fogged = mix(vec4(gFogColor, 1.0), texColor, fogFactor);
        FragColor = fogged;
    }
    else
    {
        FragColor = texColor;
    }
}