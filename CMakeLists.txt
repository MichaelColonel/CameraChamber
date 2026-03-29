cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(ProfileCamera2D)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(ROOT REQUIRED)
include(${ROOT_USE_FILE})

find_package(boost_headers 1.73.0 REQUIRED CONFIG)
if(NOT boost_headers_FOUND)
  error("boost_headers package is not found. Program cannot be built.")
endif()

if(NOT withqt)
  if(ROOT_qt6web_FOUND)
    set(withqt 6)
  elseif(ROOT_qt5web_FOUND)
    set(withqt 5)
  else()
    message(FATAL_ERROR "Neither qt5web nor qt6web components were found")
  endif()
endif()

if(withqt EQUAL 6)
  find_package(Qt6 COMPONENTS Core Widgets WebEngineCore Xml OpenGL WebEngineWidgets SerialPort OpcUa OpenGLWidgets StateMachine Core5Compat CONFIG)
  set(qt_libs Qt6::Core Qt6::Widgets Qt6::WebEngineCore Qt6::WebEngineWidgets Qt6::SerialPort Qt6::OpcUa Qt6::Xml Qt6::OpenGL Qt6::OpenGLWidgets Qt6::StateMachine Qt6::Core5Compat)
  set(root_qtlib ROOT::ROOTQt6WebDisplay)
elseif(withqt EQUAL 5)
  find_package(Qt5 COMPONENTS Core Widgets WebEngine Xml OpenGL WebEngineWidgets SerialPort OpcUa REQUIRED)
  set(qt_libs Qt5::Core Qt5::Widgets Qt5::WebEngine Qt5::WebEngineWidgets Qt5::SerialPort Qt5::OpcUa Qt5::Xml Qt5::OpenGL)
  set(root_qtlib ROOT::ROOTQt5WebDisplay)
else()
  message(FATAL_ERROR "No matching withqt parameter found, check if qt5web or qt6web components are there")
endif()

find_package(CTK REQUIRED IMPORTED)
if(NOT CTK_FOUND)
  error("CTK package is not found. Program cannot be built.")
endif()
include(${CTK_USE_FILE})

set(RESOURCE_FILES
  Cameras.json
  C26AIC01/ChipsPosition.json
  C26AIC01/Chip1.11.json
  C26AIC01/Chip1.2.json
  C26AIC01/Chip1.3.json
  C26AIC01/Chip1.4.json
  C26AIC01/Chip1.5.json
  C26AIC01/Chip1.8.json
  C26AIC01/Chip2.1.json
  C26AIC01/Chip2.2.json
  C26AIC01/Chip2.4.json
  C26AIC01/Chip2.5.json
  C26AIC01/Chip2.6.json
  C26AIC01/Chip2.7.json
  C26AIC02/ChipsPosition.json
  C26AIC02/Chip1.1.json
  C26AIC02/Chip1.10.json
  C26AIC02/Chip1.12.json
  C26AIC02/Chip1.6.json
  C26AIC02/Chip1.7.json
  C26AIC02/Chip1.9.json
  C26AIC02/Chip2.10.json
  C26AIC02/Chip2.3.json
  C26AIC02/Chip2.8.json
  C26AIC02/Chip2.9.json
  C26AIC02/ChipX.X.json
  C26AIC03/ChipsPosition.json
  C26AIC03/Chip3.1.json
  C26AIC03/Chip3.10.json
  C26AIC03/Chip3.11.json
  C26AIC03/Chip3.12.json
  C26AIC03/Chip3.13.json
  C26AIC03/Chip3.14.json
  C26AIC03/Chip3.2.json
  C26AIC03/Chip3.3.json
  C26AIC03/Chip3.5.json
  C26AIC03/Chip3.7.json
  C26AIC03/Chip3.8.json
  C26AIC03/ChipX.X.json
  C26AIC04/ChipsPosition.json
  C26AIC04/ChipX.X.json
)

file(GLOB forms ${PROJECT_SOURCE_DIR}/*.ui)
file(GLOB sources ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB headers ${PROJECT_SOURCE_DIR}/*.h)
file(GLOB batch_mode ${PROJECT_SOURCE_DIR}/root_profile_batch_mode.cc)

add_executable(ProfileCamera2D
  ${sources}
  ${headers}
  ${forms}
)

add_executable(profile_offline ${batch_mode})

target_link_libraries(ProfileCamera2D
  CTKWidgets
  ${root_qtlib}
  ${qt_libs}
  ${ROOT_LIBRARIES}
  ROOT::ROOTWebDisplay
  ROOT::WebGui6
  ROOT::Gpad
  ROOT::ROOTGpadv7
  ROOT::ROOTCanvasPainter
  ROOT::Geom
  ROOT::ROOTBrowserv7
  ROOT::ROOTGeomViewer
)

target_link_libraries(profile_offline ${ROOT_LIBRARIES})

install(FILES Cameras.json DESTINATION bin)
install(DIRECTORY C26AIC01 DESTINATION bin)
install(DIRECTORY C26AIC02 DESTINATION bin)
install(DIRECTORY C26AIC03 DESTINATION bin)
install(DIRECTORY C26AIC04 DESTINATION bin)

#set_target_properties(ProfileCamera2D PROPERTIES
#  MACOSX_BUNDLE TRUE
#  MACOSX_FRAMEWORK_IDENTIFIER org.cmake.ProfileCamera2D
#  RESOURCE "${RESOURCE_FILES}"
#)

install(TARGETS ProfileCamera2D DESTINATION bin)
install(TARGETS profile_offline DESTINATION bin)
