cmake_minimum_required( VERSION 3.10 )
project(dab_cmdline_example10 LANGUAGES C CXX)
option(ENABLE_LTO "Enable link-time optimization" ON)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-aligned-new -Wno-stringop-overflow -std=c++11")

# Raspberry Pi/ARM Linux builds often suffer from slow/fragile LTO link steps.
# Disable LTO automatically on these platforms unless the project is adapted later.
if (ENABLE_LTO AND UNIX AND NOT APPLE)
	string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _cpu)
	if (_cpu MATCHES "armv6|armv7|armv8|aarch64|arm64")
		message(STATUS "Disabling ENABLE_LTO on ARM Linux (Raspberry Pi compatibility)")
		set(ENABLE_LTO OFF)
	endif()
endif()

if (ENABLE_LTO)
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
	set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
else()
	string(REPLACE " -flto" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	string(REPLACE "-flto " "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	string(REPLACE " -flto" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
	string(REPLACE "-flto " "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
endif()
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set (CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "default install path" FORCE )
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
OPTION(ABRA_DABSDR_PROBE "Build AbracaDABra libdabsdr probe tool" OFF)
OPTION(ABRA_RAW "Enable abra-raw target" OFF)
OPTION(ABRA_RTLSDR "Enable abra-rtlsdr target" OFF)

if ( (NOT RTLSDR) AND (NOT AIRSPY) AND (NOT SDRPLAY) AND (NOT WAVFILES) AND (NOT RAWFILES) AND (NOT RTL_TCP) AND
	  (NOT ABRA_DABSDR_PROBE) AND (NOT ABRA_RAW) AND (NOT ABRA_RTLSDR) )
   message("None of the Input Options selected. Using default RTLSDR")
   set(RTLSDR ON)
endif ()

set (objectName "")
set (BUILD_CLASSIC_MAIN OFF)

if(AIRSPY)
   if (objectName STREQUAL "")
     set(AIRSPY ON)
     set(objectName dab-airspy)
		 set(BUILD_CLASSIC_MAIN ON)
  else()
    message("Ignoring 2nd input option AIRSPY")
  endif()
endif ()

if(SDRPLAY)
   if (objectName STREQUAL "")
     set(SDRPLAY ON)
     set(objectName dab-sdrplay)
		 set(BUILD_CLASSIC_MAIN ON)
  else()
    message("Ignoring 2nd input option SDRPLAY")
  endif()
endif ()

if(RTLSDR)
   if (objectName STREQUAL "")
     set(RTLSDR ON)
     set(objectName dab-rtlsdr)
		 set(BUILD_CLASSIC_MAIN ON)
  else()
    message("Ignoring 2nd input option RTLSDR")
  endif()
endif ()

if(WAVFILES)
   if (objectName STREQUAL "")
     set(WAVFILES ON)
     set(objectName dab-files)
		 set(BUILD_CLASSIC_MAIN ON)
  else()
    message("Ignoring 2nd input option WAVFILES")
  endif()
endif ()

if(RAWFILES)
   if (objectName STREQUAL "")
     set(RAWFILES ON)
     set(objectName dab-raw)
		 set(BUILD_CLASSIC_MAIN ON)
  else()
    message("Ignoring 2nd input option RAWFILES")
  endif()
endif ()

if(RTL_TCP)
   if (objectName STREQUAL "")
     set(RTL_TCP ON)
     set(objectName dab-rtl_tcp)
		 set(BUILD_CLASSIC_MAIN ON)
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
	   include_directories (${LIBRTLSDR_INCLUDE_DIR})

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

	if (BUILD_CLASSIC_MAIN)
		add_executable (${objectName} 
		                ${${objectName}_SRCS}
		)

		target_link_libraries (${objectName}
		                       ${FFTW3F_LIBRARIES}
		                       ${extraLibs}
		                       ${FAAD_LIBRARIES}
		                       ${CMAKE_DL_LIBS}
		)

		INSTALL (TARGETS ${objectName} RUNTIME DESTINATION bin)  # install to default (bin)
	else()
		message(STATUS "No legacy dab-* input backend selected; skipping ${objectName} target")
	endif()

#####################################################################

	add_executable ( decodefic
	                decodefic.cpp fibbits.cpp fibbits.h proc_fig0ext6.cpp proc_fig0ext6.h
	)

	INSTALL (TARGETS decodefic RUNTIME DESTINATION bin)

########################################################################
# Create uninstall target
########################################################################

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

########################################################################
# Optional: AbracaDABra libdabsdr probe
########################################################################
set(ABRA_DABSDR_PLATFORM "")
if (WIN32)
	set(ABRA_DABSDR_PLATFORM "windows_amd64")
elseif (APPLE)
	if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
		set(ABRA_DABSDR_PLATFORM "darwin_arm64")
	else()
		set(ABRA_DABSDR_PLATFORM "darwin_x86_64")
	endif()
elseif (UNIX)
	if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
		set(ABRA_DABSDR_PLATFORM "linux_aarch64")
	else()
		set(ABRA_DABSDR_PLATFORM "linux_x86_64")
	endif()
endif()

set(ABRA_DABSDR_CMAKE
	"${CMAKE_SOURCE_DIR}/../third_party/abracadabra/lib/${ABRA_DABSDR_PLATFORM}/cmake/dabsdr.cmake")

message(STATUS "ABRA_DABSDR_PROBE=${ABRA_DABSDR_PROBE}")
message(STATUS "ABRA_RAW=${ABRA_RAW}")
message(STATUS "ABRA_RTLSDR=${ABRA_RTLSDR}")
message(STATUS "ABRA_DABSDR_PLATFORM=${ABRA_DABSDR_PLATFORM}")
message(STATUS "ABRA_DABSDR_CMAKE=${ABRA_DABSDR_CMAKE}")

if (ABRA_DABSDR_PROBE)
	if (EXISTS "${ABRA_DABSDR_CMAKE}")
		include("${ABRA_DABSDR_CMAKE}")
		add_executable(abra-dabsdr-probe
							./abra_dabsdr_probe.cpp)
		target_link_libraries(abra-dabsdr-probe dabsdr::dabsdr)
		INSTALL(TARGETS abra-dabsdr-probe RUNTIME DESTINATION bin)
	else()
		message(FATAL_ERROR "ABRA_DABSDR_PROBE=ON, but dabsdr.cmake was not found at ${ABRA_DABSDR_CMAKE} (platform='${ABRA_DABSDR_PLATFORM}'). Copy the matching AbracaDABra SDK folder into third_party/abracadabra/lib/ and reconfigure.")
	endif()
endif()

if (ABRA_RAW)
	if (EXISTS "${ABRA_DABSDR_CMAKE}")
		include("${ABRA_DABSDR_CMAKE}")
		add_executable(abra-raw
						 ./abra_dabsdr.cpp
						 ./dab_tables.cpp
						 ../devices/device-handler.cpp
						 ../devices/rawfiles/rawfiles.cpp
						 ../src/backend/charsets.cpp
						 ../src/support/band-handler.cpp)
		set_target_properties(abra-raw PROPERTIES
			INSTALL_RPATH "\$ORIGIN/../lib;${CMAKE_SOURCE_DIR}/../third_party/abracadabra/lib/${ABRA_DABSDR_PLATFORM}"
			BUILD_RPATH "${CMAKE_SOURCE_DIR}/../third_party/abracadabra/lib/${ABRA_DABSDR_PLATFORM}")
		target_include_directories(abra-raw PRIVATE
								  ${CMAKE_SOURCE_DIR}
								  ${CMAKE_SOURCE_DIR}/../
								  ${CMAKE_SOURCE_DIR}/../devices
								  ${CMAKE_SOURCE_DIR}/../devices/rawfiles
								  ${CMAKE_SOURCE_DIR}/../includes
								  ${CMAKE_SOURCE_DIR}/../includes/backend
								  ${CMAKE_SOURCE_DIR}/../includes/support)
		target_link_libraries(abra-raw dabsdr::dabsdr ${PTHREADS})
		INSTALL(TARGETS abra-raw RUNTIME DESTINATION bin)
	else()
		message(FATAL_ERROR "ABRA_RAW=ON, but dabsdr.cmake was not found at ${ABRA_DABSDR_CMAKE} (platform='${ABRA_DABSDR_PLATFORM}'). Copy the matching AbracaDABra SDK folder into third_party/abracadabra/lib/ and reconfigure.")
	endif()
endif()

if (ABRA_RTLSDR)
	if (EXISTS "${ABRA_DABSDR_CMAKE}")
		include("${ABRA_DABSDR_CMAKE}")
		find_package(LibRTLSDR)
		if (NOT LIBRTLSDR_FOUND)
			message(FATAL_ERROR "ABRA_RTLSDR=ON, but librtlsdr was not found")
		endif ()

		add_executable(abra-rtlsdr
						 ./abra_dabsdr.cpp
						 ./dab_tables.cpp
						 ../devices/device-handler.cpp
						 ../devices/rtlsdr-handler/rtlsdr-handler.cpp
						 ../src/backend/charsets.cpp
						 ../src/support/band-handler.cpp)
		set_target_properties(abra-rtlsdr PROPERTIES
			INSTALL_RPATH "\$ORIGIN/../lib;${CMAKE_SOURCE_DIR}/../third_party/abracadabra/lib/${ABRA_DABSDR_PLATFORM}"
			BUILD_RPATH "${CMAKE_SOURCE_DIR}/../third_party/abracadabra/lib/${ABRA_DABSDR_PLATFORM}")
		target_compile_definitions(abra-rtlsdr PRIVATE ABRA_INPUT_RTLSDR HAVE_RTLSDR)
		target_include_directories(abra-rtlsdr PRIVATE
								  ${CMAKE_SOURCE_DIR}
								  ${CMAKE_SOURCE_DIR}/../
								  ${CMAKE_SOURCE_DIR}/../devices
								  ${CMAKE_SOURCE_DIR}/../devices/rtlsdr-handler
								  ${CMAKE_SOURCE_DIR}/../includes
								  ${CMAKE_SOURCE_DIR}/../includes/backend
								  ${CMAKE_SOURCE_DIR}/../includes/support
								  ${LIBRTLSDR_INCLUDE_DIR})
		target_link_libraries(abra-rtlsdr dabsdr::dabsdr ${PTHREADS} ${LIBRTLSDR_LIBRARIES})
		INSTALL(TARGETS abra-rtlsdr RUNTIME DESTINATION bin)
	else()
		message(FATAL_ERROR "ABRA_RTLSDR=ON, but dabsdr.cmake was not found at ${ABRA_DABSDR_CMAKE} (platform='${ABRA_DABSDR_PLATFORM}'). Copy the matching AbracaDABra SDK folder into third_party/abracadabra/lib/ and reconfigure.")
	endif()
endif()
