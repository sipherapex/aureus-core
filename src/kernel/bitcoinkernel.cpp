// Copyright (c) 2022-present The Aureus Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define BITCOINKERNEL_BUILD

#include <kernel/aureuskernel.h>

#include <chain.h>
#include <coins.h>
#include <consensus/validation.h>
#include <dbwrapper.h>
#include <kernel/caches.h>
#include <kernel/chainparams.h>
#include <kernel/checks.h>
#include <kernel/context.h>
#include <kernel/notifications_interface.h>
#include <kernel/warning.h>
#include <logging.h>
#include <node/blockstorage.h>
#include <node/chainstate.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <serialize.h>
#include <streams.h>
#include <sync.h>
#include <uint256.h>
#include <undo.h>
#include <util/check.h>
#include <util/fs.h>
#include <util/result.h>
#include <util/signalinterrupt.h>
#include <util/task_runner.h>
#include <util/translation.h>
#include <validation.h>
#include <validationinterface.h>

#include <cstddef>
#include <cstring>
#include <exception>
#include <functional>
#include <list>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using kernel::ChainstateRole;
using util::ImmediateTaskRunner;

// Define G_TRANSLATION_FUN symbol in libaureuskernel library so users of the
// library aren't required to export this symbol
extern const TranslateFn G_TRANSLATION_FUN{nullptr};

static const kernel::Context aurk_context_static{};

namespace {

bool is_valid_flag_combination(script_verify_flags flags)
{
    if (flags & SCRIPT_VERIFY_CLEANSTACK && ~flags & (SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS)) return false;
    if (flags & SCRIPT_VERIFY_WITNESS && ~flags & SCRIPT_VERIFY_P2SH) return false;
    return true;
}

class WriterStream
{
private:
    aurk_WriteBytes m_writer;
    void* m_user_data;

public:
    WriterStream(aurk_WriteBytes writer, void* user_data)
        : m_writer{writer}, m_user_data{user_data} {}

    //
    // Stream subset
    //
    void write(std::span<const std::byte> src)
    {
        if (m_writer(src.data(), src.size(), m_user_data) != 0) {
            throw std::runtime_error("Failed to write serialization data");
        }
    }

    template <typename T>
    WriterStream& operator<<(const T& obj)
    {
        ::Serialize(*this, obj);
        return *this;
    }
};

template <typename C, typename CPP>
struct Handle {
    static C* ref(CPP* cpp_type)
    {
        return reinterpret_cast<C*>(cpp_type);
    }

    static const C* ref(const CPP* cpp_type)
    {
        return reinterpret_cast<const C*>(cpp_type);
    }

    template <typename... Args>
    static C* create(Args&&... args)
    {
        auto cpp_obj{std::make_unique<CPP>(std::forward<Args>(args)...)};
        return ref(cpp_obj.release());
    }

    static C* copy(const C* ptr)
    {
        auto cpp_obj{std::make_unique<CPP>(get(ptr))};
        return ref(cpp_obj.release());
    }

    static const CPP& get(const C* ptr)
    {
        return *reinterpret_cast<const CPP*>(ptr);
    }

    static CPP& get(C* ptr)
    {
        return *reinterpret_cast<CPP*>(ptr);
    }

    static void operator delete(void* ptr)
    {
        delete reinterpret_cast<CPP*>(ptr);
    }
};

} // namespace

struct aurk_BlockTreeEntry: Handle<aurk_BlockTreeEntry, CBlockIndex> {};
struct aurk_Block : Handle<aurk_Block, std::shared_ptr<const CBlock>> {};
struct aurk_BlockValidationState : Handle<aurk_BlockValidationState, BlockValidationState> {};

namespace {

BCLog::Level get_bclog_level(aurk_LogLevel level)
{
    switch (level) {
    case aurk_LogLevel_INFO: {
        return BCLog::Level::Info;
    }
    case aurk_LogLevel_DEBUG: {
        return BCLog::Level::Debug;
    }
    case aurk_LogLevel_TRACE: {
        return BCLog::Level::Trace;
    }
    }
    assert(false);
}

BCLog::LogFlags get_bclog_flag(aurk_LogCategory category)
{
    switch (category) {
    case aurk_LogCategory_BENCH: {
        return BCLog::LogFlags::BENCH;
    }
    case aurk_LogCategory_BLOCKSTORAGE: {
        return BCLog::LogFlags::BLOCKSTORAGE;
    }
    case aurk_LogCategory_COINDB: {
        return BCLog::LogFlags::COINDB;
    }
    case aurk_LogCategory_LEVELDB: {
        return BCLog::LogFlags::LEVELDB;
    }
    case aurk_LogCategory_MEMPOOL: {
        return BCLog::LogFlags::MEMPOOL;
    }
    case aurk_LogCategory_PRUNE: {
        return BCLog::LogFlags::PRUNE;
    }
    case aurk_LogCategory_RAND: {
        return BCLog::LogFlags::RAND;
    }
    case aurk_LogCategory_REINDEX: {
        return BCLog::LogFlags::REINDEX;
    }
    case aurk_LogCategory_VALIDATION: {
        return BCLog::LogFlags::VALIDATION;
    }
    case aurk_LogCategory_KERNEL: {
        return BCLog::LogFlags::KERNEL;
    }
    case aurk_LogCategory_ALL: {
        return BCLog::LogFlags::ALL;
    }
    }
    assert(false);
}

aurk_SynchronizationState cast_state(SynchronizationState state)
{
    switch (state) {
    case SynchronizationState::INIT_REINDEX:
        return aurk_SynchronizationState_INIT_REINDEX;
    case SynchronizationState::INIT_DOWNLOAD:
        return aurk_SynchronizationState_INIT_DOWNLOAD;
    case SynchronizationState::POST_INIT:
        return aurk_SynchronizationState_POST_INIT;
    } // no default case, so the compiler can warn about missing cases
    assert(false);
}

aurk_Warning cast_aurk_warning(kernel::Warning warning)
{
    switch (warning) {
    case kernel::Warning::UNKNOWN_NEW_RULES_ACTIVATED:
        return aurk_Warning_UNKNOWN_NEW_RULES_ACTIVATED;
    case kernel::Warning::LARGE_WORK_INVALID_CHAIN:
        return aurk_Warning_LARGE_WORK_INVALID_CHAIN;
    } // no default case, so the compiler can warn about missing cases
    assert(false);
}

struct LoggingConnection {
    std::unique_ptr<std::list<std::function<void(const std::string&)>>::iterator> m_connection;
    void* m_user_data;
    std::function<void(void* user_data)> m_deleter;

