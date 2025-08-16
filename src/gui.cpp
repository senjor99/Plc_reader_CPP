#pragma once

#include <gui.hpp>

// class MainGuiController
/*

*/
MainGUIController::MainGUIController()
    :   upper_bar(std::make_unique<ConnectionBar>(this)),
        body(std::make_unique<Body>()),
        CommMan(std::make_unique<CommManager>()),
        _FilterBar(std::make_unique<FilterBar>(this)){}


void MainGUIController::pick_db(std::string k_in)
{
    CommMan->set_database_scope(k_in);
    for(auto i :CommMan->get_udt_keys() )

    _FilterBar->set_udt_keys(CommMan->get_udt_keys());
};

void MainGUIController::activate_filter(bool any_selected)
{
    if(any_selected)
    {  
        CommMan->set_filter_mode(_FilterBar->get_filter_status());
    }
    else 
    {
        CommMan->reset_filter_mode();
    }
};

void MainGUIController::draw()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 work_pos  = viewport->WorkPos;
    const ImVec2 work_size = viewport->WorkSize;

    const float margin = 20.0f;                    
    const float x      = work_pos.x + margin;
    float       y      = work_pos.y + margin;
    const float width  = work_size.x - margin * 2.0f;

    const float header_h = 40.0f;
    const float filter_h = 40.0f;

    // Header
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, header_h));
    ImGui::Begin("HeaderWindow", nullptr, window_type::blank);
    upper_bar->draw();
    ImGui::End();
    y += header_h + ImGui::GetStyle().ItemSpacing.y; 
    if (CommMan->get_database() != nullptr) {
        // Filter bar
        ImGui::SetNextWindowPos(ImVec2(x, y));
        ImGui::SetNextWindowSize(ImVec2(width, filter_h));
        ImGui::Begin("FilterBar", nullptr, window_type::blank);
        _FilterBar->draw();
        ImGui::End();
        y += filter_h + ImGui::GetStyle().ItemSpacing.y;

        float remaining_h = (work_pos.y + work_size.y) - y - margin;
        if (remaining_h < 0.0f) remaining_h = 0.0f;   

        ImGui::SetNextWindowPos(ImVec2(x, y));
        ImGui::SetNextWindowSize(ImVec2(width, remaining_h));
        ImGui::Begin("BodyWindow", nullptr, window_type::blank);
        body->draw(CommMan->get_database());
        ImGui::End();
    }
};

// class MainGuiController

// class ConnectionBar
/*

*/
ConnectionBar::ConnectionBar(MainGUIController* controller)
    : this_controller(controller){}

std::string ConnectionBar::get_device_combo_name(){ return device_combo_name;}
std::string ConnectionBar::get_db_combo_name(){ return db_combo_name;}

void ConnectionBar::set_device_combo_name(std::string in){ device_combo_name = in;}
void ConnectionBar::set_db_combo_name(std::string in){db_combo_name = in;}

