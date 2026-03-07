# Aureus Core [AURE] 
### The Institutional-Grade SHA-256 Layer 1 Blockchain

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Build Status](https://img.shields.io/badge/Build-Passing-brightgreen.svg)]()
[![Version](https://img.shields.io/badge/Release-v0.2x.Stable-orange.svg)]()

Aureus Core is a high-performance, security-centric Layer 1 blockchain designed for the next generation of decentralized value transfer. Built on a modernized Bitcoin Core architecture, Aureus combines the industrial-strength security of **SHA-256 Proof-of-Work** with advanced cryptographic features like **Taproot**, **SegWit**, and **V2 P2P Transport Encryption**.

---

## 🌐 Official Resources
| Resource | Link |
| :--- | :--- |
| **Official Website** | [https://aureuscoin.site](https://aureuscoin.site) |
| **Block Explorer** | [https://explore.aureuscoin.site](https://explore.aureuscoin.site) |
| **Source Code** | [https://github.com/sipherapex/aureus-core](https://github.com/sipherapex/aureus-core) |
| **Technical Documentation** | [Explore /doc folder](https://github.com/sipherapex/aureus-core/tree/master/doc) |

---

## ⚙️ Technical Specifications
* **Ticker:** `AURE`
* **Algorithm:** SHA-256 (ASIC-Optimized)
* **Max Supply:** 31000000 AURE
* **Block Maturity:** 100 Confirmations
* **Network Ports:** P2P: `11995` 

---

## 🚀 Key Features
* **Taproot & Schnorr Signatures:** Enhanced privacy and smart contract efficiency.
* **V2 P2P Transport:** Native node-to-node encryption (BIP324) to prevent traffic analysis and MitM attacks.
* **Modern Descriptor Wallets:** Moving beyond legacy `wallet.dat` for robust hardware and multi-sig support.
* **Strategic Ecosystem Reserve:** 500,000 AURE dedicated to infrastructure, exchange liquidity, and long-term network stability.

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
mkdir build && cd build
cp ../src/bitcoin-wallet.cpp ../src/aureus-wallet.cpp
cp ../src/init/bitcoin-wallet.cpp ../src/init/aureus-wallet.cpp
cmake .. -DENABLE_WALLET=ON -DBUILD_WALLET=ON -DBUILD_GUI=OFF -DENABLE_IPC=OFF -DBUILD_AUREUS_TESTS=OFF
make aureusd aureus-cli aureus-tx -j$(nproc)
Binaries (aureusd and aureus-cli) will be generated in the /bin directory.

### 🐧 Linux (Pre-compiled Binaries)
For quick deployment, we provide pre-compiled binaries for **Ubuntu 24.04 LTS**.
1. Go to the [Releases Page](https://github.com/sipherapex/aureus-core/releases).
2. Download `aureusd and aureus-cli`.
   chmod +x aureusd aureus-cli
   ./aureusd -daemon



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

🏛️ THE ARCHITECTURE OF SCARCITY: 6,000,000 $AUR SECURED

To our community and the SHA-256 enthusiasts, transparency is the cornerstone of the Aureus Empire. We are officially releasing the details of our Strategic Reserve and the Whitepaper to clarify our long-term monetary policy.

I. Monetary Policy & Strategic Ledger
The AUREUS protocol enforces a strictly deflationary cap of 31M $AUR. To ensure terminal scarcity and ecosystem stability, 6,000,000 $AUR is locked across twelve (12) verifiable strategic vaults.

The Explorer never lies. Transparency is not an option; it is built into our code.

Quote
TOTAL ESCROW RESERVE: 6,000,000 $AUR
PROOF OF SCARCITY: VERIFIED ON-CHAIN

Strategic Vault Schedule:
Code:
VAULT | VERIFIABLE PUBLIC ADDRESS                         | AMOUNT  | UNLOCK
V_01  | 5EuFGUFXkDt8xeFmxGZ6MdTS7Ghf2sUtks                | 500,000 | 2031
V_02  | AVqnrHmUAYgir1ErXvmV8Zfba1H5Wj4Tr5                | 500,000 | 2035
V_03  | aur1qdfddpt8p7ec2r76wey0kc6n9ulywh5tdwfzyu3       | 500,000 | 2037
V_04  | aur1pl47gsjm4nhn7r9lphlhemphagxu6gfmy...          | 500,000 | 2039
V_05  | 5DoKULL5SWiUHpoZ9dycK2QMP9UQwpQxVX                | 500,000 | 2042
V_06  | aur1qdrr29cujn80jr6n290wd2c3ua32yne8nl9hc8w       | 500,000 | 2044
V_07  | AVvkisUarGTp91b3WBqFHUz88VsB4vgPun                | 500,000 | 2045
V_08  | Ac47eeZSLmxQAwvFpsnwgZbEvayd6ZrW1m                | 500,000 | 2046
V_09  | ALyGD2RJkRaNb42JqWGnzJSYnPu11LmU95                | 500,000 | 2047
V_10  | 5QMd6s1zpxv2n69sJmyfnuvrSc3toPsTTE                | 500,000 | 2048
V_11  | AR7oERzAxoAyhmVi3dKUpf4ucxYmFnQPJG                | 500,000 | 2049
V_12  | 59PZ5oMpMVRDVLc6z8T7vsnVFFSMDRJmaH                | 500,000 | 2050

II. Mining Opportunities
With the escrow phase concluded, we want to highlight that over 20,000,000 $AUR are ready to be mined with absolute security. The network is open, decentralized, and governed by mathematical certainty.

III. Official Whitepaper
Our Whitepaper details the technical roadmap, Atomic Swap infrastructure, and our vision for a Sovereign Financial Layer.
👉 Read the AUREUS CORE Whitepaper Here

We welcome technical scrutiny based on data. Let the work prove the value.

The Explorer never lies. Stay Sovereign. Stay Aureus 🔱
