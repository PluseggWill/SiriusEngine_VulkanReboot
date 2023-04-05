echo "Compiling Shader..."

cd %~dp0

..\..\lib\VulkanSDK\1.2.182.0\Bin32\glslc.exe TriangleVertex.vert -o TriangleVertex.spv
..\..\lib\VulkanSDK\1.2.182.0\Bin32\glslc.exe TriangleFrag.frag -o TriangleFrag.spv
..\..\lib\VulkanSDK\1.2.182.0\Bin32\glslc.exe TriangleFrag_Lit.frag -o TriangleFrag_Lit.spv

echo "Compile Finished!"