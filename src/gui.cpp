#include <gui.hpp>
#include <managers.hpp>
#include <classes.hpp>

/// \brief Main GUI controller: owns top bar, body, comm manager, and filter bar.
/// \details Initializes all UI components and the communication layer.
MainGUIController::MainGUIController()
    :   upper_bar(std::make_unique<ConnectionBar>(this)),
        body(std::make_unique<Body>(this)),
        CommMan(std::make_unique<CommManager>()),
        _FilterBar(std::make_unique<FilterBar>(this))
        {};

/// \brief Applies the current filter settings to the database view.
/// \details Delegates to CommManager, which passes the filter to FilterManager.
void MainGUIController::activate_filter()
{
    CommMan->set_filter_mode(_FilterBar->get_filter_status());
};

/// \brief Lays out and draws the main UI: header, optional filter bar, and body.
/// \details Creates a layout with margins, draws header and filter (if DB loaded),
/// and then draws the file explorer and data viewer panes.
void MainGUIController::draw()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 work_pos  = viewport->WorkPos;
    const ImVec2 work_size = viewport->WorkSize;

    cursor = std::make_unique<DrawingInfo>(work_pos,work_size);

    const float margin = 20.0f;                    
    const float x      = work_pos.x + margin;
    float       y      = work_pos.y + margin;
    const float width  = work_size.x - margin * 2.0f;


    const float header_h = 46.0f;
    const float filter_h = 46.0f;

    // Header
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, header_h));
    ImGui::Begin("HeaderWindow", nullptr, window_type::blank);
    upper_bar->Draw();
    ImGui::End();
    y += header_h + ImGui::GetStyle().ItemSpacing.y; 
    if (CommMan->DataMan.get_db() != nullptr) {
        // Filter bar
        ImGui::SetNextWindowPos(ImVec2(x, y));
        ImGui::SetNextWindowSize(ImVec2(width, filter_h));
        ImGui::Begin("FilterBar", nullptr, window_type::blank);
        _FilterBar->draw();
        ImGui::End();
        y += filter_h + ImGui::GetStyle().ItemSpacing.y; 
    }

        cursor->Cursor.x = x;
        cursor->Cursor.y = y;

        body->Draw(CommMan->DataMan.get_db());

};

/// \brief Connection bar: device selection, DB number, adapter, and actions.
/// \param controller Owning MainGUIController for callbacks/data access.
ConnectionBar::ConnectionBar(MainGUIController* controller)
    : this_controller(controller){};

    
/// \brief Returns the label currently shown in the Device combo box.
std::string ConnectionBar::get_device_combo_name(){ return device_combo_name;}

/// \brief Sets the label used by the Device combo box.
void ConnectionBar::set_device_combo_name(std::string in){ device_combo_name = in;}

/// \brief Draws the Device combo for selecting the active PLC/device.
/// \details Populates from CommManager’s device map; on selection updates scope.
void ConnectionBar::DrawDeviceCombo()
{
    auto devices = this_controller->CommMan->NetMan.get_devices();

    if (ImGui::BeginCombo("Device", device_combo_name.c_str())) {
        if(!devices->empty())   
        { 
            for (int i = 0; i < devices->size(); ++i) {
                bool is_selected = (device_combo_name == devices->at(i).StationName.value());
                if (ImGui::Selectable(devices->at(i).StationName.value().c_str(), is_selected)) {
                    this_controller->CommMan->NetMan.set_ip(std::move(devices->at(i).ip.value().get_ip()));
                    device_combo_name = devices->at(i).StationName.value();
                    ImGui::SetItemDefaultFocus();
                }   
            }
        }
        ImGui::EndCombo();
    }
}

/// \brief Draws the Network Adapter combo for picking the capture/IO card.
/// \details Reads available adapters from NetManager and updates the selection.
void ConnectionBar::DrawNetCardCombo()
{
    std::map<std::string,std::string> net_cards = this_controller->CommMan->NetMan.get_netCards();

    if (ImGui::BeginCombo("Adapter", card_combo_name.c_str())) {
        if(!net_cards.empty())   
        { 
            for (auto i : net_cards) {
                bool is_selected = (card_combo_name ==  i.first);
                
                if (ImGui::Selectable(i.first.c_str(), is_selected)) {
                    this_controller->CommMan->NetMan.set_netCard(i.second);
                    card_combo_name = i.first;
                    ImGui::SetItemDefaultFocus();
                }   
            }
        }
        ImGui::EndCombo();
    }
}

