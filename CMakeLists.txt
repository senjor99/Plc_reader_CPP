cmake_minimum_required(VERSION 3.20)
project(PlcReader LANGUAGES C CXX)

# ------------------------------
# 1) Standard / output
# ------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# eseguibili in bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Warning carini
if (MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ------------------------------
# 2) Opzioni
# ------------------------------
option(BUILD_GUI   "Compila GUI (ImGui + GLFW + OpenGL)" ON)
option(WITH_SNAP7  "Link a Snap7"                        ON)
option(WITH_TAO "Link a Snap7"                        ON)

# ------------------------------
# 3) Sorgenti progetto
#    (Aggiungi/porta i tuoi .cpp reali)
# ------------------------------
add_executable(plc_reader
  main.cpp
  src/classes.cpp
  src/gui.cpp
  src/hw_interface.cpp
  src/managers.cpp
  src/parser.cpp
  external/snap7/release/wrappers/c-cpp/snap7.cpp
)

# Dove stanno i tuoi header
target_include_directories(plc_reader PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)

# ------------------------------
# 4) SNAP7 (opzionale)
#    Metti la tua lib in ./lib oppure installata a sistema
# ------------------------------
if (WITH_SNAP7)
  if (EXISTS ${CMAKE_SOURCE_DIR}/external/snap7)
    target_include_directories(plc_reader PRIVATE
    ${CMAKE_SOURCE_DIR}/external/snap7/build/bin
    ${CMAKE_SOURCE_DIR}/external/snap7/release/wrappers/c-cpp
    )


    set(SNAP7_HINT_DIR "${CMAKE_SOURCE_DIR}/external/snap7" CACHE PATH "Cartella con librerie Snap7")
    

    if (WIN32)
      find_library(SNAP7_LIB NAMES snap7 libsnap7
                  PATHS ${SNAP7_HINT_DIR} /build/bin/win64/snap7.lib)
    

    elseif (UNIX AND NOT APPLE)
      if(EXISTS ${CMAKE_SOURCE_DIR}/external/snap7/build/bin/linux/libsnap7.so)  
        message("esiste")
      endif()
      set(SNAP7_LIB ${CMAKE_SOURCE_DIR}/external/snap7/build/bin/linux/libsnap7.so)
    endif()
    
    target_link_libraries(plc_reader PRIVATE ${SNAP7_LIB})
    target_compile_definitions(plc_reader PRIVATE WITH_SNAP7=1)
    if (WIN32)
      target_link_libraries(plc_reader PRIVATE ws2_32)
    else()
      target_link_libraries(plc_reader PRIVATE pthread)
    endif()

  else()
    message(WARNING "${CMAKE_SOURCE_DIR} not found")
  endif()
endif()
# ------------------------------
# 5) GUI: ImGui + GLFW + OpenGL + GLAD (opzionale)
# ------------------------------
if (BUILD_GUI)
  # a) ImGui (vendorizzata)
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
    message(WARNING "ImGui non trovato in ${IMGUI_DIR}. Disabilito BUILD_GUI.")
    set(BUILD_GUI OFF)
  endif()
endif()

if (BUILD_GUI)
  # OpenGL
  find_package(OpenGL REQUIRED)
  target_link_libraries(plc_reader PRIVATE OpenGL::GL)

  # GLAD vendorizzato (accetta glad2 o glad1)
  set(GLAD_DIR ${CMAKE_SOURCE_DIR}/external/glad)
  set(GLAD_SRC "")
  if (EXISTS ${GLAD_DIR}/src/gl.c)         # glad2
    set(GLAD_SRC ${GLAD_DIR}/src/gl.c)
    target_compile_definitions(plc_reader PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
  elseif (EXISTS ${GLAD_DIR}/src/glad.c)   # glad1
    set(GLAD_SRC ${GLAD_DIR}/src/glad.c)
    target_compile_definitions(plc_reader PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
  endif()

  if (GLAD_SRC STREQUAL "")
    message(WARNING "GLAD non trovato in ${GLAD_DIR}. Genera i file con glad2 (gl.c) o glad1 (glad.c).")
  else()
    target_sources(plc_reader PRIVATE ${GLAD_SRC})
    target_include_directories(plc_reader PRIVATE ${GLAD_DIR}/include)
  endif()

  # GLFW: usa pacchetto di sistema (consigliato su Ubuntu)
  # Se preferisci il vendorizzato, lascia la tua add_subdirectory(external/glfw)
  find_package(glfw3 QUIET CONFIG)
  set(GLFW_DIR ${CMAKE_SOURCE_DIR}/external/glfw)

  if (TARGET glfw)
    target_link_libraries(plc_reader PRIVATE glfw)
    target_include_directories(plc_reader PRIVATE 
    ${GLFW_DIR}/include/GLFW
    )
  else()

    find_package(PkgConfig QUIET)
    if (PkgConfig_FOUND)
      pkg_check_modules(GLFW3 QUIET glfw3)
      if (GLFW3_FOUND)
        target_include_directories(plc_reader PRIVATE ${GLFW3_INCLUDE_DIRS})
        target_link_libraries(plc_reader PRIVATE ${GLFW3_LIBRARIES})
      else()
        message(WARNING "GLFW non trovato. Installa libglfw3-dev oppure usa external/glfw come subdir.")
      endif()
    else()
      message(WARNING "pkg-config non trovato. Installa pkg-config o usa glfw come subdir.")
    endif()
  endif()

  # Dipendenze X11 (in genere portate da glfw; se serve, scommenta)
  if (UNIX AND NOT APPLE)
    # target_link_libraries(plc_reader PRIVATE dl pthread X11 Xrandr Xi Xcursor Xinerama)
  endif()

  target_compile_definitions(plc_reader PRIVATE BUILD_GUI=1)
endif()

# ------------------------------
# 6) Copia risorse (comodo a runtime)
# ------------------------------
add_custom_command(TARGET plc_reader POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:plc_reader>/font"
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:plc_reader>/instances"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/font" "$<TARGET_FILE_DIR:plc_reader>/font"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/instances" "$<TARGET_FILE_DIR:plc_reader>/instances"
  COMMENT "Copia font/ e instances/ vicino all'eseguibile"
)

# ------------------------------
# 7) RPATH (Linux) per librerie accanto all'eseguibile
# ------------------------------
if (UNIX)
  set_target_properties(plc_reader PROPERTIES
    BUILD_RPATH "\$ORIGIN:\$ORIGIN/../lib"
    INSTALL_RPATH "\$ORIGIN:\$ORIGIN/../lib"
  )
endif()

# ------------------------------
# 8) TAO/PEGTL (opzionale)
#    Metti la tua lib in ./lib oppure installata a sistema
# ------------------------------
# ------------------------------
# TAO/PEGTL (header-only)
# ------------------------------
option(WITH_TAO "Usa TAO/PEGTL" ON)

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

message(STATUS "BUILD_GUI=${BUILD_GUI}  WITH_SNAP7=${WITH_SNAP7}")
