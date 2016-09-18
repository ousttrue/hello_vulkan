#include "glfwmanager.h"
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <functional>
#include <Windows.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


template<typename T>
const T* data_or_null(const std::vector<T> &v)
{
	return v.empty() ? nullptr : v.data();
}


static std::vector<unsigned int> read(const std::string &path)
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


static glm::mat4 calcMVP(const int width, int height)
{
	float fov = glm::radians(45.0f);
	if (width > height) {
		fov *= static_cast<float>(height) / static_cast<float>(width);
	}

	auto Projection = glm::perspective(fov,
		static_cast<float>(width) /
		static_cast<float>(height), 0.1f, 100.0f);

	auto View = glm::lookAt(
		glm::vec3(-5, 3, -10),  // Camera is at (-5,3,-10), in World Space
		glm::vec3(0, 0, 0),  // and looks at the origin
		glm::vec3(0, -1, 0)   // Head is up (set to 0,-1,0 to look upside-down)
	);

	auto Model = glm::mat4(1.0f);

	// Vulkan clip space has inverted Y and half Z.
	auto Clip = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.5f, 0.0f,
		0.0f, 0.0f, 0.5f, 1.0f);

	return Clip * Projection * View * Model;
}


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
	OutputDebugStringA("\n");
	return VK_FALSE;
}


class InstanceManager
{
	VkInstance m_inst = nullptr;
	VkDebugReportCallbackEXT m_callback;
	PFN_vkCreateDebugReportCallbackEXT g_vkCreateDebugReportCallbackEXT = nullptr;
	PFN_vkDebugReportMessageEXT g_vkDebugReportMessageEXT = nullptr;
	PFN_vkDestroyDebugReportCallbackEXT g_vkDestroyDebugReportCallbackEXT = nullptr;

	InstanceManager(VkInstance inst)
		: m_inst(inst)
	{}

public:
	~InstanceManager()
	{
#ifndef NDEBUG
		g_vkDestroyDebugReportCallbackEXT(m_inst, m_callback, nullptr);
#endif
		vkDestroyInstance(m_inst, nullptr);
	}

	VkInstance get()const { return m_inst; }

	bool setupDebugCallback()
	{
		// Load VK_EXT_debug_report entry points in debug builds
		g_vkCreateDebugReportCallbackEXT =
			reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>
			(vkGetInstanceProcAddr(m_inst, "vkCreateDebugReportCallbackEXT"));
		g_vkDebugReportMessageEXT =
			reinterpret_cast<PFN_vkDebugReportMessageEXT>
			(vkGetInstanceProcAddr(m_inst, "vkDebugReportMessageEXT"));
		g_vkDestroyDebugReportCallbackEXT =
			reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>
			(vkGetInstanceProcAddr(m_inst, "vkDestroyDebugReportCallbackEXT"));

		// Setup callback creation information
		VkDebugReportCallbackCreateInfoEXT callbackCreateInfo;
		callbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
		callbackCreateInfo.pNext = nullptr;
		callbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
			VK_DEBUG_REPORT_WARNING_BIT_EXT |
			VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		callbackCreateInfo.pfnCallback = &MyDebugReportCallback;
		callbackCreateInfo.pUserData = nullptr;

		// Register the callback
		VkResult result = g_vkCreateDebugReportCallbackEXT(m_inst, &callbackCreateInfo, nullptr, &m_callback);
		if (result != VK_SUCCESS) {
			return false;
		}

		return true;
	}

	static std::shared_ptr<InstanceManager> create(const std::string &app_name
		, const std::string &engine_name)
	{
		std::vector<const char *> m_instance_layer_names;
		std::vector<const char *> m_instance_extension_names;
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

		VkApplicationInfo app_info = {};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pNext = nullptr;
		app_info.pApplicationName = app_name.c_str();
		app_info.applicationVersion = 1;
		app_info.pEngineName = engine_name.c_str();
		app_info.engineVersion = 1;
		app_info.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo inst_info = {};
		inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		inst_info.pNext = nullptr;
		inst_info.flags = 0;
		inst_info.pApplicationInfo = &app_info;
		// layers
		inst_info.enabledLayerCount = m_instance_layer_names.size();
		inst_info.ppEnabledLayerNames = data_or_null(m_instance_layer_names);
		// extensions
		inst_info.enabledExtensionCount = m_instance_extension_names.size();
		inst_info.ppEnabledExtensionNames = data_or_null(m_instance_extension_names);

		VkInstance inst;
		VkResult res = vkCreateInstance(&inst_info, nullptr, &inst);
		if (res != VK_SUCCESS) {
			return nullptr;
		}
		auto instanceManager = std::shared_ptr<InstanceManager>(new InstanceManager(inst));

#ifndef NDEBUG
		if (!instanceManager->setupDebugCallback()) {
			return nullptr;
		}
#endif

		return instanceManager;
	}
};


class GpuManager
{
	VkPhysicalDevice m_gpu;
	std::vector<VkQueueFamilyProperties> m_queue_props;
	VkPhysicalDeviceMemoryProperties m_memory_properties = {};
	VkPhysicalDeviceProperties m_gpu_props = {};

	uint32_t m_graphics_queue_family_index = -1;
	uint32_t m_present_queue_family_index = -1;

	VkFormat m_format = VK_FORMAT_UNDEFINED;

	GpuManager(VkPhysicalDevice gpu)
		: m_gpu(gpu)
	{
	}
public:

	static std::vector<std::shared_ptr<GpuManager>> enumerate_gpu(VkInstance inst)
	{
		std::vector<std::shared_ptr<GpuManager>> gpus;
		uint32_t pdevice_count = 0;
		VkResult res = vkEnumeratePhysicalDevices(inst, &pdevice_count, nullptr);
		if (pdevice_count > 0) {
			std::vector<VkPhysicalDevice> pdevices(pdevice_count);
			res = vkEnumeratePhysicalDevices(inst, &pdevice_count, pdevices.data());
			if (res == VK_SUCCESS) {
				for (auto pdevice : pdevices)
				{
					auto gpuManager = std::shared_ptr<GpuManager>(new GpuManager(pdevice));
					if (!gpuManager->initialize()) {
						break;
					}
					gpus.push_back(gpuManager);
				}
			}
		}
		return gpus;
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
			&queue_family_count, nullptr);
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

	bool prepare(VkSurfaceKHR surface)
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
			&formatCount, nullptr);
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


