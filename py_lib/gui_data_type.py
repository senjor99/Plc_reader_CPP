from __future__ import annotations

from typing import List,Optional,Dict
import data_type as dt
import gui_lib as gl
import dearpygui.dearpygui as dpg
import re

class GUI_UDT:    
    def __init__(self,element:dt.TiaElement,editor:str,pos:list,is_vis:bool,callback):
    
        self._type: str 
        self._is_array:bool
        self._array_length: Optional[int] = None
        self._allowed_element: List[str] = []
        self._self_node:int
        self._node_in:int
        self._node_out:int
        self._buttons_disable: dict = {}
        self._btn_expand:int
        self._btn_filter_all:int
        self._btn_unfilter_all:int
        self._filters:dict = {}
        self._hangingObject:List[GUI_STD|GUI_STRUCT|GUI_UDT]=[]
        self.link:int=0
        self._is_expanded:bool = False
        self._name = element._name
        self._type = element._type
        self._is_array = element._is_array
        self._array_length = None if element._array_length is None else element._array_length 
        self._result = element._result
        
        with dpg.node(label=element._name, parent=editor,show=is_vis) as node:
            self._self_node = node
            
            
            with dpg.node_attribute(attribute_type=dpg.mvNode_Attr_Input) as attribute:
                dpg.add_text(f"Type: {element._type}")
                self.node_in = attribute
            
            if element._is_array:
           
                for s in element._hangingObject:
                    
                    element_ = gl.return_obj_by_type(s,editor,[0,0],False,callback)
                    
                    self._hangingObject.append(element_) 
                    self._allowed_element.append(s._name)
                    
                    if s._array_nr == 1:
                        for elem in s._hangingObject:
                            with dpg.node_attribute(attribute_type=dpg.mvNode_Attr_Input):
                                self._buttons_disable[elem._name] = dpg.add_button(label=f"Disable {elem._name}",callback= self.disable_element,user_data=elem._name)
                                self._filters[elem._name] = dpg.add_input_text(callback= self.filter_element,width=90,user_data=elem._name)

                with dpg.node_attribute(attribute_type=dpg.mvNode_Attr_Output) as attribute:
                    self._btn_expand = dpg.add_button(label="Expand",user_data=element._name, callback= self.expand)
                    self._btn_filter_all = dpg.add_button(label="Enable All",user_data=element._name, callback= self.enable_all_element)
                    self._btn_unfilter_all = dpg.add_button(label="Disable All",user_data=element._name, callback= self.disable_all_element)
                    self.node_out = attribute
                
                
            else:
                with dpg.node_attribute(attribute_type=dpg.mvNode_Attr_Output) as attribute:
                    self._btn_expand = dpg.add_button(label="Expand",user_data=element._name, callback= self.expand)
                    self.node_out = attribute

                for s in element._hangingObject:
                    element = gl.return_obj_by_type(s,editor,[0,0],False,callback)
                    if type(element._name) is tuple:
                        element._name = element._name[0]
                        
                    self._hangingObject.append(element) 
                    self._allowed_element.append(element._name)

            dpg.set_item_pos(self._self_node,pos)
        

    def expand(self):
        if not self._allowed_element:
            return

        dpg.set_item_label(self._btn_expand, "Collapse")
        dpg.set_item_callback(self._btn_expand, self.collapse)

        padding = 10
        x_base, y_base = dpg.get_item_pos(self._self_node)
        w_base, _ = dpg.get_item_rect_size(self._self_node)
        editor_width = dpg.get_viewport_width()-100
        print(dpg.get_item_pos("editor"))
        x = x_base + w_base + padding
        y = max(y_base, 10)
        row_height = 0

        w,h = 0,0
        for element in self._hangingObject:
            if element._name not in self._allowed_element:
                continue

            dpg.show_item(element._self_node)
            
            while w == 0 and h == 0:
                w, h = dpg.get_item_rect_size(element._self_node)

            if x + w + padding > x_base + editor_width:
                x = x_base + w_base + padding
                y += row_height + padding
                row_height = 0

            dpg.set_item_pos(element._self_node, [x, y])
            row_height = max(row_height, h)
            x += w + padding

            # Gestione link
            if element.link == 0:
                element.link = dpg.add_node_link(self.node_out, element.node_in, parent="editor")
            else:
                dpg.show_item(element.link)
            w,h=0,0

        self._is_expanded = True
    
    def collapse(self):
        dpg.set_item_label(self._btn_expand,"Expand")
        dpg.set_item_callback(self._btn_expand,self.expand)
        for _element in self._hangingObject:
            if _element._name in self._allowed_element:
                dpg.hide_item(_element._self_node)
                dpg.hide_item(_element.link)
                dpg.set_item_pos(_element._self_node,[10000,10000])
        self._is_expanded = False
    
    def enable_element(self,sender, app_data, user_data):
        for elem in self._hangingObject:
            if not user_data in elem._allowed_element: 
                elem._allowed_element.append(user_data)

        dpg.set_item_label(sender,f"Disable {user_data}")
        dpg.set_item_callback(sender,self.enable_element)

        if self._is_expanded:
            self.expand() 
    
    def enable_all_element(self,sender, app_data, user_data):
        
        for element in self._hangingObject:
            
            element._allowed_element.clear()
            self._allowed_element.append(element._name[0])
    
       
        for element in self._hangingObject[0]._hangingObject:
            dpg.set_item_label(item=self._buttons_disable[element._name],label=f"Disable {element._name}")
            dpg.set_item_callback(item=self._buttons_disable[element._name],callback=self.enable_element)

    def disable_element(self,sender, app_data, user_data):
        for elem in self._hangingObject:
            if user_data in elem._allowed_element:
                elem._allowed_element.remove(user_data)

        dpg.set_item_label(item=sender,label=f"Enable {user_data}")
        dpg.set_item_callback(item=sender,callback=self.enable_element)
        if self._is_expanded:
            self.collapse()
            self.expand()
        
    def disable_all_element(self,sender, app_data, user_data):
        if self._is_expanded:
            self.collapse()
        for element in self._hangingObject:
            element._allowed_element.clear()

        
        for element in self._hangingObject[0]._hangingObject:
           
            dpg.set_item_label(item=self._buttons_disable[element._name],label=f"Enable {element._name}")
            dpg.set_item_callback(item=self._buttons_disable[element._name],callback=self.enable_element)
        


    def filter_element(self,sender, app_data,user_data ):
        #print(sender, app_data, user_data)
        if self._is_expanded:
            self.collapse()
        self._allowed_element = []
        for i in self._hangingObject:
            for element in i._hangingObject:
               
                if str(element._name) == str(user_data) and str(app_data) in str(element._result):
                    self._allowed_element.append(i._name)
        
        self.expand()

    def unfilter_element(sender, app_data, user_data):
        pass
    
        
