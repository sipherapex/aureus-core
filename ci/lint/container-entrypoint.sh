#!/usr/bin/env bash
#
# Copyright (c) The Aureus Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

export LC_ALL=C

# Fixes permission issues when there is a container UID/GID mismatch with the owner
# of the mounted aureus src dir.
git config --global --add safe.directory /aureus

export PATH="/python_build/bin:${PATH}"

./ci/lint/06_script.sh "$@"
