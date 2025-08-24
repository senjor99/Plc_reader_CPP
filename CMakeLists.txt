cmake_minimum_required(VERSION 3.20)
project(PlcReader LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (WIN32)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/win)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/win)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/win)
  set(BIN_DIR ${CMAKE_SOURCE_DIR}/bin/win)        # FIX: era linux
elseif (UNIX AND NOT APPLE)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/linux)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/linux)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/linux)
  set(BIN_DIR ${CMAKE_SOURCE_DIR}/bin/linux)
endif()

if (MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(BUILD_GUI  "Compila GUI (ImGui + GLFW + OpenGL)" ON)
option(WITH_SNAP7 "Link a Snap7"                        ON)
option(WITH_TAO   "Usa TAO/PEGTL (header-only)"         ON)

add_executable(plc_reader
  main.cpp
  src/classes.cpp
  src/gui.cpp
  src/hw_interface.cpp
  src/managers.cpp
  src/parser.cpp
  src/profi_DCP.cpp
)

target_include_directories(plc_reader PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)

# --- PCAP header include path ---

if (WIN32)  
  set(NPCAP_DIR ${CMAKE_SOURCE_DIR}/external/pcap)
  target_include_directories(plc_reader PRIVATE
    ${NPCAP_DIR}
    ${NPCAP_DIR}/pcap
  )
    find_library(NPCAP_LIB NAMES wpcap Packet PATHS ${NPCAP_DIR}/Lib/x64 )
    if(NPCAP_LIB)
      set(NPCAP_LIB_FOLDER ${NPCAP_DIR}/Lib/x64)
      target_link_libraries(plc_reader PRIVATE ${NPCAP_LIB_FOLDER}/Packet.lib)
      target_link_libraries(plc_reader PRIVATE ${NPCAP_LIB_FOLDER}/wpcap.lib)
      message("-- Npcap library linked correctly")
    else()
      message(WARNING "Npcap lib not found")
    endif()
    target_link_libraries(plc_reader PRIVATE
    ws2_32
    iphlpapi)
else()
  target_link_libraries(plc_reader PRIVATE pcap)
endif()

# ==== ImGui (vendorizzata) ====
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external/imgui)
if (EXISTS ${IMGUI_DIR}/imgui.cpp)
  target_sources(plc_reader PRIVATE
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
  )
  target_include_directories(plc_reader PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
  )
else()
  message(FATAL_ERROR "ImGui non trovato in ${IMGUI_DIR}")
endif()

# ==== OpenGL ====
find_package(OpenGL REQUIRED)
target_link_libraries(plc_reader PRIVATE OpenGL::GL)