class GUI_STD:
    def __init__(self,element:dt.TiaElement,editor:str,pos:list,is_vis:bool,callback=None):
        self._name = element._name
        self._type = element._type
        self._is_array = element._is_array
        self._array_length = None if element._array_length is None else element._array_length 
        self._result = element._result
        self._self_node:int
        self._node_in:int
        self._node_out:int
        self.node_result:int
        self.link:int=0
        self._offset = element._offset
        self.callback = callback
        self._hangingObject = []
      
        if element._is_array:
            for s in element._hangingObject:
                self._hangingObject.append(gl.return_obj_by_type(s,"editor",[0.0],False,callback)) 
        
        if element._is_array:
            with dpg.node(label=element._name, parent=editor) as node:
                self._self_node = node
                with dpg.node_attribute(attribute_type=dpg.mvNode_Attr_Input) as attribute:
                    dpg.add_text(f"Type: Array of {element._type}")
                    dpg.add_text(f"Length {element._array_length}")
                    self.node_in = attribute

                with dpg.node_attribute(attribute_type=dpg.mvNode_Attr_Static) as attribute:
                    if self._type.lower() in dt.numerical_type:
                        result = dpg.add_input_text(label=f"Result",callback=self.result_modified,width=120,decimal=True)
                    if self._type.lower() in dt.string_type:
                        result = dpg.add_input_text(label=f"Result",callback=self.result_modified,width=120)

                    self.node_result = result
        else:
            with dpg.node(label=element._name, parent=editor) as node:
                self._self_node = node
                with dpg.node_attribute(attribute_type=dpg.mvNode_Attr_Input) as attribute:
                    dpg.add_text(f"Type: {element._type}")
                    
                    self.node_in = attribute
                
                with dpg.node_attribute(attribute_type=dpg.mvNode_Attr_Static) as attribute:
                    if self._type.lower() in dt.numerical_type:
                        result = dpg.add_input_text(label=f"Result",callback=self.result_modified,width=120,decimal=True,)
                    if self._type.lower() in dt.string_type:
                        result = dpg.add_input_text(label=f"Result",callback=self.result_modified,width=120)

                    self.node_result = result

        dpg.set_item_pos(self._self_node,pos)
        if not is_vis:
            dpg.hide_item(self._self_node)
    
    def result_modified(self,caller,appdata,user_data):
        if appdata != '':
            if self._is_array:
                if len(appdata) > len(self._hangingObject):
                    appdata = appdata[:self._hangingObject]
                    

                for i,elem in enumerate(self._hangingObject):
                    if i < len(appdata):
                        elem._result = appdata[i]
                    else:
                        elem._result = " "
                    self.callback(elem)                
            else:
                if len(appdata) >1 and self._type.lower() == 'char' and " "in appdata:    
                    appdata = re.sub(" ","",appdata)

                self._result=appdata
                self.callback(self)
        else:
            if self._is_array:
                self._result=''
                for i in range(len(self._hangingObject)):
                    self._result += ' '
            else:
                self._result= " "
                self.callback(self)

        dpg.set_value(caller,self._result)

class GUI_STRUCT:
    def __init__(self,element:dt.TiaElement,editor:str,pos:list,is_vis:bool,main_callback=None):
        self._name: str 
        self._type: str 
        self._is_array:bool
        self._array_length: Optional[int] = None
        self._self_node:int
        self._node_in:int
        self._node_out:int
        self.node_result:int
        self.link:int=0
        self._hangingObject=[]
        self._is_expanded:bool = False

        with dpg.node(label=element._name, parent=editor) as node:
            self._self_node = node
            with dpg.node_attribute(attribute_type=dpg.mvNode_Attr_Input) as attribute:
                dpg.add_text(f"Type: {element._type}")
                
                self.node_in = attribute
            
            with dpg.node_attribute(attribute_type=dpg.mvNode_Attr_Static) as attribute:
                dpg.add_text(f"Result: {element._result}")
                
                self.node_result = attribute

        dpg.set_item_pos(self._self_node,pos)
        if not is_vis:
            dpg.hide_item(self._self_node)

        self._name = element._name,
        self._type = element._type,
        self._is_array = element._is_array,
        self._array_length = None if element._array_length is None else element._array_length 
        self._result = element._result

