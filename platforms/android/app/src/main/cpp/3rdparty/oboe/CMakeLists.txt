cmake_minimum_required(VERSION 3.22.1)

# Set the name of the project and store it in PROJECT_NAME. Also set the following variables:
# PROJECT_SOURCE_DIR (usually the root directory where Oboe has been cloned e.g.)
# PROJECT_BINARY_DIR (usually the containing project's binary directory,
# e.g. ${OBOE_HOME}/samples/RhythmGame/.externalNativeBuild/cmake/ndkExtractorDebug/x86/oboe-bin)
project(oboe)

set (oboe_sources
    src/aaudio/AAudioLoader.cpp
    src/aaudio/AudioStreamAAudio.cpp
    src/common/AdpfWrapper.cpp
    src/common/AudioStream.cpp
    src/common/AudioStreamBuilder.cpp
    src/common/FixedBlockAdapter.cpp
    src/common/FixedBlockReader.cpp
    src/common/FixedBlockWriter.cpp
    src/common/LatencyTuner.cpp
    src/common/OboeExtensions.cpp
    src/common/Utilities.cpp
    src/common/QuirksManager.cpp
    src/fifo/FifoBuffer.cpp
    src/fifo/FifoController.cpp
    src/fifo/FifoControllerBase.cpp
    src/fifo/FifoControllerIndirect.cpp
    src/opensles/AudioInputStreamOpenSLES.cpp
    src/opensles/AudioOutputStreamOpenSLES.cpp
    src/opensles/AudioStreamBuffered.cpp
    src/opensles/AudioStreamOpenSLES.cpp
    src/opensles/EngineOpenSLES.cpp
    src/opensles/OpenSLESUtilities.cpp
    src/opensles/OutputMixerOpenSLES.cpp
    src/common/StabilizedCallback.cpp
    src/common/Trace.cpp
    src/common/Version.cpp
    )

set (data_conversion_sources
    src/common/AudioSourceCaller.cpp
    src/common/DataConversionFlowGraph.cpp
    src/common/FilterAudioStream.cpp
    src/common/SourceFloatCaller.cpp
    src/common/SourceI16Caller.cpp
    src/common/SourceI24Caller.cpp
    src/common/SourceI32Caller.cpp
    src/flowgraph/FlowGraphNode.cpp
    src/flowgraph/ChannelCountConverter.cpp
    src/flowgraph/ClipToRange.cpp
    src/flowgraph/Limiter.cpp
    src/flowgraph/ManyToMultiConverter.cpp
    src/flowgraph/MonoBlend.cpp
    src/flowgraph/MonoToMultiConverter.cpp
    src/flowgraph/MultiToManyConverter.cpp
    src/flowgraph/MultiToMonoConverter.cpp
    src/flowgraph/RampLinear.cpp
    src/flowgraph/SampleRateConverter.cpp
    src/flowgraph/SinkFloat.cpp
    src/flowgraph/SinkI16.cpp
    src/flowgraph/SinkI24.cpp
    src/flowgraph/SinkI32.cpp
    src/flowgraph/SinkI8_24.cpp
    src/flowgraph/SourceFloat.cpp
    src/flowgraph/SourceI16.cpp
    src/flowgraph/SourceI24.cpp
    src/flowgraph/SourceI32.cpp
    src/flowgraph/SourceI8_24.cpp
    src/flowgraph/resampler/IntegerRatio.cpp
    src/flowgraph/resampler/LinearResampler.cpp
    src/flowgraph/resampler/MultiChannelResampler.cpp
    src/flowgraph/resampler/PolyphaseResampler.cpp
    src/flowgraph/resampler/PolyphaseResamplerMono.cpp
    src/flowgraph/resampler/PolyphaseResamplerStereo.cpp
    src/flowgraph/resampler/SincResampler.cpp
    src/flowgraph/resampler/SincResamplerStereo.cpp
)

# Option to disable data conversion features. Default is OFF (i.e., conversion is ON).
option(OBOE_DISABLE_CONVERSION "Exclude data conversion source files and features" OFF)

if(OBOE_DISABLE_CONVERSION)
    message(STATUS "Oboe: Data conversion is DISABLED.")
else()
    message(STATUS "Oboe: Data conversion is ENABLED.")
    list(APPEND oboe_sources ${data_conversion_sources})
endif()

add_library(oboe ${oboe_sources})

# Specify directories which the compiler should look for headers
target_include_directories(oboe
        PRIVATE src
        PUBLIC include)

# Compile Flags:
#     Enable -Werror when building debug config
#     Enable -Ofast
target_compile_options(oboe
        PRIVATE
        -std=c++17
        -Wall
        -Wextra-semi
        -Wshadow
        -Wshadow-field
        "$<$<CONFIG:RELEASE>:-Ofast>"
        "$<$<CONFIG:DEBUG>:-O3>"
        "$<$<CONFIG:DEBUG>:-Werror>")

target_compile_definitions(oboe PRIVATE $<$<BOOL:${OBOE_DISABLE_CONVERSION}>:DISABLE_CONVERSION=1>)

# Enable logging of D,V for debug builds
target_compile_definitions(oboe PUBLIC $<$<CONFIG:DEBUG>:OBOE_ENABLE_LOGGING=1>)

option(OBOE_DO_NOT_DEFINE_OPENSL_ES_CONSTANTS "Do not define OpenSLES constants" OFF)
target_compile_definitions(oboe PRIVATE $<$<BOOL:${OBOE_DO_NOT_DEFINE_OPENSL_ES_CONSTANTS}>:DO_NOT_DEFINE_OPENSL_ES_CONSTANTS=1>)

target_link_libraries(oboe PRIVATE log OpenSLES)
target_link_options(oboe PRIVATE "-Wl,-z,max-page-size=16384")

# When installing oboe put the libraries in the lib/<ABI> folder e.g. lib/arm64-v8a
install(TARGETS oboe
        LIBRARY DESTINATION lib/${ANDROID_ABI}
        ARCHIVE DESTINATION lib/${ANDROID_ABI})

# Also install the headers
install(DIRECTORY include/oboe DESTINATION include)
