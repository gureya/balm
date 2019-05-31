cmake_minimum_required (VERSION 2.6)

project (bw-manager)

include(GNUInstallDirs)
include(CTest)

set(CMAKE_VERBOSE_MAKEFILE OFF)

# require boost program options
find_package(Boost COMPONENTS program_options REQUIRED)

# get list of source files
file(GLOB_RECURSE SOURCE_FILES relative ${CMAKE_CURRENT_SOURCE_DIR} "src/*.cpp" "src/*c")
add_executable(bw-manager ${SOURCE_FILES})

target_compile_options(bw-manager PRIVATE -g -Wall -pedantic -Wshadow -Wfatal-errors)

target_include_directories(bw-manager
	PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/include>
	PUBLIC $<INSTALL_INTERFACE:include>
	PRIVATE ${Boost_INCLUDE_DIRS}
	PRIVATE src)
	
target_link_libraries(bw-manager
	numa
	${Boost_LIBRARIES}
	likwid
)
