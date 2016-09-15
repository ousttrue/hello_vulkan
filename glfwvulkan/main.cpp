#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <string>
#include <vector>
#include <memory>
#include <Windows.h>


class GlfwManager
{
	GLFWwindow *m_window = nullptr;

public:
	GlfwManager()
	{
	}

	~GlfwManager()
	{
		glfwTerminate();
	}

	bool initilize() {
		if (!glfwInit()) {
			return false;
		}

		if (!glfwVulkanSupported())
		{
			return false;
		}

		return true;
	}

	bool createWindow(int w, int h
		, const std::string &title)
	{
		if (m_window) {
			return false;
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		m_window = glfwCreateWindow(w, h
			, title.c_str(), NULL, NULL);
		if (!m_window) {
			return false;
		}
		//glfwMakeContextCurrent(m_window);
		return true;
	}

	HWND getWindow()const
	{
		return glfwGetWin32Window(m_window);
	}

	bool runLoop()
	{
		if (glfwWindowShouldClose(m_window)) {
			return false;
		}
		glfwPollEvents();
		return true;
	}

	void swapBuffer()
	{
		glfwSwapBuffers(m_window);
	}
};


template<typename T>
const T* data_or_null(const std::vector<T> &v)
{
	return v.empty() ? nullptr : v.data();
}


PFN_vkCreateDebugReportCallbackEXT g_vkCreateDebugReportCallbackEXT = nullptr;
PFN_vkDebugReportMessageEXT g_vkDebugReportMessageEXT = nullptr;
PFN_vkDestroyDebugReportCallbackEXT g_vkDestroyDebugReportCallbackEXT = nullptr;


VKAPI_ATTR VkBool32 VKAPI_CALL MyDebugReportCallback(
	VkDebugReportFlagsEXT       flags,
	VkDebugReportObjectTypeEXT  objectType,
	uint64_t                    object,
	size_t                      location,
	int32_t                     messageCode,
	const char*                 pLayerPrefix,
	const char*                 pMessage,
	void*                       pUserData)
{
	OutputDebugStringA(pMessage);
	return VK_FALSE;
}


class GpuManager
{
	VkPhysicalDevice m_gpu;
	std::vector<VkQueueFamilyProperties> m_queue_props;
	VkPhysicalDeviceMemoryProperties m_memory_properties = {};
	VkPhysicalDeviceProperties m_gpu_props = {};

	uint32_t m_graphics_queue_family_index = -1;
	uint32_t m_present_queue_family_index = -1;

	VkFormat m_format = VK_FORMAT_UNDEFINED;

public:
	GpuManager(VkPhysicalDevice gpu)
		: m_gpu(gpu)
	{
	}

	VkPhysicalDevice get()const { return m_gpu; }
	VkFormat getPrimaryFormat()const { return m_format; }
	uint32_t get_graphics_queue_family_index()const
	{
		return m_graphics_queue_family_index;
	}
	uint32_t get_present_queue_family_index()const
	{
		return m_present_queue_family_index;
	}
	/*
	const VkPhysicalDeviceMemoryProperties& get_memory_properties()const
	{
		return m_memory_properties;
	}
	*/
	bool memory_type_from_properties(
		uint32_t typeBits,
		VkFlags requirements_mask,
		uint32_t *typeIndex)const
	{
		// Search memtypes to find first index with those properties
		for (uint32_t i = 0; i < m_memory_properties.memoryTypeCount; i++) {
			if ((typeBits & 1) == 1) {
				// Type is available, does it match user properties?
				if ((m_memory_properties.memoryTypes[i].propertyFlags &
					requirements_mask) == requirements_mask) {
					*typeIndex = i;
					return true;
				}
			}
			typeBits >>= 1;
		}
		// No memory types matched, return failure
		return false;
	}

