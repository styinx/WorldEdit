
#include "material_normal.hlsli"
#include "samplers.hlsli"

struct input_vertex {
   float2 texcoords : TEXCOORD;
};

void main(input_vertex input_vertex)
{
   Texture2D<float4> diffuse_map = ResourceDescriptorHeap[material.diffuse_map_index];

   float4 diffuse_color = diffuse_map.Sample(sampler_trilinear_wrap, input_vertex.texcoords);

   if (diffuse_color.a < 0.5) discard;
}