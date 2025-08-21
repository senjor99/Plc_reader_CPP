cmake_minimum_required(VERSION 3.20)
project(PlcReader LANGUAGES C CXX)

# ------------------------------
# 1) Standard / output dirs
# ------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (WIN32)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/win)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/win)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/win)
  set(BIN_DIR ${CMAKE_SOURCE_DIR}/bin/linux)

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

# ------------------------------
# 2) Opzioni
# ------------------------------
option(BUILD_GUI   "Compila GUI (ImGui + GLFW + OpenGL)" ON)
option(WITH_SNAP7  "Link a Snap7"                        ON)
option(WITH_TAO    "Usa TAO/PEGTL (header-only)"         ON)

# ------------------------------
# 3) Sorgenti progetto
# ------------------------------
add_executable(plc_reader
  main.cpp
  src/classes.cpp
  src/gui.cpp
  src/hw_interface.cpp
  src/managers.cpp
  src/parser.cpp
  #src/profi_DCP.cpp
)

target_include_directories(plc_reader PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/external/pcap
)

# ------------------------------
# 4) Snap7
#    - Se usi il wrapper C++ (TS7Client), aggiungi anche snap7.cpp dei wrappers.
#    - Se usi la C API pura, NON aggiungere snap7.cpp.
# ------------------------------
if (WITH_SNAP7)
  set(SNAP7_ROOT ${CMAKE_SOURCE_DIR}/external/snap7)

  if (EXISTS ${SNAP7_ROOT})
    # header (C API + wrapper C++)
    target_include_directories(plc_reader PRIVATE
      ${SNAP7_ROOT}/release/wrappers/c-cpp
      ${SNAP7_ROOT}/build/bin         # dove spesso metti lib/dll
    )

    # libreria precompilata
    if (WIN32)
      # .lib per MinGW: controlla il path esatto del tuo build
      set(SNAP7_LIB ${SNAP7_ROOT}/build/bin/win64/snap7.lib)
      if (EXISTS ${SNAP7_LIB})
        target_link_libraries(plc_reader PRIVATE ${SNAP7_LIB})
      else()
        message(FATAL_ERROR "Snap7 .lib non trovato: ${SNAP7_LIB}")
      endif()
      # ws2_32 per winsock
      target_link_libraries(plc_reader PRIVATE ws2_32)
    elseif (UNIX AND NOT APPLE)
      set(SNAP7_SO ${SNAP7_ROOT}/build/bin/linux/libsnap7.so)
      if (EXISTS ${SNAP7_SO})
        target_link_libraries(plc_reader PRIVATE ${SNAP7_SO} pthread)
      else()
        # fallback: prova a cercarla a sistema
        find_library(SNAP7_LIB NAMES snap7 libsnap7 PATHS /usr/lib /usr/local/lib)
        if (SNAP7_LIB)
          target_link_libraries(plc_reader PRIVATE ${SNAP7_LIB} pthread)
        else()
          message(FATAL_ERROR "libsnap7 non trovata. Compilala o correggi il path.")
        endif()
      endif()
    endif()

    # Se usi TS7Client (wrapper C++), aggiungi il sorgente del wrapper:
    # (se invece usi solo C API, commenta questa riga)
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
    find_library(NPCAP_LIB NAMES wpcap Packet PATHS ${NPCAP_DIR}/Lib )
    if(NPCAP_LIB)
      target_link_libraries(plc_reader PRIVATE ${NPCAP_LIB})
      message("-- Npcap library linked correctly")
    else()
      message(WARNING "Npcap lib not found")
    endif()
  endif()
endif()

# ------------------------------
# 5) GUI: ImGui + GLFW + OpenGL + GLAD
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
    message(FATAL_ERROR "ImGui non trovato in ${IMGUI_DIR}")
  endif()

  # b) OpenGL
  find_package(OpenGL REQUIRED)
  target_link_libraries(plc_reader PRIVATE OpenGL::GL)

  # c) GLAD vendorizzato (glad1 o glad2)
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
    message(FATAL_ERROR "GLAD non trovato: genera i file in ${GLAD_DIR}/src e ${GLAD_DIR}/include")
  endif()

  # d) GLFW vendorizzato (robusto su Windows/MinGW)
  set(GLFW_DIR ${CMAKE_SOURCE_DIR}/external/glfw)
  if (EXISTS ${GLFW_DIR}/CMakeLists.txt)
    # evita target extra
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    add_subdirectory(${GLFW_DIR} ${CMAKE_BINARY_DIR}/_deps/glfw-build)
    target_link_libraries(plc_reader PRIVATE glfw)
    target_include_directories(plc_reader PRIVATE ${GLFW_DIR}/include)
  else()
    # fallback: package (Linux con pacchetti di sistema)
    find_package(glfw3 QUIET CONFIG)
    if (TARGET glfw)
      target_link_libraries(plc_reader PRIVATE glfw)
    elseif (TARGET glfw3)
      target_link_libraries(plc_reader PRIVATE glfw3)
    else()
      message(FATAL_ERROR "GLFW non trovato: aggiungi external/glfw o installa il pacchetto")
    endif()
  endif()

  # e) X11 deps (in genere GLFW le porta; abilita solo se ti mancano simboli)
  if (UNIX AND NOT APPLE)
    # target_link_libraries(plc_reader PRIVATE dl pthread X11 Xrandr Xi Xcursor Xinerama)
  endif()

  target_compile_definitions(plc_reader PRIVATE BUILD_GUI=1)
endif()

# ------------------------------
# 6) TAO/PEGTL (header-only)
# ------------------------------
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

# ------------------------------
# 7) Copia risorse accanto all'eseguibile
# ------------------------------
add_custom_command(TARGET plc_reader POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:plc_reader>/font"

  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/font" "$<TARGET_FILE_DIR:plc_reader>/font"

  COMMENT "Copia font/ e instances/ vicino all'eseguibile"
  )
add_custom_command(TARGET plc_reader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:plc_reader>/root"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/root"
            "$<TARGET_FILE_DIR:plc_reader>/root"
    COMMENT "Copying assets next to the executable"
)
# ------------------------------
# 7) Copia risorse accanto all'eseguibile
# ------------------------------

  add_custom_command(TARGET plc_reader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "C:/mingw64/bin/libstdc++-6.dll" "$<TARGET_FILE_DIR:plc_reader>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "C:/mingw64/bin/libgcc_s_seh-1.dll" "$<TARGET_FILE_DIR:plc_reader>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "C:/mingw64/bin/libwinpthread-1.dll" "$<TARGET_FILE_DIR:plc_reader>"
  )

file(COPY "${CMAKE_SOURCE_DIR}/root" DESTINATION "${BIN_DIR}")

# Copia snap7.dll accanto all'exe su Windows (se presente)
if (WIN32 AND WITH_SNAP7)
  set(SNAP7_DLL ${SNAP7_ROOT}/build/bin/win64/snap7.dll)
  if (EXISTS ${SNAP7_DLL})
    add_custom_command(TARGET plc_reader POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${SNAP7_DLL}" "$<TARGET_FILE_DIR:plc_reader>"
      COMMENT "Copia snap7.dll vicino all'eseguibile")
    endif()
endif()

# ------------------------------
# 8) RPATH (Linux)
# ------------------------------
if (UNIX AND NOT APPLE)
  set_target_properties(plc_reader PROPERTIES
    BUILD_RPATH "\$ORIGIN"
    INSTALL_RPATH "\$ORIGIN"
  )
endif()

message(STATUS "BUILD_GUI=${BUILD_GUI}  WITH_SNAP7=${WITH_SNAP7}  WITH_TAO=${WITH_TAO}")
