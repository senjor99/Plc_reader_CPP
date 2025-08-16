#pragma once

#include "managers.cpp"
#include <external/imgui/imgui.h>
#include <external/imgui/backends/imgui_impl_glfw.h>
#include <external/imgui/backends/imgui_impl_opengl3.h>

class ConnectionBar;
class Body;
class FilterBar;

class MainGUIController {
public:
    MainGUIController();

    void draw();
    void pick_db(std::string k_in);
    void activate_filter(bool any_selected);

    std::unique_ptr<ConnectionBar> upper_bar ;
    std::unique_ptr<Body> body ;
    std::unique_ptr<FilterBar> _FilterBar;
    std::unique_ptr<CommManager> CommMan;
};

class ConnectionBar{
protected:
    MainGUIController* this_controller;
    std::string device_combo_name ="--None--";
    std::string db_combo_name = "--None--";

public:
    ConnectionBar() = default;
    ConnectionBar(MainGUIController* controller);

    std::string get_device_combo_name();
    std::string get_db_combo_name();

    void set_device_combo_name(std::string in);
    void set_db_combo_name(std::string in);
    
    void draw();
    void add_db(){}
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

        

        void set_udt_keys(std::vector<std::string> k_in);
        void activate();
        Filter::filterElem get_filter_status() const;
        void draw();

};

class Body {
public:
    Body() = default;
    void draw_node(const VariantElement& element,int&depth_in);
    void draw(const std::shared_ptr<DB>& db);
    
private:
    std::string current_filter;
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