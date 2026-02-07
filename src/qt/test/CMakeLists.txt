# Copyright (c) 2024-present The Aureus Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

set(CMAKE_AUTOMOC_MOC_OPTIONS "-p${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(test_aureus-qt
  apptests.cpp
  optiontests.cpp
  rpcnestedtests.cpp
  test_main.cpp
  uritests.cpp
  util.cpp
  ../../init/aureus-qt.cpp
)

add_windows_application_manifest(test_aureus-qt)

target_link_libraries(test_aureus-qt
  core_interface
  aureusqt
  test_util
  aureus_node
  Boost::headers
  Qt6::Test
)

import_plugins(test_aureus-qt)

if(ENABLE_WALLET)
  target_sources(test_aureus-qt
    PRIVATE
      addressbooktests.cpp
      wallettests.cpp
      ../../wallet/test/wallet_test_fixture.cpp
  )
endif()

add_test(NAME test_aureus-qt
  COMMAND test_aureus-qt
)
if(WIN32 AND VCPKG_TARGET_TRIPLET)
  set(plugin_path "$<SHELL_PATH:$<PATH:GET_PARENT_PATH,$<PATH:GET_PARENT_PATH,$<TARGET_PROPERTY:Qt6::QWindowsIntegrationPlugin,LOCATION_$<CONFIG>>>>>")
  set_property(TEST test_aureus-qt APPEND PROPERTY
    ENVIRONMENT_MODIFICATION
      # On Windows, vcpkg configures Qt with `-opengl dynamic`, which makes
      # the "minimal" platform plugin unusable due to internal Qt bugs.
      QT_QPA_PLATFORM=set:windows
      QT_PLUGIN_PATH=set:${plugin_path}
  )
  unset(plugin_path)
endif()

install_binary_component(test_aureus-qt INTERNAL)
