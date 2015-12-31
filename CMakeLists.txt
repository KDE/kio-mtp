project(kio-mtp)

cmake_minimum_required(VERSION 2.8.12)

find_package(ECM 1.0.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

find_package(Qt5 CONFIG REQUIRED COMPONENTS Core)

find_package(Mtp)
find_package(KF5 REQUIRED COMPONENTS
    I18n
    KIO
    Solid
)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)
include(ECMPackageConfigHelpers)

add_definitions(-DQT_NO_CAST_FROM_ASCII)

set(kio_mtp_PART_SRCS
    devicecache.cpp
    filecache.cpp
    kio_mtp.cpp
    kio_mtp_helpers.cpp
)

add_library(kio_mtp MODULE ${kio_mtp_PART_SRCS} )
target_link_libraries(kio_mtp ${MTP_LIBRARIES} Qt5::Core KF5::KIOCore KF5::I18n KF5::Solid)

install(TARGETS kio_mtp DESTINATION ${PLUGIN_INSTALL_DIR})

########### install files ###############

install( FILES mtp-network.desktop  DESTINATION  ${DATA_INSTALL_DIR}/konqueror/dirtree/remote )
install( FILES solid_mtp.desktop  DESTINATION  ${DATA_INSTALL_DIR}/solid/actions )
install( FILES mtp.protocol  DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES mtp-network.desktop  DESTINATION  ${DATA_INSTALL_DIR}/remoteview )

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