void ConnectionBar::draw() {
    std::vector<std::string> device_keys =this_controller->CommMan->get_devices_keys();
    std::shared_ptr<DeviceInfo> device_scope = this_controller->CommMan->get_device_scope();
    std::map<std::string,std::shared_ptr<DeviceInfo>> device_map = this_controller->CommMan->get_devices_map();
    ImGui::SetNextItemWidth(200);
    if (ImGui::BeginCombo("Device", device_combo_name.c_str())) {
        if(!device_keys.empty())   
        { 
            for (int i = 0; i < device_keys.size(); ++i) {
                bool is_selected = (device_scope != nullptr && device_scope == device_map[device_keys[i]]);
                if (ImGui::Selectable(device_keys[i].c_str(), is_selected)) {
                    this_controller->CommMan->set_device_scope(device_keys[i]);
                    device_combo_name = device_keys[i];
                    ImGui::SetItemDefaultFocus();
                }   
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    std::vector<std::string> db_keys =this_controller->CommMan->get_databases_keys();
    DbInfo db_scope = this_controller->CommMan->get_database_scope();
    ImGui::SetNextItemWidth(200);
    if (ImGui::BeginCombo("DB Struct", db_combo_name.c_str())) {
        for (int i = 0; i < db_keys.size(); ++i) {
            bool is_selected = (db_keys[i] == db_scope.name);
            if (ImGui::Selectable(db_keys[i].c_str(), is_selected)) {
                this_controller->pick_db(db_keys[i]);
                db_combo_name = db_keys[i];
                ImGui::SetItemDefaultFocus();
                //ImGui::SetKeyboardFocusHere(-1);                
            }
            
                
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    
    if(this_controller->CommMan->get_database() != nullptr){
        std::string lb = "DB NR";
        int db_nr = this_controller->CommMan->get_database_scope().default_number;
        ImGui::SetNextItemWidth(100);
        if(ImGui::InputScalar(lb.c_str(),ImGuiDataType_S32,&db_nr,nullptr,nullptr),ImGuiInputTextFlags_CharsDecimal){
            this_controller->CommMan->set_db_nr(&db_nr);
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Refresh Devices")) {
        this_controller->CommMan->update_elem();
    }

    ImGui::SameLine();

    if (ImGui::Button("Add Structure")) {
        add_db(); 
    }

    if( this_controller->CommMan->get_device_scope()!= nullptr  &&
        this_controller->CommMan->get_database()!=nullptr ){
        ImGui::SameLine();
        
        if (ImGui::Button("Get Data")) {
            this_controller->CommMan->get_plc_data();
        }
    }

};

// class ConnectionBar

// class FilterBar
/*

*/
FilterBar::FilterBar(MainGUIController* controller)
    : this_controller(controller) {}
    void FilterBar::set_udt_keys(std::vector<std::string> k_in){ udt_keys = std::move(k_in); }
    void FilterBar::activate(){ active = !active; }
    Filter::filterElem FilterBar::get_filter_status() const { return f_el; }
    
    const char* FilterBar::mode_label(Mode m) 
    {
        switch(m){
            case Mode::None:      return "--None--";
            case Mode::Value:     return "Value";
            case Mode::Name:      return "Name";
            case Mode::ValueName: return "Value-Name";
            case Mode::UdtValue:  return "Deprecated";
        }
        return "";
    }

void FilterBar::draw()
{
    const char* btn = active ? "Unfilter" : "Filter";
    if (ImGui::Button(btn)) activate();
    ImGui::SameLine();

    if (!active) {
        // disattivo tutto
        f_el.value_in.reset();
        f_el.name.reset();
        f_el.udt_value.reset();
        f_el.udt_name.reset();
        this_controller->activate_filter(false);
        return;
    }

    // --- combo modalità ---
    ImGui::SetNextItemWidth(120);
    if (ImGui::BeginCombo("Filter by", mode_label(mode))) {
        auto pick = [&](Mode m){
            bool sel = (mode == m);
            if (ImGui::Selectable(mode_label(m), sel)) mode = m;
            if (sel) ImGui::SetItemDefaultFocus();
        };
        pick(Mode::None);
        pick(Mode::Value);
        pick(Mode::Name);
        pick(Mode::ValueName);
        pick(Mode::UdtValue);
        ImGui::EndCombo();
    }

    ImGui::SameLine();

    bool needValue = (mode == Mode::Value || mode == Mode::ValueName || mode == Mode::UdtValue);
    bool needName  = (mode == Mode::Name  || mode == Mode::ValueName);
    bool needUdt   = (mode == Mode::UdtValue);

    // VALUE
    if (needValue) {
        ImGui::SetNextItemWidth(140);
        if (ImGui::InputText("Value", value_buf.data(), (int)value_buf.size())) {
            std::string s = value_buf.data();
            Value v = translate::parse_type(s);  // tua funzione
            if (mode == Mode::UdtValue) {
                f_el.udt_value = v;
                f_el.value_in.reset();
            } else {
                f_el.value_in  = v;
                f_el.udt_value.reset();
            }
        }
    } else {
        f_el.value_in.reset();
        f_el.udt_value.reset();
    }

    ImGui::SameLine();

    // NAME
    if (needName) {
        ImGui::SetNextItemWidth(140);
        if (ImGui::InputText("Name", name_buf.data(), (int)name_buf.size())) {
            if (name_buf[0] != '\0') f_el.name = std::string(name_buf.data());
            else f_el.name.reset();
        }
    } else {
        f_el.name.reset();
    }

    ImGui::SameLine();
    f_el.udt_name.reset();

    // --- attiva/disattiva il filtro a controller ---
    bool any_active = false;
    switch (mode) {
        case Mode::None:      any_active = false; break;
        case Mode::Value:     any_active = f_el.value_in.has_value(); break;
        case Mode::Name:      any_active = f_el.name.has_value(); break;
        case Mode::ValueName: any_active = f_el.value_in.has_value() || f_el.name.has_value(); break;
        case Mode::UdtValue:  any_active = f_el.udt_value.has_value() && f_el.udt_name.has_value(); break;
    }
    this_controller->activate_filter(any_active);
}

// class FilterBar

// class Body
/*

*/

void Body::draw_node(const VariantElement& element,int& depth_in){

    std::visit([&](auto&& ptr) {
        using T = std::decay_t<decltype(*ptr)>;
        std::string label = ptr->get_name() ;
        if constexpr (std::is_base_of_v<BASE_CONTAINER, T>) {
            if(ptr->get_vis())
                if (ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_Framed|ImGuiTreeNodeFlags_OpenOnDoubleClick|ImGuiTreeNodeFlags_OpenOnArrow)) {
                    ++depth_in;
                    for (const auto& child : ptr->get_childs()) {
                        if(ptr->get_vis())
                            draw_node(child, depth_in);
                    }
                    --depth_in;
                    ImGui::TreePop();
                }
        }
        else if constexpr (std::is_base_of_v<BASE, T>) {
            if(ptr->get_vis())
                if (ImGui::TreeNodeEx(label.c_str(),ImGuiTreeNodeFlags_Leaf| ImGuiTreeNodeFlags_DefaultOpen|ImGuiTreeNodeFlags_Framed|ImGuiTreeNodeFlags_OpenOnDoubleClick)) {
                    
                    ImGui::Text("%s : %s",("Parent: "),ptr->get_parent()->get_name().c_str());

                    ImGui::Text("%s: %s", ("Type: "), ptr->get_type().c_str());
                    ImGui::Text("%s: %s.%s", ("Offset: "),std::to_string(ptr->get_offset().first).c_str(),std::to_string(ptr->get_offset().second).c_str());
                    Value data=ptr->get_data();
                    std::string data_label  = "Data";
                    ImGui::Text("%s: ", data_label.c_str());
                    ImGui::SetNextItemWidth(300);
                    ImGui::SameLine();
                    std::visit([&](auto& val) {
                        using V = std::decay_t<decltype(val)>;
                        if constexpr (std::is_same_v<V, int>) {
                            if (ImGui::InputScalar(("##"+data_label).c_str(),ImGuiDataType_S32, &val)) {
                                
                            }
                        } 
                        else if constexpr (std::is_same_v<V, bool>) {
                            if (ImGui::Checkbox(("##"+data_label).c_str(), &val)) {
                                
                            }
                        } 
                        else if constexpr (std::is_same_v<V, std::string>) {
                            static char buffer[128];
                            strncpy(buffer, val.c_str(), sizeof(buffer));
                            if (ImGui::InputText(("##"+data_label).c_str(), buffer, IM_ARRAYSIZE(buffer))) {
                                
                            }
                        }
                    },data);
                    ImGui::TreePop();
                }
        }
    },element);
}

void Body::draw(const std::shared_ptr<DB>& db) {
    int depth;
    for (const auto& element : db->get_childs()) {
        draw_node(element, depth);
    }
}




