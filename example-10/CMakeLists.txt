cmake_minimum_required( VERSION 2.8.11 )
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-aligned-new -std=c++11 -flto")
set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set (CMAKE_INSTALL_PREFIX "/usr/local/bin" CACHE PATH "default install path" FORCE )
endif()
#set (CMAKE_INSTALL_PREFIX /usr/local/bin)

if(MINGW)
    add_definitions ( -municode)
endif()

########################################################################
# select the release build type by default to get optimization flags
########################################################################
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
   message(STATUS "Build type not specified: defaulting to release.")
endif(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")

### make sure our local CMake Modules path comes first
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake/Modules)

######################################################################
#
########################################################################
OPTION(RTLSDR   "Input: RTLSDR"   OFF)
OPTION(AIRSPY   "Input: AirSpy"   OFF)
OPTION(SDRPLAY  "Input: SDRPlay"  OFF)
OPTION(WAVFILES "Input: WAVFILES" OFF)
OPTION(RAWFILES "Input: RAWFILES" OFF)
OPTION(RTL_TCP  "Input: RTL_TCP"  OFF)

if ( (NOT RTLSDR) AND (NOT AIRSPY) AND (NOT SDRPLAY) AND (NOT WAVFILES) AND (NOT RAWFILES) AND (NOT RTL_TCP) )
   message("None of the Input Options selected. Using default RTLSDR")
   set(RTLSDR ON)
endif ()

set (objectName "")

if(AIRSPY)
   if (objectName STREQUAL "")
     set(AIRSPY ON)
     set(objectName dab-airspy)
  else()
    message("Ignoring 2nd input option AIRSPY")
  endif()
endif ()

if(SDRPLAY)
   if (objectName STREQUAL "")
     set(SDRPLAY ON)
     set(objectName dab-sdrplay)
  else()
    message("Ignoring 2nd input option SDRPLAY")
  endif()
endif ()

if(RTLSDR)
   if (objectName STREQUAL "")
     set(RTLSDR ON)
     set(objectName dab-rtlsdr)
  else()
    message("Ignoring 2nd input option RTLSDR")
  endif()
endif ()

if(WAVFILES)
   if (objectName STREQUAL "")
     set(WAVFILES ON)
     set(objectName dab-files)
  else()
    message("Ignoring 2nd input option WAVFILES")
  endif()
endif ()

if(RAWFILES)
   if (objectName STREQUAL "")
     set(RAWFILES ON)
     set(objectName dab-raw)
  else()
    message("Ignoring 2nd input option RAWFILES")
  endif()
endif ()

if(RTL_TCP)
   if (objectName STREQUAL "")
     set(RTL_TCP ON)
     set(objectName dab-rtl_tcp)
  else()
    message("Ignoring 2nd input option RTL_TCP")
  endif()
endif ()

if (objectName STREQUAL "")
   set (objectName dab_cmdline)
endif ()

if (DEFINED_SERVER)
    add_definitions(-DHAVE_SERVER)
endif ()

#########################################################################
	find_package (PkgConfig)

        find_package(FFTW3f)
        if (NOT FFTW3F_FOUND)
            message(FATAL_ERROR "please install FFTW3")
        endif ()

        find_package(Faad)
        if (NOT FAAD_FOUND )
            message(FATAL_ERROR "please install libfaad")
        endif ()

        find_package(zlib)
	if (NOT ZLIB_FOUND)
            message(FATAL_ERROR "please install libz")
        endif ()
	list(APPEND extraLibs ${ZLIB_LIBRARY})


	find_library (PTHREADS pthread)
	if (NOT(PTHREADS))
	   message (FATAL_ERROR "please install libpthread")
	else (NOT(PTHREADS))
	   set (extraLibs ${extraLibs} ${PTHREADS})
	endif (NOT(PTHREADS))


#########################################################################
        find_package (PkgConfig)

