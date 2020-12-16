#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler;

void main() {
    //outColor = vec4(fragUV, 0.0, 1.0);

    vec4 textureColour = texture(texSampler, fragUV);
    if(textureColour.a != 0.0)
    {
        outColor = textureColour;
    }
    else
    {
        // Discard fully transparent fragments
        discard;
    }
}