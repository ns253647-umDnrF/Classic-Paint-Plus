cmake_minimum_required(VERSION 3.15)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(ReactOSPaintStandalone LANGUAGES C CXX RC)
if(NOT MSVC)
    message(FATAL_ERROR "This standalone configuration requires MSVC")
endif()
set(PAINT "${CMAKE_CURRENT_SOURCE_DIR}/..")
include(ExternalProject)
set(JPEG_SOURCE "${PAINT}/third_party/libjpeg-turbo-3.2.1")
set(JPEG_BUILD "${CMAKE_CURRENT_BINARY_DIR}/jpeg")
file(MAKE_DIRECTORY "${JPEG_BUILD}")
ExternalProject_Add(paint_jpeg_build
    SOURCE_DIR "${JPEG_SOURCE}" BINARY_DIR "${JPEG_BUILD}"
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DENABLE_SHARED=OFF -DENABLE_STATIC=ON
        -DWITH_CRT_DLL=OFF -DWITH_SIMD=OFF -DWITH_TURBOJPEG=OFF -DWITH_TOOLS=OFF
        -DWITH_TESTS=OFF -DCMAKE_MT=${CMAKE_MT}
    BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --config Release --target jpeg-static
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS "${JPEG_BUILD}/jpeg-static.lib")
add_library(paint_jpeg STATIC IMPORTED)
set_target_properties(paint_jpeg PROPERTIES IMPORTED_LOCATION "${JPEG_BUILD}/jpeg-static.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${JPEG_SOURCE}/src;${JPEG_BUILD}")
add_dependencies(paint_jpeg paint_jpeg_build)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(WEBP_LINK_STATIC ON CACHE BOOL "" FORCE)
foreach(option ANIM_UTILS CWEBP DWEBP GIF2WEBP IMG2WEBP VWEBP WEBPINFO LIBWEBPMUX WEBPMUX EXTRAS)
    set(WEBP_BUILD_${option} OFF CACHE BOOL "" FORCE)
endforeach()
add_subdirectory("${PAINT}/third_party/libwebp-1.6.0" "webp" EXCLUDE_FROM_ALL)
add_library(paint_png STATIC "${PAINT}/third_party/lodepng/lodepng.cpp")
set(SQUISH_SOURCE "${PAINT}/third_party/libsquish-1.15")
set(SQUISH_FILES alpha.cpp clusterfit.cpp colourblock.cpp colourfit.cpp colourset.cpp maths.cpp rangefit.cpp singlecolourfit.cpp squish.cpp)
list(TRANSFORM SQUISH_FILES PREPEND "${SQUISH_SOURCE}/")
add_library(paint_squish STATIC ${SQUISH_FILES})
target_compile_definitions(paint_squish PRIVATE SQUISH_USE_SSE=2)
set(SOURCES shellintegration.cpp paintmetadata.cpp documentalpha.cpp pixelunits.cpp panelsettings.cpp panelicons.cpp figures.cpp figuregeometry.cpp figuregroups.cpp figurecontrols.cpp parameterstate.cpp parameterbar.cpp selectionresize.cpp selectionrotation.cpp selectiongeometry.cpp figureclipboard.cpp canvasdialogs.cpp settings.cpp colorpicker.cpp canvas.cpp dialogs.cpp dib.cpp drawing.cpp fullscreen.cpp history.cpp
    main.cpp applicationinfo.cpp miniature.cpp mouse.cpp palette.cpp palettemodel.cpp registry.cpp
    selectionmodel.cpp sizebox.cpp textedit.cpp editframe.cpp transformframe.cpp skewcontrols.cpp toolbox.cpp toolsettings.cpp
    toolsmodel.cpp jpegcodec.cpp jpegoptions.cpp advanced.cpp brushpanel.cpp texttransform.cpp textwarp.cpp theme.cpp raster.cpp quantize.cpp imagecodec.cpp tgacodec.cpp ddscodec.cpp tiffcodec.cpp icocodec.cpp imageoptions.cpp)
