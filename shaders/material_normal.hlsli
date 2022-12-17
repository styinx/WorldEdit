#pragma once

#include "bindings.hlsli"

struct material_input {
   uint flags;
   uint diffuse_map_index;
   uint normal_map_index;
   float3 specular_color;
};

ConstantBuffer<material_input> material : register(MATERIAL_CB_REGISTER);