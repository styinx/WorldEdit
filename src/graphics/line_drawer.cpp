
#include "line_drawer.hpp"

namespace sk::graphics {

namespace {

constexpr auto max_buffered_lines =
   gpu::dynamic_buffer_allocator::alignment * 24 / (sizeof(float3) * 2);

}

void line_draw_context::add(const float3 begin, const float3 end)
{
   std::array<float3, 2> line{begin, end};

   std::memcpy(current_allocation.cpu_address + (sizeof(line) * buffered_lines),
               &line, sizeof(line));

   buffered_lines += 1;

   if (buffered_lines == max_buffered_lines) {
      draw_buffered();

      current_allocation =
         buffer_allocator.allocate(max_buffered_lines * sizeof(float3) * 2);
      buffered_lines = 0;
   }
}

void line_draw_context::draw_buffered()
{
   const D3D12_VERTEX_BUFFER_VIEW vbv{.BufferLocation = current_allocation.gpu_address,
                                      .SizeInBytes =
                                         static_cast<UINT>(current_allocation.size),
                                      .StrideInBytes = sizeof(float3)};

   command_list.IASetVertexBuffers(0, 1, &vbv);
   command_list.DrawInstanced(buffered_lines * 2, 1, 0, 0);
}

void draw_lines(ID3D12GraphicsCommandList5& command_list, gpu::device& device,
                gpu::dynamic_buffer_allocator& buffer_allocator,
                const line_draw_state draw_state,
                std::function<void(line_draw_context&)> draw_callback)
{
   command_list.SetPipelineState(device.pipelines.meta_line.get());
   command_list.SetGraphicsRootSignature(device.root_signatures.meta_line.get());

   command_list.SetGraphicsRootConstantBufferView(0, draw_state.camera_constants_address);

   {
      auto allocation = buffer_allocator.allocate(sizeof(float4));

      const float4 color{draw_state.line_color, 1.0f};

      std::memcpy(allocation.cpu_address, &color, sizeof(float4));

      command_list.SetGraphicsRootConstantBufferView(1, allocation.gpu_address);
   }

   command_list.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

   line_draw_context context{command_list, buffer_allocator,
                             buffer_allocator.allocate(max_buffered_lines *
                                                       sizeof(float3) * 2)};

   draw_callback(context);

   if (context.buffered_lines > 0) context.draw_buffered();
}

}