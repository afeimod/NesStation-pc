# ============================================================
# Qt6OpenGLConfigVersion.cmake —— 与 Qt6OpenGLConfig.cmake 配套
# （Qt6Charts 的依赖链以 find_dependency(Qt6OpenGL 6.4.2) 请求版本，
#   需要本文件声明 6.4.2；详见 Qt6OpenGLConfig.cmake 头部说明）
# ============================================================
set(PACKAGE_VERSION "6.4.2")
if(PACKAGE_FIND_VERSION VERSION_EQUAL "6.4.2")
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
    set(PACKAGE_VERSION_EXACT TRUE)
elseif(PACKAGE_FIND_VERSION VERSION_LESS_EQUAL "6.4.2")
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
endif()
