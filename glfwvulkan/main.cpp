#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <functional>
#include <Windows.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


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


template<class T>
class DeviceResource
{
	VkDevice m_device=nullptr;

	// avoid copy
	DeviceResource(const DeviceResource &) = delete;
	DeviceResource& operator=(const DeviceResource &) = delete;

	T m_t;

public:
	DeviceResource()
	{
	}

	virtual ~DeviceResource()
	{
		m_t.onDestroy(m_device);
	}

	bool create(VkDevice device)
	{
		m_device = device;
		return m_t.onCreate(m_device);
	}

	T &resource() { return m_t; }
	const T &resource()const { return m_t; }
};


struct IDeviceResource
{
	virtual void onDestroy(VkDevice device) = 0;
	virtual bool onCreate(VkDevice device) = 0;
};


class Framebuffer: public IDeviceResource
{
	VkFramebuffer m_framebuffer = nullptr;
	VkFramebufferCreateInfo m_fb_info = {};

public:
	void onDestroy(VkDevice device)override
	{
		if (m_framebuffer) {
			vkDestroyFramebuffer(device, m_framebuffer, NULL);
			m_framebuffer = nullptr;
		}
	}

	bool onCreate(VkDevice device)override
	{
		auto res = vkCreateFramebuffer(device, &m_fb_info, nullptr,
			&m_framebuffer);
		if (res != VK_SUCCESS) {
			return false;
		}
		return true;
	}

	bool configure(VkRenderPass render_pass,
		const VkImageView *attachments, int count, int w, int h)
	{
		m_fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		m_fb_info.pNext = nullptr;

		m_fb_info.renderPass = render_pass;
		m_fb_info.attachmentCount = count;

		m_fb_info.pAttachments = attachments;
		m_fb_info.width = w;
		m_fb_info.height = h;
		m_fb_info.layers = 1;

		return true;
	}
};
typedef DeviceResource<Framebuffer> FramebufferResource;


class Buffer : public IDeviceResource
{
	VkBuffer m_buf = nullptr;
	VkBufferCreateInfo m_buf_info = {};
	VkDeviceMemory m_mem = nullptr;
	VkDescriptorBufferInfo m_buffer_info = {};

public:
	void onDestroy(VkDevice device)override
	{
		vkDestroyBuffer(device, m_buf, NULL);
		vkFreeMemory(device, m_mem, NULL);
	}

	bool onCreate(VkDevice device)override
	{
		auto res = vkCreateBuffer(device, &m_buf_info, NULL
			, &m_buf);
		if (res != VK_SUCCESS) {
			return false;
		}
		return true;
	}

	bool configure(uint32_t dataSize)
	{
		m_buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		m_buf_info.pNext = NULL;
		m_buf_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		m_buf_info.size = dataSize;
		m_buf_info.queueFamilyIndexCount = 0;
		m_buf_info.pQueueFamilyIndices = NULL;
		m_buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		m_buf_info.flags = 0;
		return true;
	}

	bool allocate(VkDevice device, const std::shared_ptr<GpuManager> &gpu)
	{
		VkMemoryRequirements mem_reqs;
		vkGetBufferMemoryRequirements(device, m_buf, &mem_reqs);

		VkMemoryAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.memoryTypeIndex = 0;
		alloc_info.allocationSize = mem_reqs.size;
		if (!gpu->memory_type_from_properties(mem_reqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&alloc_info.memoryTypeIndex)) {
			//assert(pass && "No mappable, coherent memory");
			return false;
		}
		auto res = vkAllocateMemory(device, &alloc_info, NULL, &m_mem);
		if (res != VK_SUCCESS) {
			return false;
		}

		m_buffer_info.range = mem_reqs.size;
		m_buffer_info.offset = 0;

		res = vkBindBufferMemory(device, m_buf,
			m_mem, 0);
		if (res != VK_SUCCESS) {
			return false;
		}

		return true;
	}

	bool map(VkDevice device, const std::function<void(uint8_t*, uint32_t)> &mapCallback)
	{
		uint8_t *pData;
		auto res = vkMapMemory(device, m_mem, 0, m_buffer_info.range, 0
			, (void **)&pData);
		if (res != VK_SUCCESS) {
			return false;
		}

		mapCallback(pData, m_buffer_info.range);

		vkUnmapMemory(device, m_mem);

		return true;
	}
};
typedef DeviceResource<Buffer> BufferResource;