/// \brief Draws the input for DB number (default DB index).
/// \details If a DB is loaded, allows editing the default DB number used for IO.
void ConnectionBar::DrawDbNr()
{
    if(this_controller->CommMan->DataMan.get_db() != nullptr)
    {
        std::string lb = "DB NR";
        int db_nr = this_controller->CommMan->DataMan.get_db_default_number();
        ImGui::SetNextItemWidth(100);
        if(ImGui::InputScalar(lb.c_str(),ImGuiDataType_S32,&db_nr,nullptr,nullptr))
        {
            this_controller->CommMan->DataMan.set_db_nr(&db_nr);
        }
    }
}


/// \brief Renders the full connection bar: device, DB number, adapter, and buttons.
/// \details Includes “Refresh Devices” and “Get Data” actions.
void ConnectionBar::Draw() {

    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    DrawDeviceCombo();

    //ImGui::SameLine();
    //ImGui::SetNextItemWidth(200);
    //DrawDbCombo();

    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    DrawDbNr();

    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    DrawNetCardCombo();
    
    
    static char buffer[256];
    ImVec2 curs = ImGui::GetCursorPos();
    
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if (ImGui::Button("Refresh Devices")) {
        this_controller->CommMan->NetMan.scan_network();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if( this_controller->CommMan->NetMan.get_ip().has_value() &&
        this_controller->CommMan->DataMan.get_db()!=nullptr )
        {
            if (ImGui::Button("Get Data")) 
                this_controller->CommMan->get_plc_data();
            
        }

};

/// \brief Filter bar controller: UI to select and apply filtering on the DB view.
/// \param controller Owning MainGUIController.
FilterBar::FilterBar(MainGUIController* controller)
    : this_controller(controller) {}

/// \brief Toggles filter on/off and resets values when turning off.
void FilterBar::activate(){ active = !active; }

/// \brief Returns the current filter element (mode + optional name/value).
Filter::filterElem FilterBar::get_filter_status() const { return f_el; }
    
/// \brief Human-readable label for the current filter mode.
const char* FilterBar::mode_label(Mode m) 
{
    switch(m){
        case Mode::None:      return "None";
        case Mode::Value:     return "Value";
        case Mode::Name:      return "Name";
        case Mode::ValueName: return "Value-Name";
    }
    return "";
}

/// \brief Draws the filter UI (mode picker, inputs) and applies the filter.
/// \details When disabled, clears the filter; otherwise updates f_el and triggers apply.
void FilterBar::draw()
{
   

    const char* btn = active ? "Unfilter" : "Filter";
    if (ImGui::Button(btn)) activate();
    ImGui::SameLine();
    if (!active) {
        f_el.value_in.reset();
        f_el.name.reset();
        this_controller->activate_filter();
        mode = Mode::None;
        return;
    }

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
        ImGui::EndCombo();
    }

    ImGui::SameLine();

    bool needValue = (mode == Mode::Value || mode == Mode::ValueName);
    bool needName  = (mode == Mode::Name  || mode == Mode::ValueName);

    if (needValue) 
    {
        ImGui::SetNextItemWidth(140);
        if (ImGui::InputText("Value", value_buf.data(), (int)value_buf.size())) {
            std::string s_ = value_buf.data();
            Value v_ = translate::parse_type(s_);  
            if (s_ != "") f_el.value_in  = v_;
            else f_el.value_in.reset();
        }
    } 

    ImGui::SameLine();

    if (needName) 
    {
        ImGui::SetNextItemWidth(140);
        if (ImGui::InputText("Name", name_buf.data(), (int)name_buf.size())) {
            std::string s = name_buf.data();
            if (s != "")f_el.name  = s;
            else f_el.name.reset();
        }
    } 

    ImGui::SameLine();

    bool any_active = false;
    switch (mode) {
        case Mode::None:      any_active = false; break;
        case Mode::Value:     any_active = f_el.value_in.has_value(); break;
        case Mode::Name:      any_active = f_el.name.has_value(); break;
        case Mode::ValueName: any_active = f_el.value_in.has_value() || f_el.name.has_value(); break;
    }
    this_controller->activate_filter();
}

/// \brief Main content body: explorer (projects/files) and DB viewer trees.
/// \param main Owning MainGUIController.
Body::Body(MainGUIController* main)
    :this_controller(main){};