    LoggingConnection(aurk_LogCallback callback, void* user_data, aurk_DestroyCallback user_data_destroy_callback)
    {
        LOCK(cs_main);

        auto connection{LogInstance().PushBackCallback([callback, user_data](const std::string& str) { callback(user_data, str.c_str(), str.length()); })};

        // Only start logging if we just added the connection.
        if (LogInstance().NumConnections() == 1 && !LogInstance().StartLogging()) {
            LogError("Logger start failed.");
            LogInstance().DeleteCallback(connection);
            if (user_data && user_data_destroy_callback) {
                user_data_destroy_callback(user_data);
            }
            throw std::runtime_error("Failed to start logging");
        }

        m_connection = std::make_unique<std::list<std::function<void(const std::string&)>>::iterator>(connection);
        m_user_data = user_data;
        m_deleter = user_data_destroy_callback;

        LogDebug(BCLog::KERNEL, "Logger connected.");
    }

    ~LoggingConnection()
    {
        LOCK(cs_main);
        LogDebug(BCLog::KERNEL, "Logger disconnecting.");

        // Switch back to buffering by calling DisconnectTestLogger if the
        // connection that we are about to remove is the last one.
        if (LogInstance().NumConnections() == 1) {
            LogInstance().DisconnectTestLogger();
        } else {
            LogInstance().DeleteCallback(*m_connection);
        }

        m_connection.reset();
        if (m_user_data && m_deleter) {
            m_deleter(m_user_data);
        }
    }
};

class KernelNotifications final : public kernel::Notifications
{
private:
    aurk_NotificationInterfaceCallbacks m_cbs;

public:
    KernelNotifications(aurk_NotificationInterfaceCallbacks cbs)
        : m_cbs{cbs}
    {
    }

    ~KernelNotifications()
    {
        if (m_cbs.user_data && m_cbs.user_data_destroy) {
            m_cbs.user_data_destroy(m_cbs.user_data);
        }
        m_cbs.user_data_destroy = nullptr;
        m_cbs.user_data = nullptr;
    }

    kernel::InterruptResult blockTip(SynchronizationState state, const CBlockIndex& index, double verification_progress) override
    {
        if (m_cbs.block_tip) m_cbs.block_tip(m_cbs.user_data, cast_state(state), aurk_BlockTreeEntry::ref(&index), verification_progress);
        return {};
    }
    void headerTip(SynchronizationState state, int64_t height, int64_t timestamp, bool presync) override
    {
        if (m_cbs.header_tip) m_cbs.header_tip(m_cbs.user_data, cast_state(state), height, timestamp, presync ? 1 : 0);
    }
    void progress(const bilingual_str& title, int progress_percent, bool resume_possible) override
    {
        if (m_cbs.progress) m_cbs.progress(m_cbs.user_data, title.original.c_str(), title.original.length(), progress_percent, resume_possible ? 1 : 0);
    }
    void warningSet(kernel::Warning id, const bilingual_str& message) override
    {
        if (m_cbs.warning_set) m_cbs.warning_set(m_cbs.user_data, cast_aurk_warning(id), message.original.c_str(), message.original.length());
    }
    void warningUnset(kernel::Warning id) override
    {
        if (m_cbs.warning_unset) m_cbs.warning_unset(m_cbs.user_data, cast_aurk_warning(id));
    }
    void flushError(const bilingual_str& message) override
    {
        if (m_cbs.flush_error) m_cbs.flush_error(m_cbs.user_data, message.original.c_str(), message.original.length());
    }
    void fatalError(const bilingual_str& message) override
    {
        if (m_cbs.fatal_error) m_cbs.fatal_error(m_cbs.user_data, message.original.c_str(), message.original.length());
    }
};

class KernelValidationInterface final : public CValidationInterface
{
public:
    aurk_ValidationInterfaceCallbacks m_cbs;

    explicit KernelValidationInterface(const aurk_ValidationInterfaceCallbacks vi_cbs) : m_cbs{vi_cbs} {}

    ~KernelValidationInterface()
    {
        if (m_cbs.user_data && m_cbs.user_data_destroy) {
            m_cbs.user_data_destroy(m_cbs.user_data);
        }
        m_cbs.user_data = nullptr;
        m_cbs.user_data_destroy = nullptr;
    }

protected:
    void BlockChecked(const std::shared_ptr<const CBlock>& block, const BlockValidationState& stateIn) override
    {
        if (m_cbs.block_checked) {
            m_cbs.block_checked(m_cbs.user_data,
                                aurk_Block::copy(aurk_Block::ref(&block)),
                                aurk_BlockValidationState::ref(&stateIn));
        }
    }

    void NewPoWValidBlock(const CBlockIndex* pindex, const std::shared_ptr<const CBlock>& block) override
    {
        if (m_cbs.pow_valid_block) {
            m_cbs.pow_valid_block(m_cbs.user_data,
                                  aurk_Block::copy(aurk_Block::ref(&block)),
                                  aurk_BlockTreeEntry::ref(pindex));
        }
    }

    void BlockConnected(const ChainstateRole& role, const std::shared_ptr<const CBlock>& block, const CBlockIndex* pindex) override
    {
        if (m_cbs.block_connected) {
            m_cbs.block_connected(m_cbs.user_data,
                                  aurk_Block::copy(aurk_Block::ref(&block)),
                                  aurk_BlockTreeEntry::ref(pindex));
        }
    }

    void BlockDisconnected(const std::shared_ptr<const CBlock>& block, const CBlockIndex* pindex) override
    {
        if (m_cbs.block_disconnected) {
            m_cbs.block_disconnected(m_cbs.user_data,
                                     aurk_Block::copy(aurk_Block::ref(&block)),
                                     aurk_BlockTreeEntry::ref(pindex));
        }
    }
};

struct ContextOptions {
    mutable Mutex m_mutex;
    std::unique_ptr<const CChainParams> m_chainparams GUARDED_BY(m_mutex);
    std::shared_ptr<KernelNotifications> m_notifications GUARDED_BY(m_mutex);
    std::shared_ptr<KernelValidationInterface> m_validation_interface GUARDED_BY(m_mutex);
};

class Context
{
public:
    std::unique_ptr<kernel::Context> m_context;

    std::shared_ptr<KernelNotifications> m_notifications;

    std::unique_ptr<util::SignalInterrupt> m_interrupt;

    std::unique_ptr<ValidationSignals> m_signals;

    std::unique_ptr<const CChainParams> m_chainparams;

    std::shared_ptr<KernelValidationInterface> m_validation_interface;

