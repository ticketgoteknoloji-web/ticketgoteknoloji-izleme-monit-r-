cmake_minimum_required(VERSION 3.24)
project(ticketgo_izleme VERSION 0.1.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Network Sql WebSockets Quick Quick3D Widgets Test)

qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

add_library(ops_core STATIC
  src/core/OpsTypes.cpp
  src/core/HealthEval.cpp
  src/core/IncidentTracker.cpp
  src/core/Store.cpp
  src/core/CredentialStore.cpp
  src/core/ProbeClient.cpp
  src/core/PathProbe.cpp
  src/core/HostMetrics.cpp
  src/core/Engine.cpp
  src/core/LogHub.cpp
  src/core/ModuleLinks.cpp
  src/core/ReportWriter.cpp
  src/core/ApiCatalog.cpp
)
target_include_directories(ops_core PUBLIC src/core)
target_link_libraries(ops_core PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::Sql Qt6::WebSockets)
if(APPLE)
  target_link_libraries(ops_core PUBLIC "-framework Security" "-framework CoreFoundation" "-framework AudioToolbox")
endif()
if(WIN32)
  target_link_libraries(ops_core PUBLIC Advapi32)
endif()

qt_add_executable(ticketgo-izleme
  MACOSX_BUNDLE
  WIN32
  src/app/main.cpp
  src/app/OpsController.cpp
)
target_include_directories(ticketgo-izleme PRIVATE src/app src/core)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(ticketgo-izleme
  URI TicketGo.Ops
  VERSION 1.0
  QML_FILES
    qml/Theme.qml
    qml/Main.qml
    qml/ModuleCard.qml
    qml/ModuleDetail.qml
    qml/MarkImage.qml
    qml/ServerView.qml
    qml/Sparkline.qml
    qml/OverviewPage.qml
    qml/ModulesPage.qml
    qml/FocusPage.qml
    qml/IntegrationsPage.qml
    qml/AlertsPage.qml
    qml/LogsPage.qml
    qml/LogLoginDialog.qml
    qml/PerformancePage.qml
    qml/IncidentsPage.qml
    qml/ReportsPage.qml
    qml/SettingsPage.qml
    qml/UpdateDialog.qml
    qml/ApiCenterPage.qml
  RESOURCES
    qml/assets/logo.jpg
    qml/assets/logo-ticketgo.png
    qml/assets/logo-gochat-mark.png
    qml/assets/logo-ferry.png
    qml/assets/app-icon.png
    qml/assets/logo-program.png
    schemas/monitoring-contract-1.0.schema.json
)

if(APPLE)
  set(APP_ICON_MACOS "${CMAKE_CURRENT_SOURCE_DIR}/resources/AppIcon.icns")
  set_source_files_properties(${APP_ICON_MACOS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
  target_sources(ticketgo-izleme PRIVATE ${APP_ICON_MACOS})
endif()
if(WIN32)
  set(APP_ICON_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/resources/AppIcon.ico")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/app.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/app.rc" @ONLY)
  target_sources(ticketgo-izleme PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
endif()

qt_add_resources(ticketgo-izleme controls_conf
  PREFIX "/"
  FILES qtquickcontrols2.conf
)
target_link_libraries(ticketgo-izleme PRIVATE ops_core Qt6::Quick Qt6::Quick3D Qt6::Widgets)
set_target_properties(ticketgo-izleme PROPERTIES
  OUTPUT_NAME "TicketGoIzleme"
  MACOSX_BUNDLE_GUI_IDENTIFIER tr.ticketgoteknoloji.izleme
  MACOSX_BUNDLE_ICON_FILE "AppIcon.icns"
  MACOSX_BUNDLE_BUNDLE_NAME "Ticket Go İzleme"
  MACOSX_BUNDLE_INFO_STRING "Ticketgo Teknoloji Gözlem Merkezi"
  MACOSX_BUNDLE_COPYRIGHT "Ticketgo Teknoloji Gözlem Merkezi"
  MACOSX_BUNDLE_BUNDLE_VERSION 0.1.3
  MACOSX_BUNDLE_SHORT_VERSION_STRING 0.1.3
)
if(WIN32)
  set_target_properties(ticketgo-izleme PROPERTIES
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME "TicketGoIzleme"
  )
  target_compile_definitions(ticketgo-izleme PRIVATE UNICODE _UNICODE)
endif()

add_library(update_core STATIC
  src/update/SemVer.cpp
  src/update/ReleaseParser.cpp
  src/update/ZipSafety.cpp
  src/update/PackageVerifier.cpp
  src/update/InstallPlan.cpp
  src/update/UpdateManager.cpp
)
target_include_directories(update_core PUBLIC src/update)
target_link_libraries(update_core PUBLIC Qt6::Core Qt6::Network)
target_compile_definitions(update_core PUBLIC TICKETGO_APP_VERSION="${PROJECT_VERSION}" TICKETGO_DEVELOPER_ID="")
target_link_libraries(ticketgo-izleme PRIVATE update_core)
target_compile_definitions(ticketgo-izleme PRIVATE TICKETGO_APP_VERSION="${PROJECT_VERSION}")

add_executable(ticketgo-update-helper src/update/helper_main.cpp)
target_link_libraries(ticketgo-update-helper PRIVATE update_core Qt6::Core)
if(APPLE)
  add_custom_command(TARGET ticketgo-izleme POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ticketgo-izleme>/../Helpers"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:ticketgo-update-helper>" "$<TARGET_FILE_DIR:ticketgo-izleme>/../Helpers/ticketgo-update-helper"
  )
  add_dependencies(ticketgo-izleme ticketgo-update-helper)
endif()
if(WIN32)
  add_custom_command(TARGET ticketgo-izleme POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:ticketgo-update-helper>" "$<TARGET_FILE_DIR:ticketgo-izleme>/ticketgo-update-helper.exe"
  )
  add_dependencies(ticketgo-izleme ticketgo-update-helper)
endif()

enable_testing()
qt_add_executable(ops_core_tests tests/test_ops.cpp)
target_link_libraries(ops_core_tests PRIVATE ops_core Qt6::Test)
add_test(NAME ops_core COMMAND ops_core_tests)
qt_add_executable(update_tests tests/test_update.cpp)
target_link_libraries(update_tests PRIVATE update_core Qt6::Test Qt6::Network)
add_test(NAME update_core COMMAND update_tests)
