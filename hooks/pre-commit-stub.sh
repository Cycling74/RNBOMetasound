#!/bin/sh
set -e
cd "$(git rev-parse --show-toplevel)"
if [ -x "./hooks/pre-commit.sh" ]; then
    eval "./hooks/pre-commit.sh"
fi