    Context(const ContextOptions* options, bool& sane)
        : m_context{std::make_unique<kernel::Context>()},
          m_interrupt{std::make_unique<util::SignalInterrupt>()}
    {
        if (options) {
            LOCK(options->m_mutex);
            if (options->m_chainparams) {
                m_chainparams = std::make_unique<const CChainParams>(*options->m_chainparams);
            }
            if (options->m_notifications) {
                m_notifications = options->m_notifications;
            }
            if (options->m_validation_interface) {
                m_signals = std::make_unique<ValidationSignals>(std::make_unique<ImmediateTaskRunner>());
                m_validation_interface = options->m_validation_interface;
                m_signals->RegisterSharedValidationInterface(m_validation_interface);
            }
        }

        if (!m_chainparams) {
            m_chainparams = CChainParams::Main();
        }
        if (!m_notifications) {
            m_notifications = std::make_shared<KernelNotifications>(aurk_NotificationInterfaceCallbacks{
                nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr});
        }

        if (!kernel::SanityChecks(*m_context)) {
            sane = false;
        }
    }

    ~Context()
    {
        if (m_signals) {
            m_signals->UnregisterSharedValidationInterface(m_validation_interface);
        }
    }
};

//! Helper struct to wrap the ChainstateManager-related Options
struct ChainstateManagerOptions {
    mutable Mutex m_mutex;
    ChainstateManager::Options m_chainman_options GUARDED_BY(m_mutex);
    node::BlockManager::Options m_blockman_options GUARDED_BY(m_mutex);
    std::shared_ptr<const Context> m_context;
    node::ChainstateLoadOptions m_chainstate_load_options GUARDED_BY(m_mutex);

    ChainstateManagerOptions(const std::shared_ptr<const Context>& context, const fs::path& data_dir, const fs::path& blocks_dir)
        : m_chainman_options{ChainstateManager::Options{
              .chainparams = *context->m_chainparams,
              .datadir = data_dir,
              .notifications = *context->m_notifications,
              .signals = context->m_signals.get()}},
          m_blockman_options{node::BlockManager::Options{
              .chainparams = *context->m_chainparams,
              .blocks_dir = blocks_dir,
              .notifications = *context->m_notifications,
              .block_tree_db_params = DBParams{
                  .path = data_dir / "blocks" / "index",
                  .cache_bytes = kernel::CacheSizes{DEFAULT_KERNEL_CACHE}.block_tree_db,
              }}},
          m_context{context}, m_chainstate_load_options{node::ChainstateLoadOptions{}}
    {
    }
};

struct ChainMan {
    std::unique_ptr<ChainstateManager> m_chainman;
    std::shared_ptr<const Context> m_context;

    ChainMan(std::unique_ptr<ChainstateManager> chainman, std::shared_ptr<const Context> context)
        : m_chainman(std::move(chainman)), m_context(std::move(context)) {}
};

} // namespace

struct aurk_Transaction : Handle<aurk_Transaction, std::shared_ptr<const CTransaction>> {};
struct aurk_TransactionOutput : Handle<aurk_TransactionOutput, CTxOut> {};
struct aurk_ScriptPubkey : Handle<aurk_ScriptPubkey, CScript> {};
struct aurk_LoggingConnection : Handle<aurk_LoggingConnection, LoggingConnection> {};
struct aurk_ContextOptions : Handle<aurk_ContextOptions, ContextOptions> {};
struct aurk_Context : Handle<aurk_Context, std::shared_ptr<const Context>> {};
struct aurk_ChainParameters : Handle<aurk_ChainParameters, CChainParams> {};
struct aurk_ChainstateManagerOptions : Handle<aurk_ChainstateManagerOptions, ChainstateManagerOptions> {};
struct aurk_ChainstateManager : Handle<aurk_ChainstateManager, ChainMan> {};
struct aurk_Chain : Handle<aurk_Chain, CChain> {};
struct aurk_BlockSpentOutputs : Handle<aurk_BlockSpentOutputs, std::shared_ptr<CBlockUndo>> {};
struct aurk_TransactionSpentOutputs : Handle<aurk_TransactionSpentOutputs, CTxUndo> {};
struct aurk_Coin : Handle<aurk_Coin, Coin> {};
struct aurk_BlockHash : Handle<aurk_BlockHash, uint256> {};
struct aurk_TransactionInput : Handle<aurk_TransactionInput, CTxIn> {};
struct aurk_TransactionOutPoint: Handle<aurk_TransactionOutPoint, COutPoint> {};
struct aurk_Txid: Handle<aurk_Txid, Txid> {};
struct aurk_PrecomputedTransactionData : Handle<aurk_PrecomputedTransactionData, PrecomputedTransactionData> {};
struct aurk_BlockHeader: Handle<aurk_BlockHeader, CBlockHeader> {};

aurk_Transaction* aurk_transaction_create(const void* raw_transaction, size_t raw_transaction_len)
{
    if (raw_transaction == nullptr && raw_transaction_len != 0) {
        return nullptr;
    }
    try {
        DataStream stream{std::span{reinterpret_cast<const std::byte*>(raw_transaction), raw_transaction_len}};
        return aurk_Transaction::create(std::make_shared<const CTransaction>(deserialize, TX_WITH_WITNESS, stream));
    } catch (...) {
        return nullptr;
    }
}

size_t aurk_transaction_count_outputs(const aurk_Transaction* transaction)
{
    return aurk_Transaction::get(transaction)->vout.size();
}

const aurk_TransactionOutput* aurk_transaction_get_output_at(const aurk_Transaction* transaction, size_t output_index)
{
    const CTransaction& tx = *aurk_Transaction::get(transaction);
    assert(output_index < tx.vout.size());
    return aurk_TransactionOutput::ref(&tx.vout[output_index]);
}

size_t aurk_transaction_count_inputs(const aurk_Transaction* transaction)
{
    return aurk_Transaction::get(transaction)->vin.size();
}

const aurk_TransactionInput* aurk_transaction_get_input_at(const aurk_Transaction* transaction, size_t input_index)
{
    assert(input_index < aurk_Transaction::get(transaction)->vin.size());
    return aurk_TransactionInput::ref(&aurk_Transaction::get(transaction)->vin[input_index]);
}

const aurk_Txid* aurk_transaction_get_txid(const aurk_Transaction* transaction)
{
    return aurk_Txid::ref(&aurk_Transaction::get(transaction)->GetHash());
}

aurk_Transaction* aurk_transaction_copy(const aurk_Transaction* transaction)
{
    return aurk_Transaction::copy(transaction);
}

int aurk_transaction_to_bytes(const aurk_Transaction* transaction, aurk_WriteBytes writer, void* user_data)
{
    try {
        WriterStream ws{writer, user_data};
        ws << TX_WITH_WITNESS(aurk_Transaction::get(transaction));
        return 0;
    } catch (...) {
        return -1;
    }
}

void aurk_transaction_destroy(aurk_Transaction* transaction)
{
    delete transaction;
}

