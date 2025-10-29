cmake_minimum_required(VERSION 3.29.2)

set(PROGRAM_VERSION "KalaServer 0.2.0 Alpha")
set(PROGRAM_VERSION_NUMBER 0.2.0.0)

project("KalaServer" VERSION ${PROGRAM_VERSION_NUMBER} LANGUAGES C CXX)

# Adds a '_' prefix in front of the created binary files if set to TRUE
set(KALA_PRERELEASE TRUE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Runtime lib handling for MSVC
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Platform Detection
if (WIN32)
    message(STATUS "[KALASERVER] Platform = Windows")
else()
    message(FATAL_ERROR "[KALASERVER] Unsupported platform. Only Windows is supported.")
endif()

# Build Type Detection
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(IS_DEBUG TRUE)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(IS_RELEASE TRUE)
else()
    message(FATAL_ERROR "Unknown CMAKE_BUILD_TYPE: '${CMAKE_BUILD_TYPE}'! Must be Debug, Release, or RelWithDebInfo.")
endif()

# Paths
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")

set(EXT_SHARED_DIR "${CMAKE_SOURCE_DIR}/_external_shared")
set(EXT_OPENSSL_DIR "${EXT_SHARED_DIR}/OpenSSL")

# Library Paths
if(IS_RELEASE)
	set(OPENSSL_LIBSSL "${EXT_OPENSSL_DIR}/release/libssl.lib")
	set(OPENSSL_LIBCRYPTO "${EXT_OPENSSL_DIR}/release/libcrypto.lib")
else()
	set(OPENSSL_LIBSSL "${EXT_OPENSSL_DIR}/debug/libssl.lib")
	set(OPENSSL_LIBCRYPTO "${EXT_OPENSSL_DIR}/debug/libcrypto.lib")
endif()

# Source Files
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS
    "${SRC_DIR}/*.cpp"
    "${SRC_DIR}/*/*.cpp"
)

# SHARED LIBRARY (.dll)
add_library(KalaServer SHARED ${SOURCE_FILES})

# Enable C++ exception handling semantics for MSVC
if (MSVC)
    target_compile_options(KalaServer PRIVATE /EHsc)
endif()

# Get prefix
set(KALA_PREFIX "")
if (KALA_PRERELEASE)
	set(KALA_PREFIX "_")
endif()
	
# Get version
set(KALA_VERSION "${PROJECT_VERSION_MAJOR}${PROJECT_VERSION_MINOR}")
	
# Get suffix
if (IS_RELEASE)
	set(KALA_SUFFIX "")
else()
	set(KALA_SUFFIX "d")
endif()

# Set output name with renderer suffix and debug postfix
set_target_properties(KalaServer PROPERTIES
	OUTPUT_NAME "${KALA_PREFIX}KalaServer${KALA_VERSION}${KALA_SUFFIX}"
)

# Includes
file(GLOB_RECURSE HEADERS
	configure_depends
	"${CMAKE_SOURCE_DIR}/include/*.hpp"
)
target_sources(KalaServer PRIVATE ${HEADERS})
target_include_directories(KalaServer PRIVATE
	"${INCLUDE_DIR}"
	"${EXT_SHARED_DIR}"
	"${EXT_SHARED_DIR}/OpenSSL/include"
)

# Preprocessor Defines
target_compile_definitions(KalaServer PRIVATE 
	LIB_EXPORT
    WIN32_LEAN_AND_MEAN
    NOMINMAX
	UNICODE
	_UNICODE
)

# Link libraries
target_link_libraries(KalaServer PRIVATE
	${WINDOW_LIBRARY_PATH}
	${OPENSSL_LIBSSL}
	${OPENSSL_LIBCRYPTO}
)

# Expose version string
add_compile_definitions(
	PROGRAM_VERSION="${PROGRAM_VERSION}"
)

# Set Windows Details tab data
set(ICON_DIR "${CMAKE_SOURCE_DIR}/files/icon.ico")

if (IS_RELEASE)
    set(FILEFLAGS "0x00")
else()
	set(FILEFLAGS "0x01")
endif()
	
set(FILETYPE 0x1)    # 0x1 - executable, 0x2 - dynamic-link library
set(FILESUBTYPE 0x0) # 0x0 = normal DLL, only change if you're making drivers/fonts

# Public fields displayed in Windows Explorer
set(FILE_DESCRIPTION  "KalaServer")
set(FILE_VERSION      "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}.${PROJECT_VERSION_TWEAK}")
set(PRODUCT_NAME      "KalaServer")
set(PRODUCT_VERSION   "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}.${PROJECT_VERSION_TWEAK}")
set(COMPANY_NAME      "Lost Empire Entertainment")
set(LEGAL_COPYRIGHT   "(C) 2025 Lost Empire Entertainment")
set(LEGAL_TRADEMARKS  "Trademark of Lost Empire Entertainment")
set(ORIGINAL_FILENAME "${PRODUCT_NAME}")

# Private/optional fields
set(INTERNAL_NAME "${PRODUCT_NAME}")
set(COMMENTS      "${PRODUCT_NAME}")
set(PRIVATE_BUILD "${PRODUCT_NAME}")
set(SPECIAL_BUILD "${PRODUCT_NAME}")

# Generate version.rc
configure_file(
	${CMAKE_SOURCE_DIR}/version.rc.in
	${CMAKE_BINARY_DIR}/version.rc
	@ONLY
)

target_sources(KalaServer PRIVATE "${CMAKE_BINARY_DIR}/version.rc")

# Copy files directory
add_custom_command(TARGET KalaServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:KalaServer>/files"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/files" "$<TARGET_FILE_DIR:KalaServer>/files"
)

# Copy external binaries
if(IS_RELEASE)
	set(BIN_OPENSSL "${EXT_OPENSSL_DIR}/release")
else()
	set(BIN_OPENSSL "${EXT_OPENSSL_DIR}/release")
endif()

if (IS_RELEASE)
	file(GLOB BIN_FILES
		"${BIN_OPENSSL}/*.dll"
	)
else()
	file(GLOB BIN_FILES
		"${BIN_OPENSSL}/*.dll"
	)
endif()

set(BIN_TARGET_DIR "$<TARGET_FILE_DIR:KalaServer>")

foreach(BIN_FILE ${BIN_FILES})
    get_filename_component(BIN_NAME ${BIN_FILE} NAME)
    add_custom_command(TARGET KalaServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy 
			"${BIN_FILE}"
			"${BIN_TARGET_DIR}/${BIN_NAME}"
    )
endforeach()

# Copy important files
set(COPY_TARGETS README.md LICENSE.md logo.png)
foreach(FILE ${COPY_TARGETS})
add_custom_command(TARGET KalaServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -f 
		"$<TARGET_FILE_DIR:KalaServer>/${FILE}"
    COMMAND ${CMAKE_COMMAND} -E copy
		"${CMAKE_SOURCE_DIR}/${FILE}"
		"$<TARGET_FILE_DIR:KalaServer>/${FILE}"
)
endforeach()

# Copy files directory
add_custom_command(TARGET KalaServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:KalaServer>/files"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/files" "$<TARGET_FILE_DIR:KalaServer>/files"
)

# Copy entire docs folder
add_custom_command(TARGET KalaServer POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E rm -rf
		"$<TARGET_FILE_DIR:KalaServer>/docs"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_SOURCE_DIR}/docs"
		"$<TARGET_FILE_DIR:KalaServer>/docs"
)