project(kio-mtp)

cmake_minimum_required(VERSION 2.6)

set( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules )

find_package(KDE4 REQUIRED)
find_package(Mtp)

#add_definitions(-DKDE_DEFAULT_DEBUG_AREA=7999)

set( kio_mtp_PART_SRCS
     filecache.cpp
     kio_mtp.cpp
)

include_directories(
    ${KDE4_INCLUDES}
    ${CMAKE_CURRENT_BINARY_DIR}
)

kde4_add_plugin( kio_mtp ${kio_mtp_PART_SRCS} )
target_link_libraries( kio_mtp ${KDE4_KIO_LIBRARY} ${MTP_LIBRARIES} ${KDE4_SOLID_LIBS} )

install( TARGETS kio_mtp DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES mtp-network.desktop  DESTINATION  ${DATA_INSTALL_DIR}/konqueror/dirtree/remote )
install( FILES solid_mtp.desktop  DESTINATION  ${DATA_INSTALL_DIR}/solid/actions )
install( FILES mtp.protocol  DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES mtp-network.desktop  DESTINATION  ${DATA_INSTALL_DIR}/remoteview )
