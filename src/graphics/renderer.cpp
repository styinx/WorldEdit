
#include "renderer.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx12.h"

#include <d3dx12.h>

namespace sk::graphics {

namespace {

constexpr float temp_barrier_height = 64.0f;

}

renderer::renderer(const HWND window) : _window{window}, _device{window}
{
   auto imgui_font_descriptor = _device.descriptor_heap.allocate_static(1);

   ImGui_ImplDX12_Init(_device.device_d3d.get(), gpu::render_latency,
                       gpu::swap_chain::format_rtv, &_device.descriptor_heap.get(),
                       imgui_font_descriptor.start().cpu,
                       imgui_font_descriptor.start().gpu);
}

void renderer::draw_frame(const camera& camera, const world::world& world,
                          const std::unordered_map<std::string, world::object_class>& world_classes)
{
   build_object_render_list(world, world_classes);

   auto& swap_chain = _device.swap_chain;
   swap_chain.wait_for_ready();

   _device.copy_manager.enqueue_fence_wait_if_needed(*_device.command_queue);

   auto& command_allocator = *_world_command_allocators[_device.frame_index];
   auto& command_list = *_world_command_list;
   auto [back_buffer, back_buffer_rtv] = swap_chain.current_back_buffer();
   ID3D12DescriptorHeap* const descriptor_heap = &_device.descriptor_heap.get();

   throw_if_failed(command_allocator.Reset());
   throw_if_failed(command_list.Reset(&command_allocator, nullptr));
   _dynamic_buffer_allocator.reset(_device.frame_index);

   command_list.SetDescriptorHeaps(1, &descriptor_heap);

   const auto rt_barrier =
      CD3DX12_RESOURCE_BARRIER::Transition(&back_buffer, D3D12_RESOURCE_STATE_PRESENT,
                                           D3D12_RESOURCE_STATE_RENDER_TARGET);

   command_list.ResourceBarrier(1, &rt_barrier);

   command_list.ClearRenderTargetView(back_buffer_rtv,
                                      std::array{0.0f, 0.0f, 0.0f, 1.0f}.data(),
                                      0, nullptr);
   command_list.ClearDepthStencilView(_depth_stencil_texture.depth_stencil_view,
                                      D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0x0, 0, nullptr);

   const D3D12_VIEWPORT viewport{.Width =
                                    static_cast<float>(_device.swap_chain.width()),
                                 .Height =
                                    static_cast<float>(_device.swap_chain.height()),
                                 .MaxDepth = 1.0f};
   const D3D12_RECT sissor_rect{.right = static_cast<LONG>(_device.swap_chain.width()),
                                .bottom =
                                   static_cast<LONG>(_device.swap_chain.height())};
   command_list.RSSetViewports(1, &viewport);
   command_list.RSSetScissorRects(1, &sissor_rect);
   command_list.OMSetRenderTargets(1, &back_buffer_rtv, true,
                                   &_depth_stencil_texture.depth_stencil_view);

   command_list.SetGraphicsRootSignature(
      _device.root_signatures.basic_object_mesh.get());
   command_list.SetPipelineState(_device.pipelines.basic_object_mesh.get());

   // TEMP Camera Setup
   {
      auto allocation = _dynamic_buffer_allocator.allocate(sizeof(float4x4));

      std::memcpy(allocation.cpu_address, &camera.view_projection_matrix(),
                  sizeof(float4x4));

      command_list.SetGraphicsRootConstantBufferView(0, allocation.gpu_address);
   }

   command_list.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

   // TEMP object placeholder rendering
   for (auto& object : _object_render_list) {
      // TEMP object constants setup
      {
         auto allocation = _dynamic_buffer_allocator.allocate(sizeof(float4x4));

         std::memcpy(allocation.cpu_address, &object.transform, sizeof(float4x4));

         command_list.SetGraphicsRootConstantBufferView(1, allocation.gpu_address);
      }

      command_list.IASetVertexBuffers(0,
                                      static_cast<UINT>(
                                         object.vertex_buffer_views.size()),
                                      object.vertex_buffer_views.data());
      command_list.IASetIndexBuffer(&object.index_buffer_view);
      command_list.DrawIndexedInstanced(object.index_count, 1, object.start_index,
                                        object.start_vertex, 0);
   }

   // Render World Meta Objects
   draw_world_meta_objects(camera, world, command_list);

   // Render ImGui
   ImGui::Render();
   ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), &command_list);

   const auto present_barrier =
      CD3DX12_RESOURCE_BARRIER::Transition(&back_buffer,
                                           D3D12_RESOURCE_STATE_RENDER_TARGET,
                                           D3D12_RESOURCE_STATE_PRESENT);

   command_list.ResourceBarrier(1, &present_barrier);

   throw_if_failed(command_list.Close());

   ID3D12CommandList* exec_command_list = &command_list;

   _device.command_queue->ExecuteCommandLists(1, &exec_command_list);

   swap_chain.present();

   _device.end_frame();
}

