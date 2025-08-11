
#include "opc_connection.hpp"
#include <GLFW/glfw3.h>
#include <type_traits>

namespace pt = tao::pegtl;


void draw_variant_element(const VariantElement& elem) {
    std::visit([](auto&& ptr) {
        using T = std::decay_t<decltype(ptr)>;
       
        std::string name = ptr->get_name() + " " + ptr->get_type();
    
        if constexpr (
                std::is_same_v<T, std::shared_ptr<UDT_ARRAY>> ||
                std::is_same_v<T, std::shared_ptr<UDT_SINGLE>> ||
                std::is_same_v<T, std::shared_ptr<STD_ARRAY>>  ||
                std::is_same_v<T, std::shared_ptr<UDT_ARR_ELEM>> ||
                std::is_same_v<T, std::shared_ptr<STRUCT_SINGLE>>
            ) {
            if (ImGui::TreeNode(name.c_str())) {
                for (const auto& sub : ptr->get_childs()) {
                    draw_variant_element(sub);
                }
                ImGui::TreePop();
            }
        } 
        else if constexpr (std::is_same_v<T, std::shared_ptr<STD_SINGLE>>||
                        std::is_same_v<T, std::shared_ptr<STD_ARR_ELEM>>){
            auto [byte, bit] = ptr->get_offset();
            std::string label = name + "[" +
                            std::to_string(byte) + "." +
                            std::to_string(bit) + "]";
            ImGui::BulletText("%s", label.c_str());
        }

    }, elem);
}
void draw_structure(const std::shared_ptr<DB>& container) {
    ImGui::Begin("DB Visualizer");

    if (ImGui::TreeNode(container->get_name().c_str())) {
        for (const auto& child : container->get_childs()) {
            draw_variant_element(child);
        }
        ImGui::TreePop();
    }

    ImGui::End();
}


int main() {
 
    if (!glfwInit()) return -1;

    GLFWwindow* window = glfwCreateWindow(1280, 720, "PLC-Reader", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    MainGUIController main;


    while (!glfwWindowShouldClose(window)) {

        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("MainWindow", nullptr,window_type::blank_);
        main.draw();
        ImGui::End();
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
