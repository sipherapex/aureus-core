# Aureus Core [AUR] 
### The Institutional-Grade SHA-256 Layer 1 Blockchain

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Build Status](https://img.shields.io/badge/Build-Passing-brightgreen.svg)]()
[![Version](https://img.shields.io/badge/Release-v0.2x.Stable-orange.svg)]()

Aureus Core is a high-performance, security-centric Layer 1 blockchain designed for the next generation of decentralized value transfer. Built on a modernized Bitcoin Core architecture, Aureus combines the industrial-strength security of **SHA-256 Proof-of-Work** with advanced cryptographic features like **Taproot**, **SegWit**, and **V2 P2P Transport Encryption**.

---

## 🌐 Official Resources
| Resource | Link |
| :--- | :--- |
| **Official Website** | [https://auracoin.gt.tc](https://auracoin.gt.tc) |
| **Block Explorer** | [https://aurascan.gt.tc](https://aurascan.gt.tc) |
| **Source Code** | [https://github.com/sipherapex/aureus-core](https://github.com/sipherapex/aureus-core) |
| **Technical Documentation** | [Explore /doc folder](https://github.com/sipherapex/aureus-core/tree/master/doc) |

---

## ⚙️ Technical Specifications
* **Ticker:** `AUR`
* **Algorithm:** SHA-256 (ASIC-Optimized)
* **Max Supply:** 31000000 AUR
* **Block Maturity:** 100 Confirmations
* **Network Ports:** P2P: `11995` 

---

## 🚀 Key Features
* **Taproot & Schnorr Signatures:** Enhanced privacy and smart contract efficiency.
* **V2 P2P Transport:** Native node-to-node encryption (BIP324) to prevent traffic analysis and MitM attacks.
* **Modern Descriptor Wallets:** Moving beyond legacy `wallet.dat` for robust hardware and multi-sig support.
* **Strategic Ecosystem Reserve:** 500,000 AUR dedicated to infrastructure, exchange liquidity, and long-term network stability.

---

## 🛠 Installation & Build Support

### Windows (Pre-compiled)
Download the latest graphical interface (QT Wallet) from our [Releases Page](https://github.com/sipherapex/aureus-core/releases).

### Linux (Build from Source)
Aureus Core is designed to be built using the `cmake` system for maximum hardware optimization.

```bash
# Install dependencies
sudo apt-get install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils python3 libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-test-dev libboost-thread-dev

# Clone and Build
git clone [https://github.com/sipherapex/aureus-core.git](https://github.com/sipherapex/aureus-core.git)
cd aureus-core
ln -sf src/bitcoin-wallet.cpp src/aureus-wallet.cpp
ln -sf src/init/bitcoin-wallet.cpp src/init/aureus-wallet.cpp
OR
ln -s ${PROJECT_ROOT}/src/bitcoin-wallet.cpp ${PROJECT_ROOT}/src/aureus-wallet.cpp
ln -s ${PROJECT_ROOT}/src/init/bitcoin-wallet.cpp ${PROJECT_ROOT}/src/init/aureus-wallet.cpp
mkdir build && cd build
cmake .. -DENABLE_WALLET=ON -DBUILD_WALLET=ON -DBUILD_GUI=OFF -DENABLE_IPC=OFF
make -j$(nproc)
Binaries (aureusd and aureus-cli) will be generated in the /src directory.

Technical Fix for Server Wallets:
Since some internal core components and scripts in this version are mapped to legacy names, you may encounter errors when calling wallet functions. To bypass this and ensure the Aureus Server Wallet activates correctly, create symbolic links to redirect legacy calls to the new binaries:

🔒 Security & Governance
Aureus Core implements Hardcoded Checkpoints to protect the chain from 51% reorgs during early-stage growth. The network is supported by high-availability V-Seeds ensuring 99.9% global connectivity.

🤝 Community & Ecosystem
Join our growing community to stay updated on the latest developments, token issuance tools, and exchange listings.

Twitter (X): https://x.com/sipher_apex

Telegram: https://t.me/AUREUS_AUR

Discord: https://discord.gg/BVru9jAzgk

📄 License
Aureus Core is released under the terms of the MIT License.

Built by developers, for the future of decentralized finance.
