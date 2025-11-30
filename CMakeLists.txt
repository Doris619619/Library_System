cmake_minimum_required(VERSION 3.24)
project(Library_System LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===================== 1) Qt（前端基准） =====================
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Charts WebSockets)
qt_standard_project_setup()

# ===================== 2) 选项开关（避免无依赖时卡住） =====================
option(BUILD_VISION "Build vision (OpenCV/ONNX/yaml-cpp/spdlog)" ON)
option(BUILD_JUDGER "Build judger (seat_state_judger)" ON)
option(BUILD_DB     "Build database core (SQLiteCpp/sqlite3)" ON)
option(BUILD_JUDGER_CLI "Build standalone CLI for judger" ON)  # 新增：判定模块命令行工具开关

# ===================== 3) 头文件总入口（供 QtCreator 展示） =====================
set(SEATUI_PUBLIC_HEADERS
  include/seatui/launcher/login_window.hpp
  include/seatui/launcher/role_selector.hpp
  include/seatui/student/student_window.hpp
  include/seatui/student/navigation_canvas.hpp
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
)

# ===================== 4) 可执行程序（Qt主程序） =====================
qt_add_executable(Library_System
  WIN32
  # 入口 & 启动流程
  src/launcher_app/main.cpp
  src/launcher_app/login_window.cpp
  src/launcher_app/role_selector.cpp
  # 学生端
  src/student_app/student_window.cpp
  src/student_app/navigation_canvas.cpp
  # 管理端
  src/admin_app/admin_window.cpp
  # 公共小部件
  src/widgets/card_dialog.cpp
)

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

target_link_libraries(Library_System PRIVATE Qt6::Core Qt6::Widgets Qt6::Charts Qt6::WebSockets)

# ===================== 5) 判断模块（Judger，B模块） =====================
if(BUILD_JUDGER)
  add_library(judger STATIC
    src/judger_core/seat_state_judger.cpp
  )
  target_include_directories(judger
    PUBLIC
      ${CMAKE_SOURCE_DIR}/include
      ${CMAKE_SOURCE_DIR}/third_party/nlohmann   # 单头 json.hpp
      ${CMAKE_SOURCE_DIR}/third_party/sqlite/include  # 添加这一行

      C:/Users/LiangYS/vcpkg/installed/x64-windows/include/opencv4  # 直接使用路径
      C:/Users/LiangYS/vcpkg/installed/x64-windows/include
  )

  # ===================== 新增：判定模块命令行工具 =====================
  if(BUILD_JUDGER_CLI)
    add_executable(judger_cli
      tools/judger_cli.cpp            # CLI 工具源文件（有 main 函数）
    )
    target_include_directories(judger_cli
      PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/third_party/nlohmann
        ${CMAKE_SOURCE_DIR}/third_party/sqlite/include  # 添加这一行
        ${OpenCV_INCLUDE_DIRS}  # 添加这一行！

    )
    target_link_libraries(judger_cli PRIVATE judger)
  endif()
endif()

# ===================== 6) 数据库模块（SQLiteCpp + sqlite3） =====================
if(BUILD_DB)
  add_library(dbcore STATIC
    src/db_core/DatabaseInitializer.cpp
    src/db_core/SeatDatabase.cpp
    src/db_core/TimeUtils.cpp
  )
  target_include_directories(dbcore
    PUBLIC
      ${CMAKE_SOURCE_DIR}/include
      ${CMAKE_SOURCE_DIR}/third_party/sqlite/include
      ${CMAKE_SOURCE_DIR}/third_party/nlohmann
  )
  target_link_libraries(dbcore PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/sqlite/lib/libSQLiteCpp.a
    ${CMAKE_SOURCE_DIR}/third_party/sqlite/lib/libsqlite3.a
  )
endif()

# ===================== 7) 视觉模块（Vision，A模块） =====================
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
  )

  target_include_directories(vision
    PUBLIC
      ${CMAKE_SOURCE_DIR}/include
      ${CMAKE_SOURCE_DIR}/third_party/nlohmann
      ${CMAKE_SOURCE_DIR}/third_party/onnxruntime/include  # 添加 ONNX Runtime 头文件
  )

  # 修复：使用正确的 OpenCV 目标名称
  target_link_libraries(vision
    PUBLIC
      ${OpenCV_LIBS}
  )

  # 链接 ONNX Runtime 库
  target_link_libraries(vision
    PUBLIC
      ${CMAKE_SOURCE_DIR}/third_party/onnxruntime/lib/onnxruntime.lib
  )

  # 运行时 DLL 拷贝（Windows 需要）
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

# ===================== 8) 主程序链接各模块 =====================
if(BUILD_JUDGER)
  target_link_libraries(Library_System PRIVATE judger)
endif()
if(BUILD_DB)
  target_link_libraries(Library_System PRIVATE dbcore)
endif()
if(BUILD_VISION)
  target_link_libraries(Library_System PRIVATE vision)
endif()

# ===================== 9) 运行期资源拷贝 =====================
add_custom_command(TARGET Library_System POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets/vision
          $<TARGET_FILE_DIR:Library_System>/assets/vision
  COMMENT "Copy assets/vision to runtime output"
)

# 如果 judger_cli 也需要资源文件，可以添加类似的拷贝命令
if(BUILD_JUDGER_CLI)
  add_custom_command(TARGET judger_cli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets/vision
            $<TARGET_FILE_DIR:judger_cli>/assets/vision
    COMMENT "Copy assets/vision to judger_cli output"
  )
endif()

# ===================== 10) 安装与部署 =====================
include(GNUInstallDirs)
install(TARGETS Library_System
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# 可选：安装 judger_cli
if(BUILD_JUDGER_CLI)
  install(TARGETS judger_cli
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()

qt_generate_deploy_app_script(
    TARGET Library_System
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