##########################################################################
#	The devices
#

	if (RTL_TCP)
	   include_directories (
	     ../devices/rtl_tcp
	   )
	   set ($(objectName)_HDRS
	        ${${objectName}_HDRS}
	        ../devices/rtl_tcp/rtl_tcp-client.h
           )

	   set (${objectName}_SRCS
	        ${${objectName}_SRCS}
	        ../devices/rtl_tcp/rtl_tcp-client.cpp
	   )

	 add_definitions (-DHAVE_RTL_TCP)
	endif (RTL_TCP)
	
	if (SDRPLAY)
	   find_path (SDRPLAYLIB_INCLUDE_DIR
                      NAMES mirsdrapi-rsp.h
                      PATHS
                      /usr/local/include/
	   )
           include_directories (${SDRPLAYLIB_INCLUDE_DIR})

           find_library (SDRPLAYLIB mirsdrapi-rsp)
           if(NOT(SDRPLAYLIB))
              message(FATAL_ERROR "please install -lmirsdrapi-rsp")
           else(NOT(SDRPLAYLIB))
              list (APPEND extraLibs ${SDRPLAYLIB})
           endif(NOT(SDRPLAYLIB))

	   include_directories (
	     ../devices/sdrplay-handler
	   )

	   set ($(objectName)_HDRS
	        ${${objectName}_HDRS}
	        ../devices/sdrplay-handler/sdrplay-handler.h
           )

	   set (${objectName}_SRCS
	        ${${objectName}_SRCS}
	        ../devices/sdrplay-handler/sdrplay-handler.cpp
	   )

	 add_definitions (-DHAVE_SDRPLAY)
	endif (SDRPLAY)

	if (AIRSPY)
           find_package(LibAIRSPY)
           if (NOT LIBAIRSPY_FOUND)
               message(FATAL_ERROR "please install airspy library")
           endif ()
	   ### include_directories (${AIRSPYLIB_INCLUDE_DIR})

	   include_directories (
	     ../devices/airspy-handler
	   )

	   set ($(objectName)_HDRS
	        ${${objectName}_HDRS}
	        ../devices/airspy-handler/airspy-handler.h
	   )

	   set (${objectName}_SRCS
	        ${${objectName}_SRCS}
	        ../devices/airspy-handler/airspy-handler.cpp
	   )

	   add_definitions (-DHAVE_AIRSPY)
	endif (AIRSPY)


	if (RTLSDR)
           find_package(LibRTLSDR)
           if (NOT LIBRTLSDR_FOUND)
               message(FATAL_ERROR "please install librtlsdr")
           endif ()
	   ###include_directories (${RTLSDR_INCLUDE_DIR})

	   include_directories (
	        ../devices/rtlsdr-handler/
	   )

	   set (${objectName}_HDRS
	        ${${objectName}_HDRS}
	        ../devices/rtlsdr-handler/rtlsdr-handler.h
	   )

	   set (${objectName}_SRCS
	        ${${objectName}_SRCS}
	        ../devices/rtlsdr-handler/rtlsdr-handler.cpp
	   )

	   add_definitions (-DHAVE_RTLSDR)
	endif()

	if (WAVFILES)
	   include_directories (
	        ../devices/wavfiles/
	   )

	   set (${objectName}_HDRS
	        ${${objectName}_HDRS}
	        ../devices/wavfiles/wavfiles.h
	   )

	   set (${objectName}_SRCS
	        ${${objectName}_SRCS}
	        ../devices/wavfiles/wavfiles.cpp
	   )
	   find_package(LibSndFile)
	   if (NOT LIBSNDFILE_FOUND)
	      message(FATAL_ERROR "please install libsndfile")
	   endif ()
	   list(APPEND extraLibs ${LIBSNDFILE_LIBRARY})

	   add_definitions (-DHAVE_WAVFILES)
	endif()

	if (RAWFILES)
           include_directories (
                ../devices/rawfiles/
           )

           set (${objectName}_HDRS
                ${${objectName}_HDRS}
                ../devices/rawfiles/rawfiles.h
           )

           set (${objectName}_SRCS
                ${${objectName}_SRCS}
                ../devices/rawfiles/rawfiles.cpp
           )

           add_definitions (-DHAVE_RAWFILES)
        endif()

