set(Eigen_VERSION 3.4.0)

CPMAddPackage(
  NAME Eigen
  VERSION ${Eigen_VERSION}
  URL https://gitlab.com/libeigen/eigen/-/archive/${Eigen_VERSION}/eigen-${Eigen_VERSION}.tar.gz
  DOWNLOAD_ONLY YES 
)
if(Eigen_ADDED)
  add_library(Eigen INTERFACE IMPORTED)
  target_include_directories(Eigen INTERFACE ${Eigen_SOURCE_DIR})
endif()
