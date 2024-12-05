echo "Compiling Shader..."

cd %~dp0

:: ..\..\lib\VulkanSDK\1.2.182.0\Bin32\glslc.exe TriangleVertex.vert -o TriangleVertex.spv
:: ..\..\lib\VulkanSDK\1.2.182.0\Bin32\glslc.exe TriangleFrag.frag -o TriangleFrag.spv
:: ..\..\lib\VulkanSDK\1.2.182.0\Bin32\glslc.exe TriangleFrag_Lit.frag -o TriangleFrag_Lit.spv
dxc -T vs_6_0 -spirv -E VSMain -Fo TriangleVert.spv TriangleShader.hlsl
dxc -T ps_6_0 -spirv -E PSMain -Fo TrianglePix.spv TriangleShader.hlsl

echo "Compile Finished!"

:: pause