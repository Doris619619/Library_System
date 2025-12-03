cmake_minimum_required(VERSION 3.24)
project(Library_System LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===================== 编译器特性设置 =====================
if(MSVC)
    add_compile_options(/utf-8)
    add_compile_options(/Zc:__cplusplus)
    add_compile_options(/permissive-)
    add_compile_definitions(_UNICODE UNICODE)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ===================== Qt（前端基准） =====================
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Charts WebSockets Network)
qt_standard_project_setup()

# ===================== 选项开关 =====================
option(BUILD_VISION      "Build vision (OpenCV/ONNX/yaml-cpp/spdlog)" ON)
option(BUILD_JUDGER      "Build judger (seat_state_judger)" ON)
option(BUILD_DB          "Build database core (SQLiteCpp/sqlite3)" ON)
# 单进程集成 WS：默认关闭独立 ws_service（以后想用多进程再打开）
option(BUILD_WS_SERVICE  "Build standalone WebSocket push service" OFF)

# ===================== 头文件总入口 =====================
set(SEATUI_PUBLIC_HEADERS
  include/seatui/launcher/login_window.hpp
  include/seatui/launcher/role_selector.hpp
  include/seatui/student/student_window.hpp
  include/seatui/student/navigation_canvas.hpp
  include/seatui/student/book_search.hpp
  include/seatui/admin/admin_window.hpp
  include/seatui/widgets/card_dialog.hpp

  # judger
  include/seatui/judger/seat_state_judger.hpp
  include/seatui/judger/data_structures.hpp

  # vision
  include/seatui/vision/Config.h
  include/seatui/vision/Enums.h
  include/seatui/vision/FrameProcessor.h
  include/seatui/vision/Mog2.h
  include/seatui/vision/OrtYolo.h
  include/seatui/vision/Publish.h
  include/seatui/vision/SeatRoi.h
  include/seatui/vision/Snapshotter.h
  include/seatui/vision/Types.h
  include/seatui/vision/VisionA.h
  include/seatui/vision/VisionClient.h
  include/seatui/vision/Nms.h

  # WS（注意：你当前工程里放在 includ，e/ws/ 路径）
  include/ws/ws_hub.hpp
)

# ===================== 可执行程序（Qt主程序） =====================
qt_add_executable(Library_System
  WIN32
  # 启动/登录/角色
  src/launcher_app/main.cpp
  src/launcher_app/login_window.cpp
  src/launcher_app/role_selector.cpp
  # 学生端
  src/student_app/student_window.cpp
  src/student_app/navigation_canvas.cpp
  src/student_app/book_search.cpp
  # 管理端
  src/admin_app/admin_window.cpp
  # 公共小部件
  src/widgets/card_dialog.cpp

  # ★ 单进程：把 WS 服务实现编进主程序（若你不想这样可删）
  src/ws_service/ws_hub.cpp
)


set_target_properties(Library_System PROPERTIES AUTOMOC ON)  # ← 添加这行！

target_sources(Library_System
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_SOURCE_DIR}/include
    FILES ${SEATUI_PUBLIC_HEADERS}
)