void renderer::window_resized(uint16 width, uint16 height)
{
   if (width == _device.swap_chain.width() and height == _device.swap_chain.height()) {
      return;
   }

   _device.wait_for_idle();
   _device.swap_chain.resize(width, height);
   _depth_stencil_texture =
      {_device,
       {.format = DXGI_FORMAT_D24_UNORM_S8_UINT,
        .width = _device.swap_chain.width(),
        .height = _device.swap_chain.height(),
        .optimized_clear_value = {.Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
                                  .DepthStencil = {.Depth = 0.0f, .Stencil = 0x0}}},
       D3D12_HEAP_TYPE_DEFAULT,
       D3D12_RESOURCE_STATE_DEPTH_WRITE};
}

void renderer::draw_world_meta_objects(const camera& camera, const world::world& world,
                                       ID3D12GraphicsCommandList5& command_list)
{
   command_list.SetGraphicsRootSignature(
      _device.root_signatures.meta_object_mesh.get());
   command_list.SetPipelineState(_device.pipelines.meta_object_mesh.get());

   // TEMP Camera Setup
   {
      auto allocation = _dynamic_buffer_allocator.allocate(sizeof(float4x4));

      std::memcpy(allocation.cpu_address, &camera.view_projection_matrix(),
                  sizeof(float4x4));

      command_list.SetGraphicsRootConstantBufferView(0, allocation.gpu_address);
   }

   // Set Regions Color
   {
      const float4 color{0.25f, 0.4f, 1.0f, 0.3f};

      auto allocation = _dynamic_buffer_allocator.allocate(sizeof(float4));

      std::memcpy(allocation.cpu_address, &color, sizeof(float4));

      command_list.SetGraphicsRootConstantBufferView(2, allocation.gpu_address);
   }

   for (auto& region : world.regions) {
      // TEMP constants setup
      {
         const float3 scale = [&] {
            switch (region.shape) {
            default:
            case world::region_shape::box: {
               return region.size;
            }
            case world::region_shape::sphere: {
               return float3{glm::length(region.size)};
            }
            case world::region_shape::cylinder: {
               const float cylinder_length =
                  glm::length(float2{region.size.x, region.size.z});
               return float3{cylinder_length, region.size.y, cylinder_length};
            }
            }
         }();

         float4x4 transform = static_cast<float4x4>(region.rotation) *
                              glm::mat4{{scale.x, 0.0f, 0.0f, 0.0f},
                                        {0.0f, scale.y, 0.0f, 0.0f},
                                        {0.0f, 0.0f, scale.z, 0.0f},
                                        {0.0f, 0.0f, 0.0f, 1.0f}};

         transform[3] = {region.position, 1.0f};

         auto allocation = _dynamic_buffer_allocator.allocate(sizeof(float4x4));

         std::memcpy(allocation.cpu_address, &transform, sizeof(float4x4));

         command_list.SetGraphicsRootConstantBufferView(1, allocation.gpu_address);
      }

      const geometric_shape shape = [&] {
         switch (region.shape) {
         default:
         case world::region_shape::box:
            return _geometric_shapes.cube();
         case world::region_shape::sphere:
            return _geometric_shapes.icosphere();
         case world::region_shape::cylinder:
            return _geometric_shapes.cylinder();
         }
      }();

      command_list.IASetVertexBuffers(0, 1, &shape.position_vertex_buffer_view);
      command_list.IASetIndexBuffer(&shape.index_buffer_view);
      command_list.DrawIndexedInstanced(shape.index_count, 1, 0, 0, 0);
   }

   // Set Barriers Color
   {
      const float4 color{1.0f, 0.1f, 0.05f, 0.3f};

      auto allocation = _dynamic_buffer_allocator.allocate(sizeof(float4));

      std::memcpy(allocation.cpu_address, &color, sizeof(float4));

      command_list.SetGraphicsRootConstantBufferView(2, allocation.gpu_address);
   }

   // Set Barriers IA State
   {
      const geometric_shape shape = _geometric_shapes.cube();

      command_list.IASetVertexBuffers(0, 1, &shape.position_vertex_buffer_view);
      command_list.IASetIndexBuffer(&shape.index_buffer_view);
   }

   for (auto& barrier : world.barriers) {
      // TEMP constants setup
      {
         const float2 position = (barrier.corners[0] + barrier.corners[2]) / 2.0f;
         const float2 size{glm::distance(barrier.corners[0], barrier.corners[3]),
                           glm::distance(barrier.corners[0], barrier.corners[1])};
         const float angle =
            std::atan2(barrier.corners[1].x - barrier.corners[0].x,
                       barrier.corners[1].y - barrier.corners[0].y);

         const quaternion rotation{float3{0.0f, angle, 0.0f}};

         float4x4 transform = static_cast<float4x4>(rotation) *
                              glm::mat4{{size.x / 2.0f, 0.0f, 0.0f, 0.0f},
                                        {0.0f, temp_barrier_height, 0.0f, 0.0f},
                                        {0.0f, 0.0f, size.y / 2.0f, 0.0f},
                                        {0.0f, 0.0f, 0.0f, 1.0f}};

         transform[3] = {position.x, 0.0f, position.y, 1.0f};

         auto allocation = _dynamic_buffer_allocator.allocate(sizeof(float4x4));

         std::memcpy(allocation.cpu_address, &transform, sizeof(float4x4));

         command_list.SetGraphicsRootConstantBufferView(1, allocation.gpu_address);
      }

      command_list.DrawIndexedInstanced(_geometric_shapes.cube().index_count, 1,
                                        0, 0, 0);
   }
}

void renderer::build_object_render_list(
   const world::world& world,
   const std::unordered_map<std::string, world::object_class>& world_classes)
{
   _object_render_list.clear();
   _object_render_list.reserve(world.objects.size());

   for (auto& object : world.objects) {
      auto& model = _model_manager.get(world_classes.at(object.class_name).model);

      float4x4 transform = static_cast<float4x4>(object.rotation);
      transform[3] = float4{object.position, 1.0f};

      for (const auto& mesh : model.parts) {
         _object_render_list.push_back(
            {.distance = 0.0f, // TODO: Distance, frustum culling...
             .index_count = mesh.index_count,
             .start_index = mesh.start_index,
             .start_vertex = mesh.start_vertex,
             .index_buffer_view = model.gpu_buffer.index_buffer_view,
             .vertex_buffer_views = {model.gpu_buffer.position_vertex_buffer_view,
                                     model.gpu_buffer.normal_vertex_buffer_view,
                                     model.gpu_buffer.texcoord_vertex_buffer_view},
             .transform = transform});
      }
   }
}
}