class DeviceManager
{
	std::vector<const char*> m_device_extension_names;
	std::vector<const char*> m_device_layer_names;
	VkDevice m_device;

public:
	DeviceManager()
	{
		m_device_extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#ifndef NDEBUG
		// Enable validation layers in debug builds to detect validation errors
		m_device_layer_names.push_back("VK_LAYER_LUNARG_standard_validation");
#endif
	}

	~DeviceManager()
	{
		vkDestroyDevice(m_device, nullptr);
	}

	VkDevice get()const { return m_device; }

	bool create(const std::shared_ptr<GpuManager> &gpu)
	{
		int graphics_queue_family_index = gpu->get_graphics_queue_family_index();
		if (graphics_queue_family_index < 0) {
			return false;
		}

		VkDeviceQueueCreateInfo queue_info = {};
		float queue_priorities[1] = { 0.0 };
		queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_info.pNext = nullptr;
		queue_info.queueCount = 1;
		queue_info.pQueuePriorities = queue_priorities;
		queue_info.queueFamilyIndex = graphics_queue_family_index;

		VkDeviceCreateInfo device_info = {};
		device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_info.pNext = nullptr;
		device_info.queueCreateInfoCount = 1;
		device_info.pQueueCreateInfos = &queue_info;

		device_info.enabledExtensionCount = m_device_extension_names.size();
		device_info.ppEnabledExtensionNames = data_or_null(m_device_extension_names);

		device_info.enabledLayerCount = m_device_layer_names.size();
		device_info.ppEnabledLayerNames = data_or_null(m_device_layer_names);

		device_info.pEnabledFeatures = nullptr;

		auto res = vkCreateDevice(gpu->get(), &device_info, nullptr, &m_device);
		if (res != VK_SUCCESS) {
			return false;
		}

		return true;
	}
};


class SurfaceManager
{
	std::shared_ptr<InstanceManager> m_inst;
	VkSurfaceKHR m_surface = nullptr;

	VkSurfaceCapabilitiesKHR m_surfCapabilities = {};
	std::vector<VkPresentModeKHR> m_presentModes;

public:
	SurfaceManager(const std::shared_ptr<InstanceManager> &inst)
		: m_inst(inst)
	{

	}

	~SurfaceManager()
	{
		vkDestroySurfaceKHR(m_inst->get(), m_surface, nullptr);
	}

	VkSurfaceKHR get()const { return m_surface; }

	bool create(HINSTANCE hInstance, HWND hWnd)
	{
		VkWin32SurfaceCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		createInfo.pNext = nullptr;
		createInfo.hinstance = hInstance;
		createInfo.hwnd = hWnd;
		auto res = vkCreateWin32SurfaceKHR(m_inst->get(), &createInfo,
			nullptr, &m_surface);
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
			&presentModeCount, nullptr);
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


class FramebufferResource
{
	std::shared_ptr<DeviceManager> m_device;

	VkRenderPass m_renderpass = nullptr;
	VkFramebuffer m_framebuffer = nullptr;

	VkAttachmentReference m_color_reference = {};
	VkAttachmentReference m_depth_reference = {};
	std::vector<VkSubpassDescription> m_subpasses;
	std::vector<VkAttachmentDescription> m_attachments;
	std::vector<VkImageView> m_views;

public:
	FramebufferResource(const std::shared_ptr<DeviceManager> &device)
		: m_device(device)
	{}

	~FramebufferResource()
	{
		vkDestroyFramebuffer(m_device->get(), m_framebuffer, nullptr);
		vkDestroyRenderPass(m_device->get(), m_renderpass, nullptr);
	}

	VkRenderPass getRenderPass()const { return m_renderpass; }
	VkFramebuffer getFramebuffer()const { return m_framebuffer; }

	bool create(int w, int h)
	{
		// renderpass
		VkRenderPassCreateInfo rp_info = {};
		rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		rp_info.pNext = nullptr;
		rp_info.attachmentCount = m_attachments.size();
		rp_info.pAttachments = m_attachments.data();
		rp_info.subpassCount = m_subpasses.size();
		rp_info.pSubpasses = m_subpasses.data();
		rp_info.dependencyCount = 0;
		rp_info.pDependencies = nullptr;
		auto res = vkCreateRenderPass(m_device->get(), &rp_info, nullptr, &m_renderpass);
		if (res != VK_SUCCESS) {
			return false;
		}

		// framebuffer
		VkFramebufferCreateInfo fb_info = {};
		fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb_info.pNext = nullptr;
		fb_info.renderPass = m_renderpass;
		fb_info.attachmentCount = m_views.size();
		fb_info.pAttachments = m_views.data();
		fb_info.width = w;
		fb_info.height = h;
		fb_info.layers = 1;
		res = vkCreateFramebuffer(m_device->get(), &fb_info, nullptr,
			&m_framebuffer);
		if (res != VK_SUCCESS) {
			return false;
		}

		return true;
	}

	void attachColor(VkImageView view, VkFormat format, VkSampleCountFlagBits samples
		, bool clear=true)
	{
		m_views.push_back(view);
		m_attachments.push_back(VkAttachmentDescription());
		auto &a = m_attachments.back();
		a.format = format;
		a.samples = samples;
		a.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		a.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		a.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		a.flags = 0;
	}

	void attachDepth(VkImageView view, VkFormat format, VkSampleCountFlagBits samples
		, bool clear = true)
	{
		m_views.push_back(view);
		m_attachments.push_back(VkAttachmentDescription());
		auto &a = m_attachments.back();
		a.format = format;
		a.samples = samples;
		a.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		a.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		a.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		a.flags = 0;
	}

	void pushSubpass(int colorIndex, int depthIndex)
	{
		m_subpasses.push_back(VkSubpassDescription());
		auto &subpass = m_subpasses.back();

		m_color_reference.attachment = colorIndex;
		m_color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		m_depth_reference.attachment = depthIndex;
		m_depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.flags = 0;
		subpass.inputAttachmentCount = 0;
		subpass.pInputAttachments = nullptr;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &m_color_reference;
		subpass.pResolveAttachments = nullptr;
		subpass.pDepthStencilAttachment = &m_depth_reference;
		subpass.preserveAttachmentCount = 0;
		subpass.pPreserveAttachments = nullptr;
	}
};


class BufferResource
{
	std::shared_ptr<DeviceManager> m_device;

	VkBuffer m_buf = nullptr;
	VkDeviceMemory m_mem = nullptr;
	VkDescriptorBufferInfo m_buffer_info = {};

public:
	BufferResource(const std::shared_ptr<DeviceManager> &device)
		: m_device(device)
	{}

