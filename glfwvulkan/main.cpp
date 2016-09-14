#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include <memory>


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


class GpuManager
{
	VkPhysicalDevice m_gpu;
	std::vector<VkQueueFamilyProperties> m_queue_props;
	VkPhysicalDeviceMemoryProperties m_memory_properties;
	VkPhysicalDeviceProperties m_gpu_props;

public:
	GpuManager(VkPhysicalDevice gpu)
		: m_gpu(gpu)
	{
	}

	VkPhysicalDevice get()const { return m_gpu; }

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

		/* This is as good a place as any to do this */
		vkGetPhysicalDeviceMemoryProperties(m_gpu, &m_memory_properties);
		vkGetPhysicalDeviceProperties(m_gpu, &m_gpu_props);
		return true;
	}

    int get_graphics_queue_family_index()const
    {
        for (size_t i = 0; i < m_queue_props.size(); i++) {
            if (m_queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                return i;
            }
        }
        return -1;
    }
};


class DeviceManager
{
	VkDevice m_device = nullptr;
	std::vector<const char*> m_device_extension_names;

public:
	DeviceManager()
	{
	}

	~DeviceManager()
	{
		vkDestroyDevice(m_device, NULL);
	}

	bool create(const std::shared_ptr<GpuManager> &gpu)
	{
        int graphics_queue_family_index=gpu->get_graphics_queue_family_index();
        if(graphics_queue_family_index<0){
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

public:
	InstanceManager(const std::string &app_name
		, const std::string &engine_name)
		: m_app_short_name(app_name), m_engine_name(engine_name)
		, m_device(new DeviceManager)
	{
	}

	~InstanceManager()
	{
		m_device.reset();
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

#if 0
        vkGetPhysicalDeviceQueueFamilyProperties(info.gpus[0],
                                                 &info.queue_family_count, NULL);
        assert(info.queue_family_count >= 1);

        info.queue_props.resize(info.queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(
            info.gpus[0], &info.queue_family_count, info.queue_props.data());
        assert(info.queue_family_count >= 1);

        /* This is as good a place as any to do this */
        vkGetPhysicalDeviceMemoryProperties(info.gpus[0], &info.memory_properties);
        vkGetPhysicalDeviceProperties(info.gpus[0], &info.gpu_props);

        return res;
    }
#endif

	bool createDevice(size_t gpuIndex)
	{
        if(gpuIndex>=m_gpus.size()){
            return false;
        }
		return m_device->create(m_gpus[gpuIndex]);
	}
};


int main(void)
{
	GlfwManager glfw;
	if (!glfw.initilize()) {
		return 1;
	}

	if (!glfw.createWindow(640, 480, "Hello vulkan")) {
		return 2;
	}

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

	while (glfw.runLoop())
	{
		/* Render here */
		//glClear(GL_COLOR_BUFFER_BIT);

		glfw.swapBuffer();
	}

	return 0;
}