#######################################################################
#
#	Here we really start

	include_directories (
	           ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
	           .
	           ./
	           ./server-thread
	           ../
	           ../devices
	           ../includes
	           ../includes/ofdm
	           ../includes/backend
	           ../includes/backend/audio
	           ../includes/backend/data
	           ../includes/backend/data/mot
	           ../includes/backend/data/journaline
	           ../includes/protection
	           ../includes/support
	           /usr/include/
	)

	set (${objectName}_HDRS
	     ${${objectName}_HDRS}
	     ./fibbits.h
	     ./proc_fig0ext6.h
	     ./ringbuffer.h
	     ./dab_tables.h
	     ./server-thread/tcp-server.h
	     ../dab-api.h
	     ../convenience.h
	     ../devices/device-handler.h
	     ../includes/dab-constants.h
	     ../includes/dab-processor.h
	     ../includes/ofdm/phasereference.h
	     ../includes/ofdm/phasetable.h
	     ../includes/ofdm/freq-interleaver.h
	     ../includes/ofdm/timesyncer.h
	     ../includes/ofdm/fic-handler.h
	     ../includes/ofdm/fib-decoder.h
	     ../includes/ofdm/tii_detector.h
	     ../includes/ofdm/sample-reader.h
	     ../includes/backend/firecode-checker.h
	     ../includes/backend/backend-base.h
	     ../includes/backend/charsets.h
	     ../includes/backend/galois.h
	     ../includes/backend/reed-solomon.h
	     ../includes/backend/msc-handler.h
	     ../includes/backend/virtual-backend.h
	     ../includes/backend/audio-backend.h
	     ../includes/backend/data-backend.h
	     ../includes/backend/audio/faad-decoder.h
	     ../includes/backend/audio/mp4processor.h 
	     ../includes/backend/audio/mp2processor.h 
	     ../includes/backend/data/virtual-datahandler.h 
	     ../includes/backend/data/tdc-datahandler.h 
	     ../includes/backend/data/pad-handler.h 
	     ../includes/backend/data/data-processor.h
	     ../includes/backend/data/mot/mot-handler.h
	     ../includes/backend/data/mot/mot-dir.h
	     ../includes/backend/data/mot/mot-object.h
	     ../includes/protection/protection.h
	     ../includes/protection/protTables.h
	     ../includes/protection/uep-protection.h
	     ../includes/protection/eep-protection.h
	     ../includes/support/band-handler.h
	     ../includes/support/viterbi-handler.h
	     ../includes/support/fft_handler.h
	     ../includes/support/dab-params.h
	     ../includes/support/tii_table.h
	)

	set (${objectName}_SRCS
	     ${${objectName}_SRCS}
	     ./main.cpp
#	     ./fibbits.cpp
	     ./proc_fig0ext6.cpp
	     ./dab_tables.cpp
	     ./server-thread/tcp-server.cpp
	     ../convenience.c
	     ../devices/device-handler.cpp
	     ../src/dab-api.cpp
	     ../src/dab-processor.cpp
	     ../src/ofdm/ofdm-decoder.cpp
	     ../src/ofdm/phasereference.cpp
	     ../src/ofdm/phasetable.cpp
	     ../src/ofdm/freq-interleaver.cpp
	     ../src/ofdm/timesyncer.cpp
	     ../src/ofdm/sample-reader.cpp
	     ../src/ofdm/fib-decoder.cpp
	     ../src/ofdm/fic-handler.cpp
	     ../src/ofdm/tii_detector.cpp
	     ../src/backend/firecode-checker.cpp
	     ../src/backend/backend-base.cpp
	     ../src/backend/charsets.cpp
	     ../src/backend/galois.cpp
	     ../src/backend/reed-solomon.cpp
	     ../src/backend/msc-handler.cpp
	     ../src/backend/virtual-backend.cpp
	     ../src/backend/audio-backend.cpp
	     ../src/backend/data-backend.cpp
	     ../src/backend/audio/mp4processor.cpp 
	     ../src/backend/audio/mp2processor.cpp 
	     ../src/backend/data/virtual-datahandler.cpp 
	     ../src/backend/data/tdc-datahandler.cpp 
	     ../src/backend/data/pad-handler.cpp 
	     ../src/backend/data/data-processor.cpp
	     ../src/backend/data/mot/mot-handler.cpp
	     ../src/backend/data/mot/mot-dir.cpp
	     ../src/backend/data/mot/mot-object.cpp
	     ../src/protection/protection.cpp
	     ../src/protection/protTables.cpp
	     ../src/protection/eep-protection.cpp
	     ../src/protection/uep-protection.cpp
	     ../src/support/band-handler.cpp
	     ../src/support/viterbi-handler.cpp
	     ../src/support/fft_handler.cpp
	     ../src/support/dab-params.cpp
	     ../src/support/tii_table.cpp
	)

#
	include_directories (
	          ${FFTW_INCLUDE_DIRS}
	          ${FAAD_INCLUDE_DIRS}
	          ${SNDFILES_INCLUDE_DIRS}
	)

#####################################################################

	add_executable (${objectName} 
	                ${${objectName}_SRCS}
	)

	target_link_libraries (${objectName}
	                       ${FFTW3F_LIBRARIES}
	                       ${extraLibs}
	                       ${FAAD_LIBRARIES}
	                       ${CMAKE_DL_LIBS}
	)

	INSTALL (TARGETS ${objectName} DESTINATION .)

#####################################################################

	add_executable ( decodefic
	                decodefic.cpp fibbits.cpp fibbits.h proc_fig0ext6.cpp proc_fig0ext6.h
	)

	INSTALL (TARGETS decodefic DESTINATION .)

########################################################################
# Create uninstall target
########################################################################

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