# ==== GLAD vendorizzato (glad1 o glad2) ====
set(GLAD_DIR ${CMAKE_SOURCE_DIR}/external/glad)
if (EXISTS ${GLAD_DIR}/src/gl.c)
  target_sources(plc_reader PRIVATE ${GLAD_DIR}/src/gl.c)
  target_include_directories(plc_reader PRIVATE ${GLAD_DIR}/include)
  target_compile_definitions(plc_reader PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
elseif (EXISTS ${GLAD_DIR}/src/glad.c)
  target_sources(plc_reader PRIVATE ${GLAD_DIR}/src/glad.c)
  target_include_directories(plc_reader PRIVATE ${GLAD_DIR}/include)
  target_compile_definitions(plc_reader PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
else()
  message(FATAL_ERROR "GLAD not found generate compile GLAD in: ${GLAD_DIR}/src and ${GLAD_DIR}/include")
endif()

# ==== GLFW (vendorizzato se presente, altrimenti pacchetto) ====
set(GLFW_DIR ${CMAKE_SOURCE_DIR}/external/glfw)
if (EXISTS ${GLFW_DIR}/CMakeLists.txt)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
  add_subdirectory(${GLFW_DIR} ${CMAKE_BINARY_DIR}/_deps/glfw-build)
  target_link_libraries(plc_reader PRIVATE glfw)
  target_include_directories(plc_reader PRIVATE ${GLFW_DIR}/include)
else()

  find_package(glfw3 QUIET CONFIG)
    if (TARGET glfw)
      target_link_libraries(plc_reader PRIVATE glfw)
    elseif (TARGET glfw3)
      target_link_libraries(plc_reader PRIVATE glfw3)
    else()
      message(FATAL_ERROR "GLFW non trovato: aggiungi external/glfw o installa il pacchetto")
    endif()


if (UNIX AND NOT APPLE)
   target_link_libraries(plc_reader PRIVATE dl pthread X11 Xrandr Xi Xcursor Xinerama)
  endif()

  target_compile_definitions(plc_reader PRIVATE BUILD_GUI=1)
endif()

# ------------------------------ # 6) TAO/PEGTL # ------------------------------ 
if (WITH_TAO)
  set(PEGTL_DIR ${CMAKE_SOURCE_DIR}/external/PEGTL) 
  if (EXISTS ${PEGTL_DIR}/include) 
    target_include_directories(plc_reader PRIVATE ${PEGTL_DIR}/include) 
    target_compile_definitions(plc_reader PRIVATE WITH_TAO=1) 
  else() 
    message(WARNING "PEGTL non trovata in ${PEGTL_DIR}/include — disabilito WITH_TAO.") 
    set(WITH_TAO OFF) 
  endif() 
endif()

# ---------------- Snap7 ----------------
if (WITH_SNAP7)
  set(SNAP7_ROOT ${CMAKE_SOURCE_DIR}/external/snap7)

  if (EXISTS ${SNAP7_ROOT})
    target_include_directories(plc_reader PRIVATE
      ${SNAP7_ROOT}/release/wrappers/c-cpp
      ${SNAP7_ROOT}/build/bin
    )

    if (WIN32)
      set(SNAP7_LIB ${SNAP7_ROOT}/build/bin/win64/snap7.lib)
      if (EXISTS ${SNAP7_LIB})
        target_link_libraries(plc_reader PRIVATE ${SNAP7_LIB})
      else()
        message(FATAL_ERROR "Snap7 .lib non trovato: ${SNAP7_LIB}")
      endif()

    elseif (UNIX AND NOT APPLE)
      set(SNAP7_SO ${SNAP7_ROOT}/build/bin/linux/libsnap7.so)
      if (EXISTS ${SNAP7_SO})
        target_link_libraries(plc_reader PRIVATE ${SNAP7_SO} pthread)
      else()
        find_library(SNAP7_LIB NAMES snap7 libsnap7 PATHS /usr/lib /usr/local/lib)
        if (SNAP7_LIB)
          target_link_libraries(plc_reader PRIVATE ${SNAP7_LIB} pthread)
        else()
          message(FATAL_ERROR "libsnap7 non trovata. Compilala o correggi il path.")
        endif()
      endif()
    endif()

    set(SNAP7_WRAPPER_CPP ${SNAP7_ROOT}/release/wrappers/c-cpp/snap7.cpp)
    if (EXISTS ${SNAP7_WRAPPER_CPP})
      target_sources(plc_reader PRIVATE ${SNAP7_WRAPPER_CPP})
    endif()

    target_compile_definitions(plc_reader PRIVATE WITH_SNAP7=1)
  else()
    message(FATAL_ERROR "Cartella Snap7 non trovata in: ${SNAP7_ROOT}")
  endif()

  if (WIN32)
    set(NPCAP_DIR ${CMAKE_SOURCE_DIR}/external/pcap)
    # linka esplicitamente entrambe le lib (non con find_library singolo)
    if (EXISTS ${NPCAP_DIR}/Lib/x64/wpcap.lib AND EXISTS ${NPCAP_DIR}/Lib/x64/Packet.lib)
      target_link_libraries(plc_reader PRIVATE
        ${NPCAP_DIR}/Lib/x64/wpcap.lib
        ${NPCAP_DIR}/Lib/x64/Packet.lib
      )
      message("-- Npcap library linked correctly")
    else()
      message(WARNING "Npcap lib not found in ${NPCAP_DIR}/Lib/x64")
    endif()
  endif()
endif()

add_custom_command(TARGET plc_reader POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:plc_reader>/font"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/font" "$<TARGET_FILE_DIR:plc_reader>/font"
  COMMENT "Copia font/ e instances/ vicino all'eseguibile"
)
add_custom_command(TARGET plc_reader POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:plc_reader>/root"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/root"
          "$<TARGET_FILE_DIR:plc_reader>/root"
  COMMENT "Copying assets next to the executable"
)
if(WIN32)
  if(EXISTS "C:/mingw64")
    set(MINGW_DIR "C:/mingw64/bin")
    add_custom_command(TARGET plc_reader POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${MINGW_DIR}/libstdc++-6.dll" "$<TARGET_FILE_DIR:plc_reader>"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${MINGW_DIR}//libgcc_s_seh-1.dll" "$<TARGET_FILE_DIR:plc_reader>"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${MINGW_DIR}//libwinpthread-1.dll" "$<TARGET_FILE_DIR:plc_reader>"
    )
    else()
      message(WARNING "No Mingw64 compiler found")
    endif()
endif()

file(COPY "${CMAKE_SOURCE_DIR}/root" DESTINATION "${BIN_DIR}")

if (WIN32 AND WITH_SNAP7)
  set(SNAP7_DLL ${SNAP7_ROOT}/build/bin/win64/snap7.dll)
  if (EXISTS ${SNAP7_DLL})
    add_custom_command(TARGET plc_reader POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${SNAP7_DLL}" "$<TARGET_FILE_DIR:plc_reader>"
      COMMENT "Copia snap7.dll vicino all'eseguibile")
  endif()
endif()

if (UNIX AND NOT APPLE)
  set_target_properties(plc_reader PROPERTIES
    BUILD_RPATH "\$ORIGIN"
    INSTALL_RPATH "\$ORIGIN"
  )
endif()

message(STATUS "BUILD_GUI=${BUILD_GUI}  WITH_SNAP7=${WITH_SNAP7}  WITH_TAO=${WITH_TAO}")
