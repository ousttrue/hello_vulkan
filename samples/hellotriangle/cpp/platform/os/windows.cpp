#include "windows.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef SUCCEEDED
#undef FAILED

#include "asset_manager.hpp"
#include "platform/os.hpp"

#include "framework/application.hpp"

#include "framework/common.hpp"
#include "platform/os.hpp"
#include "platform/platform.hpp"

// Stub implementation.

namespace MaliSDK
{
AssetManager &OS::getAssetManager()
{
	return *static_cast<AssetManager *>(nullptr);
}

unsigned OS::getNumberOfCpuThreads()
{
	return 1;
}

double OS::getCurrentTime()
{
	return 0.0;
}
} // namespace MaliSDK

using namespace MaliSDK;

int main(int argc, char **argv)
{
	Platform &platform = Platform::get();
	if (FAILED(platform.initialize()))
	{
		LOGE("Failed to initialize platform.\n");
		return 1;
	}

	Platform::SwapchainDimensions dim = platform.getPreferredSwapchain();
	if (FAILED(platform.createWindow(dim)))
	{
		LOGE("Failed to create platform window.\n");
		return 1;
	}

	VulkanApplication *app = createApplication();
	if (!app)
	{
		LOGE("Failed to create application.\n");
		return 1;
	}

	if (!app->initialize(&platform.getContext()))
	{
		LOGE("Failed to initialize application.\n");
		return 1;
	}

  std::vector<VkImage> images;
	platform.getCurrentSwapchain(&images, &dim);
	app->updateSwapchain(images, dim);

	unsigned frameCount = 0;
	double startTime = OS::getCurrentTime();

	unsigned maxFrameCount = 0;
	bool useMaxFrameCount = false;
	if (argc == 2)
	{
		maxFrameCount = strtoul(argv[1], nullptr, 0);
		useMaxFrameCount = true;
	}

	while (platform.getWindowStatus() == Platform::STATUS_RUNNING)
	{
		unsigned index;
		Result res = platform.acquireNextImage(&index);
		while (res == RESULT_ERROR_OUTDATED_SWAPCHAIN)
		{
			res = platform.acquireNextImage(&index);
			platform.getCurrentSwapchain(&images, &dim);
			app->updateSwapchain(images, dim);
		}

		if (FAILED(res))
		{
			LOGE("Unrecoverable swapchain error.\n");
			break;
		}

		app->render(index, 0.0166f);
		res = platform.presentImage(index);

		// Handle Outdated error in acquire.
		if (FAILED(res) && res != RESULT_ERROR_OUTDATED_SWAPCHAIN)
			break;

		frameCount++;
		if (frameCount == 100)
		{
			double endTime = OS::getCurrentTime();
			LOGI("FPS: %.3f\n", frameCount / (endTime - startTime));
			frameCount = 0;
			startTime = endTime;
		}

		if (useMaxFrameCount && (--maxFrameCount == 0))
			break;
	}

	app->terminate();
	delete app;
	platform.terminate();
}
