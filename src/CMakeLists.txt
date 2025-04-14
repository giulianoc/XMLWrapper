
# Copyright (C) Giuliano Catrambone (giuliano.catrambone@catrasoftware.it)

# This program is free software; you can redistribute it and/or 
# modify it under the terms of the GNU General Public License 
# as published by the Free Software Foundation; either 
# version 2 of the License, or (at your option) any later 
# version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

# Commercial use other than under the terms of the GNU General Public
# License is allowed only after express negotiation of conditions
# with the authors.

SET (SOURCES
	XMLWrapper.cpp
)

SET (HEADERS
	XMLWrapper.h
)

include_directories("${SPDLOG_INCLUDE_DIR}")
include_directories("${NLOHMANN_INCLUDE_DIR}")
include_directories("${CURLWRAPPER_INCLUDE_DIR}")
include_directories("${XML2_INCLUDE_DIR}")

add_library (XMLWrapper SHARED ${SOURCES} ${HEADERS})

if(APPLE)
	target_link_libraries(XMLWrapper CurlWrapper)
	target_link_libraries(XMLWrapper xml2)
endif()

if(ZORAC)
	install (TARGETS XMLWrapper DESTINATION services/cms-import)
	#install (TARGETS XMLWrapper DESTINATION services/cms-pusher)
	#install (TARGETS XMLWrapper DESTINATION services/cms-getter)
else()
	install (TARGETS XMLWrapper DESTINATION lib)
	install (FILES ${HEADERS} DESTINATION include)
endif()

