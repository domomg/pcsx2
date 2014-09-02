# Try to find AIO
# Once done, this will define
#
# AIO_FOUND - system has AIO
# AIO_INCLUDE_DIR - the AIO include directories
# AIO_LIBRARIES - link these to use AIO

if(AIO_INCLUDE_DIR AND AIO_LIBRARIES)
    set(AIO_FIND_QUIETLY TRUE)
endif(AIO_INCLUDE_DIR AND AIO_LIBRARIES)

if(APPLE)
    set(AIO_HEADER "aio.h")
else()
    set(AIO_HEADER "libaio.h")
    find_library(LIBAIO NAMES "aio")
endif()

# include dir
find_path(AIO_INCLUDE_DIR ${AIO_HEADER})

# finally the library itself
set(AIO_LIBRARIES ${LIBAIO})

# handle the QUIETLY and REQUIRED arguments and set AIO_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)

if(APPLE)
    find_package_handle_standard_args(AIO DEFAULT_MSG AIO_INCLUDE_DIR)
    mark_as_advanced(AIO_INCLUDE_DIR)
else()
    find_package_handle_standard_args(AIO DEFAULT_MSG AIO_LIBRARIES AIO_INCLUDE_DIR)
    mark_as_advanced(AIO_LIBRARIES AIO_INCLUDE_DIR)
endif()