aurk_ScriptPubkey* aurk_script_pubkey_create(const void* script_pubkey, size_t script_pubkey_len)
{
    if (script_pubkey == nullptr && script_pubkey_len != 0) {
        return nullptr;
    }
    auto data = std::span{reinterpret_cast<const uint8_t*>(script_pubkey), script_pubkey_len};
    return aurk_ScriptPubkey::create(data.begin(), data.end());
}

int aurk_script_pubkey_to_bytes(const aurk_ScriptPubkey* script_pubkey_, aurk_WriteBytes writer, void* user_data)
{
    const auto& script_pubkey{aurk_ScriptPubkey::get(script_pubkey_)};
    return writer(script_pubkey.data(), script_pubkey.size(), user_data);
}

aurk_ScriptPubkey* aurk_script_pubkey_copy(const aurk_ScriptPubkey* script_pubkey)
{
    return aurk_ScriptPubkey::copy(script_pubkey);
}

void aurk_script_pubkey_destroy(aurk_ScriptPubkey* script_pubkey)
{
    delete script_pubkey;
}

aurk_TransactionOutput* aurk_transaction_output_create(const aurk_ScriptPubkey* script_pubkey, int64_t amount)
{
    return aurk_TransactionOutput::create(amount, aurk_ScriptPubkey::get(script_pubkey));
}

aurk_TransactionOutput* aurk_transaction_output_copy(const aurk_TransactionOutput* output)
{
    return aurk_TransactionOutput::copy(output);
}

const aurk_ScriptPubkey* aurk_transaction_output_get_script_pubkey(const aurk_TransactionOutput* output)
{
    return aurk_ScriptPubkey::ref(&aurk_TransactionOutput::get(output).scriptPubKey);
}

int64_t aurk_transaction_output_get_amount(const aurk_TransactionOutput* output)
{
    return aurk_TransactionOutput::get(output).nValue;
}

void aurk_transaction_output_destroy(aurk_TransactionOutput* output)
{
    delete output;
}

aurk_PrecomputedTransactionData* aurk_precomputed_transaction_data_create(
    const aurk_Transaction* tx_to,
    const aurk_TransactionOutput** spent_outputs_, size_t spent_outputs_len)
{
    try {
        const CTransaction& tx{*aurk_Transaction::get(tx_to)};
        auto txdata{aurk_PrecomputedTransactionData::create()};
        if (spent_outputs_ != nullptr && spent_outputs_len > 0) {
            assert(spent_outputs_len == tx.vin.size());
            std::vector<CTxOut> spent_outputs;
            spent_outputs.reserve(spent_outputs_len);
            for (size_t i = 0; i < spent_outputs_len; i++) {
                const CTxOut& tx_out{aurk_TransactionOutput::get(spent_outputs_[i])};
                spent_outputs.push_back(tx_out);
            }
            aurk_PrecomputedTransactionData::get(txdata).Init(tx, std::move(spent_outputs));
        } else {
            aurk_PrecomputedTransactionData::get(txdata).Init(tx, {});
        }

        return txdata;
    } catch (...) {
        return nullptr;
    }
}

aurk_PrecomputedTransactionData* aurk_precomputed_transaction_data_copy(const aurk_PrecomputedTransactionData* precomputed_txdata)
{
    return aurk_PrecomputedTransactionData::copy(precomputed_txdata);
}

void aurk_precomputed_transaction_data_destroy(aurk_PrecomputedTransactionData* precomputed_txdata)
{
    delete precomputed_txdata;
}

int aurk_script_pubkey_verify(const aurk_ScriptPubkey* script_pubkey,
                              const int64_t amount,
                              const aurk_Transaction* tx_to,
                              const aurk_PrecomputedTransactionData* precomputed_txdata,
                              const unsigned int input_index,
                              const aurk_ScriptVerificationFlags flags,
                              aurk_ScriptVerifyStatus* status)
{
    // Assert that all specified flags are part of the interface before continuing
    assert((flags & ~aurk_ScriptVerificationFlags_ALL) == 0);

    if (!is_valid_flag_combination(script_verify_flags::from_int(flags))) {
        if (status) *status = aurk_ScriptVerifyStatus_ERROR_INVALID_FLAGS_COMBINATION;
        return 0;
    }

    const CTransaction& tx{*aurk_Transaction::get(tx_to)};
    assert(input_index < tx.vin.size());

    const PrecomputedTransactionData& txdata{precomputed_txdata ? aurk_PrecomputedTransactionData::get(precomputed_txdata) : PrecomputedTransactionData(tx)};

    if (flags & aurk_ScriptVerificationFlags_TAPROOT && txdata.m_spent_outputs.empty()) {
        if (status) *status = aurk_ScriptVerifyStatus_ERROR_SPENT_OUTPUTS_REQUIRED;
        return 0;
    }

    if (status) *status = aurk_ScriptVerifyStatus_OK;

    bool result = VerifyScript(tx.vin[input_index].scriptSig,
                               aurk_ScriptPubkey::get(script_pubkey),
                               &tx.vin[input_index].scriptWitness,
                               script_verify_flags::from_int(flags),
                               TransactionSignatureChecker(&tx, input_index, amount, txdata, MissingDataBehavior::FAIL),
                               nullptr);
    return result ? 1 : 0;
}

aurk_TransactionInput* aurk_transaction_input_copy(const aurk_TransactionInput* input)
{
    return aurk_TransactionInput::copy(input);
}

const aurk_TransactionOutPoint* aurk_transaction_input_get_out_point(const aurk_TransactionInput* input)
{
    return aurk_TransactionOutPoint::ref(&aurk_TransactionInput::get(input).prevout);
}

void aurk_transaction_input_destroy(aurk_TransactionInput* input)
{
    delete input;
}

aurk_TransactionOutPoint* aurk_transaction_out_point_copy(const aurk_TransactionOutPoint* out_point)
{
    return aurk_TransactionOutPoint::copy(out_point);
}

uint32_t aurk_transaction_out_point_get_index(const aurk_TransactionOutPoint* out_point)
{
    return aurk_TransactionOutPoint::get(out_point).n;
}

const aurk_Txid* aurk_transaction_out_point_get_txid(const aurk_TransactionOutPoint* out_point)
{
    return aurk_Txid::ref(&aurk_TransactionOutPoint::get(out_point).hash);
}

void aurk_transaction_out_point_destroy(aurk_TransactionOutPoint* out_point)
{
    delete out_point;
}

aurk_Txid* aurk_txid_copy(const aurk_Txid* txid)
{
    return aurk_Txid::copy(txid);
}

void aurk_txid_to_bytes(const aurk_Txid* txid, unsigned char output[32])
{
    std::memcpy(output, aurk_Txid::get(txid).begin(), 32);
}

int aurk_txid_equals(const aurk_Txid* txid1, const aurk_Txid* txid2)
{
    return aurk_Txid::get(txid1) == aurk_Txid::get(txid2);
}

