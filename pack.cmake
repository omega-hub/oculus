if(WIN32)
    file(INSTALL DESTINATION ${PACKAGE_DIR}/bin
        TYPE FILE
        FILES
            ${BIN_DIR}/oculus.pyd
        )
endif()

file(INSTALL DESTINATION ${PACKAGE_DIR}/modules/oculus
    TYPE FILE
    FILES
        ${SOURCE_DIR}/modules/oculus/rift.cfg
    )
	