	bool initialize()
	{
		uint32_t queue_family_count;
		vkGetPhysicalDeviceQueueFamilyProperties(m_gpu,
			&queue_family_count, NULL);
		if (queue_family_count == 0) {
			return false;
		}

		m_queue_props.resize(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(
			m_gpu, &queue_family_count, m_queue_props.data());
		if (queue_family_count != m_queue_props.size()) {
			return false;
		}

		// This is as good a place as any to do this
		vkGetPhysicalDeviceMemoryProperties(m_gpu, &m_memory_properties);
		vkGetPhysicalDeviceProperties(m_gpu, &m_gpu_props);
		return true;
	}

	bool prepared(VkSurfaceKHR surface)
	{
		// Iterate over each queue to learn whether it supports presenting:
		std::vector<VkBool32> pSupportsPresent(m_queue_props.size());
		for (uint32_t i = 0; i < pSupportsPresent.size(); i++) {
			vkGetPhysicalDeviceSurfaceSupportKHR(m_gpu, i, surface,
				&pSupportsPresent[i]);
		}

		// Search for a graphics and a present queue in the array of queue
		// families, try to find one that supports both
		m_graphics_queue_family_index = UINT32_MAX;
		m_present_queue_family_index = UINT32_MAX;
		for (uint32_t i = 0; i < m_queue_props.size(); ++i) {
			if ((m_queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
				if (m_graphics_queue_family_index == UINT32_MAX) {
					m_graphics_queue_family_index = i;
				}

				if (pSupportsPresent[i] == VK_TRUE) {
					m_graphics_queue_family_index = i;
					m_present_queue_family_index = i;
					break;
				}
			}
		}

		if (m_present_queue_family_index == UINT32_MAX) {
			// If didn't find a queue that supports both graphics and present, then
			// find a separate present queue.
			for (size_t i = 0; i < m_queue_props.size(); ++i)
				if (pSupportsPresent[i] == VK_TRUE) {
					m_present_queue_family_index = i;
					break;
				}
		}

		// Generate error if could not find queues that support graphics
		// and present
		if (m_graphics_queue_family_index == UINT32_MAX ||
			m_present_queue_family_index == UINT32_MAX) {
			//throw "Could not find a queues for both graphics and present";
			return false;
		}

		// Get the list of VkFormats that are supported:
		uint32_t formatCount;
		auto res = vkGetPhysicalDeviceSurfaceFormatsKHR(m_gpu, surface,
			&formatCount, NULL);
		if (res != VK_SUCCESS) {
			return false;
		}
		if (formatCount == 0) {
			return false;
		}
		std::vector<VkSurfaceFormatKHR> surfFormats(formatCount);
		res = vkGetPhysicalDeviceSurfaceFormatsKHR(m_gpu, surface,
			&formatCount, surfFormats.data());
		if (res != VK_SUCCESS) {
			return false;
		}

		if (formatCount == 1 && surfFormats[0].format == VK_FORMAT_UNDEFINED) {
			// If the format list includes just one entry of VK_FORMAT_UNDEFINED,
			// the surface has no preferred format.  Otherwise, at least one
			// supported format will be returned.
			m_format = VK_FORMAT_B8G8R8A8_UNORM;
			return true;
		}
		else {
			m_format = surfFormats[0].format;
		}

		return true;
    }


};


class SurfaceManager
{
	VkInstance m_inst = nullptr;
	VkSurfaceKHR m_surface = nullptr;

	VkSurfaceCapabilitiesKHR m_surfCapabilities = {};
	std::vector<VkPresentModeKHR> m_presentModes;

public:
	SurfaceManager()
	{

	}

	~SurfaceManager()
	{
		vkDestroySurfaceKHR(m_inst, m_surface, nullptr);
	}

	VkSurfaceKHR get()const { return m_surface; }

	bool initialize(VkInstance inst, HINSTANCE hInstance, HWND hWnd)
	{
		VkWin32SurfaceCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		createInfo.pNext = NULL;
		createInfo.hinstance = hInstance;
		createInfo.hwnd = hWnd;
		auto res = vkCreateWin32SurfaceKHR(inst, &createInfo,
			NULL, &m_surface);
		m_inst = inst;
		return true;
	}

	bool getCapabilityFor(VkPhysicalDevice gpu)
	{
		auto res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, m_surface,
			&m_surfCapabilities);
		if (res != VK_SUCCESS) {
			return false;
		}

		uint32_t presentModeCount;
		res = vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, m_surface,
			&presentModeCount, NULL);
		if (res != VK_SUCCESS) {
			return false;
		}
		if (presentModeCount == 0) {
			return false;
		}

		m_presentModes.resize(presentModeCount);
		res = vkGetPhysicalDeviceSurfacePresentModesKHR(
			gpu, m_surface, &presentModeCount, m_presentModes.data());
		if (res != VK_SUCCESS) {
			return false;
		}

		return true;
	}