void aurk_txid_destroy(aurk_Txid* txid)
{
    delete txid;
}

void aurk_logging_set_options(const aurk_LoggingOptions options)
{
    LOCK(cs_main);
    LogInstance().m_log_timestamps = options.log_timestamps;
    LogInstance().m_log_time_micros = options.log_time_micros;
    LogInstance().m_log_threadnames = options.log_threadnames;
    LogInstance().m_log_sourcelocations = options.log_sourcelocations;
    LogInstance().m_always_print_category_level = options.always_print_category_levels;
}

void aurk_logging_set_level_category(aurk_LogCategory category, aurk_LogLevel level)
{
    LOCK(cs_main);
    if (category == aurk_LogCategory_ALL) {
        LogInstance().SetLogLevel(get_bclog_level(level));
    }

    LogInstance().AddCategoryLogLevel(get_bclog_flag(category), get_bclog_level(level));
}

void aurk_logging_enable_category(aurk_LogCategory category)
{
    LogInstance().EnableCategory(get_bclog_flag(category));
}

void aurk_logging_disable_category(aurk_LogCategory category)
{
    LogInstance().DisableCategory(get_bclog_flag(category));
}

void aurk_logging_disable()
{
    LogInstance().DisableLogging();
}

aurk_LoggingConnection* aurk_logging_connection_create(aurk_LogCallback callback, void* user_data, aurk_DestroyCallback user_data_destroy_callback)
{
    try {
        return aurk_LoggingConnection::create(callback, user_data, user_data_destroy_callback);
    } catch (const std::exception&) {
        return nullptr;
    }
}

void aurk_logging_connection_destroy(aurk_LoggingConnection* connection)
{
    delete connection;
}

aurk_ChainParameters* aurk_chain_parameters_create(const aurk_ChainType chain_type)
{
    switch (chain_type) {
    case aurk_ChainType_MAINNET: {
        return aurk_ChainParameters::ref(const_cast<CChainParams*>(CChainParams::Main().release()));
    }
    case aurk_ChainType_TESTNET: {
        return aurk_ChainParameters::ref(const_cast<CChainParams*>(CChainParams::TestNet().release()));
    }
    case aurk_ChainType_TESTNET_4: {
        return aurk_ChainParameters::ref(const_cast<CChainParams*>(CChainParams::TestNet4().release()));
    }
    case aurk_ChainType_SIGNET: {
        return aurk_ChainParameters::ref(const_cast<CChainParams*>(CChainParams::SigNet({}).release()));
    }
    case aurk_ChainType_REGTEST: {
        return aurk_ChainParameters::ref(const_cast<CChainParams*>(CChainParams::RegTest({}).release()));
    }
    }
    assert(false);
}

aurk_ChainParameters* aurk_chain_parameters_copy(const aurk_ChainParameters* chain_parameters)
{
    return aurk_ChainParameters::copy(chain_parameters);
}

void aurk_chain_parameters_destroy(aurk_ChainParameters* chain_parameters)
{
    delete chain_parameters;
}

aurk_ContextOptions* aurk_context_options_create()
{
    return aurk_ContextOptions::create();
}

void aurk_context_options_set_chainparams(aurk_ContextOptions* options, const aurk_ChainParameters* chain_parameters)
{
    // Copy the chainparams, so the caller can free it again
    LOCK(aurk_ContextOptions::get(options).m_mutex);
    aurk_ContextOptions::get(options).m_chainparams = std::make_unique<const CChainParams>(aurk_ChainParameters::get(chain_parameters));
}

void aurk_context_options_set_notifications(aurk_ContextOptions* options, aurk_NotificationInterfaceCallbacks notifications)
{
    // The KernelNotifications are copy-initialized, so the caller can free them again.
    LOCK(aurk_ContextOptions::get(options).m_mutex);
    aurk_ContextOptions::get(options).m_notifications = std::make_shared<KernelNotifications>(notifications);
}

void aurk_context_options_set_validation_interface(aurk_ContextOptions* options, aurk_ValidationInterfaceCallbacks vi_cbs)
{
    LOCK(aurk_ContextOptions::get(options).m_mutex);
    aurk_ContextOptions::get(options).m_validation_interface = std::make_shared<KernelValidationInterface>(vi_cbs);
}

void aurk_context_options_destroy(aurk_ContextOptions* options)
{
    delete options;
}

aurk_Context* aurk_context_create(const aurk_ContextOptions* options)
{
    bool sane{true};
    const ContextOptions* opts = options ? &aurk_ContextOptions::get(options) : nullptr;
    auto context{std::make_shared<const Context>(opts, sane)};
    if (!sane) {
        LogError("Kernel context sanity check failed.");
        return nullptr;
    }
    return aurk_Context::create(context);
}

aurk_Context* aurk_context_copy(const aurk_Context* context)
{
    return aurk_Context::copy(context);
}

int aurk_context_interrupt(aurk_Context* context)
{
    return (*aurk_Context::get(context)->m_interrupt)() ? 0 : -1;
}

void aurk_context_destroy(aurk_Context* context)
{
    delete context;
}

const aurk_BlockTreeEntry* aurk_block_tree_entry_get_previous(const aurk_BlockTreeEntry* entry)
{
    if (!aurk_BlockTreeEntry::get(entry).pprev) {
        LogInfo("Genesis block has no previous.");
        return nullptr;
    }

    return aurk_BlockTreeEntry::ref(aurk_BlockTreeEntry::get(entry).pprev);
}

aurk_BlockValidationState* aurk_block_validation_state_create()
{
    return aurk_BlockValidationState::create();
}

aurk_BlockValidationState* aurk_block_validation_state_copy(const aurk_BlockValidationState* state)
{
    return aurk_BlockValidationState::copy(state);
}

void aurk_block_validation_state_destroy(aurk_BlockValidationState* state)
{
    delete state;
}

aurk_ValidationMode aurk_block_validation_state_get_validation_mode(const aurk_BlockValidationState* block_validation_state_)
{
    auto& block_validation_state = aurk_BlockValidationState::get(block_validation_state_);
    if (block_validation_state.IsValid()) return aurk_ValidationMode_VALID;
    if (block_validation_state.IsInvalid()) return aurk_ValidationMode_INVALID;
    return aurk_ValidationMode_INTERNAL_ERROR;
}

