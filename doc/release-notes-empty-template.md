*The release notes draft is a temporary file that can be added to by anyone. See
[/doc/developer-notes.md#release-notes](/doc/developer-notes.md#release-notes)
for the process.*

*version* Release Notes Draft
===============================

Aureus Core version *version* is now available from:

  <https://aureuscore.org/bin/aureus-core-*version*/>

This release includes new features, various bug fixes and performance
improvements, as well as updated translations.

Please report bugs using the issue tracker at GitHub:

  <https://github.com/aureus/aureus/issues>

To receive security and update notifications, please subscribe to:

  <https://aureuscore.org/en/list/announcements/join/>

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes in some cases), then run the
installer (on Windows) or just copy over `/Applications/Aureus-Qt` (on macOS)
or `aureusd`/`aureus-qt` (on Linux).

Upgrading directly from a version of Aureus Core that has reached its EOL is
possible, but it might take some time if the data directory needs to be migrated. Old
wallet versions of Aureus Core are generally supported.

Compatibility
==============

Aureus Core is supported and tested on the following operating systems or newer:
Linux Kernel 3.17, macOS 14, and Windows 10 (version 1903). Aureus
Core should also work on most other Unix-like systems but is not as
frequently tested on them. It is not recommended to use Aureus Core on
unsupported systems.

Notable changes
===============

P2P and network changes
-----------------------

Updated RPCs
------------


Changes to wallet related RPCs can be found in the Wallet section below.

New RPCs
--------

Build System
------------

Updated settings
----------------


Changes to GUI or wallet related settings can be found in the GUI or Wallet section below.

New settings
------------

Tools and Utilities
-------------------

Wallet
------

GUI changes
-----------

Low-level changes
=================

RPC
---

Tests
-----

*version* change log
====================

Credits
=======

Thanks to everyone who directly contributed to this release:


As well as to everyone that helped with translations on
[Transifex](https://explore.transifex.com/aureus/aureus/).
