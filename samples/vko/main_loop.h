#pragma once
#include <functional>
#include <vko/vko.h>

void main_loop(const std::function<bool()> &runLoop, vko::Swapchain &swapchain,
               vko::PhysicalDevice picked, const vko::Device &device);
