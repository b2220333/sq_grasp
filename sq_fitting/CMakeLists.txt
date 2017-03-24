cmake_minimum_required(VERSION 2.8.3)
project(sq_fitting)

## Find catkin macros and libraries
## if COMPONENTS list like find_package(catkin REQUIRED COMPONENTS xyz)
## is used, also find other catkin packages
find_package(catkin REQUIRED COMPONENTS
  roscpp
  message_generation
  geometry_msgs
  pcl_ros
  pcl_conversions
)


find_package(PCL 1.8 REQUIRED)



## Generate messages in the 'msg' folder
add_message_files(
   FILES
   sq.msg
 )



## Generate added messages and services with any dependencies listed here
 generate_messages(
   DEPENDENCIES
   std_msgs  geometry_msgs # Or other packages containing msgs
 )


catkin_package(
  INCLUDE_DIRS
      include
  LIBRARIES
      sampling
      fitting
      utils
      segmentation
      sq_fitter
  CATKIN_DEPENDS roscpp
  DEPENDS system_lib
)

IF (PCL_FOUND)
  INCLUDE_DIRECTORIES(${PCL_INCLUDE_DIRS})
  LINK_DIRECTORIES(${PCL_LIBRARY_DIRS})
  ADD_DEFINITIONS(${PCL_DEFINITIONS})
ENDIF (PCL_FOUND)

include_directories(include ${catkin_INCLUDE_DIRS}  ${PCL_INCLUDE_DIR} ${CERES_INCLUDES})

###########
## Build ##
###########

## Specify additional locations of header files
## Your package locations should be listed before other locations
# include_directories(include)
include_directories(
  ${catkin_INCLUDE_DIRS}
)

add_library(sampling  src/sq_fitting/sampling.cpp)
add_library(fitting  src/sq_fitting/fitting.cpp)
add_library(utils  src/sq_fitting/utils.cpp)
add_library(segmentation  src/sq_fitting/segmentation.cpp)
add_library(sq_fitter  src/sq_fitting/sq_fitter.cpp)

target_link_libraries(sampling utils ${catkin_LIBRARIES}  ${PCL_LIBRARY_DIRS})
target_link_libraries(fitting utils ${catkin_LIBRARIES}  ${PCL_LIBRARY_DIRS})
target_link_libraries(utils  ${catkin_LIBRARIES}  ${PCL_LIBRARY_DIRS} )
target_link_libraries(segmentation  ${catkin_LIBRARIES}  ${PCL_LIBRARY_DIRS})
target_link_libraries(sq_fitter segmentation fitting utils sampling ${catkin_LIBRARIES}  ${PCL_LIBRARY_DIRS})


install(TARGETS sampling fitting segmentation utils sq_fitter
 ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
 LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
)

#Executables for nodes
add_executable(sq_fitter_node src/nodes/sq_fitter_node.cpp)
add_dependencies(sq_fitter_node ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
target_link_libraries(sq_fitter_node sq_fitter  ${catkin_LIBRARIES})



#executables for tests
add_executable(sampling_test_ros src/test/sampling_test_ros.cpp)
add_dependencies(sampling_test_ros ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
target_link_libraries(sampling_test_ros sampling  ${catkin_LIBRARIES})

add_executable(sampling_test_pcd src/test/sampling_test_pcd.cpp)
add_dependencies(sampling_test_pcd ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
target_link_libraries(sampling_test_pcd sampling  ${catkin_LIBRARIES})

add_executable(segmentation_test_pcd src/test/segmentation_test_pcd.cpp)
add_dependencies(segmentation_test_pcd ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
target_link_libraries(segmentation_test_pcd segmentation  ${catkin_LIBRARIES})

add_executable(segmentation_test_ros src/test/segmentation_test_ros.cpp)
add_dependencies(segmentation_test_ros ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
target_link_libraries(segmentation_test_ros segmentation  ${catkin_LIBRARIES})

add_executable(seg_and_fit_test_ros src/test/seg_and_fit_test_ros.cpp)
add_dependencies(seg_and_fit_test_ros ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
target_link_libraries(seg_and_fit_test_ros segmentation fitting sampling ${catkin_LIBRARIES} ${PCL_INCLUDE_DIR} )

add_executable(seg_and_fit_test_pcd src/test/seg_and_fit_test_pcd.cpp)
add_dependencies(seg_and_fit_test_pcd ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
target_link_libraries(seg_and_fit_test_pcd segmentation fitting sampling ${catkin_LIBRARIES} ${PCL_INCLUDE_DIR} )