aurk_BlockValidationResult aurk_block_validation_state_get_block_validation_result(const aurk_BlockValidationState* block_validation_state_)
{
    auto& block_validation_state = aurk_BlockValidationState::get(block_validation_state_);
    switch (block_validation_state.GetResult()) {
    case BlockValidationResult::BLOCK_RESULT_UNSET:
        return aurk_BlockValidationResult_UNSET;
    case BlockValidationResult::BLOCK_CONSENSUS:
        return aurk_BlockValidationResult_CONSENSUS;
    case BlockValidationResult::BLOCK_CACHED_INVALID:
        return aurk_BlockValidationResult_CACHED_INVALID;
    case BlockValidationResult::BLOCK_INVALID_HEADER:
        return aurk_BlockValidationResult_INVALID_HEADER;
    case BlockValidationResult::BLOCK_MUTATED:
        return aurk_BlockValidationResult_MUTATED;
    case BlockValidationResult::BLOCK_MISSING_PREV:
        return aurk_BlockValidationResult_MISSING_PREV;
    case BlockValidationResult::BLOCK_INVALID_PREV:
        return aurk_BlockValidationResult_INVALID_PREV;
    case BlockValidationResult::BLOCK_TIME_FUTURE:
        return aurk_BlockValidationResult_TIME_FUTURE;
    case BlockValidationResult::BLOCK_HEADER_LOW_WORK:
        return aurk_BlockValidationResult_HEADER_LOW_WORK;
    } // no default case, so the compiler can warn about missing cases
    assert(false);
}

aurk_ChainstateManagerOptions* aurk_chainstate_manager_options_create(const aurk_Context* context, const char* data_dir, size_t data_dir_len, const char* blocks_dir, size_t blocks_dir_len)
{
    if (data_dir == nullptr || data_dir_len == 0 || blocks_dir == nullptr || blocks_dir_len == 0) {
        LogError("Failed to create chainstate manager options: dir must be non-null and non-empty");
        return nullptr;
    }
    try {
        fs::path abs_data_dir{fs::absolute(fs::PathFromString({data_dir, data_dir_len}))};
        fs::create_directories(abs_data_dir);
        fs::path abs_blocks_dir{fs::absolute(fs::PathFromString({blocks_dir, blocks_dir_len}))};
        fs::create_directories(abs_blocks_dir);
        return aurk_ChainstateManagerOptions::create(aurk_Context::get(context), abs_data_dir, abs_blocks_dir);
    } catch (const std::exception& e) {
        LogError("Failed to create chainstate manager options: %s", e.what());
        return nullptr;
    }
}

void aurk_chainstate_manager_options_set_worker_threads_num(aurk_ChainstateManagerOptions* opts, int worker_threads)
{
    LOCK(aurk_ChainstateManagerOptions::get(opts).m_mutex);
    aurk_ChainstateManagerOptions::get(opts).m_chainman_options.worker_threads_num = worker_threads;
}

void aurk_chainstate_manager_options_destroy(aurk_ChainstateManagerOptions* options)
{
    delete options;
}

int aurk_chainstate_manager_options_set_wipe_dbs(aurk_ChainstateManagerOptions* chainman_opts, int wipe_block_tree_db, int wipe_chainstate_db)
{
    if (wipe_block_tree_db == 1 && wipe_chainstate_db != 1) {
        LogError("Wiping the block tree db without also wiping the chainstate db is currently unsupported.");
        return -1;
    }
    auto& opts{aurk_ChainstateManagerOptions::get(chainman_opts)};
    LOCK(opts.m_mutex);
    opts.m_blockman_options.block_tree_db_params.wipe_data = wipe_block_tree_db == 1;
    opts.m_chainstate_load_options.wipe_chainstate_db = wipe_chainstate_db == 1;
    return 0;
}

void aurk_chainstate_manager_options_update_block_tree_db_in_memory(
    aurk_ChainstateManagerOptions* chainman_opts,
    int block_tree_db_in_memory)
{
    auto& opts{aurk_ChainstateManagerOptions::get(chainman_opts)};
    LOCK(opts.m_mutex);
    opts.m_blockman_options.block_tree_db_params.memory_only = block_tree_db_in_memory == 1;
}

void aurk_chainstate_manager_options_update_chainstate_db_in_memory(
    aurk_ChainstateManagerOptions* chainman_opts,
    int chainstate_db_in_memory)
{
    auto& opts{aurk_ChainstateManagerOptions::get(chainman_opts)};
    LOCK(opts.m_mutex);
    opts.m_chainstate_load_options.coins_db_in_memory = chainstate_db_in_memory == 1;
}

aurk_ChainstateManager* aurk_chainstate_manager_create(
    const aurk_ChainstateManagerOptions* chainman_opts)
{
    auto& opts{aurk_ChainstateManagerOptions::get(chainman_opts)};
    std::unique_ptr<ChainstateManager> chainman;
    try {
        LOCK(opts.m_mutex);
        chainman = std::make_unique<ChainstateManager>(*opts.m_context->m_interrupt, opts.m_chainman_options, opts.m_blockman_options);
    } catch (const std::exception& e) {
        LogError("Failed to create chainstate manager: %s", e.what());
        return nullptr;
    }

    try {
        const auto chainstate_load_opts{WITH_LOCK(opts.m_mutex, return opts.m_chainstate_load_options)};

        kernel::CacheSizes cache_sizes{DEFAULT_KERNEL_CACHE};
        auto [status, chainstate_err]{node::LoadChainstate(*chainman, cache_sizes, chainstate_load_opts)};
        if (status != node::ChainstateLoadStatus::SUCCESS) {
            LogError("Failed to load chain state from your data directory: %s", chainstate_err.original);
            return nullptr;
        }
        std::tie(status, chainstate_err) = node::VerifyLoadedChainstate(*chainman, chainstate_load_opts);
        if (status != node::ChainstateLoadStatus::SUCCESS) {
            LogError("Failed to verify loaded chain state from your datadir: %s", chainstate_err.original);
            return nullptr;
        }
        if (auto result = chainman->ActivateBestChains(); !result) {
            LogError("%s", util::ErrorString(result).original);
            return nullptr;
        }
    } catch (const std::exception& e) {
        LogError("Failed to load chainstate: %s", e.what());
        return nullptr;
    }

    return aurk_ChainstateManager::create(std::move(chainman), opts.m_context);
}

const aurk_BlockTreeEntry* aurk_chainstate_manager_get_block_tree_entry_by_hash(const aurk_ChainstateManager* chainman, const aurk_BlockHash* block_hash)
{
    auto block_index = WITH_LOCK(aurk_ChainstateManager::get(chainman).m_chainman->GetMutex(),
                                 return aurk_ChainstateManager::get(chainman).m_chainman->m_blockman.LookupBlockIndex(aurk_BlockHash::get(block_hash)));
    if (!block_index) {
        LogDebug(BCLog::KERNEL, "A block with the given hash is not indexed.");
        return nullptr;
    }
    return aurk_BlockTreeEntry::ref(block_index);
}