	~BufferResource()
	{
		vkDestroyBuffer(m_device->get(), m_buf, nullptr);
		vkFreeMemory(m_device->get(), m_mem, nullptr);
	}

	VkBuffer getBuffer()const { return m_buf; }
	const VkDescriptorBufferInfo getDescInfo()const { return m_buffer_info; }

	bool create(const std::shared_ptr<GpuManager> &gpu
		, VkBufferUsageFlags usage, uint32_t dataSize)
	{
		// buffer
		VkBufferCreateInfo buf_info = {};
		buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buf_info.pNext = nullptr;
		buf_info.flags = 0;

		buf_info.usage = usage;
		buf_info.size = dataSize;
		buf_info.queueFamilyIndexCount = 0;
		buf_info.pQueueFamilyIndices = nullptr;
		buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		auto res = vkCreateBuffer(m_device->get(), &buf_info, nullptr
			, &m_buf);
		if (res != VK_SUCCESS) {
			return false;
		}
		VkMemoryRequirements mem_reqs;
		vkGetBufferMemoryRequirements(m_device->get(), m_buf, &mem_reqs);

		// alloc
		VkMemoryAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = nullptr;
		alloc_info.memoryTypeIndex = 0;
		alloc_info.allocationSize = mem_reqs.size;
		if (!gpu->memory_type_from_properties(mem_reqs.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&alloc_info.memoryTypeIndex)) {
			//assert(pass && "No mappable, coherent memory");
			return false;
		}
		res = vkAllocateMemory(m_device->get(), &alloc_info, nullptr, &m_mem);
		if (res != VK_SUCCESS) {
			return false;
		}

		// bind
		res = vkBindBufferMemory(m_device->get(), m_buf,
			m_mem, 0);
		if (res != VK_SUCCESS) {
			return false;
		}

		// desc
		m_buffer_info.buffer = m_buf;
		m_buffer_info.range = mem_reqs.size;
		m_buffer_info.offset = 0;

		return true;
	}

	bool map(const std::function<void(uint8_t*, uint32_t)> &mapCallback)
	{
		uint8_t *pData;
		auto res = vkMapMemory(m_device->get(), m_mem, 0, m_buffer_info.range, 0
			, (void **)&pData);
		if (res != VK_SUCCESS) {
			return false;
		}

		mapCallback(pData, m_buffer_info.range);

		vkUnmapMemory(m_device->get(), m_mem);

		return true;
	}
};


class VertexbufferDesc
{
	VkVertexInputBindingDescription m_vi_binding = {};
	std::vector<VkVertexInputAttributeDescription> m_vi_attribs;
public:
	VertexbufferDesc()
	{
		m_vi_binding.binding = 0;
		m_vi_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	}
	const VkVertexInputBindingDescription& getBindingDesc()const {
		return m_vi_binding;
	}
	const VkVertexInputAttributeDescription* getAttribs()const
	{
		return m_vi_attribs.empty() ? nullptr : m_vi_attribs.data();
	}
	void pushAttrib(VkFormat format= VK_FORMAT_R32G32B32A32_SFLOAT, uint32_t offset=16)
	{
		uint32_t location=m_vi_attribs.size();
		m_vi_attribs.push_back(VkVertexInputAttributeDescription());
		auto &iadesc = m_vi_attribs.back();
		iadesc.binding = 0;
		iadesc.location = location;
		iadesc.format = format;
		iadesc.offset = m_vi_binding.stride;
		m_vi_binding.stride += 16;
	}
};


class DepthbufferResource
{
	std::shared_ptr<DeviceManager> m_device;

	VkSampleCountFlagBits m_depth_samples = VK_SAMPLE_COUNT_1_BIT;
	VkFormat m_depth_format = VK_FORMAT_D16_UNORM;

	VkImage m_image = nullptr;
	VkDeviceMemory m_mem = nullptr;
	VkImageView m_view = nullptr;
	VkImageViewCreateInfo m_view_info = {};

public:
	DepthbufferResource(const std::shared_ptr<DeviceManager> &device)
		:m_device(device)
	{}

	~DepthbufferResource()
	{
		vkDestroyImageView(m_device->get(), m_view, nullptr);
		vkDestroyImage(m_device->get(), m_image, nullptr);
		vkFreeMemory(m_device->get(), m_mem, nullptr);
	}

	VkSampleCountFlagBits getSamples()const { return m_depth_samples; }
	VkFormat getFormat()const { return m_depth_format; }
	VkImageView getView()const { return m_view; }
	VkImage getImage()const { return m_image; }
	VkImageAspectFlags getAspect()const { return m_view_info.subresourceRange.aspectMask; }

	bool create(const std::shared_ptr<GpuManager> &gpu
		, int w, int h
	)
	{
		// image
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
		image_info.pNext = nullptr;
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
		image_info.pQueueFamilyIndices = nullptr;
		image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		image_info.flags = 0;
		auto res = vkCreateImage(m_device->get(), &image_info, nullptr, &m_image);
		if (res != VK_SUCCESS) {
			return false;
		}

		// allocate
		VkMemoryRequirements mem_reqs;
		vkGetImageMemoryRequirements(m_device->get(), m_image, &mem_reqs);

		VkMemoryAllocateInfo mem_alloc = {};
		mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		mem_alloc.pNext = nullptr;
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
		res = vkAllocateMemory(m_device->get(), &mem_alloc, nullptr, &m_mem);
		if (res != VK_SUCCESS) {
			return false;
		}

		// Bind memory
		res = vkBindImageMemory(m_device->get(), m_image, m_mem, 0);
		if (res != VK_SUCCESS) {
			return false;
		}

		m_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		m_view_info.pNext = nullptr;
		m_view_info.image = VK_NULL_HANDLE;
		m_view_info.format = m_depth_format;
		m_view_info.components.r = VK_COMPONENT_SWIZZLE_R;
		m_view_info.components.g = VK_COMPONENT_SWIZZLE_G;
		m_view_info.components.b = VK_COMPONENT_SWIZZLE_B;
		m_view_info.components.a = VK_COMPONENT_SWIZZLE_A;
		m_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		m_view_info.subresourceRange.baseMipLevel = 0;
		m_view_info.subresourceRange.levelCount = 1;
		m_view_info.subresourceRange.baseArrayLayer = 0;
		m_view_info.subresourceRange.layerCount = 1;
		m_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		m_view_info.flags = 0;
		if (m_depth_format == VK_FORMAT_D16_UNORM_S8_UINT ||
			m_depth_format == VK_FORMAT_D24_UNORM_S8_UINT ||
			m_depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
			m_view_info.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		// Create image view
		m_view_info.image = m_image;
		res = vkCreateImageView(m_device->get(), &m_view_info, nullptr, &m_view);
		if (res != VK_SUCCESS)
		{
			return false;
		}

		return true;
	}
};


class SwapchainResource
{
	std::shared_ptr<DeviceManager> m_device;
	VkSwapchainKHR m_swapchain = nullptr;

