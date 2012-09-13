project(kio-mtp)

cmake_minimum_required(VERSION 2.6)

set( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules )

find_package(KDE4 REQUIRED)
find_package(Mtp REQUIRED)
# find_package(PkgConfig REQUIRED)

#add_definitions(-DKDE_DEFAULT_DEBUG_AREA=7999)

set( kio_mtp_PART_SRCS
     kio_mtp.cpp
)

include_directories(
    ${KDE4_INCLUDES}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# execute_process(COMMAND pkg-config dbus-1 --variable=prefix OUTPUT_VARIABLE DBUS_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
# set(mtpd_xml ${DBUS_PREFIX}/interfaces/org.mtpd.xml)
#
# set_source_files_properties(${mtpd_xml} PROPERTIES INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}"/metatypes.h")
# qt4_add_dbus_interface(dbus_SRCS ${mtpd_xml} mtpd-interface )

kde4_add_plugin( kio_mtp ${kio_mtp_PART_SRCS} )
target_link_libraries( kio_mtp ${KDE4_KIO_LIBRARY} ${MTP_LIBRARIES} )

install( TARGETS kio_mtp DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES mtp-network.desktop  DESTINATION  ${DATA_INSTALL_DIR}/konqueror/dirtree/remote )
install( FILES mtp.protocol  DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES mtp-network.desktop  DESTINATION  ${DATA_INSTALL_DIR}/remoteview )

