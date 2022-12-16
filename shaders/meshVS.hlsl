#include "bindings.hlsli"
#include "frame_constants.hlsli"

struct object_constants {
   float4x4 world_matrix;
};

ConstantBuffer<object_constants> cb_object_constants : register(OBJECT_CB_REGISTER);

struct input_vertex {
   float3 positionOS : POSITION;
   float3 normalOS : NORMAL;
   float3 tangentOS : TANGENT;
   float3 bitangentOS : BITANGENT;
   float2 texcoords : TEXCOORD;
};

struct output_vertex {
   float3 positionWS : POSITIONWS;
   float3 normalWS : NORMAL;
   float3 tangentWS : TANGENT;
   float3 bitangentWS : BITANGENT;
   float2 texcoords : TEXCOORD;

   float4 positionPS : SV_Position;
};

output_vertex main(input_vertex input)
{
   output_vertex output;

   const float3 positionWS = mul(cb_object_constants.world_matrix, float4(input.positionOS, 1.0)).xyz;

   output.positionWS = positionWS;
   output.normalWS = mul((float3x3)cb_object_constants.world_matrix, input.normalOS);
   output.tangentWS = mul((float3x3)cb_object_constants.world_matrix, input.tangentOS);
   output.bitangentWS = mul((float3x3)cb_object_constants.world_matrix, input.bitangentOS);
   output.texcoords = input.texcoords;
   output.positionPS = mul(cb_frame.view_projection_matrix, float4(positionWS, 1.0));

   return output;
}