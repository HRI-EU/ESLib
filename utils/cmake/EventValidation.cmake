if (${CMAKE_VERSION} VERSION_LESS 3.5)
    message(WARNING "You are currently using CMake ${CMAKE_VERSION}. Please update to 3.5 or later.")

    function(target_validate_events _target)
        message(WARNING "target_validate_events for ${_target} requires CMake 3.5 or later. Because are using ${CMAKE_VERSION}, all event validation features are not available.")
    endfunction()

else()
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    cmake_policy(SET CMP0057 NEW)

    set(ESLIB_UTIL_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

    option(ESLIB_VALIDATE_EVENTS "Allow validation of event usage for build targets" OFF)
    option(ESLIB_VALIDATE_BLAME "Include git blame info in event validation errors" ON)

    if (ESLIB_VALIDATE_EVENTS)
        message(STATUS "ESLib Event Validation ON")

        # copy event visualization tools to build directory for convenience
        file(COPY
            ${ESLIB_UTIL_ROOT}/event_visualization
            DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)

        if (NOT ${CMAKE_EXPORT_COMPILE_COMMANDS})
            message(FATAL_ERROR "ERROR: In order to use event validation, you MUST enabled CMAKE_EXPORT_COMPILE_COMMANDS")
        endif()

        if (ESLIB_VALIDATE_BLAME)
            message(STATUS "ESLib Event Validation will use git blame")
        else()
            message(STATUS "ESLib Event Validation will not use git blame")
        endif()
    else()
        message(STATUS "ESLib Event Validation is DISABLED")
    endif()

    function(target_link_libraries _target)
        set(_mode "PUBLIC")
        foreach(_arg IN LISTS ARGN)
            if (_arg MATCHES "INTERFACE|PUBLIC|PRIVATE|LINK_PRIVATE|LINK_PUBLIC|LINK_INTERFACE_LIBRARIES")
                set(_mode "${_arg}")
            else()
                if (NOT _arg MATCHES "debug|optimized|general")
                    set_property(GLOBAL APPEND PROPERTY GlobalTargetDepends${_target} ${_arg})
                endif()
            endif()
        endforeach()
        _target_link_libraries(${_target} ${ARGN})
    endfunction()

    function(get_link_dependencies _target _listvar)
        set(_worklist ${${_listvar}})
        if (TARGET ${_target})
            list(APPEND _worklist ${_target})
            get_property(_dependencies GLOBAL PROPERTY GlobalTargetDepends${_target})
            foreach(_dependency IN LISTS _dependencies)
                if (NOT _dependency IN_LIST _worklist)
                    get_link_dependencies(${_dependency} _worklist)
                endif()
            endforeach()
            set(${_listvar} "${_worklist}" PARENT_SCOPE)
        endif()
    endfunction()

    function(get_target_dependent_sources _target _listvar)
        set(_outlist ${${_listvar}})
        get_link_dependencies(${_target} _deps)
        foreach(_dep IN LISTS _deps)
            get_target_property(_srcs ${_dep} SOURCES)
            get_target_property(_src_dir ${_dep} SOURCE_DIR)
            foreach(_src IN LISTS _srcs)
                if (NOT _src IN_LIST _outlist)
                    list(APPEND _outlist "${_src_dir}/${_src}")
                endif()
            endforeach()
        endforeach()
        set(${_listvar} "${_outlist}" PARENT_SCOPE)
    endfunction()

    function(target_validate_events _target)
        if (ESLIB_VALIDATE_EVENTS)
            get_target_dependent_sources(${_target} _target_total_sources)
            ADD_CUSTOM_COMMAND(TARGET ${_target}
                    POST_BUILD
                    COMMAND python "${ESLIB_UTIL_ROOT}/event-scan.py" "${CMAKE_BINARY_DIR}" "${CMAKE_BINARY_DIR}/${_target}_eventdata.json" "${CMAKE_SOURCE_DIR}" "'${_target_total_sources}'"
                    COMMENT "Collecting event usage data in ${_target}..."
                    )
            ADD_CUSTOM_COMMAND(TARGET ${_target}
                    POST_BUILD
                    COMMAND python "${ESLIB_UTIL_ROOT}/event-graph.py" "${CMAKE_BINARY_DIR}/${_target}_eventdata.json" "${CMAKE_BINARY_DIR}/${_target}_eventgraph.json"
                    COMMENT "Compiling event graphs for ${_target}..."
                    )
            ADD_CUSTOM_COMMAND(TARGET ${_target}
                    POST_BUILD
                    COMMAND python "${ESLIB_UTIL_ROOT}/gen-event-page.py" "${ESLIB_UTIL_ROOT}/event_visualization/event-graph.html" "${CMAKE_BINARY_DIR}/${_target}_eventgraph.json" "${CMAKE_BINARY_DIR}/event_visualization/${_target}_Events.html"
                    COMMENT "Generating standalone event visualization page in: ${CMAKE_BINARY_DIR}/event_visualization/${_target}_Events.html"
                    )
            if (ESLIB_VALIDATE_BLAME)
                ADD_CUSTOM_COMMAND(TARGET ${_target}
                        POST_BUILD
                        COMMAND python "${ESLIB_UTIL_ROOT}/event-check.py" "${CMAKE_BINARY_DIR}/${_target}_eventdata.json" "${CMAKE_SOURCE_DIR}"
                        COMMENT "Validating event usage in ${_target}..."
                        )
            else()
                ADD_CUSTOM_COMMAND(TARGET ${_target}
                        POST_BUILD
                        COMMAND python "${ESLIB_UTIL_ROOT}/event-check.py" "${CMAKE_BINARY_DIR}/${_target}_eventdata.json"
                        COMMENT "Validating event usage in ${_target}..."
                        )
            endif()
        endif()
    endfunction()

    if (ESLIB_VALIDATE_EVENTS)

      add_custom_target(map_all_events
            COMMAND python "${ESLIB_UTIL_ROOT}/event-scan.py" "${CMAKE_BINARY_DIR}" "${CMAKE_BINARY_DIR}/All_eventdata.json" "${CMAKE_SOURCE_DIR}"
            COMMAND python "${ESLIB_UTIL_ROOT}/event-graph.py" "${CMAKE_BINARY_DIR}/All_eventdata.json" "${CMAKE_BINARY_DIR}/All_eventgraph.json"
            COMMENT "Gathering and mapping event usage for all RCS sources..."
            )
    add_custom_target(all_events_page
            COMMAND python "${ESLIB_UTIL_ROOT}/gen-event-page.py" "${ESLIB_UTIL_ROOT}/event_visualization/event-graph.html" "${CMAKE_BINARY_DIR}/All_eventgraph.json" "${CMAKE_BINARY_DIR}/event_visualization/All_Events.html"
            DEPENDS map_all_events
            COMMENT "Generating standalone event visualization page in: ${CMAKE_BINARY_DIR}/event_visualization/All_Events.html"
            )
    add_custom_target(validate_all_events
            COMMAND python "${ESLIB_UTIL_ROOT}/event-check.py" "${CMAKE_BINARY_DIR}/All_eventdata.json" "${CMAKE_SOURCE_DIR}"
            DEPENDS map_all_events
            COMMENT "Verifying event usage for all RCS sources..."
            )

	endif (ESLIB_VALIDATE_EVENTS)
	
endif()
