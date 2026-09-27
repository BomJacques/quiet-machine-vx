cmake_minimum_required(VERSION 3.22)

project(QuietMachineVX VERSION 0.6.2 LANGUAGES C CXX)

option(QM_ENABLE_WEBVIEW2 "Build the production WebView2 editor" ON)
option(QM_BUILD_SMOKE_TESTS "Build the native processor smoke test" OFF)

if(NOT DEFINED JUCE_DIR)
    message(FATAL_ERROR "Set JUCE_DIR to a JUCE 8 checkout, for example -DJUCE_DIR=C:/SDK/JUCE")
endif()

add_subdirectory(${JUCE_DIR} JUCE)

set(QM_VENDOR_DIR "${CMAKE_CURRENT_BINARY_DIR}/vendor")
file(MAKE_DIRECTORY "${QM_VENDOR_DIR}")

function(qm_download_vendor filename url)
    set(destination "${QM_VENDOR_DIR}/${filename}")
    if(NOT EXISTS "${destination}")
        message(STATUS "Downloading ${filename} for the embedded editor")
        file(DOWNLOAD "${url}" "${destination}" STATUS download_status TLS_VERIFY ON)
        list(GET download_status 0 download_code)
        if(NOT download_code EQUAL 0)
            list(GET download_status 1 download_message)
            message(FATAL_ERROR "Could not download ${filename}: ${download_message}")
        endif()
    endif()
endfunction()

qm_download_vendor("tailwind-3.4.17.js" "https://cdn.tailwindcss.com/3.4.17")
qm_download_vendor("react-18.2.0.js" "https://cdnjs.cloudflare.com/ajax/libs/react/18.2.0/umd/react.production.min.js")
qm_download_vendor("react-dom-18.2.0.js" "https://cdnjs.cloudflare.com/ajax/libs/react-dom/18.2.0/umd/react-dom.production.min.js")

juce_add_plugin(QuietMachineVX
    COMPANY_NAME "Quiet Machine Audio"
    BUNDLE_ID "audio.quietmachine.vx"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    NEEDS_WEBVIEW2 ${QM_ENABLE_WEBVIEW2}
    PLUGIN_MANUFACTURER_CODE Qmch
    PLUGIN_CODE Qmvx
    FORMATS VST3 Standalone
    PRODUCT_NAME "Quiet Machine VX")

juce_add_binary_data(QuietMachineWeb
    NAMESPACE QuietMachineWeb
    HEADER_NAME QuietMachineWebData.h
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../index.html
        ${QM_VENDOR_DIR}/tailwind-3.4.17.js
        ${QM_VENDOR_DIR}/react-18.2.0.js
        ${QM_VENDOR_DIR}/react-dom-18.2.0.js
        ${JUCE_DIR}/modules/juce_gui_extra/native/javascript/index.js
        ${JUCE_DIR}/modules/juce_gui_extra/native/javascript/check_native_interop.js)

target_sources(QuietMachineVX PRIVATE
    Source/NativeAmbientEngine.cpp
    Source/NativeAmbientEngine.h
    Source/NativeMasterFx.cpp
    Source/NativeMasterFx.h
    Source/NativeTextureEngine.cpp
    Source/NativeTextureEngine.h
    Source/SourceOutput.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h)

target_compile_features(QuietMachineVX PRIVATE cxx_std_20)

target_compile_definitions(QuietMachineVX PRIVATE
    QM_ENABLE_WEBVIEW2=$<BOOL:${QM_ENABLE_WEBVIEW2}>
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=1
    $<$<BOOL:${QM_ENABLE_WEBVIEW2}>:JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1>
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(QuietMachineVX PRIVATE
    QuietMachineWeb
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

juce_generate_juce_header(QuietMachineVX)

if(QM_BUILD_SMOKE_TESTS)
    juce_add_console_app(QuietMachineSmoke PRODUCT_NAME "Quiet Machine Smoke Test")
    target_sources(QuietMachineSmoke PRIVATE Tests/ProcessorSmokeTest.cpp)
    target_compile_features(QuietMachineSmoke PRIVATE cxx_std_20)
    target_include_directories(QuietMachineSmoke PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/QuietMachineVX_artefacts/JuceLibraryCode)
    target_compile_definitions(QuietMachineSmoke PRIVATE
        JucePlugin_Name="Quiet Machine VX"
        QM_ENABLE_WEBVIEW2=$<BOOL:${QM_ENABLE_WEBVIEW2}>)
    target_link_libraries(QuietMachineSmoke PRIVATE
        QuietMachineVX
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra)

    if(QM_ENABLE_WEBVIEW2)
        target_link_libraries(QuietMachineSmoke PRIVATE juce::juce_webview2)
    endif()
endif()
