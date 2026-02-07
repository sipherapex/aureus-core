# Copyright (c) 2023-present The Aureus Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

function(generate_setup_nsi)
  set(abs_top_srcdir ${PROJECT_SOURCE_DIR})
  set(abs_top_builddir ${PROJECT_BINARY_DIR})
  set(CLIENT_URL ${PROJECT_HOMEPAGE_URL})
  set(CLIENT_TARNAME "aureus")
  set(BITCOIN_WRAPPER_NAME "aureus")
  set(BITCOIN_GUI_NAME "aureus-qt")
  set(BITCOIN_DAEMON_NAME "aureusd")
  set(BITCOIN_CLI_NAME "aureus-cli")
  set(BITCOIN_TX_NAME "aureus-tx")
  set(BITCOIN_WALLET_TOOL_NAME "aureus-wallet")
  set(BITCOIN_TEST_NAME "test_aureus")
  set(EXEEXT ${CMAKE_EXECUTABLE_SUFFIX})
  configure_file(${PROJECT_SOURCE_DIR}/share/setup.nsi.in ${PROJECT_BINARY_DIR}/aureus-win64-setup.nsi USE_SOURCE_PERMISSIONS @ONLY)
endfunction()