	struct swap_chain_buffer
	{
		VkImage image = nullptr;
		VkImageView view = nullptr;

		void destroy(VkDevice device)
		{
			vkDestroyImageView(device, view, nullptr);
		}
	};
	std::vector<std::unique_ptr<swap_chain_buffer>> m_buffers;

	VkSemaphoreCreateInfo m_imageAcquiredSemaphoreCreateInfo = {};
	VkSemaphore m_imageAcquiredSemaphore = nullptr;
	uint32_t m_current_buffer = 0;

	uint32_t m_present_queue_family_index = 0;
	VkQueue m_present_queue = nullptr;

public:
	SwapchainResource(const std::shared_ptr<DeviceManager> &device)
		: m_device(device)
	{
		m_imageAcquiredSemaphoreCreateInfo.sType =
			VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		m_imageAcquiredSemaphoreCreateInfo.pNext = nullptr;
		m_imageAcquiredSemaphoreCreateInfo.flags = 0;
	}

	~SwapchainResource()
	{
		vkDestroySemaphore(m_device->get(), m_imageAcquiredSemaphore, nullptr);
		for (auto &b : m_buffers)
		{
			b->destroy(m_device->get());
		}
		m_buffers.clear();
		vkDestroySwapchainKHR(m_device->get(), m_swapchain, nullptr);
	}

	uint32_t getImageCount()const { return m_buffers.size(); }
	VkImageView getView()const { return m_buffers[m_current_buffer]->view; }
	VkImage getImage()const { return m_buffers[m_current_buffer]->image; }
	VkSwapchainKHR getSwapchain()const { return m_swapchain; }
	VkSemaphore getSemaphore()const { return m_imageAcquiredSemaphore; }

	bool create(const std::shared_ptr<GpuManager> &gpu
		, const std::shared_ptr<SurfaceManager> &surface
		, int w, int h
		, VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
	{
		// swapchain
		if (!surface->getCapabilityFor(gpu->get())) {
			return false;
		}
		m_present_queue_family_index = gpu->get_present_queue_family_index();

		auto swapchainExtent = surface->getExtent(w, h);

		VkSwapchainCreateInfoKHR swapchain_ci = {};
		swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchain_ci.pNext = nullptr;
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
		swapchain_ci.pQueueFamilyIndices = nullptr;
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
		auto res = vkCreateSwapchainKHR(m_device->get(), &swapchain_ci, nullptr,
			&m_swapchain);
		if (res != VK_SUCCESS) {
			return false;
		}

		// semaphore
		res = vkCreateSemaphore(m_device->get(), &m_imageAcquiredSemaphoreCreateInfo,
			nullptr, &m_imageAcquiredSemaphore);
		if (res != VK_SUCCESS) {
			return false;
		}

		// present queue
		vkGetDeviceQueue(m_device->get(), m_present_queue_family_index, 0, &m_present_queue);

		return true;
	}

	bool prepareImages(VkFormat format)
	{
		uint32_t swapchainImageCount;
		auto res = vkGetSwapchainImagesKHR(m_device->get(), m_swapchain,
			&swapchainImageCount, nullptr);
		if (res != VK_SUCCESS) {
			return false;
		}
		if (swapchainImageCount == 0) {
			return false;
		}

		std::vector<VkImage> swapchainImages(swapchainImageCount);
		res = vkGetSwapchainImagesKHR(m_device->get(), m_swapchain,
			&swapchainImageCount, swapchainImages.data());
		if (res != VK_SUCCESS) {
			return false;
		}

		for (uint32_t i = 0; i < swapchainImageCount; i++)
		{
			VkImageViewCreateInfo color_image_view = {};
			color_image_view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			color_image_view.pNext = nullptr;
			color_image_view.format = format;
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
			res = vkCreateImageView(m_device->get(), &color_image_view, nullptr,
				&sc_buffer->view);
			if (res != VK_SUCCESS) {
				return false;
			}
			sc_buffer->image = swapchainImages[i];
			m_buffers.push_back(std::move(sc_buffer));
		}

		return true;
	}

	///
	/// get current buffer
	/// set image layout to current buffer
	///
	bool update()
	{
		// Get the index of the next available swapchain image:
		auto res = vkAcquireNextImageKHR(m_device->get()
			, m_swapchain
			, UINT64_MAX
			, m_imageAcquiredSemaphore
			, VK_NULL_HANDLE,
			&m_current_buffer);
		// TODO: Deal with the VK_SUBOPTIMAL_KHR and VK_ERROR_OUT_OF_DATE_KHR
		// return codes
		if (res != VK_SUCCESS) {
			return false;
		}

		return true;
	}

	// Now present the image in the window
	bool present()
	{
		VkPresentInfoKHR present;
		present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present.pNext = NULL;
		present.swapchainCount = 1;
		present.pSwapchains = &m_swapchain;
		present.pImageIndices = &m_current_buffer;
		present.pWaitSemaphores = NULL;
		present.waitSemaphoreCount = 0;
		present.pResults = NULL;

		auto res = vkQueuePresentKHR(m_present_queue, &present);
		if (res != VK_SUCCESS) {
			return false;
		}

		return true;
	}
};


class PipelineResource
{
	std::shared_ptr<DeviceManager> m_device;

	struct shader_stage
	{
		std::vector<unsigned int> spv;
		VkShaderModuleCreateInfo m_moduleCreateInfo;
	};
	std::vector<shader_stage> m_shaderInfos;
	VkPipelineShaderStageCreateInfo m_shaderStages[2];

	VkDescriptorSetLayoutBinding m_layout_bindings[1];
	VkDescriptorSetLayoutCreateInfo m_descriptor_layout = {};
	VkDescriptorSetLayout m_desc_layout = nullptr;

	const int NUM_DESCRIPTOR_SETS = 1;
	VkDescriptorSetAllocateInfo m_alloc_info[1];
	std::vector<VkDescriptorSet> m_desc_set;
	VkDescriptorPoolSize type_count[2];
	VkDescriptorPoolCreateInfo descriptor_pool = {};
	VkDescriptorPool m_desc_pool = nullptr;
	VkDescriptorBufferInfo m_uniform_buffer_info;