list(TRANSFORM SOURCES PREPEND "${PAINT}/")
find_package(Python3 COMPONENTS Interpreter REQUIRED)
file(GLOB PAINT_RESOURCE_INPUTS CONFIGURE_DEPENDS "${PAINT}/*.rc" "${PAINT}/lang/*.rc")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${PAINT_RESOURCE_INPUTS}
    "${CMAKE_CURRENT_SOURCE_DIR}/unicode_resources.py")
set(PAINT_UNICODE_RESOURCE "${CMAKE_CURRENT_BINARY_DIR}/paint-unicode.rc")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unicode_resources.py"
    "${PAINT}" "${PAINT_UNICODE_RESOURCE}" RESULT_VARIABLE RESOURCE_RESULT)
if(NOT RESOURCE_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not generate Unicode resources")
endif()
list(APPEND SOURCES "${PAINT_UNICODE_RESOURCE}")
set_source_files_properties("${PAINT_UNICODE_RESOURCE}" PROPERTIES
    OBJECT_DEPENDS "${PAINT}/img/iconbar-classic.bmp;${PAINT}/img/nicolay-banner.png;${PAINT}/img/swap-art/swap-100.png;${PAINT}/img/swap-art/swap-250.png")
add_executable(mspaint WIN32 ${SOURCES})
set_target_properties(mspaint PROPERTIES OUTPUT_NAME "ClassicPaintPlus")
target_include_directories(mspaint PRIVATE "${PAINT}" "${PAINT}/third_party/libwebp-1.6.0/src")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    target_compile_definitions(mspaint PRIVATE _AMD64_)
else()
    target_compile_definitions(mspaint PRIVATE _X86_)
endif()
target_compile_definitions(mspaint PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS UNREACHABLE=__assume\(0\))
target_compile_options(mspaint PRIVATE /utf-8)
file(READ "${PAINT}/rsrc.rc" RESOURCE_TEXT)
string(REGEX MATCHALL "LANGUAGE_[A-Z]+_[A-Z]+" RESOURCE_LANGUAGES "${RESOURCE_TEXT}")
list(REMOVE_DUPLICATES RESOURCE_LANGUAGES)
target_compile_definitions(mspaint PRIVATE ${RESOURCE_LANGUAGES})
target_link_options(mspaint PRIVATE /entry:wWinMainCRTStartup /MANIFEST:NO)
target_link_libraries(mspaint PRIVATE user32 gdi32 comctl32 comdlg32 advapi32
    uxtheme shell32 kernel32 uuid rpcrt4 ole32 shlwapi oleaut32 msimg32 gdiplus windowscodecs paint_jpeg paint_png paint_squish webp webpdemux)
add_custom_command(TARGET mspaint POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${PAINT}/JPEG-NOTICES.txt" "$<TARGET_FILE_DIR:mspaint>/JPEG-NOTICES.txt")
add_custom_command(TARGET mspaint POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${PAINT}/IMAGE-NOTICES.txt" "$<TARGET_FILE_DIR:mspaint>/IMAGE-NOTICES.txt")
add_custom_target(paint_manual ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${PAINT}/userguide/ClassicPaintPlus-help.html" "${PAINT}/userguide/ClassicPaintPlus-help.en.html"
        "$<TARGET_FILE_DIR:mspaint>"
    DEPENDS mspaint)

option(PAINT_BUILD_PICKER_TESTS "Build the color picker integration executable" OFF)
if(PAINT_BUILD_PICKER_TESTS)
    add_executable(picker_test ${SOURCES} picker_test.cpp)
    get_target_property(PAINT_DEFS mspaint COMPILE_DEFINITIONS)
    get_target_property(PAINT_LIBS mspaint LINK_LIBRARIES)
    target_compile_definitions(picker_test PRIVATE ${PAINT_DEFS})
    target_compile_options(picker_test PRIVATE /utf-8)
    target_compile_definitions(picker_test PRIVATE PAINT_TESTS)
    target_include_directories(picker_test PRIVATE "${PAINT}" "${PAINT}/third_party/libwebp-1.6.0/src")
    target_link_libraries(picker_test PRIVATE ${PAINT_LIBS})
    target_link_options(picker_test PRIVATE /MANIFEST:NO)
endif()
