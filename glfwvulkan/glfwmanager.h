#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>


class GlfwManager
{
	GLFWwindow *m_window = nullptr;

public:
	GlfwManager();
	~GlfwManager();

	bool initilize();

	bool createWindow(int w, int h
		, const char *title);
	
	HWND getWindow()const;

	bool runLoop();

	//void swapBuffer();
};
