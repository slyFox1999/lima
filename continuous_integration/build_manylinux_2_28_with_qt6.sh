#!/bin/bash

# SPDX-FileCopyrightText: 2022 CEA LIST <gael.de-chalendar@cea.fr>
#
# SPDX-License-Identifier: MIT

set -o errexit
set -o pipefail
set -o nounset

source python_env_qt6.sh
docker build -f Dockerfile-manylinux_2_28_with_qt6 \
    --build-arg MANYLINUX_TAG="${MANYLINUX_TAG}" \
    --build-arg GCC_VERSION="${GCC_VERSION}" \
    --build-arg LLVM_VERSION="${LLVM_VERSION}" \
    --build-arg QT_FULL_VERSION="${QT_FULL_VERSION}" \
    --build-arg QT_VERSION_MAJOR="${QT_VERSION_MAJOR}" \
    --build-arg QT_VERSION_MINOR="${QT_VERSION_MINOR}" \
    --build-arg QT_VERSION_PATCH="${QT_VERSION_PATCH}" \
    --build-arg QT_VERSION="${QT_VERSION}" \
    --build-arg PYTHON_VERSION=${PYTHON_VERSION} \
    --build-arg PYTHON_SHORT_VERSION=${PYTHON_SHORT_VERSION} \
    --build-arg PYTHON_FULL_VERSION=${PYTHON_FULL_VERSION} \
    -t aymara/manylinux_2_28_with_qt${QT_VERSION}-python${PYTHON_VERSION}:latest .

docker push aymara/manylinux_2_28_with_qt${QT_VERSION}-python${PYTHON_VERSION}:latest
