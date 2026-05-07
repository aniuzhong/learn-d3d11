# FindGLM — locate the bundled GLM headers in includes/
#
# GLM is a header-only library, so we only need the include path.

find_path(GLM_INCLUDE_DIR
    NAMES glm/glm.hpp
    PATHS ${CMAKE_SOURCE_DIR}/includes
    NO_DEFAULT_PATH
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GLM
    REQUIRED_VARS GLM_INCLUDE_DIR
    VERSION_VAR GLM_VERSION
)

if(GLM_FOUND)
    set(GLM_INCLUDE_DIRS ${GLM_INCLUDE_DIR})
    mark_as_advanced(GLM_INCLUDE_DIR)
endif()
