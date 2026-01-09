include_guard(GLOBAL)

# Requires CMake >= 3.18 for file(ARCHIVE_EXTRACT)
function(extract_github_release TARGET_DIR)
    # Usage:
    #   extract_github_release(
    #       "<target_dir>"
    #       ARCHIVE "<path-to-archive>"
    #       TAG  v1.2.3                    # optional but needed when VERSION missing
    #       VERSION 1.2.3                  # optional but needed when TAG missing
    #       ASSET_NAME "archive.zip"       # optional, only used for logging
    #   )

    set(options)
    set(oneValueArgs ARCHIVE TAG VERSION ASSET_NAME)
    set(multiValueArgs)
    cmake_parse_arguments(EGR "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(EXISTS "${TARGET_DIR}")
        return()
    endif()

    if(NOT EGR_ARCHIVE)
        message(FATAL_ERROR "extract_github_release: ARCHIVE is required")
    endif()

    if(NOT EGR_TAG AND NOT EGR_VERSION)
        message(FATAL_ERROR "extract_github_release: at least TAG or VERSION must be provided")
    endif()

    if(NOT EGR_ASSET_NAME)
        get_filename_component(EGR_ASSET_NAME "${EGR_ARCHIVE}" NAME)
    endif()

    message(STATUS "Extracting ${EGR_ASSET_NAME} to ${TARGET_DIR}…")

    set(EXTERNAL_ROOT "${TARGET_DIR}")
    file(MAKE_DIRECTORY "${EXTERNAL_ROOT}")

    file(ARCHIVE_EXTRACT
        INPUT "${EGR_ARCHIVE}"
        DESTINATION "${EXTERNAL_ROOT}"
    )

    set(_search_globs)
    if(EGR_TAG)
        list(APPEND _search_globs "${EXTERNAL_ROOT}/*${EGR_TAG}*")
    endif()
    if(EGR_VERSION)
        list(APPEND _search_globs "${EXTERNAL_ROOT}/*${EGR_VERSION}*")
    endif()

    file(GLOB EXTRACTED_DIRS
        LIST_DIRECTORIES TRUE
        ${_search_globs}
    )

    # list(LENGTH EXTRACTED_DIRS _dir_count)
    # if(_dir_count EQUAL 0)
    #     message(FATAL_ERROR
    #         "extract_github_release: could not find extracted directory for ${EGR_ARCHIVE}")
    # endif()

    # list(GET EXTRACTED_DIRS 0 EXTRACTED_DIR)
    # file(RENAME "${EXTRACTED_DIR}" "${TARGET_DIR}")
endfunction()

function(download_github_release TARGET_DIR)
    # Usage:
    #   download_github_release(
    #       "<target_dir>"
    #       REPO owner/repo
    #       TAG  v1.2.3
    #       VERSION 1.2.3                        # optional, defaults to TAG
    #       SYSTEM linux-x86_64                  # optional, defaults from CMake
    #       ASSET_TEMPLATE "mylib-{version}-{system}.zip" # optional
    #   )

    set(options)
    set(oneValueArgs REPO TAG VERSION SYSTEM ASSET_TEMPLATE)
    set(multiValueArgs)
    cmake_parse_arguments(DGR "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # ---- Required args ----
    if(NOT DGR_REPO)
        message(FATAL_ERROR "download_github_release: REPO is required")
    endif()
    if(NOT DGR_TAG)
        message(FATAL_ERROR "download_github_release: TAG is required")
    endif()

    # ---- Defaults ----
    if(NOT DGR_VERSION)
        set(DGR_VERSION "${DGR_TAG}")
    endif()

    if(NOT DGR_SYSTEM)
        # Normalize to something like "linux-x86_64"
        string(TOLOWER "${CMAKE_SYSTEM_NAME}"   _sys)
        string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _arch)
        set(DGR_SYSTEM "${_sys}-${_arch}")
    endif()

    if(NOT DGR_ASSET_TEMPLATE)
        # Default asset name: <repo-name>-<version>-<system>.zip
        string(REGEX REPLACE ".*/" "" _repo_name "${DGR_REPO}")
        set(DGR_ASSET_TEMPLATE "${_repo_name}-${DGR_VERSION}-${DGR_SYSTEM}.zip")
    endif()

    # ---- Build final asset name from template ----
    set(ASSET_NAME "${DGR_ASSET_TEMPLATE}")
    string(REPLACE "{version}" "${DGR_VERSION}" ASSET_NAME "${ASSET_NAME}")
    string(REPLACE "{system}"  "${DGR_SYSTEM}"  ASSET_NAME "${ASSET_NAME}")

    set(DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/downloads")
    file(MAKE_DIRECTORY "${DOWNLOAD_DIR}")

    set(ARCHIVE_PATH "${DOWNLOAD_DIR}/${ASSET_NAME}")

    set(RELEASE_URL "${DGR_REPO}/releases/download/${DGR_TAG}/${ASSET_NAME}")
    set(TAG_ARCHIVE_URL "${DGR_REPO}/archive/refs/tags/${DGR_TAG}.zip")

    # ---- Download ----
    if(NOT EXISTS "${ARCHIVE_PATH}")
        message(STATUS "Downloading ${RELEASE_URL}")
        file(DOWNLOAD
            "${RELEASE_URL}"
            "${ARCHIVE_PATH}"
            SHOW_PROGRESS
            STATUS DOWNLOAD_STATUS
        )

        list(GET DOWNLOAD_STATUS 0 DOWNLOAD_ERROR)
        if(DOWNLOAD_ERROR)
            message(WARNING "download_github_release: failed to get ${RELEASE_URL}, trying tag archive…")
            file(DOWNLOAD
                "${TAG_ARCHIVE_URL}"
                "${ARCHIVE_PATH}"
                SHOW_PROGRESS
                STATUS DOWNLOAD_STATUS_ALT
            )
            list(GET DOWNLOAD_STATUS_ALT 0 DOWNLOAD_ERROR_ALT)
            if(DOWNLOAD_ERROR_ALT)
                message(FATAL_ERROR
                    "download_github_release: failed to download both\n"
                    "  ${RELEASE_URL}\n"
                    "  ${TAG_ARCHIVE_URL}")
            endif()
        endif()
    endif()

    # ---- Extract ----
    extract_github_release("${TARGET_DIR}"
        ARCHIVE "${ARCHIVE_PATH}"
        TAG "${DGR_TAG}"
        VERSION "${DGR_VERSION}"
        ASSET_NAME "${ASSET_NAME}"
    )


endfunction()
