# Copyright 2002-2013 CEA LIST
# SPDX-FileCopyrightText: 2022 CEA LIST <gael.de-chalendar@cea.fr>
#
# SPDX-License-Identifier: MIT

file(GLOB DAT_FILES *.dat)

install(
  FILES ${DAT_FILES} 
  COMPONENT ud-eng 
  DESTINATION share/apps/lima/resources/SpecificEntities
)
