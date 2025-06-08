#pragma once
#include <functional>
#include <vko/vko.h>

void main_loop(const std::function<bool()> &runLoop,
               const vko::Instance &instance, const vko::Surface &surface,
               vko::PhysicalDevice picked);
