#pragma once

#include "gui.hpp"



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
void FilterBar::draw(){
    std::string filter_btn_name;
    if(!active)
        filter_btn_name="Filter";
    if(active)
        filter_btn_name="Unfilter";

    {if(ImGui::Button(filter_btn_name.c_str())) {activate();}}
    
    ImGui::SameLine();
    if(active){
        ImGui::SetNextItemWidth(100);

        if (ImGui::BeginCombo("Filter by", ty_filter_scope.c_str())) {
            for (int i = 0; i < filter_modes.size(); ++i) {
                bool is_selected = (filter_modes[i] == ty_filter_scope);
                if (ImGui::Selectable(filter_modes[i].c_str(), is_selected)) {
                    ty_filter_scope = filter_modes[i];
                    ImGui::SetItemDefaultFocus();                   
                }
            

            }
            ImGui::EndCombo();
        }
    

        ImGui::SameLine();

        if(ty_filter_scope != filter_modes[0])
        {
            if(filter_modes[1] == ty_filter_scope || filter_modes[2] == ty_filter_scope|| filter_modes[4] == ty_filter_scope)
            {  
                ImGui::SetNextItemWidth(100);
                static char value_buffer[128] = {0};  
                if (ImGui::InputText("Value", value_buffer, IM_ARRAYSIZE(value_buffer))) {
                    std::string _user_input = std::string(value_buffer);
                    Value user_input =translate::parse_type(_user_input);
                    

                    if(filter_modes[4] == ty_filter_scope) 
                    {
                        f_el.udt_value = user_input;
                        f_el.value_in = std::nullopt;   
                    }
                    else
                    {
                        f_el.value_in = user_input;
                        f_el.udt_value= std::nullopt;   
                    }
                    
                }
            }
            else
            { 
                f_el.udt_value = std::nullopt; 
                f_el.value_in = std::nullopt; 
            }
            
            ImGui::SameLine();
            
            if(filter_modes[2] == ty_filter_scope)
            {   
                ImGui::SetNextItemWidth(100);
                static char buffer[128]; 
                if (ImGui::InputText("Name", buffer, IM_ARRAYSIZE(buffer))) {

                }
            }
            else f_el.name = std::nullopt; 
            
            ImGui::SameLine();
            
            if(filter_modes[3] == ty_filter_scope || filter_modes[4] == ty_filter_scope)
            {
                ImGui::SetNextItemWidth(100);
                std::string ty_fil_name;
                if (ImGui::BeginCombo("UDT", ty_fil_name.c_str())) {
                    for (int i = 0; i < udt_keys.size(); ++i) {
                        bool is_selected = (udt_keys[i] == udt_scope);
                        if (ImGui::Selectable(udt_keys[i].c_str(), is_selected)) {
                            filter_scope= i;
                            ty_fil_name = udt_keys[i];
                            ImGui::SetItemDefaultFocus();    
                        }   
                    }
                    ImGui::EndCombo();
                }
            }
            else f_el.udt_name = std::nullopt; 
        }
    }
    this_controller->activate_filter(active);


};

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

void MainGUIController::draw(){
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 size = viewport->Size;   
    size.x -= 40.0f;      
    float cursor_global = 20.0f;
    ImVec2 padding = ImGui::GetStyle().WindowPadding;

    ImGui::SetNextWindowPos(ImVec2(20,cursor_global));
    ImGui::SetNextWindowSize(ImVec2(size.x ,40.0f));
    ImGui::Begin("HeaderWindow", nullptr,window_type::blank);
    
    upper_bar->draw();

    cursor_global += ImGui::GetWindowSize().y+padding.y;  
    ImGui::End();

    if(CommMan->get_database() != nullptr){
        
        ImGui::SetNextWindowPos(ImVec2(20,cursor_global));
        ImGui::SetNextWindowSize(ImVec2(size.x,40.0f));
        ImGui::Begin("FilterBar",nullptr,window_type::blank);
        
        _FilterBar->draw();

        cursor_global += ImGui::GetWindowSize().y+padding.y;  
        ImGui::End();

        float end_screen = size.y - 40.0f;
        ImGui::SetNextWindowPos(ImVec2(20,cursor_global));
        ImGui::SetNextWindowSize(ImVec2(size.x,end_screen));
        ImGui::Begin("BodyWindow", nullptr,window_type::blank);
        
        body->draw(CommMan->get_database());
        
        cursor_global += ImGui::GetWindowSize().y+padding.y;  
        ImGui::End();
    }
};

void MainGUIController::pick_db(std::string k_in)
    {
        CommMan->set_database_scope(k_in);
        for(auto i :CommMan->get_udt_keys() )

        _FilterBar->set_udt_keys(CommMan->get_udt_keys());
    };

void MainGUIController::activate_filter(bool& b_in)
    {
        if(b_in)
        {  
            CommMan->set_filter_mode(_FilterBar->get_filter_status());
        }
        else
        {
            CommMan->reset_filter_mode();
        }
    }

