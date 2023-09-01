#!/bin/sh
if ! command -v git-clang-format > /dev/null 2> /dev/null; then
  echo "git-clang-format NOT installed or found in system path."
  echo "git clang-format support is required for commits to master"
  echo "aborting…"
  exit 1
fi

RESULT=$(git clang-format --staged --style=file --extensions c,cpp,h,m,mm,hpp 2>&1)
STATUS=$?
if [ ${STATUS} -ne 0 ]; then
  echo "⚠️  git clang-format:"
  printf "%s\n" "${RESULT}"
  echo "⚠️  Commit aborted, reformat the necessary files and reattempt commit."
  exit ${STATUS}
fi
