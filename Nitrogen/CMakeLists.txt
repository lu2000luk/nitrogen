find_package(restinio REQUIRED)
find_package(Boost)
add_executable (Nitrogen "Nitrogen.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Nitrogen PROPERTY CXX_STANDARD 20)
endif()

target_link_libraries(Nitrogen PRIVATE restinio::restinio Boost::process Boost::headers)