const aurk_BlockTreeEntry* aurk_chainstate_manager_get_best_entry(const aurk_ChainstateManager* chainstate_manager)
{
    auto& chainman = *aurk_ChainstateManager::get(chainstate_manager).m_chainman;
    return aurk_BlockTreeEntry::ref(WITH_LOCK(chainman.GetMutex(), return chainman.m_best_header));
}

void aurk_chainstate_manager_destroy(aurk_ChainstateManager* chainman)
{
    {
        LOCK(aurk_ChainstateManager::get(chainman).m_chainman->GetMutex());
        for (const auto& chainstate : aurk_ChainstateManager::get(chainman).m_chainman->m_chainstates) {
            if (chainstate->CanFlushToDisk()) {
                chainstate->ForceFlushStateToDisk();
                chainstate->ResetCoinsViews();
            }
        }
    }

    delete chainman;
}

int aurk_chainstate_manager_import_blocks(aurk_ChainstateManager* chainman, const char** block_file_paths_data, size_t* block_file_paths_lens, size_t block_file_paths_data_len)
{
    try {
        std::vector<fs::path> import_files;
        import_files.reserve(block_file_paths_data_len);
        for (uint32_t i = 0; i < block_file_paths_data_len; i++) {
            if (block_file_paths_data[i] != nullptr) {
                import_files.emplace_back(std::string{block_file_paths_data[i], block_file_paths_lens[i]}.c_str());
            }
        }
        auto& chainman_ref{*aurk_ChainstateManager::get(chainman).m_chainman};
        node::ImportBlocks(chainman_ref, import_files);
        WITH_LOCK(::cs_main, chainman_ref.UpdateIBDStatus());
    } catch (const std::exception& e) {
        LogError("Failed to import blocks: %s", e.what());
        return -1;
    }
    return 0;
}

aurk_Block* aurk_block_create(const void* raw_block, size_t raw_block_length)
{
    if (raw_block == nullptr && raw_block_length != 0) {
        return nullptr;
    }
    auto block{std::make_shared<CBlock>()};

    DataStream stream{std::span{reinterpret_cast<const std::byte*>(raw_block), raw_block_length}};

    try {
        stream >> TX_WITH_WITNESS(*block);
    } catch (...) {
        LogDebug(BCLog::KERNEL, "Block decode failed.");
        return nullptr;
    }

    return aurk_Block::create(block);
}

aurk_Block* aurk_block_copy(const aurk_Block* block)
{
    return aurk_Block::copy(block);
}

size_t aurk_block_count_transactions(const aurk_Block* block)
{
    return aurk_Block::get(block)->vtx.size();
}

const aurk_Transaction* aurk_block_get_transaction_at(const aurk_Block* block, size_t index)
{
    assert(index < aurk_Block::get(block)->vtx.size());
    return aurk_Transaction::ref(&aurk_Block::get(block)->vtx[index]);
}

aurk_BlockHeader* aurk_block_get_header(const aurk_Block* block)
{
    const auto& block_ptr = aurk_Block::get(block);
    return aurk_BlockHeader::create(static_cast<const CBlockHeader&>(*block_ptr));
}

int aurk_block_to_bytes(const aurk_Block* block, aurk_WriteBytes writer, void* user_data)
{
    try {
        WriterStream ws{writer, user_data};
        ws << TX_WITH_WITNESS(*aurk_Block::get(block));
        return 0;
    } catch (...) {
        return -1;
    }
}

aurk_BlockHash* aurk_block_get_hash(const aurk_Block* block)
{
    return aurk_BlockHash::create(aurk_Block::get(block)->GetHash());
}

void aurk_block_destroy(aurk_Block* block)
{
    delete block;
}

aurk_Block* aurk_block_read(const aurk_ChainstateManager* chainman, const aurk_BlockTreeEntry* entry)
{
    auto block{std::make_shared<CBlock>()};
    if (!aurk_ChainstateManager::get(chainman).m_chainman->m_blockman.ReadBlock(*block, aurk_BlockTreeEntry::get(entry))) {
        LogError("Failed to read block.");
        return nullptr;
    }
    return aurk_Block::create(block);
}

aurk_BlockHeader* aurk_block_tree_entry_get_block_header(const aurk_BlockTreeEntry* entry)
{
    return aurk_BlockHeader::create(aurk_BlockTreeEntry::get(entry).GetBlockHeader());
}

int32_t aurk_block_tree_entry_get_height(const aurk_BlockTreeEntry* entry)
{
    return aurk_BlockTreeEntry::get(entry).nHeight;
}

const aurk_BlockHash* aurk_block_tree_entry_get_block_hash(const aurk_BlockTreeEntry* entry)
{
    return aurk_BlockHash::ref(aurk_BlockTreeEntry::get(entry).phashBlock);
}

int aurk_block_tree_entry_equals(const aurk_BlockTreeEntry* entry1, const aurk_BlockTreeEntry* entry2)
{
    return &aurk_BlockTreeEntry::get(entry1) == &aurk_BlockTreeEntry::get(entry2);
}

aurk_BlockHash* aurk_block_hash_create(const unsigned char block_hash[32])
{
    return aurk_BlockHash::create(std::span<const unsigned char>{block_hash, 32});
}

aurk_BlockHash* aurk_block_hash_copy(const aurk_BlockHash* block_hash)
{
    return aurk_BlockHash::copy(block_hash);
}

void aurk_block_hash_to_bytes(const aurk_BlockHash* block_hash, unsigned char output[32])
{
    std::memcpy(output, aurk_BlockHash::get(block_hash).begin(), 32);
}

int aurk_block_hash_equals(const aurk_BlockHash* hash1, const aurk_BlockHash* hash2)
{
    return aurk_BlockHash::get(hash1) == aurk_BlockHash::get(hash2);
}

void aurk_block_hash_destroy(aurk_BlockHash* hash)
{
    delete hash;
}

aurk_BlockSpentOutputs* aurk_block_spent_outputs_read(const aurk_ChainstateManager* chainman, const aurk_BlockTreeEntry* entry)
{
    auto block_undo{std::make_shared<CBlockUndo>()};
    if (aurk_BlockTreeEntry::get(entry).nHeight < 1) {
        LogDebug(BCLog::KERNEL, "The genesis block does not have any spent outputs.");
        return aurk_BlockSpentOutputs::create(block_undo);
    }
    if (!aurk_ChainstateManager::get(chainman).m_chainman->m_blockman.ReadBlockUndo(*block_undo, aurk_BlockTreeEntry::get(entry))) {
        LogError("Failed to read block spent outputs data.");
        return nullptr;
    }
    return aurk_BlockSpentOutputs::create(block_undo);
}

aurk_BlockSpentOutputs* aurk_block_spent_outputs_copy(const aurk_BlockSpentOutputs* block_spent_outputs)
{
    return aurk_BlockSpentOutputs::copy(block_spent_outputs);
}

