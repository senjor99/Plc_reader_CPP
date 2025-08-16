
#include <gui.cpp>
#include <external/GLFW/glfw3.h>
#include <type_traits>

namespace pt = tao::pegtl;

void SetModernStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Colori principali - palette flat
    colors[ImGuiCol_WindowBg]        = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);  // background scuro
    colors[ImGuiCol_Header]          = ImVec4(0.20f, 0.22f, 0.25f, 1.0f);
    colors[ImGuiCol_HeaderHovered]   = ImVec4(0.28f, 0.30f, 0.33f, 1.0f);
    colors[ImGuiCol_HeaderActive]    = ImVec4(0.25f, 0.27f, 0.30f, 1.0f);

    colors[ImGuiCol_Button]          = ImVec4(0.18f, 0.50f, 0.85f, 1.0f);  // blu flat
    colors[ImGuiCol_ButtonHovered]   = ImVec4(0.20f, 0.55f, 0.90f, 1.0f);
    colors[ImGuiCol_ButtonActive]    = ImVec4(0.15f, 0.45f, 0.80f, 1.0f);

    colors[ImGuiCol_FrameBg]         = ImVec4(0.16f, 0.17f, 0.20f, 1.0f);
    colors[ImGuiCol_FrameBgHovered]  = ImVec4(0.20f, 0.22f, 0.25f, 1.0f);
    colors[ImGuiCol_FrameBgActive]   = ImVec4(0.25f, 0.27f, 0.30f, 1.0f);

    colors[ImGuiCol_Text]            = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
    colors[ImGuiCol_Border]          = ImVec4(0.18f, 0.18f, 0.20f, 1.0f);

    // Stile generale (flat, spazi moderni)
    style.WindowRounding    = 6.0f;   // angoli stondati
    style.FrameRounding     = 4.0f;
    style.GrabRounding      = 4.0f;
    style.ScrollbarRounding = 6.0f;

    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;

    style.ItemSpacing       = ImVec2(8, 6);
    style.ItemInnerSpacing  = ImVec2(6, 4);

    style.WindowPadding     = ImVec2(12, 10);
}

int main() {
 
    if (!glfwInit()) return -1;

    GLFWwindow* window = glfwCreateWindow(1280, 720, "PLC-Reader", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    SetModernStyle();

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
