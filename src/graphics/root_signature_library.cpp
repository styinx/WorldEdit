
#include "root_signature_library.hpp"
#include "hresult_error.hpp"

#include <algorithm>

#include <boost/container/static_vector.hpp>

namespace we::graphics {

namespace {
constexpr uint32 mesh_register_space = 0;
constexpr uint32 material_register_space = 1;
constexpr uint32 terrain_register_space = 2;
constexpr uint32 lights_register_space = 3;
constexpr uint32 lights_tile_register_space = 4;
constexpr uint32 bindless_srv_space = 1000;

const gpu::static_sampler_desc
   trilinear_sampler{.filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
                     .address_u = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                     .address_v = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                     .address_w = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                     .mip_lod_bias = 0.0f,
                     .max_anisotropy = 0,
                     .comparison_func = D3D12_COMPARISON_FUNC_ALWAYS,
                     .border_color = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
                     .min_lod = 0.0f,
                     .max_lod = D3D12_FLOAT32_MAX};

const gpu::static_sampler_desc
   bilinear_sampler{.filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
                    .address_u = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                    .address_v = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                    .address_w = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                    .mip_lod_bias = 0.0f,
                    .max_anisotropy = 0,
                    .comparison_func = D3D12_COMPARISON_FUNC_ALWAYS,
                    .border_color = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
                    .min_lod = 0.0f,
                    .max_lod = D3D12_FLOAT32_MAX};

const gpu::static_sampler_desc
   shadow_sampler{.filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
                  .address_u = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                  .address_v = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                  .address_w = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                  .mip_lod_bias = 0.0f,
                  .max_anisotropy = 0,
                  .comparison_func = D3D12_COMPARISON_FUNC_LESS_EQUAL,
                  .border_color = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
                  .min_lod = 0.0f,
                  .max_lod = D3D12_FLOAT32_MAX};

const gpu::root_parameter_descriptor_table bindless_srv_table{
   .ranges =
      {
         {.type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
          .count = UINT_MAX,
          .base_shader_register = 0,
          .register_space = bindless_srv_space + 0,
          .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
          .offset_in_descriptors_from_table_start = 0},

         {.type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
          .count = UINT_MAX,
          .base_shader_register = 0,
          .register_space = bindless_srv_space + 1,
          .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
          .offset_in_descriptors_from_table_start = 0},

         {.type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
          .count = UINT_MAX,
          .base_shader_register = 0,
          .register_space = bindless_srv_space + 2,
          .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
          .offset_in_descriptors_from_table_start = 0},

         {.type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
          .count = UINT_MAX,
          .base_shader_register = 0,
          .register_space = bindless_srv_space + 3,
          .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
          .offset_in_descriptors_from_table_start = 0},

         {.type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
          .count = UINT_MAX,
          .base_shader_register = 0,
          .register_space = bindless_srv_space + 4,
          .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
          .offset_in_descriptors_from_table_start = 0},

         {.type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
          .count = UINT_MAX,
          .base_shader_register = 0,
          .register_space = bindless_srv_space + 5,
          .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
          .offset_in_descriptors_from_table_start = 0},
      },
   .visibility = D3D12_SHADER_VISIBILITY_PIXEL,
};

const gpu::root_parameter_descriptor_table lights_input_descriptor_table{
   .ranges =
      {
         {.type = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
          .count = 1,
          .base_shader_register = 0,
          .register_space = lights_register_space,
          .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
          .offset_in_descriptors_from_table_start = 0},

         {.type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
          .count = 1,
          .base_shader_register = 1,
          .register_space = lights_register_space,
          .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
          .offset_in_descriptors_from_table_start = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND},

         {.type = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
          .count = 1,
          .base_shader_register = 2,
          .register_space = lights_register_space,
          .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
          .offset_in_descriptors_from_table_start = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND},

         {.type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
          .count = 1,
          .base_shader_register = 3,
          .register_space = lights_register_space,
          .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
          .offset_in_descriptors_from_table_start = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND},

         {.type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
          .count = 1,
          .base_shader_register = 4,
          .register_space = lights_register_space,
          .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
          .offset_in_descriptors_from_table_start = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND},
      },
   .visibility = D3D12_SHADER_VISIBILITY_PIXEL,
};

const gpu::root_signature_desc mesh_desc{
   .name = "mesh_root_signature",

   .parameters =
      {// per-object constants
       gpu::root_parameter_cbv{
          .shader_register = 1,
          .register_space = mesh_register_space,
          .flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
          .visibility = D3D12_SHADER_VISIBILITY_VERTEX,
       },

       // material constants
       gpu::root_parameter_descriptor_table{
          .ranges =
             {
                {.type = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                 .count = 1,
                 .base_shader_register = 0,
                 .register_space = material_register_space,
                 .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
                 .offset_in_descriptors_from_table_start = 0},
             },
          .visibility = D3D12_SHADER_VISIBILITY_PIXEL,
       },

       // camera descriptors
       gpu::root_parameter_descriptor_table{
          .ranges =
             {
                {.type = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                 .count = 1,
                 .base_shader_register = 0,
                 .register_space = mesh_register_space,
                 .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
                 .offset_in_descriptors_from_table_start = 0},
             },
          .visibility = D3D12_SHADER_VISIBILITY_VERTEX,
       },

       // lights descriptors
       lights_input_descriptor_table,

       // bindless descriptors
       bindless_srv_table

      },

   .samplers =
      {
         {.sampler = trilinear_sampler,
          .shader_register = 0,
          .register_space = 0,
          .visibility = D3D12_SHADER_VISIBILITY_PIXEL},

         {.sampler = shadow_sampler,
          .shader_register = 2,
          .register_space = lights_register_space,
          .visibility = D3D12_SHADER_VISIBILITY_PIXEL},
      },

   .flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};

const gpu::root_signature_desc terrain_desc{
   .name = "terrain_root_signature",

   .parameters =
      {
         // camera descriptor
         gpu::root_parameter_descriptor_table{
            .ranges =
               {
                  {.type = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                   .count = 1,
                   .base_shader_register = 0,
                   .register_space = mesh_register_space,
                   .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
                   .offset_in_descriptors_from_table_start = 0},
               },
            .visibility = D3D12_SHADER_VISIBILITY_VERTEX,
         },

         // lights descriptors
         lights_input_descriptor_table,

         // terrain descriptors
         gpu::root_parameter_descriptor_table{
            .ranges =
               {
                  {.type = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                   .count = 1,
                   .base_shader_register = 0,
                   .register_space = terrain_register_space,
                   .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
                   .offset_in_descriptors_from_table_start = 0},

                  {.type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                   .count = 2,
                   .base_shader_register = 0,
                   .register_space = terrain_register_space,
                   .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
                   .offset_in_descriptors_from_table_start = 1},
               },
            .visibility = D3D12_SHADER_VISIBILITY_ALL,
         },

         // terrain patch data
         gpu::root_parameter_srv{
            .shader_register = 2,
            .register_space = terrain_register_space,
            .flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
            .visibility = D3D12_SHADER_VISIBILITY_VERTEX,
         },

         // material descriptors
         gpu::root_parameter_descriptor_table{
            .ranges =
               {
                  {.type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                   .count = 16,
                   .base_shader_register = 0,
                   .register_space = material_register_space,
                   .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
                   .offset_in_descriptors_from_table_start = 0},
               },
            .visibility = D3D12_SHADER_VISIBILITY_PIXEL,
         },
      },

   .samplers =
      {
         {.sampler = bilinear_sampler,
          .shader_register = 0,
          .register_space = 0,
          .visibility = D3D12_SHADER_VISIBILITY_PIXEL},

         {.sampler = trilinear_sampler,
          .shader_register = 1,
          .register_space = 0,
          .visibility = D3D12_SHADER_VISIBILITY_PIXEL},

         {.sampler = shadow_sampler,
          .shader_register = 2,
          .register_space = lights_register_space,
          .visibility = D3D12_SHADER_VISIBILITY_PIXEL},
      },

   .flags = D3D12_ROOT_SIGNATURE_FLAG_NONE};

const gpu::root_signature_desc meta_mesh_desc{
   .name = "meta_mesh_root_signature",

   .parameters =
      {
         // per-object constants
         gpu::root_parameter_cbv{
            .shader_register = 1,
            .register_space = mesh_register_space,
            .flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
            .visibility = D3D12_SHADER_VISIBILITY_VERTEX,
         },

         // color constant (should this be a root constant?)
         gpu::root_parameter_cbv{
            .shader_register = 0,
            .register_space = mesh_register_space,
            .flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
            .visibility = D3D12_SHADER_VISIBILITY_PIXEL,
         },

         // camera descriptors
         gpu::root_parameter_descriptor_table{
            .ranges =
               {
                  {.type = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                   .count = 1,
                   .base_shader_register = 0,
                   .register_space = mesh_register_space,
                   .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
                   .offset_in_descriptors_from_table_start = 0},
               },
            .visibility = D3D12_SHADER_VISIBILITY_VERTEX,
         },
      },

   .flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};

const gpu::root_signature_desc meta_mesh_wireframe_desc{
   .name = "meta_mesh_wireframe_root_signature",

   .parameters =
      {
         // per-object constants
         gpu::root_parameter_cbv{
            .shader_register = 1,
            .register_space = mesh_register_space,
            .flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
            .visibility = D3D12_SHADER_VISIBILITY_VERTEX,
         },

         // wireframe constants
         gpu::root_parameter_cbv{
            .shader_register = 0,
            .register_space = 0,
            .flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
            .visibility = D3D12_SHADER_VISIBILITY_PIXEL,
         },

         // camera descriptors
         gpu::root_parameter_descriptor_table{
            .ranges =
               {
                  {.type = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                   .count = 1,
                   .base_shader_register = 0,
                   .register_space = mesh_register_space,
                   .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
                   .offset_in_descriptors_from_table_start = 0},
               },
            .visibility = D3D12_SHADER_VISIBILITY_VERTEX,
         },
      },

   .flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};

const gpu::root_signature_desc meta_line_desc{
   .name = "meta_line_root_signature",

   .parameters =
      {
         // color constant (should this be a root constant?)
         gpu::root_parameter_cbv{
            .shader_register = 0,
            .register_space = mesh_register_space,
            .flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
            .visibility = D3D12_SHADER_VISIBILITY_PIXEL,
         },

         // camera descriptors
         gpu::root_parameter_descriptor_table{
            .ranges =
               {
                  {.type = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                   .count = 1,
                   .base_shader_register = 0,
                   .register_space = mesh_register_space,
                   .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
                   .offset_in_descriptors_from_table_start = 0},
               },
            .visibility = D3D12_SHADER_VISIBILITY_VERTEX,
         },
      },

   .flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};

const gpu::root_signature_desc mesh_shadow_desc{
   .name = "mesh_shadow_root_signature",

   .parameters =
      {
         // transform cbv
         gpu::root_parameter_cbv{
            .shader_register = 0,
            .register_space = mesh_register_space,
            .flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
            .visibility = D3D12_SHADER_VISIBILITY_VERTEX,
         },

         // camera cbv
         gpu::root_parameter_cbv{
            .shader_register = 1,
            .register_space = mesh_register_space,
            .flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
            .visibility = D3D12_SHADER_VISIBILITY_VERTEX,
         },
      },

   .flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};

const gpu::root_signature_desc mesh_depth_prepass_desc{
   .name = "mesh_depth_prepass_root_signature",

   .parameters =
      {// per-object constants
       gpu::root_parameter_cbv{
          .shader_register = 1,
          .register_space = mesh_register_space,
          .flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
          .visibility = D3D12_SHADER_VISIBILITY_VERTEX,
       },

       // material constants
       gpu::root_parameter_descriptor_table{
          .ranges =
             {
                {.type = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                 .count = 1,
                 .base_shader_register = 0,
                 .register_space = material_register_space,
                 .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
                 .offset_in_descriptors_from_table_start = 0},
             },
          .visibility = D3D12_SHADER_VISIBILITY_PIXEL,
       },

       // camera descriptors
       gpu::root_parameter_descriptor_table{
          .ranges =
             {
                {.type = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                 .count = 1,
                 .base_shader_register = 0,
                 .register_space = mesh_register_space,
                 .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
                 .offset_in_descriptors_from_table_start = 0},
             },
          .visibility = D3D12_SHADER_VISIBILITY_VERTEX,
       },

       // bindless descriptors
       bindless_srv_table},

   .samplers =
      {
         {.sampler = trilinear_sampler,
          .shader_register = 0,
          .register_space = 0,
          .visibility = D3D12_SHADER_VISIBILITY_PIXEL},
      },

   .flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};

const gpu::root_signature_desc mesh_wireframe_desc{
   .name = "mesh_wireframe_root_signature",

   .parameters =
      {
         // per-object constants
         gpu::root_parameter_cbv{
            .shader_register = 1,
            .register_space = mesh_register_space,
            .flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
            .visibility = D3D12_SHADER_VISIBILITY_VERTEX,
         },

         // wireframe constants
         gpu::root_parameter_cbv{
            .shader_register = 0,
            .register_space = 0,
            .flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
            .visibility = D3D12_SHADER_VISIBILITY_PIXEL,
         },

         // camera descriptors
         gpu::root_parameter_descriptor_table{
            .ranges =
               {
                  {.type = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                   .count = 1,
                   .base_shader_register = 0,
                   .register_space = mesh_register_space,
                   .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
                   .offset_in_descriptors_from_table_start = 0},
               },
            .visibility = D3D12_SHADER_VISIBILITY_VERTEX,
         },
      },

   .flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};

const gpu::root_signature_desc tile_lights_clear_desc{
   .name = "tile_lights_clear_root_signature",

   .parameters = {
      // input cbv
      gpu::root_parameter_cbv{
         .shader_register = 0,
         .flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
      },

      // tiles uav
      gpu::root_parameter_uav{
         .shader_register = 0,
         .flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE,
      },
   }};

const gpu::root_signature_desc tile_lights_desc{
   .name = "tile_lights_root_signature",

   .parameters =
      {
         // instance data srv
         gpu::root_parameter_srv{
            .shader_register = 0,
            .flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
         },

         // descriptors
         gpu::root_parameter_descriptor_table{
            .ranges = {{.type = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                        .count = 1,
                        .base_shader_register = 0,
                        .flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
                        .offset_in_descriptors_from_table_start = 0},

                       {.type = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
                        .count = 1,
                        .base_shader_register = 0,
                        .flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE,
                        .offset_in_descriptors_from_table_start =
                           D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}},
         },
      },

   .flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};

}

root_signature_library::root_signature_library(gpu::device& device)
{
   mesh = device.create_root_signature(mesh_desc);
   terrain = device.create_root_signature(terrain_desc);
   meta_mesh = device.create_root_signature(meta_mesh_desc);
   meta_mesh_wireframe = device.create_root_signature(meta_mesh_wireframe_desc);
   meta_line = device.create_root_signature(meta_line_desc);
   mesh_shadow = device.create_root_signature(mesh_shadow_desc);
   mesh_depth_prepass = device.create_root_signature(mesh_depth_prepass_desc);
   mesh_wireframe = device.create_root_signature(mesh_wireframe_desc);

   tile_lights_clear = device.create_root_signature(tile_lights_clear_desc);
   tile_lights = device.create_root_signature(tile_lights_desc);
}

}
