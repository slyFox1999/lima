# Copyright 2021 CEA LIST
# SPDX-FileCopyrightText: 2022 CEA LIST <gael.de-chalendar@cea.fr>
#
# SPDX-License-Identifier: MIT

add_library(train_common
  STATIC
  train_params.cpp
)

target_link_libraries(train_common
  conllu
  Boost::filesystem
)

