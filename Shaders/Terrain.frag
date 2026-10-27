#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosition;
layout(location = 0) out vec4 outColor;

void main()
{
    // position derivatives give a geometric normal for each filled triangle
    vec3 n = cross(dFdx(fragPosition), dFdy(fragPosition));
    float lengthSquared = dot(n, n);
    vec3 normal = lengthSquared > 0.0
        ? n * inversesqrt(lengthSquared) : vec3(0.0, 1.0, 0.0);
    if (normal.y < 0.0)
        normal = -normal;

    vec3 lightDirection = normalize(vec3(-1.0, 1.0, 0.5));
    float diffuse = max(dot(normal, lightDirection), 0.0);
    float brightness = 0.25 + 0.75 * diffuse;
    outColor = vec4(fragColor * brightness, 1.0);
}
