#pragma once

#include <managers.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>


namespace Filter
{
    struct filterElem;
}
namespace translate{    
    
    Value parse_type(std::string& input);
};

class MainGUIController;

class DrawingInfo
{
    public:
        DrawingInfo()=default;
        DrawingInfo(ImVec2 StartCursor,ImVec2 StartPortView)
            :initial_work_pos(StartCursor),Initial_PortView(StartPortView){};
        
        const ImVec2 initial_work_pos  ;
        const ImVec2 Initial_PortView ;
        
        ImVec2 PortView ;
        ImVec2 Cursor;
};

class ConnectionBar{
protected:
    MainGUIController* this_controller;
    std::string device_combo_name ="Select Device";
    std::string card_combo_name = "Select Adapter";
public:
    ConnectionBar()=default ;
    ConnectionBar(MainGUIController* controller);

    std::string get_device_combo_name();

    void set_device_combo_name(std::string in);
    void Draw();
    void DrawDeviceCombo();
    void DrawDbNr();
    void DrawNetCardCombo();
    void add_db();
};

class FilterBar {
    protected:
        
        bool active = false;
        Mode mode = Mode::None;
    
        std::array<char,128> value_buf{};  
        std::array<char,128> name_buf{};   
        std::vector<std::string> udt_keys;
    
        Filter::filterElem f_el;
        MainGUIController* this_controller;
    
        // helper
        static const char* mode_label(Mode m);

    public:
        FilterBar() = default;    
        FilterBar(MainGUIController* controller);

        void activate();
        Filter::filterElem get_filter_status() const;
        void draw();

};

class Body {
public:
    Body()=default;
    Body(MainGUIController* main);
    void Draw_node(const VariantElement& element,int&depth_in);
    void Draw_Explorer();
    void Draw_DirectoryTree(const FileFolderVar& el);
    void Draw(const std::shared_ptr<DB>& db);
private:
    std::string current_filter;
    MainGUIController* this_controller;
};

class MainGUIController {
public:
    MainGUIController();

    void draw();
    void activate_filter();

    std::unique_ptr<DrawingInfo> cursor;
    std::unique_ptr<ConnectionBar> upper_bar ;
    std::unique_ptr<Body> body ;
    std::unique_ptr<FilterBar> _FilterBar;
    std::unique_ptr<CommManager> CommMan;
};

enum window_type {
    blank_=
    (
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus
    ),
    blank=  
    (
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings 
    ),

    blank_no_title = 
    (
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse|
        ImGuiWindowFlags_NoSavedSettings 
    ),

    blank_title = 
        (
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings
    ),
    blank_scroll=  
    (
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings
    )
};