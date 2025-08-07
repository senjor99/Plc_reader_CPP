#ifndef PYLIB
#define PYLIB

#include <pybind11/embed.h>

namespace py = pybind11;





std::map<std::string, std::shared_ptr<DeviceInfo>> return_scan_for_ip() {
    py::scoped_interpreter guard{};
    
    py::object lib = py::module::import("py_lib.pylib");
    py::tuple obj = lib.attr("scan_for_ip")().cast<py::tuple>();
    py::dict devices = obj[0].cast<py::dict>();
    py::list keys = obj[1].cast<py::list>();
    std::map<std::string, std::shared_ptr<DeviceInfo>> Devices;

    for(auto key:keys){
        std::shared_ptr<DeviceInfo> device = std::make_shared<DeviceInfo>();
        device->ip = py::str(devices[key]["IP"]); 
        device->raw_name = py::str(devices[key]["Raw_ProfiName"]); 
        device->profiname = py::str(devices[key]["ProfiName"]); 
        std::string _key_ = py::str(key);

        Devices[_key_] = device;
    }
    
    return Devices;
}



std::map<std::string, DbInfo> return_get_db_list() {
    py::scoped_interpreter guard{};
    
    py::object lib = py::module::import("py_lib.pylib");
    py::tuple obj = lib.attr("get_db_list_")().cast<py::tuple>();
    py::dict db = obj[0].cast<py::dict>();
    py::list keys = obj[1].cast<py::list>();
    std::map<std::string, DbInfo> DBs;

    for(auto key:keys){
        DbInfo device;
        device.name = py::str(db[key]["name"]); 
        device.path = py::str(db[key]["path"]); 
        device.default_number = std::stoi(py::str(db[key]["default_number"])); 
        std::string _key_ = py::str(key);

        DBs[_key_] = device;
    }

    return DBs;
}

#endif