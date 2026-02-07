# Copyright (c) 2023-present The Aureus Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

add_executable(bench_aureus
  bench_aureus.cpp
  bench.cpp
  nanobench.cpp
# Benchmarks:
  addrman.cpp
  base58.cpp
  bech32.cpp
  bip324_ecdh.cpp
  block_assemble.cpp
  blockencodings.cpp
  ccoins_caching.cpp
  chacha20.cpp
  checkblock.cpp
  checkblockindex.cpp
  checkqueue.cpp
  cluster_linearize.cpp
  connectblock.cpp
  crypto_hash.cpp
  descriptors.cpp
  disconnected_transactions.cpp
  duplicate_inputs.cpp
  ellswift.cpp
  examples.cpp
  gcs_filter.cpp
  hashpadding.cpp
  index_blockfilter.cpp
  load_external.cpp
  lockedpool.cpp
  logging.cpp
  mempool_ephemeral_spends.cpp
  mempool_eviction.cpp
  mempool_stress.cpp
  merkle_root.cpp
  obfuscation.cpp
  parse_hex.cpp
  peer_eviction.cpp
  poly1305.cpp
  pool.cpp
  prevector.cpp
  random.cpp
  readwriteblock.cpp
  rollingbloom.cpp
  rpc_blockchain.cpp
  rpc_mempool.cpp
  sign_transaction.cpp
  streams_findbyte.cpp
  strencodings.cpp
  txgraph.cpp
  txorphanage.cpp
  util_time.cpp
  verify_script.cpp
)

add_windows_application_manifest(bench_aureus)

include(TargetDataSources)
target_raw_data_sources(bench_aureus NAMESPACE benchmark::data
  data/block413567.raw
)

target_link_libraries(bench_aureus
  core_interface
  test_util
  aureus_node
  Boost::headers
)

if(ENABLE_WALLET)
  target_sources(bench_aureus
    PRIVATE
      coin_selection.cpp
      wallet_balance.cpp
      wallet_create.cpp
      wallet_create_tx.cpp
      wallet_loading.cpp
      wallet_ismine.cpp
      wallet_migration.cpp
  )
  target_link_libraries(bench_aureus aureus_wallet)
endif()

install_binary_component(bench_aureus INTERNAL)