target_include_directories(Library_System
  PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(Library_System PRIVATE
    Qt6::Core Qt6::Widgets Qt6::Charts Qt6::WebSockets Qt6::Network
)

# ===================== Judger（保持你的写法，仅示例） =====================
if(BUILD_JUDGER)
  add_library(judger STATIC
    src/judger_core/seat_state_judger.cpp
  )
  target_include_directories(judger
    PUBLIC
      ${CMAKE_SOURCE_DIR}/include
      ${CMAKE_SOURCE_DIR}/third_party/nlohmann
      # 若 Judger 需要 OpenCV/其他头，可在此继续添加
      ${CMAKE_SOURCE_DIR}/third_party/sqlite/include  # 添加这一行

      C:/Users/LiangYS/vcpkg/installed/x64-windows/include/opencv4  # 直接使用路径
      C:/Users/LiangYS/vcpkg/installed/x64-windows/include
  )
endif()

# ===================== 数据库模块（稳健写法，兼容多种目标名） =====================
if(BUILD_DB)
  # 1) 只强制找 SQLiteCpp（你已用 vcpkg 安装）；
  #    sqlite3 让它跟随 SQLiteCpp 的传递依赖，或在找得到时再补链。
  find_package(SQLiteCpp CONFIG REQUIRED)

  add_library(dbcore STATIC
    src/db_core/DatabaseInitializer.cpp
    src/db_core/SeatDatabase.cpp
    src/db_core/TimeUtils.cpp
  )

  target_include_directories(dbcore
    PUBLIC
      ${CMAKE_SOURCE_DIR}/include
      ${CMAKE_SOURCE_DIR}/third_party/nlohmann
  )

  # 2) 兼容两种常见目标名（vcpkg 一般是没有命名空间的 SQLiteCpp）
  if(TARGET SQLiteCpp)
    target_link_libraries(dbcore PUBLIC SQLiteCpp)
  elseif(TARGET SQLiteCpp::SQLiteCpp)
    target_link_libraries(dbcore PUBLIC SQLiteCpp::SQLiteCpp)
  else()
    message(FATAL_ERROR "Found SQLiteCpp package but no usable target (SQLiteCpp / SQLiteCpp::SQLiteCpp).")
  endif()

  # 3) sqlite3：若 vcpkg 提供的是 unofficial-sqlite3::sqlite3 就链接它；
  #    若环境里是官方的 SQLite::SQLite3 也能识别；都找不到就忽略（大多数场景不需要你手动链）。
  find_package(unofficial-sqlite3 CONFIG QUIET)
  if(TARGET unofficial-sqlite3::sqlite3)
    target_link_libraries(dbcore PUBLIC unofficial-sqlite3::sqlite3)
  else()
    find_package(SQLite3 QUIET) # 有些环境会导出 SQLite::SQLite3
    if(TARGET SQLite::SQLite3)
      target_link_libraries(dbcore PUBLIC SQLite::SQLite3)
    endif()
  endif()
endif()

# ===================== 视觉模块（保持原有逻辑） =====================
if(BUILD_VISION)
  find_package(OpenCV REQUIRED)
  find_package(nlohmann_json CONFIG QUIET)
  find_package(yaml-cpp CONFIG QUIET)
  find_package(spdlog CONFIG QUIET)

  add_library(vision STATIC
    src/vision_core/Config.cpp
    src/vision_core/FrameProcessor.cpp
    src/vision_core/Logging.cpp
    src/vision_core/Mog2.cpp
    src/vision_core/Nms.cpp
    src/vision_core/OrtYolo.cpp
    src/vision_core/Publish.cpp
    src/vision_core/SeatRoi.cpp
    src/vision_core/Snapshotter.cpp
    src/vision_core/Types.cpp
    src/vision_core/VisionA.cpp
    src/vision_core/Nms.cpp
    src/vision_core/VisionClient.cpp
  )

  target_include_directories(vision
    PUBLIC
      ${CMAKE_SOURCE_DIR}/include
      ${CMAKE_SOURCE_DIR}/third_party/nlohmann
      ${CMAKE_SOURCE_DIR}/third_party/onnxruntime/include
  )

  target_link_libraries(vision PUBLIC ${OpenCV_LIBS})
  target_link_libraries(vision PUBLIC ${CMAKE_SOURCE_DIR}/third_party/onnxruntime/lib/onnxruntime.lib)

  if(WIN32)
    add_custom_command(TARGET vision POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy
              ${CMAKE_SOURCE_DIR}/third_party/onnxruntime/lib/onnxruntime.dll
              $<TARGET_FILE_DIR:vision>/
      COMMENT "Copy onnxruntime.dll to output directory"
    )
  endif()

  if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(vision PUBLIC nlohmann_json::nlohmann_json)
  endif()

  if(TARGET yaml-cpp)
    target_link_libraries(vision PUBLIC yaml-cpp)
  endif()

  if(TARGET spdlog::spdlog)
    target_link_libraries(vision PUBLIC spdlog::spdlog)
  endif()

  if(MSVC)
    set_property(TARGET vision PROPERTY MSVC_RUNTIME_LIBRARY "$<IF:$<CONFIG:Debug>,MultiThreadedDebugDLL,MultiThreadedDLL>")
  endif()
endif()

# ===================== 主程序链接各模块 =====================
if(BUILD_JUDGER)
  target_link_libraries(Library_System PRIVATE judger)
endif()
if(BUILD_DB)
  target_link_libraries(Library_System PRIVATE dbcore)
endif()
if(BUILD_VISION)
  target_link_libraries(Library_System PRIVATE vision)
endif()

# ===================== WebSocket 推送服务（独立进程，默认 OFF） =====================
if(BUILD_WS_SERVICE)
  if(BUILD_DB)
    add_executable(ws_service
      src/ws_service/ws_main.cpp
      src/ws_service/ws_hub.cpp
      include/ws/ws_hub.hpp
    )

    # 添加这行：为 ws_service 也启用 AUTOMOC
    set_target_properties(ws_service PROPERTIES AUTOMOC ON)


    target_include_directories(ws_service PUBLIC ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(ws_service PRIVATE
      Qt6::Core Qt6::WebSockets Qt6::Network
      dbcore
    )
  else()
    message(WARNING "BUILD_WS_SERVICE=ON 但 BUILD_DB=OFF，ws_service 需要数据库模块，已跳过构建。")
  endif()
endif()

# ===================== 运行期资源拷贝 =====================
add_custom_command(TARGET Library_System POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets/vision
          $<TARGET_FILE_DIR:Library_System>/assets/vision
  COMMENT "Copy assets/vision to runtime output"
)

add_custom_command(TARGET Library_System POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/Input
          $<TARGET_FILE_DIR:Library_System>/Input
  COMMENT "Copy Input to runtime output (books.txt)"
)

# ===================== 安装与部署 =====================
include(GNUInstallDirs)
install(TARGETS Library_System
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(BUILD_WS_SERVICE AND BUILD_DB)
  install(TARGETS ws_service
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()

qt_generate_deploy_app_script(
    TARGET Library_System
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
