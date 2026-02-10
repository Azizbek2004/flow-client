if(WIN32)
    set(RootDir "@RootDir@")
    configure_file(
        ${CMAKE_CURRENT_LIST_DIR}/config/windows.xml.in
        ${CMAKE_BINARY_DIR}/installer/config/windows.xml
    )
elseif(LINUX)
    set(ApplicationsDir "@ApplicationsDir@")
    configure_file(
        ${CMAKE_CURRENT_LIST_DIR}/config/linux.xml.in
        ${CMAKE_BINARY_DIR}/installer/config/linux.xml
    )
    
    configure_file(
        ${CMAKE_CURRENT_LIST_DIR}/config/Flow.desktop.in
        ${CMAKE_BINARY_DIR}/../AppDir/Flow.desktop
    )
endif()

configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/packages/org.flow.client/meta/package.xml.in
    ${CMAKE_BINARY_DIR}/installer/packages/org.flow.client/meta/package.xml
)
