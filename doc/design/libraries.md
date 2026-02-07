# Libraries

| Name                     | Description |
|--------------------------|-------------|
| *libaureus_cli*         | RPC client functionality used by *aureus-cli* executable |
| *libaureus_common*      | Home for common functionality shared by different executables and libraries. Similar to *libaureus_util*, but higher-level (see [Dependencies](#dependencies)). |
| *libaureus_consensus*   | Consensus functionality used by *libaureus_node* and *libaureus_wallet*. |
| *libaureus_crypto*      | Hardware-optimized functions for data encryption, hashing, message authentication, and key derivation. |
| *libaureus_kernel*      | Consensus engine and support library used for validation by *libaureus_node*. |
| *libaureusqt*           | GUI functionality used by *aureus-qt* and *aureus-gui* executables. |
| *libaureus_ipc*         | IPC functionality used by *aureus-node* and *aureus-gui* executables to communicate when [`-DENABLE_IPC=ON`](multiprocess.md) is used. |
| *libaureus_node*        | P2P and RPC server functionality used by *aureusd* and *aureus-qt* executables. |
| *libaureus_util*        | Home for common functionality shared by different executables and libraries. Similar to *libaureus_common*, but lower-level (see [Dependencies](#dependencies)). |
| *libaureus_wallet*      | Wallet functionality used by *aureusd* and *aureus-wallet* executables. |
| *libaureus_wallet_tool* | Lower-level wallet functionality used by *aureus-wallet* executable. |
| *libaureus_zmq*         | [ZeroMQ](../zmq.md) functionality used by *aureusd* and *aureus-qt* executables. |

## Conventions

- Most libraries are internal libraries and have APIs which are completely unstable! There are few or no restrictions on backwards compatibility or rules about external dependencies. An exception is *libaureus_kernel*, which, at some future point, will have a documented external interface.

- Generally each library should have a corresponding source directory and namespace. Source code organization is a work in progress, so it is true that some namespaces are applied inconsistently, and if you look at [`add_library(aureus_* ...)`](../../src/CMakeLists.txt) lists you can see that many libraries pull in files from outside their source directory. But when working with libraries, it is good to follow a consistent pattern like:

  - *libaureus_node* code lives in `src/node/` in the `node::` namespace
  - *libaureus_wallet* code lives in `src/wallet/` in the `wallet::` namespace
  - *libaureus_ipc* code lives in `src/ipc/` in the `ipc::` namespace
  - *libaureus_util* code lives in `src/util/` in the `util::` namespace
  - *libaureus_consensus* code lives in `src/consensus/` in the `Consensus::` namespace

## Dependencies

- Libraries should minimize what other libraries they depend on, and only reference symbols following the arrows shown in the dependency graph below:

<table><tr><td>

```mermaid

%%{ init : { "flowchart" : { "curve" : "basis" }}}%%

graph TD;

aureus-cli[aureus-cli]-->libaureus_cli;

aureusd[aureusd]-->libaureus_node;
aureusd[aureusd]-->libaureus_wallet;

aureus-qt[aureus-qt]-->libaureus_node;
aureus-qt[aureus-qt]-->libaureusqt;
aureus-qt[aureus-qt]-->libaureus_wallet;

aureus-wallet[aureus-wallet]-->libaureus_wallet;
aureus-wallet[aureus-wallet]-->libaureus_wallet_tool;

libaureus_cli-->libaureus_util;
libaureus_cli-->libaureus_common;

libaureus_consensus-->libaureus_crypto;

libaureus_common-->libaureus_consensus;
libaureus_common-->libaureus_crypto;
libaureus_common-->libaureus_util;

libaureus_kernel-->libaureus_consensus;
libaureus_kernel-->libaureus_crypto;
libaureus_kernel-->libaureus_util;

libaureus_node-->libaureus_consensus;
libaureus_node-->libaureus_crypto;
libaureus_node-->libaureus_kernel;
libaureus_node-->libaureus_common;
libaureus_node-->libaureus_util;

libaureusqt-->libaureus_common;
libaureusqt-->libaureus_util;

libaureus_util-->libaureus_crypto;

libaureus_wallet-->libaureus_common;
libaureus_wallet-->libaureus_crypto;
libaureus_wallet-->libaureus_util;

libaureus_wallet_tool-->libaureus_wallet;
libaureus_wallet_tool-->libaureus_util;

classDef bold stroke-width:2px, font-weight:bold, font-size: smaller;
class aureus-qt,aureusd,aureus-cli,aureus-wallet bold
```
</td></tr><tr><td>

**Dependency graph**. Arrows show linker symbol dependencies. *Crypto* lib depends on nothing. *Util* lib is depended on by everything. *Kernel* lib depends only on consensus, crypto, and util.

</td></tr></table>

- The graph shows what _linker symbols_ (functions and variables) from each library other libraries can call and reference directly, but it is not a call graph. For example, there is no arrow connecting *libaureus_wallet* and *libaureus_node* libraries, because these libraries are intended to be modular and not depend on each other's internal implementation details. But wallet code is still able to call node code indirectly through the `interfaces::Chain` abstract class in [`interfaces/chain.h`](../../src/interfaces/chain.h) and node code calls wallet code through the `interfaces::ChainClient` and `interfaces::Chain::Notifications` abstract classes in the same file. In general, defining abstract classes in [`src/interfaces/`](../../src/interfaces/) can be a convenient way of avoiding unwanted direct dependencies or circular dependencies between libraries.

- *libaureus_crypto* should be a standalone dependency that any library can depend on, and it should not depend on any other libraries itself.

- *libaureus_consensus* should only depend on *libaureus_crypto*, and all other libraries besides *libaureus_crypto* should be allowed to depend on it.

- *libaureus_util* should be a standalone dependency that any library can depend on, and it should not depend on other libraries except *libaureus_crypto*. It provides basic utilities that fill in gaps in the C++ standard library and provide lightweight abstractions over platform-specific features. Since the util library is distributed with the kernel and is usable by kernel applications, it shouldn't contain functions that external code shouldn't call, like higher level code targeted at the node or wallet. (*libaureus_common* is a better place for higher level code, or code that is meant to be used by internal applications only.)

- *libaureus_common* is a home for miscellaneous shared code used by different Aureus Core applications. It should not depend on anything other than *libaureus_util*, *libaureus_consensus*, and *libaureus_crypto*.

- *libaureus_kernel* should only depend on *libaureus_util*, *libaureus_consensus*, and *libaureus_crypto*.

- The only thing that should depend on *libaureus_kernel* internally should be *libaureus_node*. GUI and wallet libraries *libaureusqt* and *libaureus_wallet* in particular should not depend on *libaureus_kernel* and the unneeded functionality it would pull in, like block validation. To the extent that GUI and wallet code need scripting and signing functionality, they should be able to get it from *libaureus_consensus*, *libaureus_common*, *libaureus_crypto*, and *libaureus_util*, instead of *libaureus_kernel*.

- GUI, node, and wallet code internal implementations should all be independent of each other, and the *libaureusqt*, *libaureus_node*, *libaureus_wallet* libraries should never reference each other's symbols. They should only call each other through [`src/interfaces/`](../../src/interfaces/) abstract interfaces.

## Work in progress

- Validation code is moving from *libaureus_node* to *libaureus_kernel* as part of [The libaureuskernel Project #27587](https://github.com/aureus/aureus/issues/27587)