size_t aurk_block_spent_outputs_count(const aurk_BlockSpentOutputs* block_spent_outputs)
{
    return aurk_BlockSpentOutputs::get(block_spent_outputs)->vtxundo.size();
}

const aurk_TransactionSpentOutputs* aurk_block_spent_outputs_get_transaction_spent_outputs_at(const aurk_BlockSpentOutputs* block_spent_outputs, size_t transaction_index)
{
    assert(transaction_index < aurk_BlockSpentOutputs::get(block_spent_outputs)->vtxundo.size());
    const auto* tx_undo{&aurk_BlockSpentOutputs::get(block_spent_outputs)->vtxundo.at(transaction_index)};
    return aurk_TransactionSpentOutputs::ref(tx_undo);
}

void aurk_block_spent_outputs_destroy(aurk_BlockSpentOutputs* block_spent_outputs)
{
    delete block_spent_outputs;
}

aurk_TransactionSpentOutputs* aurk_transaction_spent_outputs_copy(const aurk_TransactionSpentOutputs* transaction_spent_outputs)
{
    return aurk_TransactionSpentOutputs::copy(transaction_spent_outputs);
}

size_t aurk_transaction_spent_outputs_count(const aurk_TransactionSpentOutputs* transaction_spent_outputs)
{
    return aurk_TransactionSpentOutputs::get(transaction_spent_outputs).vprevout.size();
}

void aurk_transaction_spent_outputs_destroy(aurk_TransactionSpentOutputs* transaction_spent_outputs)
{
    delete transaction_spent_outputs;
}

const aurk_Coin* aurk_transaction_spent_outputs_get_coin_at(const aurk_TransactionSpentOutputs* transaction_spent_outputs, size_t coin_index)
{
    assert(coin_index < aurk_TransactionSpentOutputs::get(transaction_spent_outputs).vprevout.size());
    const Coin* coin{&aurk_TransactionSpentOutputs::get(transaction_spent_outputs).vprevout.at(coin_index)};
    return aurk_Coin::ref(coin);
}

aurk_Coin* aurk_coin_copy(const aurk_Coin* coin)
{
    return aurk_Coin::copy(coin);
}

uint32_t aurk_coin_confirmation_height(const aurk_Coin* coin)
{
    return aurk_Coin::get(coin).nHeight;
}

int aurk_coin_is_coinbase(const aurk_Coin* coin)
{
    return aurk_Coin::get(coin).IsCoinBase() ? 1 : 0;
}

const aurk_TransactionOutput* aurk_coin_get_output(const aurk_Coin* coin)
{
    return aurk_TransactionOutput::ref(&aurk_Coin::get(coin).out);
}

void aurk_coin_destroy(aurk_Coin* coin)
{
    delete coin;
}

int aurk_chainstate_manager_process_block(
    aurk_ChainstateManager* chainman,
    const aurk_Block* block,
    int* _new_block)
{
    bool new_block;
    auto result = aurk_ChainstateManager::get(chainman).m_chainman->ProcessNewBlock(aurk_Block::get(block), /*force_processing=*/true, /*min_pow_checked=*/true, /*new_block=*/&new_block);
    if (_new_block) {
        *_new_block = new_block ? 1 : 0;
    }
    return result ? 0 : -1;
}

int aurk_chainstate_manager_process_block_header(
    aurk_ChainstateManager* chainstate_manager,
    const aurk_BlockHeader* header,
    aurk_BlockValidationState* state)
{
    try {
        auto& chainman = aurk_ChainstateManager::get(chainstate_manager).m_chainman;
        auto result = chainman->ProcessNewBlockHeaders({&aurk_BlockHeader::get(header), 1}, /*min_pow_checked=*/true, aurk_BlockValidationState::get(state), /*ppindex=*/nullptr);

        return result ? 0 : -1;
    } catch (const std::exception& e) {
        LogError("Failed to process block header: %s", e.what());
        return -1;
    }
}

const aurk_Chain* aurk_chainstate_manager_get_active_chain(const aurk_ChainstateManager* chainman)
{
    return aurk_Chain::ref(&WITH_LOCK(aurk_ChainstateManager::get(chainman).m_chainman->GetMutex(), return aurk_ChainstateManager::get(chainman).m_chainman->ActiveChain()));
}

int aurk_chain_get_height(const aurk_Chain* chain)
{
    LOCK(::cs_main);
    return aurk_Chain::get(chain).Height();
}

const aurk_BlockTreeEntry* aurk_chain_get_by_height(const aurk_Chain* chain, int height)
{
    LOCK(::cs_main);
    return aurk_BlockTreeEntry::ref(aurk_Chain::get(chain)[height]);
}

int aurk_chain_contains(const aurk_Chain* chain, const aurk_BlockTreeEntry* entry)
{
    LOCK(::cs_main);
    return aurk_Chain::get(chain).Contains(&aurk_BlockTreeEntry::get(entry)) ? 1 : 0;
}

aurk_BlockHeader* aurk_block_header_create(const void* raw_block_header, size_t raw_block_header_len)
{
    if (raw_block_header == nullptr && raw_block_header_len != 0) {
        return nullptr;
    }
    auto header{std::make_unique<CBlockHeader>()};
    DataStream stream{std::span{reinterpret_cast<const std::byte*>(raw_block_header), raw_block_header_len}};

    try {
        stream >> *header;
    } catch (...) {
        LogError("Block header decode failed.");
        return nullptr;
    }

    return aurk_BlockHeader::ref(header.release());
}

aurk_BlockHeader* aurk_block_header_copy(const aurk_BlockHeader* header)
{
    return aurk_BlockHeader::copy(header);
}

aurk_BlockHash* aurk_block_header_get_hash(const aurk_BlockHeader* header)
{
    return aurk_BlockHash::create(aurk_BlockHeader::get(header).GetHash());
}

const aurk_BlockHash* aurk_block_header_get_prev_hash(const aurk_BlockHeader* header)
{
    return aurk_BlockHash::ref(&aurk_BlockHeader::get(header).hashPrevBlock);
}

uint32_t aurk_block_header_get_timestamp(const aurk_BlockHeader* header)
{
    return aurk_BlockHeader::get(header).nTime;
}

uint32_t aurk_block_header_get_bits(const aurk_BlockHeader* header)
{
    return aurk_BlockHeader::get(header).nBits;
}

int32_t aurk_block_header_get_version(const aurk_BlockHeader* header)
{
    return aurk_BlockHeader::get(header).nVersion;
}

uint32_t aurk_block_header_get_nonce(const aurk_BlockHeader* header)
{
    return aurk_BlockHeader::get(header).nNonce;
}

void aurk_block_header_destroy(aurk_BlockHeader* header)
{
    delete header;
}
