#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;   // Also used as color
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragPosition;

// set = 0 is the default, so we don't have to write it
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 cameraPosition;
    vec4 lightPosition;
    vec4 lightColor;
    vec4 lightParams;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 objectColor;
    vec4 specularParams;      //strenght, exponent, 0, 0
} push;

void main()
{
    gl_Position = ubo.proj * ubo.view * push.model * vec4(inPosition, 1.0);

    //texture coordinate just sent directly through:
    fragTexCoord = inTexCoord;

    //fragment normal used for light reflections
    //look up the math for why this is so convoluted
    fragNormal = mat3(transpose(inverse(push.model))) * inNormal;

    //fragment position is used for light reflections
    fragPosition = vec3(push.model * vec4(inPosition, 1.0));
}