	VkPresentModeKHR getSwapchainPresentMode()const
	{
		// If mailbox mode is available, use it, as is the lowest-latency non-
		// tearing mode.  If not, try IMMEDIATE which will usually be available,
		// and is fastest (though it tears).  If not, fall back to FIFO which is
		// always available.
		VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
		for (auto mode : m_presentModes) {
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
				swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}
			if ((swapchainPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) &&
				(mode == VK_PRESENT_MODE_IMMEDIATE_KHR)) {
				swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			}
		}

		return swapchainPresentMode;
	}

	uint32_t getDesiredNumberOfSwapchainImages()const
	{
		// Determine the number of VkImage's to use in the swap chain.
		// We need to acquire only 1 presentable image at at time.
		// Asking for minImageCount images ensures that we can acquire
		// 1 presentable image as long as we present it before attempting
		// to acquire another.
		return m_surfCapabilities.minImageCount;
	}

	VkSurfaceTransformFlagBitsKHR getPreTransform()const
	{
		if (m_surfCapabilities.supportedTransforms &
			VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
			return VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		}
		else {
			return m_surfCapabilities.currentTransform;
		}
	}

	VkExtent2D getExtent(int w, int h)const
	{
		// width and height are either both 0xFFFFFFFF, or both not 0xFFFFFFFF.
		if (m_surfCapabilities.currentExtent.width != 0xFFFFFFFF) {
			return m_surfCapabilities.currentExtent;
		}

		VkExtent2D swapchainExtent;
		// If the surface size is undefined, the size is set to
		// the size of the images requested.
		swapchainExtent.width = w;
		swapchainExtent.height = h;
		if (swapchainExtent.width < m_surfCapabilities.minImageExtent.width) {
			swapchainExtent.width = m_surfCapabilities.minImageExtent.width;
		}
		else if (swapchainExtent.width >
			m_surfCapabilities.maxImageExtent.width) {
			swapchainExtent.width = m_surfCapabilities.maxImageExtent.width;
		}

		if (swapchainExtent.height < m_surfCapabilities.minImageExtent.height) {
			swapchainExtent.height = m_surfCapabilities.minImageExtent.height;
		}
		else if (swapchainExtent.height >
			m_surfCapabilities.maxImageExtent.height) {
			swapchainExtent.height = m_surfCapabilities.maxImageExtent.height;
		}
		return swapchainExtent;
	}
};


class CommandBufferManager
{
	VkCommandBuffer m_cmd = nullptr;

public:
};


class DeviceManager
{
	VkDevice m_device = nullptr;
	std::vector<const char*> m_device_extension_names;
	VkSwapchainKHR m_swapchain = nullptr;

	/*
	* Keep each of our swap chain buffers' image, command buffer and view in one
	* spot
	*/
	struct swap_chain_buffer
	{
		VkImage image = nullptr;
		VkImageView view = nullptr;

		void destroy(VkDevice device)
		{
			vkDestroyImageView(device, view, NULL);
		}
	};
	std::vector<std::unique_ptr<swap_chain_buffer>> m_buffers;

	struct depth_buffer
	{
		//VkFormat format = VK_FORMAT_D16_UNORM;
		VkImage image = nullptr;
		VkDeviceMemory mem = nullptr;
		VkImageView view = nullptr;

