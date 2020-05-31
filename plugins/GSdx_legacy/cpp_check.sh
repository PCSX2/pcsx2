cppcheck --enable=warning,style,missingInclude -j 16 --platform=unix32 -D__linux__ -UENABLE_VTUNE -U_WINDOWS -U_M_AMD64 -U_MSC_VER . |& tee cpp_check.log
