cmake_minimum_required(VERSION 3.9 FATAL_ERROR)
   set(CMAKE_VERBOSE_MAKEFILE ON)
   project(kalisc)

   #
   # Properties
   #
   set(DEBUG_LIBUSB "OFF" CACHE BOOL "verbose libusb debug output")
   set(USE_LTO "OFF" CACHE BOOL "use link time optimization")
   
   if(NOT CMAKE_DEBUG_POSTFIX)
      set(CMAKE_DEBUG_POSTFIX d)
   endif()
   
   #
   # Compiler flags
   #
   set(CMAKE_C_FLAGS "-Wall")
   set(CMAKE_C_FLAGS_DEBUG "-O0 -g -ggdb")
   set(CMAKE_C_FLAGS_RELEASE "-pipe -g -O3 -march=native -fomit-frame-pointer -fno-signaling-nans -fno-trapping-math -ftree-vectorize -fopt-info-vec-missed=vec.opt -lpthread -lstdc++")
   set(CMAKE_CXX_FLAGS "-Wall")
   set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -ggdb")
   set(CMAKE_CXX_FLAGS_RELEASE "-pipe -g -O3 -march=native -fomit-frame-pointer -fno-signaling-nans -fno-trapping-math -ftree-vectorize -fopt-info-vec-missed=vec.opt -lpthread -lstdc++")
   
   #
   # Find packages
   #
   find_package(HDF5 REQUIRED COMPONENTS C CXX)
   find_package(OpenMP REQUIRED)
   find_package(Protobuf REQUIRED)
   find_package(CapnProto REQUIRED)
   find_package(Boost 1.70 REQUIRED COMPONENTS iostreams)
   find_package(SQLite3 REQUIRED)
   find_package(PkgConfig)
   pkg_check_modules(libusb-1.0 REQUIRED libusb-1.0)
   pkg_check_modules(libzstd REQUIRED libzstd)
   include_directories(${HDF5_INCLUDE_DIRS})
   include_directories(${CMAKE_BINARY_DIR}/include)
   LINK_DIRECTORIES(${CMAKE_SOURCE_DIR}/libs/linux)

   #
   # capnproto
   #
   capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS ttdata.capnp cp_tags.capnp)
   
   
   add_executable(kalis helpers.cpp kalis.cpp ${CAPNP_SRCS})
   set_property(TARGET kalis PROPERTY CXX_STANDARD 20)
   target_link_libraries(kalis ${HDF5_HL_LIBRARIES} ${HDF5_CXX_LIBRARIES} ${HDF5_LIBRARIES} ${PROTOBUF_LIBRARY} OpenMP::OpenMP_CXX libtimetag64.so ${LIBUSB_1_LIBRARIES} ${libusb-1.0__LIBS} usb-1.0 CapnProto::capnp CapnProto::capnp-rpc ${ZSTD_LIBRARIES} Boost::iostreams ${SQLite3_LIBRARIES})
   target_include_directories(kalis PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${SQLite3_INCLUDE_DIRS})

   #
   # IPO / LTO
   #
   include(CheckIPOSupported)
   check_ipo_supported(RESULT hasipo OUTPUT error)
   IF(CMAKE_BUILD_TYPE MATCHES Release)
    message("release mode")
    if( hasipo )
       message(STATUS "IPO / LTO enabled")
       if (USE_LTO)
          set_property(TARGET kalis PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
       endif()
    else()
       message(STATUS "IPO / LTO not supported: <${error}>")
    endif()
   ENDIF(CMAKE_BUILD_TYPE MATCHES Release)

   
   #
   # LIBUSB Debug
   #
   if(DEBUG_LIBUSB)
      message(STATUS "verbose libusb output enabled")
      add_compile_definitions(DEBUG_LIBUSB)
   endif()
      
   message(STATUS "INCLUDE LOCATION" ${HDF5_INCLUDE_DIRS})
   message(STATUS "version" ${HDF5_VERSION})
   message(STATUS "DEFINITIONS" ${HDF5_DEFINITIONS})
   message(STATUS "LIBRARIES" ${HDF5_LIBRARIES})
   message(STATUS "HL_LIBRARIES" ${HDF5_HL_LIBRARIES})

   add_custom_target(misc_files ALL
       SOURCES ttdata.capnp cp_tags.capnp todo.txt)