		void destroy(VkDevice device)
		{
			vkDestroyImageView(device, view, NULL);
			vkDestroyImage(device, image, NULL);
			vkFreeMemory(device, mem, NULL);
		}
	};
	std::unique_ptr<depth_buffer> m_depth;

public:
	DeviceManager()
		: m_depth(new depth_buffer)
	{
		m_device_extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	}

	~DeviceManager()
	{
		m_depth->destroy(m_device);
		m_depth.reset();
		for (auto &b : m_buffers)
		{
			b->destroy(m_device);
		}
		m_buffers.clear();
		vkDestroySwapchainKHR(m_device, m_swapchain, NULL);
		vkDestroyDevice(m_device, NULL);
	}

	bool create(const std::shared_ptr<GpuManager> &gpu)
	{
		int graphics_queue_family_index = gpu->get_graphics_queue_family_index();
		if (graphics_queue_family_index < 0) {
			return false;
		}

		VkDeviceQueueCreateInfo queue_info = {};
		float queue_priorities[1] = { 0.0 };
		queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_info.pNext = NULL;
		queue_info.queueCount = 1;
		queue_info.pQueuePriorities = queue_priorities;
		queue_info.queueFamilyIndex = graphics_queue_family_index;

		VkDeviceCreateInfo device_info = {};
		device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_info.pNext = NULL;
		device_info.queueCreateInfoCount = 1;
		device_info.pQueueCreateInfos = &queue_info;
		device_info.enabledExtensionCount = m_device_extension_names.size();
		device_info.ppEnabledExtensionNames = data_or_null(m_device_extension_names);
		device_info.pEnabledFeatures = NULL;

		auto res = vkCreateDevice(gpu->get(), &device_info, NULL, &m_device);

		if (res != VK_SUCCESS) {
			return false;
		}

		return true;
	}

