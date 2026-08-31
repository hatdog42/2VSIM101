#!/bin/bash

# Compile shaders glslangValidator->glslang
# Seems Mac does not like the extra stuff from glslangValidator
/Users/ole/VulkanSDK/1.4.341.1/macOS/bin/glslc ./Shader.vert -o ./vert.spv
/Users/ole/VulkanSDK/1.4.341.1/macOS/bin/glslc ./Shader.frag -o ./frag.spv
/Users/ole/VulkanSDK/1.4.341.1/macOS/bin/glslc ./PhongShader.vert -o ./PhongVert.spv
/Users/ole/VulkanSDK/1.4.341.1/macOS/bin/glslc ./PhongShader.frag -o ./PhongFrag.spv

# Pause equivalent (wait for user input)
# echo "Press any key to continue..."
# read -n 1 -s
