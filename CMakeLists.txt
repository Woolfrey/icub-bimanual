cmake_minimum_required(VERSION 2.8.9)					                            # Minimum cmake requirement

project(icub-bimanual)							                            # ${PROJECT_NAME}

if(NOT CMAKE_BUILD_TYPE)
#	set(CMAKE_BUILD_TYPE Debug)                                                                 # Not optimised
	set(CMAKE_BUILD_TYPE Release)                                                               # Optimised
#	set(CMAKE_BUILD_TYPE RelWithDebInfo)                                    
#	set(CMAKE_BUILD_TYPE MinSizeRel)
endif()

################################## Necessary packages to compile ###################################
find_package(Eigen3 REQUIRED)
#find_package(ICUB REQUIRED)                                                                        # Links below to ICUB::iKin
find_package(iDynTree REQUIRED)                                                                     # Links below to iDynTree
find_package(YARP 3.3.0 REQUIRED)                                                                   # Links below to ${YARP_LIBRARIES}


######################################## iCub related stuff ########################################
find_package(ICUBcontrib REQUIRED)                                                                  # Mandatory
list(APPEND CMAKE_MODULE_PATH ${ICUBCONTRIB_MODULE_PATH})                                           # Extend the current search path to load helpers
include(ICUBcontribHelpers)                                                                         # I don't know what this does
include(ICUBcontribOptions)                                                                         # I don't know what this does
#icubcontrib_set_default_prefix()
icubcontrib_add_uninstall_target()					                            # Enables uninstall

####################################### Other files to be built ####################################
add_subdirectory(gazebo)                                                                            # Location of other CMakeLists
include_directories(include)                                                                        # Location of header files

#################################### Executables to be compiled ####################################
add_executable(test_build src/test_build.cpp)
target_link_libraries(test_build Eigen3::Eigen iDynTree::idyntree-high-level ${YARP_LIBRARIES})

add_executable(grasp-demo src/grasp-demo.cpp)
target_link_libraries(grasp-demo Eigen3::Eigen iDynTree::idyntree-high-level ${YARP_LIBRARIES})
