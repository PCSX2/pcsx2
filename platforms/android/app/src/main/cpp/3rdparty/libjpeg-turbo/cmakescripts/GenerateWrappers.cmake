if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR must be specified")
endif()

foreach(FILE jcapistd.c jccolor.c jcdiffct.c jclossls.c jcmainct.c jcprepct.c
  jcsample.c jdapistd.c jdcolor.c jddiffct.c jdlossls.c jdmainct.c jdpostct.c
  jdsample.c jutils.c rdpng.c rdppm.c wrpng.c wrppm.c)

  foreach(BITS 8 12 16)

    string(REGEX REPLACE "\\.c" "-${BITS}.c" WRAPPER_FILE ${FILE})
    if(BITS EQUAL 8)
      set(BITS_RANGE "2 to 8")
    elseif(BITS EQUAL 12)
      set(BITS_RANGE "9 to 12")
    else()
      set(BITS_RANGE "13 to 16")
    endif()

    configure_file(${SOURCE_DIR}/template.c ${SOURCE_DIR}/${WRAPPER_FILE})

  endforeach()
endforeach()

foreach(FILE jccoefct.c jcdctmgr.c jdcoefct.c jddctmgr.c jdmerge.c jfdctfst.c
  jfdctint.c jidctflt.c jidctfst.c jidctint.c jidctred.c jquant1.c jquant2.c
  rdcolmap.c wrgif.c)

  foreach(BITS 8 12)

    string(REGEX REPLACE "\\.c" "-${BITS}.c" WRAPPER_FILE ${FILE})
    set(BITS_RANGE ${BITS})

    configure_file(${SOURCE_DIR}/template.c ${SOURCE_DIR}/${WRAPPER_FILE})

  endforeach()
endforeach()