	VkPipelineLayoutCreateInfo m_pPipelineLayoutCreateInfo = {};
	VkPipelineLayout m_pipeline_layout = nullptr;

	VkPipelineCache m_pipelineCache = nullptr;
	VkPipelineCacheCreateInfo m_pipelineCacheInfo = {};

	VkDynamicState dynamicStateEnables[VK_DYNAMIC_STATE_RANGE_SIZE];
	VkPipelineDynamicStateCreateInfo dynamicState = {};
	VkPipelineVertexInputStateCreateInfo vi = {};
	VkPipelineInputAssemblyStateCreateInfo ia = {};
	VkPipelineRasterizationStateCreateInfo rs = {};
	VkPipelineColorBlendStateCreateInfo cb = {};
	VkPipelineColorBlendAttachmentState att_state[1];
	VkPipelineViewportStateCreateInfo vp = {};
	VkPipelineDepthStencilStateCreateInfo ds = {};
	VkPipelineMultisampleStateCreateInfo ms = {};
	VkGraphicsPipelineCreateInfo pipeline = {};
	VkPipeline m_pipeline = nullptr;

	/* Number of viewports and number of scissors have to be the same */
	/* at pipeline creation and in any call to set them dynamically   */
	/* They also have to be the same as each other                    */
	const int NUM_VIEWPORTS = 1;
	const int NUM_SCISSORS = NUM_VIEWPORTS;
	VkSampleCountFlagBits NUM_SAMPLES = VK_SAMPLE_COUNT_1_BIT;

public:
	PipelineResource(const std::shared_ptr<DeviceManager> &device)
		: m_device(device)
	{
		memset(dynamicStateEnables, 0, sizeof dynamicStateEnables);
	}

	~PipelineResource()
	{
		vkDestroyDescriptorPool(m_device->get(), m_desc_pool, nullptr);
		vkDestroyPipeline(m_device->get(), m_pipeline, nullptr);
		vkDestroyPipelineCache(m_device->get(), m_pipelineCache, nullptr);
		vkDestroyDescriptorSetLayout(m_device->get(), m_desc_layout, nullptr);
		vkDestroyPipelineLayout(m_device->get(), m_pipeline_layout, nullptr);
		for (auto &stage : m_shaderStages)
		{
			vkDestroyShaderModule(m_device->get(), stage.module, nullptr);
		}
	}

	VkPipeline getPipeline()const { return m_pipeline; }
	VkPipelineLayout getPipelineLayout()const { return m_pipeline_layout; }
	const VkDescriptorSet* getDescriptorSet()const { return m_desc_set.data(); }
	uint32_t getDescriptorSetCount()const { return m_desc_set.size(); }

