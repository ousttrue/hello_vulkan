#pragma once
#include <functional>
#include <vko/vko.h>

void main_loop(const std::function<bool()> &runLoop,
               const vko::Surface &surface, vko::PhysicalDevice picked,
               const vko::Device &device, void *p);
