# Resolves an imported target that provides the Boost.Asio headers.
#
# Included both by this project when it builds and by the installed package config, so
# that a package built against one Boost provider still works for a consumer that has a
# different one. Only headers are needed, so any of these targets will do:
#
#   1. modular Boost / vcpkg (boost_asio-config.cmake) -> Boost::asio
#   2. a monolithic BoostConfig.cmake                  -> Boost::headers
#   3. no CMake package at all (apt, brew, old Boost)  -> FindBoost module
#
# Sets <out_var> to the target name, or to an empty string if Boost was not found. The
# caller decides what an empty result means (a fatal error while building, a not-found
# package while being consumed).
#
# Imported targets created here live in the caller directory scope, as usual for targets,
# even though this is a function - only the variables are scoped.

function(slick_dynamic_buffer_find_boost_asio out_var)
    set(${out_var} "" PARENT_SCOPE)

    if(NOT TARGET Boost::asio)
        find_package(Boost CONFIG QUIET COMPONENTS asio)
    endif()
    if(TARGET Boost::asio)
        set(${out_var} Boost::asio PARENT_SCOPE)
        return()
    endif()

    if(NOT TARGET Boost::headers)
        find_package(Boost CONFIG QUIET)
    endif()

    if(NOT TARGET Boost::headers AND NOT TARGET Boost::boost)
        if(POLICY CMP0167)
            cmake_policy(PUSH)
            cmake_policy(SET CMP0167 OLD)   # keep the deprecated FindBoost module usable
        endif()
        find_package(Boost MODULE QUIET)
        if(POLICY CMP0167)
            cmake_policy(POP)
        endif()
    endif()

    if(TARGET Boost::headers)
        set(${out_var} Boost::headers PARENT_SCOPE)
    elseif(TARGET Boost::boost)
        set(${out_var} Boost::boost PARENT_SCOPE)
    endif()
endfunction()
