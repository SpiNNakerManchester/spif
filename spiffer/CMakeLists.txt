project(spiffer)

cmake_minimum_required(VERSION 3.5)

set (CMAKE_CXX_STANDARD 17)

# use colour to highlight messages
string (ASCII 27 ESC)
set (BoldRed     "${ESC}[1;31m")
set (BoldGreen   "${ESC}[1;32m")
set (BoldYellow  "${ESC}[1;33m")
set (ColourReset "${ESC}[m")

# spif / spiffer header files
include_directories (../test_code/include)

# find USB camera support libraries
# check for libcaer presence - support for Inivation cameras
find_package (libcaer QUIET)
IF (libcaer_FOUND)
  message (STATUS "${BoldGreen}libcaer found - Inivation camera support activated${ColourReset}")
  set (SPIFFER_CAER_SRC spiffer_caer_support.cpp)
  set (CAER_LIB caer)
	add_definitions (-DCAER_SUPPORT=1)
else ()
	message (STATUS "${BoldYellow}libcaer not found - Inivation camera support skipped${ColourReset}")
ENDIF ()

# check for Metavision SDK presence - support for Prophesee cameras
find_package (MetavisionSDK COMPONENTS core driver QUIET)
IF (MetavisionSDK_FOUND)
  message (STATUS "${BoldGreen}Metavision SDK found - Prophesee camera support activated${ColourReset}")
  set (SPIFFER_META_SRC spiffer_meta_support.cpp)
  set (META_LIBS MetavisionSDK::core MetavisionSDK::driver)
	add_definitions (-DMETA_SUPPORT=1)
else ()
	message (STATUS "${BoldYellow}Metavision SDK not found - Prophesee camera support skipped${ColourReset}")
ENDIF ()

add_executable (spiffer spiffer.cpp ${SPIFFER_CAER_SRC} ${SPIFFER_META_SRC})
target_link_libraries (spiffer pthread ${CAER_LIB} ${META_LIBS})