	bool createSwapchain(const std::shared_ptr<GpuManager> &gpu
		, const std::shared_ptr<SurfaceManager> &surface
		, int w, int h
		, VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
	{
		if (!surface->getCapabilityFor(gpu->get())) {
			return false;
		}

		auto swapchainExtent = surface->getExtent(w, h);

		VkSwapchainCreateInfoKHR swapchain_ci = {};
		swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchain_ci.pNext = NULL;
		swapchain_ci.surface = surface->get();
		swapchain_ci.minImageCount = surface->getDesiredNumberOfSwapchainImages();
		swapchain_ci.imageFormat = gpu->getPrimaryFormat();
		swapchain_ci.imageExtent.width = swapchainExtent.width;
		swapchain_ci.imageExtent.height = swapchainExtent.height;
		swapchain_ci.preTransform = surface->getPreTransform();
		swapchain_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchain_ci.imageArrayLayers = 1;
		swapchain_ci.presentMode = surface->getSwapchainPresentMode();
		swapchain_ci.oldSwapchain = VK_NULL_HANDLE;
		swapchain_ci.clipped = true;

		swapchain_ci.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		swapchain_ci.imageUsage = usageFlags;
		swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchain_ci.queueFamilyIndexCount = 0;
		swapchain_ci.pQueueFamilyIndices = NULL;

		if (gpu->get_graphics_queue_family_index() != gpu->get_present_queue_family_index()) {
			// If the graphics and present queues are from different queue families,
			// we either have to explicitly transfer ownership of images between the
			// queues, or we have to create the swapchain with imageSharingMode
			// as VK_SHARING_MODE_CONCURRENT
			uint32_t queueFamilyIndices[2] = {
				gpu->get_graphics_queue_family_index(),
				gpu->get_present_queue_family_index(),
			};
			swapchain_ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapchain_ci.queueFamilyIndexCount = 2;
			swapchain_ci.pQueueFamilyIndices = queueFamilyIndices;
		}

		auto res = vkCreateSwapchainKHR(m_device, &swapchain_ci, NULL,
			&m_swapchain);
		if (res != VK_SUCCESS) {
			return false;
		}

		uint32_t swapchainImageCount;
		res = vkGetSwapchainImagesKHR(m_device, m_swapchain,
			&swapchainImageCount, NULL);
		if (res != VK_SUCCESS) {
			return false;
		}
		if (swapchainImageCount == 0) {
			return false;
		}

		std::vector<VkImage> swapchainImages(swapchainImageCount);
		res = vkGetSwapchainImagesKHR(m_device, m_swapchain,
			&swapchainImageCount, swapchainImages.data());
		if (res != VK_SUCCESS) {
			return false;
		}

		for (uint32_t i = 0; i < swapchainImageCount; i++)
		{
			VkImageViewCreateInfo color_image_view = {};
			color_image_view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			color_image_view.pNext = NULL;
			color_image_view.format = gpu->getPrimaryFormat();
			color_image_view.components.r = VK_COMPONENT_SWIZZLE_R;
			color_image_view.components.g = VK_COMPONENT_SWIZZLE_G;
			color_image_view.components.b = VK_COMPONENT_SWIZZLE_B;
			color_image_view.components.a = VK_COMPONENT_SWIZZLE_A;
			color_image_view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			color_image_view.subresourceRange.baseMipLevel = 0;
			color_image_view.subresourceRange.levelCount = 1;
			color_image_view.subresourceRange.baseArrayLayer = 0;
			color_image_view.subresourceRange.layerCount = 1;
			color_image_view.viewType = VK_IMAGE_VIEW_TYPE_2D;
			color_image_view.flags = 0;
			color_image_view.image = swapchainImages[i];

			auto sc_buffer = std::make_unique<swap_chain_buffer>();
			res = vkCreateImageView(m_device, &color_image_view, NULL,
				&sc_buffer->view);
			if (res != VK_SUCCESS) {
				return false;
			}
			sc_buffer->image = swapchainImages[i];
			m_buffers.push_back(std::move(sc_buffer));
		}

		return true;
	}


	bool createDepthbuffer(const std::shared_ptr<GpuManager> &gpu
		, int w, int h
		, VkFormat depth_format = VK_FORMAT_D16_UNORM
		, VkSampleCountFlagBits samples= VK_SAMPLE_COUNT_1_BIT
		)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(gpu->get(), depth_format, &props);

		VkImageCreateInfo image_info = {};
		if (props.linearTilingFeatures &
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
			image_info.tiling = VK_IMAGE_TILING_LINEAR;
		}
		else if (props.optimalTilingFeatures &
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
			image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		}
		else {
			/* Try other depth formats? */
			//std::cout << "depth_format " << depth_format << " Unsupported.\n";
			return false;
		}
		image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_info.pNext = NULL;
		image_info.imageType = VK_IMAGE_TYPE_2D;
		image_info.format = depth_format;
		image_info.extent.width = w;
		image_info.extent.height = h;
		image_info.extent.depth = 1;
		image_info.mipLevels = 1;
		image_info.arrayLayers = 1;
		image_info.samples = samples;
		image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image_info.queueFamilyIndexCount = 0;
		image_info.pQueueFamilyIndices = NULL;
		image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		image_info.flags = 0;
		auto res = vkCreateImage(m_device, &image_info, NULL, &m_depth->image);
		if (res != VK_SUCCESS) {
			return false;
		}

		VkMemoryRequirements mem_reqs;
		vkGetImageMemoryRequirements(m_device, m_depth->image, &mem_reqs);
		VkMemoryAllocateInfo mem_alloc = {};
		mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		mem_alloc.pNext = NULL;
		mem_alloc.allocationSize = 0;
		mem_alloc.memoryTypeIndex = 0;
		mem_alloc.allocationSize = mem_reqs.size;
		// Use the memory properties to determine the type of memory required
		auto pass = gpu->memory_type_from_properties(mem_reqs.memoryTypeBits,
		0, // No requirements
		&mem_alloc.memoryTypeIndex);
		if (!pass) {
			return false;
		}
		// Allocate memory
		res = vkAllocateMemory(m_device, &mem_alloc, NULL, &m_depth->mem);
		if (res != VK_SUCCESS) {
			return false;
		}
		/* Bind memory */
		res = vkBindImageMemory(m_device, m_depth->image, m_depth->mem, 0);
		if (res != VK_SUCCESS) {
			return false;
		}

		VkImageViewCreateInfo view_info = {};
		view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_info.pNext = NULL;
		view_info.image = VK_NULL_HANDLE;
		view_info.format = depth_format;
		view_info.components.r = VK_COMPONENT_SWIZZLE_R;
		view_info.components.g = VK_COMPONENT_SWIZZLE_G;
		view_info.components.b = VK_COMPONENT_SWIZZLE_B;
		view_info.components.a = VK_COMPONENT_SWIZZLE_A;
		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		view_info.subresourceRange.baseMipLevel = 0;
		view_info.subresourceRange.levelCount = 1;
		view_info.subresourceRange.baseArrayLayer = 0;
		view_info.subresourceRange.layerCount = 1;
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_info.flags = 0;
		if (depth_format == VK_FORMAT_D16_UNORM_S8_UINT ||
			depth_format == VK_FORMAT_D24_UNORM_S8_UINT ||
			depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
			view_info.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		// Create image view
		view_info.image = m_depth->image;
		res = vkCreateImageView(m_device, &view_info, NULL, &m_depth->view);
		if (res != VK_SUCCESS)
		{
			return false;
		}

		/*
		// Set the image layout to depth stencil optimal
		set_image_layout(info, info.depth.image,
		view_info.subresourceRange.aspectMask,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		*/

		return true;
	}

};


class InstanceManager
{
	std::string m_app_short_name;
	std::string m_engine_name;

