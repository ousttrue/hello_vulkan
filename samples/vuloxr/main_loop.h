#pragma once
#include <vuloxr/gui.h>
#include <vuloxr/vk.h>
#include <vuloxr/vk/swapchain.h>

void main_loop(const vuloxr::gui::WindowLoopOnce &windowLoopOnce,
               const vuloxr::vk::Instance &instance,
               vuloxr::vk::Swapchain &swapchain,
               const vuloxr::vk::PhysicalDevice &physicalDevice,
               const vuloxr::vk::Device &device, void *window);
