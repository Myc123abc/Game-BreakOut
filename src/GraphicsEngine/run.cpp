#include "GraphicsEngine.hpp"
#include "ShaderStructs.hpp"
#include "ErrorHandling.hpp"
#include "constant.hpp"

#include <SDL3/SDL_events.h>
#include <glm/gtc/matrix_transform.hpp>

#include <thread>
#include <chrono>
#include <numbers>

namespace tk { namespace graphics_engine {

void GraphicsEngine::run()
{
  bool quit  = false;
  bool pause = false;

  SDL_Event event;

  while (!quit)
  {
    while (SDL_PollEvent(&event))
    {
      switch (event.type)
      {
      case SDL_EVENT_QUIT:
        quit = true;
        break;
      case SDL_EVENT_WINDOW_MINIMIZED:
        pause = true;
        break;
      case SDL_EVENT_WINDOW_MAXIMIZED:
        pause = false;
        break;
      }
    }

    if (!pause)
    {
      update();
      draw();
    }
    else
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

VkClearColorValue Clear_Value;

void GraphicsEngine::update()
{
  static auto start_time   = std::chrono::high_resolution_clock::now();
  auto        current_time = std::chrono::high_resolution_clock::now();
  float       time         = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();

  UniformBufferObject ubo;
  ubo.model = glm::mat4(1.f);
  ubo.view = glm::mat4(1.f);
  ubo.proj = glm::mat4(1.f);
  // ubo.model = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  // ubo.view = glm::lookAt(glm::vec3(.0f, .0f, -1.f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, .0f));
  // ubo.proj = glm::perspective(45.0f, _swapchain_image_extent.width / (float) _swapchain_image_extent.height, 0.1f, 10.0f);
  // ubo.view  = glm::lookAt(glm::vec3(0.f, 0.f, 1.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f));
  // ubo.proj  = glm::ortho(-1.f, 1.f, -1.f, 1.f, -1.f, 100.f);

  // TODO: use vma to presently mapped, and vma's copy memory function
  vmaCopyMemoryToAllocation(_vma_allocator, &ubo, _uniform_buffer_allocations[_current_frame], 0, sizeof(ubo));

  // clear value
  uint32_t circle = time / 3;
  auto val = std::abs(std::sin(time / 3 * M_PI));
  float r{}, g{}, b{};
  auto mod = circle % 3;
  if (mod == 0)
    r = val;
  else if (mod == 1)
    g = val;
  else
    b = val;
  Clear_Value = { { r, g, b, 1.f } };
}

void GraphicsEngine::draw()
{
  //
  // get current frame resource
  //
  auto frame = get_current_frame();

  //
  // wait commands completely submitted to GPU,
  // and we can record next frame's commands.
  //

  // set forth parameter to 0, make vkWaitForFences return immediately,
  // so you can know whether finished for commands handled by GPU.
  throw_if(vkWaitForFences(_device, 1, &frame.fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS,
           "failed to wait fence");
  throw_if(vkResetFences(_device, 1, &frame.fence) != VK_SUCCESS,
           "failed to reset fence");

  //
  // acquire an available image which GPU not used currently,
  // so we can save render result on it.
  //
  uint32_t image_index;
  throw_if(vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX, frame.image_available_sem, VK_NULL_HANDLE, &image_index) != VK_SUCCESS,
           "failed to acquire swap chain image");

  //
  // now we know current frame resource is available,
  // and we need to reset command buffer to begin to record commands.
  // after record we need to end command so it can be submitted to queue.
  //
  throw_if(vkResetCommandBuffer(frame.command_buffer, 0) != VK_SUCCESS,
           "failed to reset command buffer");
  VkCommandBufferBeginInfo beg_info
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  vkBeginCommandBuffer(frame.command_buffer, &beg_info);

  // HACK: make frame.command_buffer and _swapchain_images[image_index] to frame resource function
  // only need like as follow:
  //   render_begin();  // get current frame, available command buffer and image.
  //                    // also switch command buffer to available, and clear vlaue.
  //   some_render_ops(); 
  //   render_end();    // submit commands to queue and end everything like command buffer, etc.

  // transition image layout to writeable
  transition_image_layout(frame.command_buffer, _swapchain_images[image_index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
  // clear color
  VkClearColorValue clear_value;

  // 
  // change to red to green to blue
  //
  auto clear_range = get_image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
  vkCmdClearColorImage(frame.command_buffer, _swapchain_images[image_index], VK_IMAGE_LAYOUT_GENERAL, &Clear_Value, 1, &clear_range);
  // transition image layout to presentable
  transition_image_layout(frame.command_buffer, _swapchain_images[image_index], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  // record_command_buffer(frame.command_buffer, image_index);

  throw_if(vkEndCommandBuffer(frame.command_buffer) != VK_SUCCESS,
           "failed to end command buffer");

  //
  // submit commands to queue, which will copied to GPU.
  //
  VkCommandBufferSubmitInfo cmd_submit_info
  {
    .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
    .commandBuffer = frame.command_buffer,
  };
  VkSemaphoreSubmitInfo wait_sem_submit_info
  {
    .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
    .semaphore = frame.image_available_sem,
    .value     = 1,
    .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
  };
  auto signal_sem_submit_info      = wait_sem_submit_info;
  signal_sem_submit_info.semaphore = frame.render_finished_sem;
  signal_sem_submit_info.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;

  VkSubmitInfo2 submit_info
  {
    .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
    .waitSemaphoreInfoCount   = 1,
    .pWaitSemaphoreInfos      = &wait_sem_submit_info,
    .commandBufferInfoCount   = 1,
    .pCommandBufferInfos      = &cmd_submit_info,
    .signalSemaphoreInfoCount = 1,
    .pSignalSemaphoreInfos    = &signal_sem_submit_info,
  };
  throw_if(vkQueueSubmit2(_graphics_queue, 1, &submit_info, frame.fence),
           "failed to submit to queue");

  //
  // present to screen
  //
  VkPresentInfoKHR presentation_info
  {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores    = &frame.render_finished_sem,
    .swapchainCount     = 1,
    .pSwapchains        = &_swapchain,
    .pImageIndices      = &image_index,
  };
  throw_if(vkQueuePresentKHR(_present_queue, &presentation_info) != VK_SUCCESS,
           "failed to present swapchain image");

  // update frame index
  _current_frame = ++_current_frame % Max_Frame_Number;
}
    
void GraphicsEngine::record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_index)
{
  VkClearValue clear
  {
    (float)40/255,
    (float)44/255,
    (float)52/255,
    1.f,
  };
  VkRenderPassBeginInfo render_pass_begin_info
  {
    .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass  = _render_pass,
    .framebuffer = _framebuffers[image_index],
    .renderArea  =
    {
      .offset = { 0, 0 },
      .extent = _swapchain_image_extent,
    },
    .clearValueCount = 1,
    .pClearValues    = &clear,
  };
  vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);

  VkViewport viewport
  {
    .width    = (float)_swapchain_image_extent.width,
    .height   = (float)_swapchain_image_extent.height,
    .maxDepth = 1.f,
  };
  vkCmdSetViewport(command_buffer, 0, 1, &viewport);

  VkRect2D scissor
  {
    .offset = { 0, 0 },
    .extent = _swapchain_image_extent,
  };
  vkCmdSetScissor(command_buffer, 0, 1, &scissor);

  VkDeviceSize offsets[] = { 0 };
  vkCmdBindVertexBuffers(command_buffer, 0, 1, &_vertex_buffer, offsets);
  vkCmdBindIndexBuffer(command_buffer, _index_buffer, 0, VK_INDEX_TYPE_UINT16);

  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline_layout, 0, 1, &_descriptor_sets[_current_frame], 0, nullptr);

  vkCmdDrawIndexed(command_buffer, (uint32_t)Indices.size(), 1, 0, 0, 0);

  vkCmdEndRenderPass(command_buffer);
}

} }