	VkInstance m_inst = nullptr;

	std::vector<const char *> m_instance_layer_names;
	std::vector<const char *> m_instance_extension_names;

	std::shared_ptr<DeviceManager> m_device;

    std::vector<std::shared_ptr<GpuManager>> m_gpus;

	VkDebugReportCallbackEXT m_callback;

	std::shared_ptr<SurfaceManager> m_surface;

public:
	InstanceManager(const std::string &app_name
		, const std::string &engine_name)
		: m_app_short_name(app_name), m_engine_name(engine_name)
		, m_device(new DeviceManager)
		, m_surface(new SurfaceManager)
	{
		m_instance_extension_names.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
		m_instance_extension_names.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);

#ifndef NDEBUG
        // Enable validation layers in debug builds 
        // to detect validation errors
        m_instance_layer_names.push_back("VK_LAYER_LUNARG_standard_validation");
        // Enable debug report extension in debug builds 
        // to be able to consume validation errors 
        m_instance_extension_names.push_back("VK_EXT_debug_report");
#endif
	}

	~InstanceManager()
	{
#ifndef NDEBUG
		g_vkDestroyDebugReportCallbackEXT(m_inst, m_callback, nullptr);
#endif
		m_device.reset();
		m_surface.reset();
		vkDestroyInstance(m_inst, NULL);
	}

	bool create()
	{
		VkApplicationInfo app_info = {};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pNext = NULL;
		app_info.pApplicationName = m_app_short_name.c_str();
		app_info.applicationVersion = 1;
		app_info.pEngineName = m_engine_name.c_str();
		app_info.engineVersion = 1;
		app_info.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo inst_info = {};
		inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		inst_info.pNext = NULL;
		inst_info.flags = 0;
		inst_info.pApplicationInfo = &app_info;
		// layers
		inst_info.enabledLayerCount = m_instance_layer_names.size();
		inst_info.ppEnabledLayerNames = data_or_null(m_instance_layer_names);
		// extensions
		inst_info.enabledExtensionCount = m_instance_extension_names.size();
		inst_info.ppEnabledExtensionNames = data_or_null(m_instance_extension_names);

		VkResult res = vkCreateInstance(&inst_info, NULL, &m_inst);
		if (res != VK_SUCCESS) {
			return false;
		}

#ifndef NDEBUG
        /* Load VK_EXT_debug_report entry points in debug builds */
        g_vkCreateDebugReportCallbackEXT =
            reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>
            (vkGetInstanceProcAddr(m_inst, "vkCreateDebugReportCallbackEXT"));
        g_vkDebugReportMessageEXT =
            reinterpret_cast<PFN_vkDebugReportMessageEXT>
            (vkGetInstanceProcAddr(m_inst, "vkDebugReportMessageEXT"));
        g_vkDestroyDebugReportCallbackEXT =
            reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>
            (vkGetInstanceProcAddr(m_inst, "vkDestroyDebugReportCallbackEXT"));
#endif

		return true;
	}