/// \brief Recursively draws a DB element (container or leaf) as an ImGui tree node.
/// \param element Variant element to draw.
/// \param depth_in Current recursion depth (incremented/decremented during traversal).
void Body::Draw_node(const VariantElement& element,int& depth_in){

    std::visit([&](auto&& ptr) {
        using T = std::decay_t<decltype(*ptr)>;
        std::string label = ptr->get_name() ;
        
        if constexpr (std::is_base_of_v<BASE_CONTAINER, T>) {
            if(ptr->get_vis())
                if (ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_Framed|ImGuiTreeNodeFlags_OpenOnDoubleClick|ImGuiTreeNodeFlags_OpenOnArrow)) {
                    ++depth_in;
                    for (const auto& child : ptr->get_childs()) {
                        if(ptr->get_vis())
                            Draw_node(child, depth_in);
                    }
                    --depth_in;
                    ImGui::TreePop();
                }
        }
        else if constexpr (std::is_base_of_v<BASE, T>) {
            if(ptr->get_vis())
                if (ImGui::TreeNodeEx(label.c_str(),ImGuiTreeNodeFlags_Leaf| ImGuiTreeNodeFlags_DefaultOpen|ImGuiTreeNodeFlags_Framed|ImGuiTreeNodeFlags_OpenOnDoubleClick)) {
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

/// \brief Draws the left-hand explorer: buttons and directory tree.
/// \details Uses CommManager to open DB files from the scanned “Projects” root.
void Body::Draw_Explorer()
{
    if (ImGui::Button("Add DB")) {
        //this_controller->CommMan->DataMan.add_new_db();
    }
    ImGui::SameLine();

    if (ImGui::Button("Add Directory")) {
        //this_controller->CommMan->DataMan.add_directory();
    }
    _folder_ dirs = this_controller->CommMan->get_directory();
    Draw_DirectoryTree(dirs);

}

/// \brief Recursively renders a folder/file item in the explorer.
/// \param el Folder or file variant; files offer an “Open” button to load a DB.
void Body::Draw_DirectoryTree(const FileFolderVar& el)
{
    if (std::holds_alternative<_folder_>(el))
    {
        _folder_ f = std::get<_folder_>(el);
        if (ImGui::TreeNodeEx(f.name.c_str(), ImGuiTreeNodeFlags_Framed|ImGuiTreeNodeFlags_OpenOnDoubleClick|ImGuiTreeNodeFlags_OpenOnArrow))
        {
            for(auto i: f.elements) Draw_DirectoryTree(i);
            ImGui::TreePop();
        }
    }
    else
    {
        _file_ f = std::get<_file_>(el);
        if (ImGui::TreeNodeEx(f.name.c_str(), ImGuiTreeNodeFlags_Framed|ImGuiTreeNodeFlags_OpenOnDoubleClick|ImGuiTreeNodeFlags_OpenOnArrow))
        {
            if (ImGui::Button("Open")) {
                DbInfo db = DbInfo();
                db.default_number = 0;
                db.name = f.name;
                db.path = f.path;
                this_controller->CommMan->DataMan.set_db_scope(db);
            }
            ImGui::SameLine();

            if (ImGui::Button("TEST")) {
                //this_controller->CommMan->add_directory();
            }
            ImGui::TreePop();
        }
    }  
};

/// \brief Lays out and renders the explorer pane and (if present) the DB viewer.
/// \param db Optional DB pointer; when present, draws a second pane with its tree.
void Body::Draw(const std::shared_ptr<DB>& db) {
    
    int depth;
    const float margin = 20.0f;   
    
    ImVec2 Cursor= this_controller->cursor->Cursor;
    ImVec2 PortView =this_controller->cursor->Initial_PortView;

    ImVec2 NextWin_Size = ImVec2( PortView.x/4.0f,PortView.y -(Cursor.y+ImGui::GetStyle().ItemSpacing.y+ImGui::GetStyle().WindowPadding.y));
    ImVec2 NextWin_Pos = ImVec2(Cursor.x ,Cursor.y);

    ImGui::SetNextWindowSize(NextWin_Size);
    ImGui::SetNextWindowPos(NextWin_Pos);
    
    Cursor.x = NextWin_Pos.x+NextWin_Size.x;

    ImGui::Begin("FileExplorer", nullptr, window_type::blank);
    Draw_Explorer();
    ImGui::End();

    NextWin_Pos = ImVec2(Cursor.x+ImGui::GetStyle().ItemSpacing.y,Cursor.y);
    NextWin_Size = ImVec2(PortView.x -(NextWin_Pos.x+ImGui::GetStyle().ItemSpacing.x+ImGui::GetStyle().WindowPadding.x),PortView.y -(NextWin_Pos.y+ImGui::GetStyle().ItemSpacing.y+ImGui::GetStyle().WindowPadding.y));

    if (db != nullptr) 
    {
        ImGui::SetNextWindowSize(NextWin_Size);
        ImGui::SetNextWindowPos(NextWin_Pos);
        ImGui::Begin("FileShower", nullptr, window_type::blank);
            for (const auto& element : db->get_childs()) {
                Draw_node(element, depth);
            }
        ImGui::End();
    }
}




