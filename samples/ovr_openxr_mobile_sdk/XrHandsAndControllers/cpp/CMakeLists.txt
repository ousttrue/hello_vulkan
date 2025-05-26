add_executable(xrhandsandcontrollers main.cpp ActionSetDisplayPanel.cpp
                                     EnvironmentRenderer.cpp SkyboxRenderer.cpp)
target_link_libraries(xrhandsandcontrollers samplexrframework)