    bool enumerate_gpu() 
    {
        uint32_t gpu_count=0;
        VkResult res = vkEnumeratePhysicalDevices(m_inst, &gpu_count, NULL);
        if(gpu_count==0){
            return false;
        }

        std::vector<VkPhysicalDevice> gpus(gpu_count);
        res = vkEnumeratePhysicalDevices(m_inst, &gpu_count, gpus.data());
        if(res!=VK_SUCCESS){
            return false;
        }

        for(auto gpu: gpus)
        {
            auto gpuManager=std::make_shared<GpuManager>(gpu);
            if(!gpuManager->initialize()){
                return false;
            }
            m_gpus.push_back(gpuManager);
        }

        return true;
    }

	bool createDevice(size_t gpuIndex)
	{
        if(gpuIndex>=m_gpus.size()){
            return false;
        }
		auto gpu = m_gpus[gpuIndex];
		if (!gpu->prepared(m_surface->get())) {
			return false;
		}
		if (!m_device->create(gpu)) {
			return false;
		}

#ifndef NDEBUG
		/* Setup callback creation information */
		VkDebugReportCallbackCreateInfoEXT callbackCreateInfo;
		callbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
		callbackCreateInfo.pNext = nullptr;
		callbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
			VK_DEBUG_REPORT_WARNING_BIT_EXT |
			VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		callbackCreateInfo.pfnCallback = &MyDebugReportCallback;
		callbackCreateInfo.pUserData = nullptr;

		/* Register the callback */
		VkResult result = g_vkCreateDebugReportCallbackEXT(m_inst, &callbackCreateInfo, nullptr, &m_callback);
#endif
		return true;
	}

	bool createSurfaceFromWindow(HINSTANCE hInstance, HWND hWnd)
	{
		return m_surface->initialize(m_inst, hInstance, hWnd);
	}

	bool createSwapchain(int w, int h)
	{
		if (m_gpus.empty()) {
			return false;
		}
		if (!m_device->createSwapchain(m_gpus[0], m_surface, w, h)) {
			return false;
		}
		if (!m_device->createDepthbuffer(m_gpus[0], w, h)) {
			return false;
		}
		return true;
	}
};


int WINAPI WinMain(
	HINSTANCE hInstance,      // 現在のインスタンスのハンドル
	HINSTANCE hPrevInstance,  // 以前のインスタンスのハンドル
	LPSTR lpCmdLine,          // コマンドライン
	int nCmdShow              // 表示状態
)
{
	int w = 640;
	int h = 480;

	GlfwManager glfw;
	if (!glfw.initilize()) {
		return 1;
	}

	if (!glfw.createWindow(w, h, "Hello vulkan")) {
		return 2;
	}

	// instance
	InstanceManager instance("glfwvulkan", "vulkan engine");
	if (!instance.create()) {
		return 3;
	}

    if(!instance.enumerate_gpu()){
        return 4;
    }

    if(!instance.createDevice(0)){
        return 5;
    }

	if (!instance.createSurfaceFromWindow(hInstance, glfw.getWindow()))
	{
		return 6;
	}

	if (!instance.createSwapchain(w, h))
	{
		return 7;
	}


	while (glfw.runLoop())
	{
		/* Render here */
		//glClear(GL_COLOR_BUFFER_BIT);

		glfw.swapBuffer();
	}

	return 0;
}

