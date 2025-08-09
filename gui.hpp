#pragma once

#include "managers.hpp"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

class ConnectionBar;
class Body;
class FilterBar;

class MainGUIController {
public:
    MainGUIController() 
    :   upper_bar(std::make_unique<ConnectionBar>(this)),
        body(std::make_unique<Body>()),
        CommMan(std::make_unique<CommManager>()),
        _FilterBar(std::make_unique<FilterBar>(this)){}

    void draw();
    void pick_db(std::string k_in);
    void activate_filter(bool& in);

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
    std::string get_device_combo_name(){ return device_combo_name;}
    std::string get_db_combo_name(){ return db_combo_name;}

    void set_device_combo_name(std::string in){ device_combo_name = in;}
    void set_db_combo_name(std::string in){db_combo_name = in;}
    
    ConnectionBar() = default;
    ConnectionBar(MainGUIController* controller): this_controller(controller){}
    void draw();
    void add_db(){}
};

class FilterBar {
public:
    void set_udt_keys(std::vector<std::string> k_in){ udt_keys = k_in ;}
    void draw();
    void activate(){active = !active;}
    FilterBar(MainGUIController* controller): this_controller(controller){}
    Filter::filterElem get_filter_status(){return f_el;}
private:
    bool active = false;
    std::string ty_filter_scope = "--None--";
    int filter_scope;
    std::string udt_scope;
    std::vector<std::string> filter_modes = {"--None--","Value","Name","Value-Name","Value-UDT"};
    std::vector<std::string> udt_keys;
    Filter::filterElem f_el;
    MainGUIController* this_controller;

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