	bool create(const std::vector<unsigned int> &vertSpv
		, const std::vector<unsigned int> &fragSpv
		, const VertexbufferDesc &vertexbuffer
		, const std::unique_ptr<FramebufferResource> &framebuffer
		, const std::unique_ptr<BufferResource> &uniformbuffer
	) 
	{
		// shader
		m_shaderInfos.resize(2);
		{
			auto &stage = m_shaderInfos[0];
			stage.spv = vertSpv;
			stage.m_moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			stage.m_moduleCreateInfo.pNext = nullptr;
			stage.m_moduleCreateInfo.flags = 0;
			stage.m_moduleCreateInfo.codeSize = stage.spv.size() * sizeof(unsigned int);
			stage.m_moduleCreateInfo.pCode = stage.spv.data();

			m_shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			m_shaderStages[0].pNext = nullptr;
			m_shaderStages[0].flags = 0;
			m_shaderStages[0].pSpecializationInfo = nullptr;
			m_shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
			m_shaderStages[0].pName = "main";
		}
		{
			auto &stage = m_shaderInfos[1];
			stage.spv = fragSpv;
			stage.m_moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			stage.m_moduleCreateInfo.pNext = nullptr;
			stage.m_moduleCreateInfo.flags = 0;
			stage.m_moduleCreateInfo.codeSize = stage.spv.size() * sizeof(unsigned int);
			stage.m_moduleCreateInfo.pCode = stage.spv.data();

			m_shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			m_shaderStages[1].pNext = nullptr;
			m_shaderStages[1].flags = 0;
			m_shaderStages[1].pSpecializationInfo = nullptr;
			m_shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			m_shaderStages[1].pName = "main";
		}

		// binding
		m_layout_bindings[0].binding = 0;
		m_layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		m_layout_bindings[0].descriptorCount = 1;
		m_layout_bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		m_layout_bindings[0].pImmutableSamplers = nullptr;
		// Next take layout bindings and use them to create a descriptor set layout
		m_descriptor_layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		m_descriptor_layout.pNext = nullptr;
		m_descriptor_layout.bindingCount = 1;
		m_descriptor_layout.pBindings = m_layout_bindings;

		// desc_pool
		type_count[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		type_count[0].descriptorCount = 1;

		descriptor_pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptor_pool.pNext = NULL;
		descriptor_pool.maxSets = 1;
		descriptor_pool.poolSizeCount = 1;
		descriptor_pool.pPoolSizes = type_count;

		// descriptor set
		m_alloc_info[0].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		m_alloc_info[0].pNext = NULL;
		m_alloc_info[0].descriptorSetCount = NUM_DESCRIPTOR_SETS;
		m_desc_set.resize(NUM_DESCRIPTOR_SETS);

		m_uniform_buffer_info = uniformbuffer->getDescInfo();

		// Now use the descriptor layout to create a pipeline layout
		m_pPipelineLayoutCreateInfo.sType =
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		m_pPipelineLayoutCreateInfo.pNext = nullptr;
		m_pPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
		m_pPipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
		m_pPipelineLayoutCreateInfo.setLayoutCount = 1;

		m_pipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		m_pipelineCacheInfo.pNext = nullptr;
		m_pipelineCacheInfo.initialDataSize = 0;
		m_pipelineCacheInfo.pInitialData = nullptr;
		m_pipelineCacheInfo.flags = 0;

		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.pNext = nullptr;
		dynamicState.pDynamicStates = dynamicStateEnables;
		dynamicState.dynamicStateCount = 0;

		vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vi.pNext = nullptr;
		vi.flags = 0;
		vi.vertexBindingDescriptionCount = 1;
		vi.pVertexBindingDescriptions = &vertexbuffer.getBindingDesc();
		vi.vertexAttributeDescriptionCount = 2;
		vi.pVertexAttributeDescriptions = vertexbuffer.getAttribs();

		ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		ia.pNext = nullptr;
		ia.flags = 0;
		ia.primitiveRestartEnable = VK_FALSE;
		ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rs.pNext = nullptr;
		rs.flags = 0;
		rs.polygonMode = VK_POLYGON_MODE_FILL;
		rs.cullMode = VK_CULL_MODE_BACK_BIT;
		rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rs.depthClampEnable = VK_TRUE;
		rs.rasterizerDiscardEnable = VK_FALSE;
		rs.depthBiasEnable = VK_FALSE;
		rs.depthBiasConstantFactor = 0;
		rs.depthBiasClamp = 0;
		rs.depthBiasSlopeFactor = 0;
		rs.lineWidth = 1.0f;

		att_state[0].colorWriteMask = 0xf;
		att_state[0].blendEnable = VK_FALSE;
		att_state[0].alphaBlendOp = VK_BLEND_OP_ADD;
		att_state[0].colorBlendOp = VK_BLEND_OP_ADD;
		att_state[0].srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		att_state[0].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		att_state[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		att_state[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

		cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		cb.flags = 0;
		cb.pNext = nullptr;
		cb.attachmentCount = 1;
		cb.pAttachments = att_state;
		cb.logicOpEnable = VK_FALSE;
		cb.logicOp = VK_LOGIC_OP_NO_OP;
		cb.blendConstants[0] = 1.0f;
		cb.blendConstants[1] = 1.0f;
		cb.blendConstants[2] = 1.0f;
		cb.blendConstants[3] = 1.0f;

		vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		vp.pNext = nullptr;
		vp.flags = 0;
		vp.viewportCount = NUM_VIEWPORTS;
		dynamicStateEnables[dynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT;
		vp.scissorCount = NUM_SCISSORS;
		dynamicStateEnables[dynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR;
		vp.pScissors = nullptr;
		vp.pViewports = nullptr;

		ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		ds.pNext = nullptr;
		ds.flags = 0;
		ds.depthTestEnable = VK_TRUE;
		ds.depthWriteEnable = VK_TRUE;
		ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		ds.depthBoundsTestEnable = VK_FALSE;
		ds.stencilTestEnable = VK_FALSE;
		ds.back.failOp = VK_STENCIL_OP_KEEP;
		ds.back.passOp = VK_STENCIL_OP_KEEP;
		ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
		ds.back.compareMask = 0;
		ds.back.reference = 0;
		ds.back.depthFailOp = VK_STENCIL_OP_KEEP;
		ds.back.writeMask = 0;
		ds.minDepthBounds = 0;
		ds.maxDepthBounds = 0;
		ds.stencilTestEnable = VK_FALSE;
		ds.front = ds.back;

		ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		ms.pNext = nullptr;
		ms.flags = 0;
		ms.pSampleMask = nullptr;
		ms.rasterizationSamples = NUM_SAMPLES;
		ms.sampleShadingEnable = VK_FALSE;
		ms.alphaToCoverageEnable = VK_FALSE;
		ms.alphaToOneEnable = VK_FALSE;
		ms.minSampleShading = 0.0;

		pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline.pNext = nullptr;
		pipeline.basePipelineHandle = VK_NULL_HANDLE;
		pipeline.basePipelineIndex = 0;
		pipeline.flags = 0;
		pipeline.pVertexInputState = &vi;
		pipeline.pInputAssemblyState = &ia;
		pipeline.pRasterizationState = &rs;
		pipeline.pColorBlendState = &cb;
		pipeline.pTessellationState = nullptr;
		pipeline.pMultisampleState = &ms;
		pipeline.pDynamicState = &dynamicState;
		pipeline.pViewportState = &vp;
		pipeline.pDepthStencilState = &ds;

		pipeline.stageCount = 2;
		pipeline.pStages = m_shaderStages;

		pipeline.renderPass = framebuffer->getRenderPass();
		pipeline.subpass = 0;

		// shader
		for (size_t i = 0; i<m_shaderInfos.size(); ++i)
		{
			auto res = vkCreateShaderModule(m_device->get(), &m_shaderInfos[i].m_moduleCreateInfo, nullptr,
				&m_shaderStages[i].module);
			if (res != VK_SUCCESS) {
				return false;
			}
		}

		// descriptor
		auto res = vkCreateDescriptorSetLayout(m_device->get(), &m_descriptor_layout, nullptr,
			&m_desc_layout);
		if (res != VK_SUCCESS) {
			return false;
		}

		res = vkCreateDescriptorPool(m_device->get(), &descriptor_pool, NULL,
			&m_desc_pool);
		assert(res == VK_SUCCESS);

		m_alloc_info[0].descriptorPool = m_desc_pool;
		m_alloc_info[0].pSetLayouts = &m_desc_layout;
		res = vkAllocateDescriptorSets(m_device->get(), m_alloc_info, m_desc_set.data());
		if (res != VK_SUCCESS)
		{
			return false;
		}

		m_pPipelineLayoutCreateInfo.pSetLayouts = &m_desc_layout;
		res = vkCreatePipelineLayout(m_device->get(), &m_pPipelineLayoutCreateInfo, nullptr,
			&m_pipeline_layout);
		if (res != VK_SUCCESS)
		{
			return false;
		}
		pipeline.layout = m_pipeline_layout;

		VkWriteDescriptorSet writes[1];
		writes[0] = {};
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].pNext = NULL;
		writes[0].dstSet = m_desc_set[0];
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].pBufferInfo = &m_uniform_buffer_info;
		writes[0].dstArrayElement = 0;
		writes[0].dstBinding = 0;
		vkUpdateDescriptorSets(m_device->get(), 1, writes, 0, NULL);

		// pipeline cache
		res = vkCreatePipelineCache(m_device->get(), &m_pipelineCacheInfo, nullptr,
			&m_pipelineCache);
		if (res != VK_SUCCESS)
		{
			return false;
		}

		res = vkCreateGraphicsPipelines(m_device->get(), m_pipelineCache, 1,
			&pipeline, nullptr, &m_pipeline);
		if (res != VK_SUCCESS)
		{
			return false;
		}

		return true;
	}
};


class CommandBuffer: IDeviceResource
{
	VkCommandPoolCreateInfo m_cmd_pool_info = {};
	VkCommandPool m_cmd_pool;
	VkCommandBufferAllocateInfo m_cmdInfo = {};
	VkCommandBuffer m_cmd_bufs[1];
	VkClearValue m_clear_values[2];

	uint32_t m_graphics_queue_family_index = 0;
	VkQueue m_graphics_queue;

	// Amount of time, in nanoseconds, to wait for a command buffer to complete
	const int FENCE_TIMEOUT = 100000000;

	/* Number of viewports and number of scissors have to be the same */
	/* at pipeline creation and in any call to set them dynamically   */
	/* They also have to be the same as each other                    */
	const int NUM_VIEWPORTS=1;
	const int NUM_SCISSORS = NUM_VIEWPORTS;

public:
	CommandBuffer()
	{
		m_clear_values[0].color.float32[0] = 0.2f;
		m_clear_values[0].color.float32[1] = 0.2f;
		m_clear_values[0].color.float32[2] = 0.2f;
		m_clear_values[0].color.float32[3] = 0.2f;
		m_clear_values[1].depthStencil.depth = 1.0f;
		m_clear_values[1].depthStencil.stencil = 0;
	}

	void onDestroy(VkDevice device)
	{
		vkFreeCommandBuffers(device, m_cmd_pool, _countof(m_cmd_bufs), m_cmd_bufs);
		vkDestroyCommandPool(device, m_cmd_pool, nullptr);
	}

	bool onCreate(VkDevice device)
	{
		auto res =
			vkCreateCommandPool(device, &m_cmd_pool_info, nullptr, &m_cmd_pool);
		if (res != VK_SUCCESS) {
			return false;
		}

		m_cmdInfo.commandPool = m_cmd_pool;
		res = vkAllocateCommandBuffers(device, &m_cmdInfo, m_cmd_bufs);
		if (res != VK_SUCCESS) {
			return false;
		}

		vkGetDeviceQueue(device, m_graphics_queue_family_index, 0, &m_graphics_queue);

		return true;
	}

	bool configure(int graphics_queue_family_index) 
	{
		m_cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		m_cmd_pool_info.pNext = nullptr;
		m_cmd_pool_info.queueFamilyIndex = graphics_queue_family_index;
		m_cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		m_cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		m_cmdInfo.pNext = nullptr;
		m_cmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		m_cmdInfo.commandBufferCount = _countof(m_cmd_bufs);

		return true;
	}

	bool begin() 
	{
		VkCommandBufferBeginInfo cmd_buf_info = {};
		cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmd_buf_info.pNext = nullptr;
		cmd_buf_info.flags = 0;
		cmd_buf_info.pInheritanceInfo = nullptr;

		auto res = vkBeginCommandBuffer(m_cmd_bufs[0], &cmd_buf_info);
		if (res != VK_SUCCESS) {
			return false;
		}

		return true;
	}

	bool end() 
	{
		auto res = vkEndCommandBuffer(m_cmd_bufs[0]);
		if (res != VK_SUCCESS)
		{
			return false;
		}
		return true;
	}

	bool submit(VkDevice device, VkSemaphore semaphore)
	{
		VkFenceCreateInfo fenceInfo;
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.pNext = nullptr;
		fenceInfo.flags = 0;
		VkFence drawFence;
		auto res=vkCreateFence(device, &fenceInfo, nullptr, &drawFence);
		if (res != VK_SUCCESS) {
			return false;
		}

		VkPipelineStageFlags pipe_stage_flags =
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSubmitInfo submit_info[1] = {};
		submit_info[0].pNext = nullptr;
		submit_info[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		//submit_info[0].waitSemaphoreCount = 0;
		//submit_info[0].pWaitSemaphores = nullptr;
		submit_info[0].waitSemaphoreCount = 1;
		submit_info[0].pWaitSemaphores = &semaphore;
		submit_info[0].pWaitDstStageMask = &pipe_stage_flags;
		submit_info[0].commandBufferCount = _countof(m_cmd_bufs);
		submit_info[0].pCommandBuffers = m_cmd_bufs;
		submit_info[0].signalSemaphoreCount = 0;
		submit_info[0].pSignalSemaphores = nullptr;

		res = vkQueueSubmit(m_graphics_queue, 1, submit_info, drawFence);
		if (res != VK_SUCCESS) {
			return false;
		}

		do {
			res =
				vkWaitForFences(device, 1, &drawFence, VK_TRUE, FENCE_TIMEOUT);
		} while (res == VK_TIMEOUT);
		if (res != VK_SUCCESS) {
			return false;
		}

		vkDestroyFence(device, drawFence, nullptr);

		return true;
	}

	void beginRenderPass(VkRenderPass render_pass, VkFramebuffer framebuffer
	, VkRect2D rect)
	{
		VkRenderPassBeginInfo rp_begin;
		rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rp_begin.pNext = NULL;
		rp_begin.renderPass = render_pass;
		rp_begin.framebuffer = framebuffer;
		rp_begin.renderArea = rect;
		rp_begin.clearValueCount = _countof(m_clear_values);
		rp_begin.pClearValues = m_clear_values;
		vkCmdBeginRenderPass(m_cmd_bufs[0], &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
	}

	void endRenderPass()
	{
		vkCmdEndRenderPass(m_cmd_bufs[0]);
	}

	void bindPipeline(VkPipeline pipeline, VkPipelineLayout pipeline_layout
		, const VkDescriptorSet *pDesc, uint32_t descCount)
	{
		vkCmdBindPipeline(m_cmd_bufs[0], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vkCmdBindDescriptorSets(m_cmd_bufs[0], VK_PIPELINE_BIND_POINT_GRAPHICS
			, pipeline_layout, 0
			, descCount, pDesc
			, 0, NULL
			);
	}

	void bindVertexbuffer(VkBuffer buf)
	{
		const VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(m_cmd_bufs[0], 0
			, 1, &buf, offsets);	
	}

	void initViewports(int width, int height) 
	{
		VkViewport viewport;
		viewport.height = (float)height;
		viewport.width = (float)width;
		viewport.minDepth = (float)0.0f;
		viewport.maxDepth = (float)1.0f;
		viewport.x = 0;
		viewport.y = 0;
		vkCmdSetViewport(m_cmd_bufs[0], 0, NUM_VIEWPORTS, &viewport);

		VkRect2D scissor;
		scissor.extent.width = width;
		scissor.extent.height = height;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		vkCmdSetScissor(m_cmd_bufs[0], 0, NUM_SCISSORS, &scissor);
	}

	void draw()
	{
		vkCmdDraw(m_cmd_bufs[0], 12 * 3, 1, 0, 0);
	}

	void setImageLayout(
		VkImage image,
		VkImageAspectFlags aspectMask,
		VkImageLayout old_image_layout,
		VkImageLayout new_image_layout)
	{
		/* DEPENDS on info.cmd and info.queue initialized */

		//assert(info.cmd != VK_NULL_HANDLE);
		//assert(info.graphics_queue != VK_NULL_HANDLE);

		VkImageMemoryBarrier image_memory_barrier = {};
		image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		image_memory_barrier.pNext = nullptr;
		image_memory_barrier.srcAccessMask = 0;
		image_memory_barrier.dstAccessMask = 0;
		image_memory_barrier.oldLayout = old_image_layout;
		image_memory_barrier.newLayout = new_image_layout;
		image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		image_memory_barrier.image = image;
		image_memory_barrier.subresourceRange.aspectMask = aspectMask;
		image_memory_barrier.subresourceRange.baseMipLevel = 0;
		image_memory_barrier.subresourceRange.levelCount = 1;
		image_memory_barrier.subresourceRange.baseArrayLayer = 0;
		image_memory_barrier.subresourceRange.layerCount = 1;

		if (old_image_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
			image_memory_barrier.srcAccessMask =
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}

		if (new_image_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		}

		if (new_image_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
			image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		}

		if (old_image_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		}

		if (old_image_layout == VK_IMAGE_LAYOUT_PREINITIALIZED) {
			image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		}

		if (new_image_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		}

		if (new_image_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
			image_memory_barrier.dstAccessMask =
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}

		if (new_image_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			image_memory_barrier.dstAccessMask =
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}

		VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags dest_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		vkCmdPipelineBarrier(m_cmd_bufs[0], src_stages, dest_stages, 0, 0, nullptr, 0, nullptr,
			1, &image_memory_barrier);
	}
};
typedef DeviceResource<CommandBuffer> CommandBufferResource;


#include "cube_vertices.h"


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
	auto instance=InstanceManager::create("glfwvulkan", "vulkan engine");
	if (!instance) {
		return 3;
	}

	// gpu
	auto gpus = GpuManager::enumerate_gpu(instance->get());
    if(gpus.empty()){
        return 4;
    }
	auto gpu = gpus.front();

	// surface
	auto surface = std::make_shared<SurfaceManager>(instance);
	if (!surface->create(hInstance, glfw.getWindow()))
	{
		return 6;
	}
	if (!gpu->prepare(surface->get())) {
		return 6;
	}

	// device
	auto device = std::make_shared<DeviceManager>();
	if(!device->create(gpu)){
        return 7;
    }

	auto swapchain = std::make_unique<SwapchainResource>(device);
	if (!swapchain->create(gpu, surface, w, h)) {
		return 8;
	}
	if (!swapchain->prepareImages(gpu->getPrimaryFormat())) {
		return 8;
	}

	auto depth = std::make_unique<DepthbufferResource>(device);
	if (!depth->create(gpu, w, h)) {
		return 9;
	}

	auto framebuffer = std::make_unique<FramebufferResource>(device);
	{
		auto imageSamples = depth->getSamples();
		framebuffer->attachColor(swapchain->getView(), gpu->getPrimaryFormat(), depth->getSamples());
	}
	{
		auto depthView = depth->getView();
		auto depthSamples = depth->getSamples();
		auto depthFormat = depth->getFormat();
		framebuffer->attachDepth(depthView, depthFormat, depthSamples);
	}
	framebuffer->pushSubpass(0, 1);
	if (!framebuffer->create(w, h))
	{
		return 10;
	}

	VertexbufferDesc vertex_desc;
	vertex_desc.pushAttrib();
	vertex_desc.pushAttrib();

	auto vertex_buffer = std::make_unique<BufferResource>(device);
	if (!vertex_buffer->create(gpu, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(g_vb_solid_face_colors_Data))) {
		return 11;
	}
	{
		auto callback = [vertexData = g_vb_solid_face_colors_Data](uint8_t *pData, uint32_t size) {
			memcpy(pData, vertexData, size);
		};
		if (!vertex_buffer->map(callback)) {
			return 11;
		}
	}

	auto uniform_buffer = std::make_unique<BufferResource>(device);
	if (!uniform_buffer->create(gpu, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(glm::mat4))) {
		return 12;
	}
	{
		auto m = calcMVP(w, h);
		auto callback = [m](uint8_t *p, uint32_t size) {
			memcpy(p, &m, size);
		};
		if (!uniform_buffer->map(callback)) {
			return 12;
		}
	}

	auto pipeline = std::make_unique<PipelineResource>(device);
	if (!pipeline->create(read("../15-draw_cube.vert.spv"), read("../15-draw_cube.frag.spv")
		, vertex_desc, framebuffer, uniform_buffer))
	{
		return 13;
	}

	auto cmd = std::make_unique<CommandBufferResource>();
	if (!cmd->resource().configure(gpu->get_graphics_queue_family_index())) {
		return 14;
	}
	if (!cmd->create(device->get())) {
		return 14;
	}

	while (glfw.runLoop())
	{
		swapchain->update();

		// start
		if (cmd->resource().begin())
		{
			// Set the image layout to depth stencil optimal
			cmd->resource().setImageLayout(
				depth->getImage()
				, depth->getAspect() //m_view_info.subresourceRange.aspectMask
				, VK_IMAGE_LAYOUT_UNDEFINED
				, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
			);

			cmd->resource().setImageLayout(
				swapchain->getImage()
				, VK_IMAGE_ASPECT_COLOR_BIT
				, VK_IMAGE_LAYOUT_UNDEFINED
				, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			);

			VkRect2D rect = {
				0, 0, w, h
			};
			cmd->resource().beginRenderPass(
				framebuffer->getRenderPass()
				, framebuffer->getFramebuffer()
				, rect);
			{
				cmd->resource().bindPipeline(
					pipeline->getPipeline(), pipeline->getPipelineLayout()
					, pipeline->getDescriptorSet(), pipeline->getDescriptorSetCount()
				);

				cmd->resource().bindVertexbuffer(
					vertex_buffer->getBuffer());

				cmd->resource().initViewports(w, h);

				cmd->resource().draw();
			}
			cmd->resource().endRenderPass();

			if (!cmd->resource().end()) {
				return 16;
			}
			if (!cmd->resource().submit(device->get(), swapchain->getSemaphore())) {
				return 17;
			}

			swapchain->present();
		}
	}

	return 0;
}
