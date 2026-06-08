include(cmake/CPM.cmake)

add_executable (Nitrogen "Nitrogen.cpp" "Nitrogen.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Nitrogen PROPERTY CXX_STANDARD 20)
endif()
