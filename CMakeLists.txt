cmake_minimum_required (VERSION 2.6)

project (BwManager)

include(GNUInstallDirs)
include(CTest)

set(CMAKE_VERBOSE_MAKEFILE OFF)

# require pthreads
set(CMAKE_THREAD_PREFER_PTHREAD ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(OpenMP REQUIRED)

# require boost program options
find_package(Boost COMPONENTS program_options REQUIRED)

# get list of source files
file(GLOB_RECURSE SOURCE_FILES relative ${CMAKE_CURRENT_SOURCE_DIR} "src/*.cpp" "src/*c")
add_executable(BwManager ${SOURCE_FILES})

target_compile_options(BwManager PRIVATE -g -Wall -pedantic -Wshadow -Wfatal-errors)

target_include_directories(BwManager
	PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/include>
	PUBLIC $<INSTALL_INTERFACE:include>
	PRIVATE ${Boost_INCLUDE_DIRS}
	PRIVATE src)
	
target_link_libraries(BwManager
	Threads::Threads
	OpenMP::OpenMP_CXX
  ${CMAKE_DL_LIBS}
  rt
  -rdynamic
  ${Boost_LIBRARIES}
	numa
	likwid
)
