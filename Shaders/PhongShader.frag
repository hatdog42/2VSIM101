#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragPosition;

layout(location = 0) out vec4 outColor;

// set = 0 is the default, so we don't have to write it
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 cameraPosition;
    vec4 lightPosition;
    vec4 lightColor;
    vec4 lightParams;
} ubo;

// texture sampler is in set = 1
layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 objectColor;
    vec4 specularParams;      //strenght, exponent, 0, 0
} push;

// Since the Positions and Color come in as vec4 because of the alignment
// we use swizzling to get only what we need (.xyz -components)
void main()
{
    vec3 ambient = ubo.lightParams.x * ubo.lightColor.xyz;

    // Diffuse is comming from the light position
    vec3 normal = normalize(fragNormal); //just to make sure it is normalized
    vec3 lightDirecton = normalize(ubo.lightPosition.xyz - fragPosition);
    float diff = max(dot(normal, lightDirecton), 0.0);
    vec3 diffuse = ubo.lightParams.y * diff * ubo.lightColor.xyz;

    // Specular is how much of the light is reflected into the camera
    vec3 viewDirection = normalize(ubo.cameraPosition.xyz - fragPosition);  //direction from fragment to camera
    vec3 reflectDirection = reflect(-lightDirecton, normal);    //
    float specularShine = pow(max(dot(viewDirection, reflectDirection), 0.0), push.specularParams.y);
    vec3 specular = push.specularParams.x * specularShine * ubo.lightColor.xyz;

    // Sample the color from the texture
    vec4 textureColor = texture(texSampler, fragTexCoord);

    // Combine all light contributions with texture color
    vec3 result = (ambient + diffuse + specular) * textureColor.rgb;

    // Output the final color (convert to vec4)
    outColor = vec4(result, textureColor.a);
}
