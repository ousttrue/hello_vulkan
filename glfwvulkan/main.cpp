#include <Windows.h>
#include <GLFW/glfw3.h>
#include <string>


class GlfwManager
{
     GLFWwindow *m_window=nullptr;

public:
    GlfwManager()
    {
    }

    ~GlfwManager()
    {
        glfwTerminate();
    }

    bool initilize(){
        return glfwInit()!=0;
    }

    bool createWindow(int w, int h, const std::string &title)
    {
        if(m_window){
            return false;
        }

        m_window = glfwCreateWindow(w, h
                , title.c_str(), NULL, NULL);
        if(!m_window){
            return false;
        }
        glfwMakeContextCurrent(m_window);
        return true;
    }

    bool runLoop()
    {
        if(glfwWindowShouldClose(m_window)){
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


int main(void)
{
    GlfwManager glfw;
    if(!glfw.initilize()){
        return 1;
    }

    if(!glfw.createWindow(640, 480, "Hello vulkan")){
        return 2;
    }

    while(glfw.runLoop())
    {
        /* Render here */
        //glClear(GL_COLOR_BUFFER_BIT);

        glfw.swapBuffer();
    }

    return 0;
}

