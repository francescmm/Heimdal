cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

set(PROJECT_NAME "GitQlient")

project(${PROJECT_NAME} LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-Wall -Werror -pedantic-errors")

execute_process(
   COMMAND
   git -C ${PROJECT_SOURCE_DIR} rev-parse --short HEAD
   RESULT_VARIABLE
   GQ_SHA_RESULT
   OUTPUT_VARIABLE
   GQ_SHA
   OUTPUT_STRIP_TRAILING_WHITESPACE
)

if (NOT DEFINED VERSION OR "${VERSION}" STREQUAL "")
   if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
      set(VERSION "0.0.0")
   else()
      set(VERSION ${GQ_SHA})
   endif()
endif()

add_compile_definitions(SHA_VER="${GQ_SHA}")
add_compile_definitions(VER="${VERSION}")
add_compile_definitions(QT_DEPRECATED_WARNINGS)

list(APPEND CPP_FLAGS "-Wall")

if (CMAKE_BUILD_TYPE EQUAL "DEBUG" AND NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
   list(APPEND CPP_FLAGS "-Wextra" "-Wpedantic")
endif()

if (NOT EXISTS "${PROJECT_SOURCE_DIR}/src/AuxiliarCustomWidgets/.git" OR
    NOT EXISTS "${PROJECT_SOURCE_DIR}/src/QLogger/.git" OR
    NOT EXISTS "${PROJECT_SOURCE_DIR}/src/QPinnableTabWidget/.git" OR
    NOT EXISTS "${PROJECT_SOURCE_DIR}/src/git/.git")
    message(STATUS "Submodule update:")
    execute_process(COMMAND git submodule update --init --recursive
                    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                    RESULT_VARIABLE GIT_SUBMOD_RESULT)
    if(NOT GIT_SUBMOD_RESULT EQUAL "0")
        message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
    endif()
endif()

find_package(Qt6 COMPONENTS Core Widgets Network REQUIRED)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(MACOSX_BUNDLE_ICON_FILE icon.icns)
    set(MACOS_APP_ICON_GITQLIENT "${PROJECT_SOURCE_DIR}/src/resources/icon.icns")
    set_source_files_properties(${MACOS_APP_ICON_GITQLIENT} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif()

file(GLOB SRC_FILES
   ${MACOS_APP_ICON_GITQLIENT}
   ${PROJECT_SOURCE_DIR}/src/main.cpp
   ${PROJECT_SOURCE_DIR}/src/GitQlient.qrc
   ${PROJECT_SOURCE_DIR}/src/AuxiliarCustomWidgets/*.cpp
   ${PROJECT_SOURCE_DIR}/src/AuxiliarCustomWidgets/*.h
   ${PROJECT_SOURCE_DIR}/src/AuxiliarCustomWidgets/*.ui
   ${PROJECT_SOURCE_DIR}/src/AuxiliarCustomWidgets/*.qrc
   ${PROJECT_SOURCE_DIR}/src/aux_widgets/*.cpp
   ${PROJECT_SOURCE_DIR}/src/aux_widgets/*.h
   ${PROJECT_SOURCE_DIR}/src/aux_widgets/*.ui
   ${PROJECT_SOURCE_DIR}/src/big_widgets/*.cpp
   ${PROJECT_SOURCE_DIR}/src/big_widgets/*.h
   ${PROJECT_SOURCE_DIR}/src/big_widgets/*.ui
   ${PROJECT_SOURCE_DIR}/src/branches/*.cpp
   ${PROJECT_SOURCE_DIR}/src/branches/*.h
   ${PROJECT_SOURCE_DIR}/src/branches/*.ui
   ${PROJECT_SOURCE_DIR}/src/cache/*.cpp
   ${PROJECT_SOURCE_DIR}/src/cache/*.h
   ${PROJECT_SOURCE_DIR}/src/cache/*.ui
   ${PROJECT_SOURCE_DIR}/src/commits/*.cpp
   ${PROJECT_SOURCE_DIR}/src/commits/*.h
   ${PROJECT_SOURCE_DIR}/src/commits/*.ui
   ${PROJECT_SOURCE_DIR}/src/config/*.cpp
   ${PROJECT_SOURCE_DIR}/src/config/*.h
   ${PROJECT_SOURCE_DIR}/src/config/*.ui
   ${PROJECT_SOURCE_DIR}/src/diff/*.cpp
   ${PROJECT_SOURCE_DIR}/src/diff/*.h
   ${PROJECT_SOURCE_DIR}/src/diff/*.ui
   ${PROJECT_SOURCE_DIR}/src/git/*.cpp
   ${PROJECT_SOURCE_DIR}/src/git/*.h
   ${PROJECT_SOURCE_DIR}/src/git/*.ui
   ${PROJECT_SOURCE_DIR}/src/history/*.cpp
   ${PROJECT_SOURCE_DIR}/src/history/*.h
   ${PROJECT_SOURCE_DIR}/src/history/*.ui
   ${PROJECT_SOURCE_DIR}/src/QLogger/*.cpp
   ${PROJECT_SOURCE_DIR}/src/QLogger/*.h
   ${PROJECT_SOURCE_DIR}/src/QLogger/*.ui
   ${PROJECT_SOURCE_DIR}/src/QPinnableTabWidget/*.cpp
   ${PROJECT_SOURCE_DIR}/src/QPinnableTabWidget/*.h
   ${PROJECT_SOURCE_DIR}/src/QPinnableTabWidget/*.ui
)

qt_add_executable(
    ${PROJECT_NAME}
    ${SRC_FILES})

target_include_directories(
   ${PROJECT_NAME}
   PUBLIC
   ${PROJECT_SOURCE_DIR}/src
   ${PROJECT_SOURCE_DIR}/src/AuxiliarCustomWidgets
   ${PROJECT_SOURCE_DIR}/src/aux_widgets
   ${PROJECT_SOURCE_DIR}/src/big_widgets
   ${PROJECT_SOURCE_DIR}/src/branches
   ${PROJECT_SOURCE_DIR}/src/cache
   ${PROJECT_SOURCE_DIR}/src/commits
   ${PROJECT_SOURCE_DIR}/src/config
   ${PROJECT_SOURCE_DIR}/src/diff
   ${PROJECT_SOURCE_DIR}/src/git
   ${PROJECT_SOURCE_DIR}/src/history
   ${PROJECT_SOURCE_DIR}/src/QLogger
   ${PROJECT_SOURCE_DIR}/src/QPinnableTabWidget
)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        AUTOMOC TRUE
        AUTOUIC TRUE
        AUTORCC TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/Info.plist"
    )
else()
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
        AUTOMOC TRUE
        AUTOUIC TRUE
        AUTORCC TRUE
    )
endif()

qt_add_resources(${PROJECT_NAME} "translations"
    PREFIX "/i18n"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES "src/resources/translations/gitqlient_en.qm" "src/resources/translations/gitqlient_zh_CN.qm"
)

target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC
    Qt::Core
    Qt::Widgets
    Qt::Network
)


if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
   target_link_options(${PROJECT_NAME} PUBLIC "-no-pie")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
endif()

install(
    TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}"
    BUNDLE DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}"
    LIBRARY DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}"
)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_custom_target(
        "dmg"
        echo \"Generate DMG\" && rm -f *.dmg && /opt/homebrew/bin/create-dmg --volname "GitQlient" --volicon "${PROJECT_SOURCE_DIR}/src/resources/icon.icns" --background "${PROJECT_SOURCE_DIR}/src/resources/dmg_bg.png" --icon "GitQlient.app" 125 220 --icon-size 100 --window-size 600 450 --app-drop-link 475 220 "GitQlient.dmg" "GitQlient.app"
    )
endif()