class VertexBuffer: public BufferResource
{
	VkVertexInputBindingDescription m_vi_binding = {};
	std::vector<VkVertexInputAttributeDescription> m_vi_attribs;

public:
	bool configure(uint32_t dataSize, uint32_t dataStride)
	{
		if (!resource().configure(dataSize)) {
			return false;
		}

		m_vi_binding.binding = 0;
		m_vi_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		m_vi_binding.stride = dataStride;

		m_vi_attribs.resize(2);

		m_vi_attribs[0].binding = 0;
		m_vi_attribs[0].location = 0;
		m_vi_attribs[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		m_vi_attribs[0].offset = 0;

		m_vi_attribs[1].binding = 0;
		m_vi_attribs[1].location = 1;
		m_vi_attribs[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		m_vi_attribs[1].offset = 16;

		return true;
	}
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
	VkSampleCountFlagBits m_depth_samples = VK_SAMPLE_COUNT_1_BIT;
	VkFormat m_depth_format = VK_FORMAT_D16_UNORM;

	std::vector<std::unique_ptr<FramebufferResource>> m_framebuffers;

	std::unique_ptr<VertexBuffer> m_vertex_buffer;

	struct uniform_buffer
	{
		VkBuffer buf = nullptr;
		VkDeviceMemory mem = nullptr;
		//VkDescriptorBufferInfo buffer_info = {};

		void destroy(VkDevice device)
		{
			vkDestroyBuffer(device, buf, NULL);
			vkFreeMemory(device, mem, NULL);
		}
	};
	std::unique_ptr<uniform_buffer> m_uniform_data;

	VkDescriptorSetLayout m_desc_layout=nullptr;
	VkPipelineLayout m_pipeline_layout=nullptr;

	VkRenderPass m_render_pass=nullptr;

	VkShaderModule m_shaderStages[2] = {
		nullptr, nullptr,
	};

public:
	DeviceManager()
		: m_depth(new depth_buffer)
		, m_uniform_data(new uniform_buffer)
	{
		m_device_extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	}

	~DeviceManager()
	{
		m_vertex_buffer.reset();
		m_framebuffers.clear();

		vkDestroyShaderModule(m_device, m_shaderStages[0], NULL);
		vkDestroyShaderModule(m_device, m_shaderStages[1], NULL);

		vkDestroyRenderPass(m_device, m_render_pass, NULL);

		vkDestroyDescriptorSetLayout(m_device, m_desc_layout, NULL);
		vkDestroyPipelineLayout(m_device, m_pipeline_layout, NULL);

		m_uniform_data->destroy(m_device);

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
		)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(gpu->get(), m_depth_format, &props);

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
		image_info.format = m_depth_format;
		image_info.extent.width = w;
		image_info.extent.height = h;
		image_info.extent.depth = 1;
		image_info.mipLevels = 1;
		image_info.arrayLayers = 1;
		image_info.samples = m_depth_samples;
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
		view_info.format = m_depth_format;
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
		if (m_depth_format == VK_FORMAT_D16_UNORM_S8_UINT ||
			m_depth_format == VK_FORMAT_D24_UNORM_S8_UINT ||
			m_depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
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

	bool createUniformBuffer(const std::shared_ptr<GpuManager> &gpu)
	{
		glm::mat4 MVP;

		/*
		VkResult U_ASSERT_ONLY res;
		bool U_ASSERT_ONLY pass;
		float fov = glm::radians(45.0f);
		if (info.width > info.height) {
			fov *= static_cast<float>(info.height) / static_cast<float>(info.width);
		}
		info.Projection = glm::perspective(fov,
			static_cast<float>(info.width) /
			static_cast<float>(info.height), 0.1f, 100.0f);
		info.View = glm::lookAt(
			glm::vec3(-5, 3, -10),  // Camera is at (-5,3,-10), in World Space
			glm::vec3(0, 0, 0),  // and looks at the origin
			glm::vec3(0, -1, 0)   // Head is up (set to 0,-1,0 to look upside-down)
		);
		info.Model = glm::mat4(1.0f);
		// Vulkan clip space has inverted Y and half Z.
		info.Clip = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, -1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.5f, 0.0f,
			0.0f, 0.0f, 0.5f, 1.0f);

		info.MVP = info.Clip * info.Projection * info.View * info.Model;
		*/

		VkBufferCreateInfo buf_info = {};
		buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buf_info.pNext = NULL;
		buf_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		buf_info.size = sizeof(MVP);
		buf_info.queueFamilyIndexCount = 0;
		buf_info.pQueueFamilyIndices = NULL;
		buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buf_info.flags = 0;
		auto res = vkCreateBuffer(m_device, &buf_info, NULL, &m_uniform_data->buf);
		if (res != VK_SUCCESS) {
			return false;
		}
		VkMemoryRequirements mem_reqs;
		vkGetBufferMemoryRequirements(m_device, m_uniform_data->buf,
			&mem_reqs);
		VkMemoryAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = NULL;
		alloc_info.memoryTypeIndex = 0;
		alloc_info.allocationSize = mem_reqs.size;
		if (!gpu->memory_type_from_properties(mem_reqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&alloc_info.memoryTypeIndex))
		{
			//assert(pass && "No mappable, coherent memory");
			return false;
		}
		res = vkAllocateMemory(m_device, &alloc_info, NULL,
			&m_uniform_data->mem);
		if (res != VK_SUCCESS) {
			return false;
		}

		res = vkBindBufferMemory(m_device, m_uniform_data->buf,
			m_uniform_data->mem, 0);
		if (res != VK_SUCCESS) {
			return false;
		}

		{
			// MAP
			uint8_t *pData;
			res = vkMapMemory(m_device, m_uniform_data->mem, 0, mem_reqs.size, 0,
				(void **)&pData);
			if (res != VK_SUCCESS) {
				return false;
			}
			memcpy(pData, &MVP, sizeof(MVP));
			vkUnmapMemory(m_device, m_uniform_data->mem);
		}

		/*
		m_uniform_data->buffer_info.buffer = m_uniform_data->buf;
		m_uniform_data->buffer_info.offset = 0;
		m_uniform_data->buffer_info.range = sizeof(MVP);
		*/

		return true;
	}

	bool createDescriptorAndPipelineLayout()
	{
		VkDescriptorSetLayoutBinding layout_bindings[1];
		layout_bindings[0].binding = 0;
		layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layout_bindings[0].descriptorCount = 1;
		layout_bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		layout_bindings[0].pImmutableSamplers = NULL;

		// Next take layout bindings and use them to create a descriptor set layout
		VkDescriptorSetLayoutCreateInfo descriptor_layout = {};
		descriptor_layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptor_layout.pNext = NULL;
		descriptor_layout.bindingCount = 1;
		descriptor_layout.pBindings = layout_bindings;

		auto res = vkCreateDescriptorSetLayout(m_device, &descriptor_layout, NULL,
			&m_desc_layout);
		if (res != VK_SUCCESS) {
			return false;
		}

		/* Now use the descriptor layout to create a pipeline layout */
		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
		pPipelineLayoutCreateInfo.sType =
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pPipelineLayoutCreateInfo.pNext = NULL;
		pPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
		pPipelineLayoutCreateInfo.pPushConstantRanges = NULL;
		pPipelineLayoutCreateInfo.setLayoutCount = 1;
		pPipelineLayoutCreateInfo.pSetLayouts = &m_desc_layout;

		res = vkCreatePipelineLayout(m_device, &pPipelineLayoutCreateInfo, NULL,
			&m_pipeline_layout);
		if (res != VK_SUCCESS)
		{
			return false;
		}

		return true;
	}

	bool createRenderpass(const std::shared_ptr<GpuManager> &gpu
		, bool include_depth=true
		, bool clear=true
		, VkImageLayout finalLayout= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
	{
		/* DEPENDS on init_swap_chain() and init_depth_buffer() */

		/* Need attachments for render target and depth buffer */
		VkAttachmentDescription attachments[2];
		attachments[0].format = gpu->getPrimaryFormat();
		attachments[0].samples = m_depth_samples;
		attachments[0].loadOp =
			clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[0].finalLayout = finalLayout;
		attachments[0].flags = 0;

		if (include_depth) {
			attachments[1].format = m_depth_format;
			attachments[1].samples = m_depth_samples;
			attachments[1].loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR
				: VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachments[1].initialLayout =
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			attachments[1].finalLayout =
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			attachments[1].flags = 0;
		}

		VkAttachmentReference color_reference = {};
		color_reference.attachment = 0;
		color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depth_reference = {};
		depth_reference.attachment = 1;
		depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.flags = 0;
		subpass.inputAttachmentCount = 0;
		subpass.pInputAttachments = NULL;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_reference;
		subpass.pResolveAttachments = NULL;
		subpass.pDepthStencilAttachment = include_depth ? &depth_reference : NULL;
		subpass.preserveAttachmentCount = 0;
		subpass.pPreserveAttachments = NULL;

		VkRenderPassCreateInfo rp_info = {};
		rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		rp_info.pNext = NULL;
		rp_info.attachmentCount = include_depth ? 2 : 1;
		rp_info.pAttachments = attachments;
		rp_info.subpassCount = 1;
		rp_info.pSubpasses = &subpass;
		rp_info.dependencyCount = 0;
		rp_info.pDependencies = NULL;

		auto res = vkCreateRenderPass(m_device, &rp_info, NULL, &m_render_pass);
		if (res != VK_SUCCESS) {
			return false;
		}

		return true;
	}

	bool createShader(int index, const std::vector<unsigned int> &spv)
	{
		VkShaderModuleCreateInfo moduleCreateInfo;
		moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleCreateInfo.pNext = NULL;
		moduleCreateInfo.flags = 0;
		moduleCreateInfo.codeSize = spv.size() * sizeof(unsigned int);
		moduleCreateInfo.pCode = spv.data();
		auto res = vkCreateShaderModule(m_device, &moduleCreateInfo, NULL,
			&m_shaderStages[index]);
		if (res != VK_SUCCESS) {
			return false;
		}
		return true;
	}

	bool createVertexBuffer(const std::shared_ptr<GpuManager> &gpu
		, const void *vertexData
		, uint32_t dataSize, uint32_t dataStride)
	{
		m_vertex_buffer = std::make_unique<VertexBuffer>();
		if (!m_vertex_buffer->configure(dataSize, dataStride)) {
			return false;
		}
		if (!m_vertex_buffer->create(m_device)) {
			return false;
		}
		if (!m_vertex_buffer->resource().allocate(m_device, gpu)) {
			return false;
		}
		auto callback= [vertexData, dataSize](uint8_t *pData, uint32_t size) {
			memcpy(pData, vertexData, dataSize);
		};
		if (!m_vertex_buffer->resource().map(m_device, callback)) {
			return false;
		}
		return true;
	}

	bool createFramebuffers(int w, int h)
	{
		for (size_t i = 0; i < m_buffers.size(); ++i) 
		{
			VkImageView attachments[] =
			{
				m_buffers[i]->view,
				m_depth->view,
			};

			auto fb=std::make_unique<FramebufferResource>();
			if (!fb->resource().configure(m_render_pass
				, attachments, _countof(attachments)
				, w, h)) {
				return false;
			}
			if (!fb->create(m_device)) {
				return false;
			}
			m_framebuffers.push_back(std::move(fb));
		}
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
		if (!m_device->createFramebuffers(w, h)) {
			return false;
		}
		return true;
	}

	bool createDeviceResources(const void *vertexData,
		uint32_t dataSize, uint32_t dataStride)
	{
		if (!m_device->createUniformBuffer(m_gpus[0])) {
			return false;
		}
		if (!m_device->createDescriptorAndPipelineLayout()) {
			return false;
		}
		if (!m_device->createRenderpass(m_gpus[0])) {
			return false;
		}
		if (!m_device->createVertexBuffer(m_gpus[0]
			, vertexData
			, dataSize, dataStride))
		{
			return false;
		}
		return true;
	}

	std::vector<unsigned int> read(const std::string &path)
	{
		std::vector<unsigned int> buf;
		std::ifstream ifs(path.c_str(), std::ios::binary);
		if (ifs) {
			ifs.seekg(0, std::ios::end);
			auto len = ifs.tellg();
			if (len > 0 && len % 4 == 0) {
				buf.resize(len / 4);
				ifs.seekg(0, std::ios::beg);
				ifs.read((char*)&buf[0], len);
			}
		}
		return buf;
	}

	bool createPipeline(const std::string &vertSpvPath
		, const std::string &fragSpvPath
		)
	{
		if (!m_device->createShader(0, read(vertSpvPath))) {
			return false;
		}
		if (!m_device->createShader(1, read(fragSpvPath))) {
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

	if (!instance.createSurfaceFromWindow(hInstance, glfw.getWindow()))
	{
		return 6;
	}

	if(!instance.createDevice(0)){
        return 5;
    }

	struct Vertex {
		float posX, posY, posZ, posW; // Position data
		float r, g, b, a;             // Color
	};
#define XYZ1(_x_, _y_, _z_) (_x_), (_y_), (_z_), 1.f
	static const Vertex g_vb_solid_face_colors_Data[] = {
		//red face
		{ XYZ1(-1,-1, 1), XYZ1(1.f, 0.f, 0.f) },
		{ XYZ1(-1, 1, 1), XYZ1(1.f, 0.f, 0.f) },
		{ XYZ1(1,-1, 1), XYZ1(1.f, 0.f, 0.f) },
		{ XYZ1(1,-1, 1), XYZ1(1.f, 0.f, 0.f) },
		{ XYZ1(-1, 1, 1), XYZ1(1.f, 0.f, 0.f) },
		{ XYZ1(1, 1, 1), XYZ1(1.f, 0.f, 0.f) },
		//green face
		{ XYZ1(-1,-1,-1), XYZ1(0.f, 1.f, 0.f) },
		{ XYZ1(1,-1,-1), XYZ1(0.f, 1.f, 0.f) },
		{ XYZ1(-1, 1,-1), XYZ1(0.f, 1.f, 0.f) },
		{ XYZ1(-1, 1,-1), XYZ1(0.f, 1.f, 0.f) },
		{ XYZ1(1,-1,-1), XYZ1(0.f, 1.f, 0.f) },
		{ XYZ1(1, 1,-1), XYZ1(0.f, 1.f, 0.f) },
		//blue face
		{ XYZ1(-1, 1, 1), XYZ1(0.f, 0.f, 1.f) },
		{ XYZ1(-1,-1, 1), XYZ1(0.f, 0.f, 1.f) },
		{ XYZ1(-1, 1,-1), XYZ1(0.f, 0.f, 1.f) },
		{ XYZ1(-1, 1,-1), XYZ1(0.f, 0.f, 1.f) },
		{ XYZ1(-1,-1, 1), XYZ1(0.f, 0.f, 1.f) },
		{ XYZ1(-1,-1,-1), XYZ1(0.f, 0.f, 1.f) },
		//yellow face
		{ XYZ1(1, 1, 1), XYZ1(1.f, 1.f, 0.f) },
		{ XYZ1(1, 1,-1), XYZ1(1.f, 1.f, 0.f) },
		{ XYZ1(1,-1, 1), XYZ1(1.f, 1.f, 0.f) },
		{ XYZ1(1,-1, 1), XYZ1(1.f, 1.f, 0.f) },
		{ XYZ1(1, 1,-1), XYZ1(1.f, 1.f, 0.f) },
		{ XYZ1(1,-1,-1), XYZ1(1.f, 1.f, 0.f) },
		//magenta face
		{ XYZ1(1, 1, 1), XYZ1(1.f, 0.f, 1.f) },
		{ XYZ1(-1, 1, 1), XYZ1(1.f, 0.f, 1.f) },
		{ XYZ1(1, 1,-1), XYZ1(1.f, 0.f, 1.f) },
		{ XYZ1(1, 1,-1), XYZ1(1.f, 0.f, 1.f) },
		{ XYZ1(-1, 1, 1), XYZ1(1.f, 0.f, 1.f) },
		{ XYZ1(-1, 1,-1), XYZ1(1.f, 0.f, 1.f) },
		//cyan face
		{ XYZ1(1,-1, 1), XYZ1(0.f, 1.f, 1.f) },
		{ XYZ1(1,-1,-1), XYZ1(0.f, 1.f, 1.f) },
		{ XYZ1(-1,-1, 1), XYZ1(0.f, 1.f, 1.f) },
		{ XYZ1(-1,-1, 1), XYZ1(0.f, 1.f, 1.f) },
		{ XYZ1(1,-1,-1), XYZ1(0.f, 1.f, 1.f) },
		{ XYZ1(-1,-1,-1), XYZ1(0.f, 1.f, 1.f) },
	};
#undef XYZ1;
	if (!instance.createDeviceResources(g_vb_solid_face_colors_Data,
		sizeof(g_vb_solid_face_colors_Data),
		sizeof(g_vb_solid_face_colors_Data[0]))) {
		return 8;
	}

	if (!instance.createSwapchain(w, h))
	{
		return 7;
	}

	if (!instance.createPipeline(
		"../15-draw_cube.vert.spv"
		, "../15-draw_cube.frag.spv"
		
		)) {
		return 9;
	}

	while (glfw.runLoop())
	{
		/* Render here */
		//glClear(GL_COLOR_BUFFER_BIT);

		glfw.swapBuffer();
	}

	return 0;
}
