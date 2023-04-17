# Download/update with <name = fName> and set path to download as <path = fPath> from fURL with tag fTAG
# Can use master for fTAG.  Post function fNAME will point to _SOURCE_DIR where the download can be found
# Submodules are not recursed or init'd (See INIT_SUBMODULES below)
function(GetGithubCode fNAME fPATH fURL fTAG)
    message(STATUS "Processing ${fNAME} from ${fURL} TAG ${fTAG}")
    FetchContent_Declare(
        ${fNAME}
        GIT_REPOSITORY          ${fURL}
        GIT_TAG                 ${fTAG}
        GIT_SUBMODULES          ""
        GIT_SUBMODULES_RECURSE  FALSE
    )
    if(NOT ${${NAME}}_POPULATED)
        FetchContent_Populate(${fNAME})
    endif()
    string(TOLOWER ${fNAME} fLOWER_NAME)
    set(${fPATH} ${${fLOWER_NAME}_SOURCE_DIR})
    set(${fPATH} ${${fLOWER_NAME}_SOURCE_DIR} PARENT_SCOPE)
    message(STATUS "${fPATH} is ${${fPATH}}")
endfunction(GetGithubCode)

# Path name and optionally --recursive for a git folder that needs submodules init'd
function(InitSubmodules MODULE_PATH)
    # Make sure this is a git repository
    if(GIT_FOUND AND EXISTS "${MODULE_PATH}/.git")
        message(STATUS "Init the submodules at ${MODULE_PATH}")
        # Update submodules as needed
        option(GIT_SUBMODULE "Check submodules during build" ON)
        if(GIT_SUBMODULE)
            execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init ${ARGV1}
                            WORKING_DIRECTORY ${MODULE_PATH}
                            RESULT_VARIABLE GIT_SUBMOD_RESULT)
            if(NOT GIT_SUBMOD_RESULT EQUAL "0")
                message(FATAL_ERROR "git submodule update --init ${ARGV1} failed with ${GIT_SUBMOD_RESULT} in ${MODULE_PATH}. Please checkout submodules")
            endif()
        endif()
        if(NOT EXISTS "${PICO_SDK_PATH}/CMakeLists.txt")
            message(FATAL_ERROR "The submodules were not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
        endif()
    else()
        message(FATAL_ERROR "Git was not found or ${MODULE_PATH}/.git does not exist")
    endif()
endfunction(InitSubmodules)

# Set up a patch to a file, in a target, and optionally replace the original file with the patch
function(PatchFile target in_file patch_file out_file replace_infile)
    message(STATUS "PatchFile has been requested for:\n\tinfile=${in_file}\n\tpatchfile=${patch_file}\n\toutfile=${out_file}\n\treplaceinfile=${replace_infile}")
    add_custom_command(
        OUTPUT ${out_file}
        COMMAND ${CMAKE_COMMAND}
            -Din_file:FILEPATH=${in_file}
            -Dpatch_file:FILEPATH=${patch_file}
            -Dout_file:FILEPATH=${out_file}
            -Dreplace_infile:BOOL=${replace_infile}
            -P ${CMAKE_CURRENT_LIST_DIR}/CMakeModules/PatchFile.cmake
            DEPENDS ${in_file}
    )
    ADD_CUSTOM_TARGET(patch_${target} DEPENDS ${out_file})
    add_dependencies(${target} patch_${target})
endfunction(PatchFile)

# Copy source to destination only if destination.original does not exist
function(CopyConditional sentinal source destination)
    if(NOT EXISTS ${sentinal})
        if(EXISTS ${destination})
            message(STATUS "Saving ${destination} to ${sentinal}")
            file(RENAME ${destination} ${sentinal})
            else()
            message(STATUS "Touching ${sentinal}")
            file(TOUCH ${sentinal})
        endif()
        get_filename_component(destinationPath ${destination} DIRECTORY)
        get_filename_component(sourceFile ${source} NAME)
        message(STATUS "Copy ${source} to ${destination}")
        file(COPY ${source} DESTINATION ${destinationPath})
        file(RENAME ${destinationPath}/${sourceFile} ${destination})
    endif()
endfunction(CopyConditional)

# This is just a snippet of helper code for me when I debug - Stefan
# get_cmake_property(_variableNames VARIABLES)
# list (SORT _variableNames)
# foreach (_variableName ${_variableNames})
#     message(STATUS "${_variableName}=${${_variableName}}")
# endforeach()
