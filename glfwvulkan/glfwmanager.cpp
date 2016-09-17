#include "glfwmanager.h"


GlfwManager::GlfwManager()
{
}

GlfwManager::~GlfwManager()
{
    glfwTerminate();
}

bool GlfwManager::initilize() {
    if (!glfwInit()) {
        return false;
    }

    if (!glfwVulkanSupported())
    {
        return false;
    }

    return true;
}

bool GlfwManager::createWindow(int w, int h
    , const char *title)
{
    if (m_window) {
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(w, h
        , title, nullptr, nullptr);
    if (!m_window) {
        return false;
    }
    //glfwMakeContextCurrent(m_window);
    return true;
}

HWND GlfwManager::getWindow()const
{
    return glfwGetWin32Window(m_window);
}

bool GlfwManager::runLoop()
{
    if (glfwWindowShouldClose(m_window)) {
        return false;
    }
    glfwPollEvents();
    return true;
}

/*
void GlfwManager::swapBuffer()
{
    glfwSwapBuffers(m_window);
}
*/

