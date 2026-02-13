configure_file(testdir.h.in ${CMAKE_CURRENT_BINARY_DIR}/testdir.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(testpunctuation testpunctuation.cpp)
target_link_libraries(testpunctuation Fcitx5::Core Fcitx5::Module::Punctuation)
add_dependencies(testpunctuation punctuation punctuation.conf.in-fmt)
add_test(NAME testpunctuation COMMAND testpunctuation)

if (ENABLE_CLOUDPINYIN)
add_executable(testcloudpinyin testcloudpinyin.cpp)
target_link_libraries(testcloudpinyin Fcitx5::Core Fcitx5::Module::CloudPinyin)
add_dependencies(testcloudpinyin cloudpinyin copy-addon-cloudpinyin)
endif()

add_executable(testpinyinhelper testpinyinhelper.cpp)
target_link_libraries(testpinyinhelper Fcitx5::Core Fcitx5::Module::PinyinHelper)
add_dependencies(testpinyinhelper pinyin pinyin.conf.in-fmt pinyinhelper pinyinhelper.conf.in-fmt)
add_test(NAME testpinyinhelper COMMAND testpinyinhelper)

add_executable(testfullwidth testfullwidth.cpp)
target_link_libraries(testfullwidth Fcitx5::Core Fcitx5::Module::TestFrontend Fcitx5::Module::TestIM)
add_dependencies(testfullwidth fullwidth fullwidth.conf.in-fmt)
add_test(NAME testfullwidth COMMAND testfullwidth)

add_subdirectory(inputmethod)
add_executable(testchttrans testchttrans.cpp)
target_link_libraries(testchttrans Fcitx5::Core Fcitx5::Module::TestFrontend Fcitx5::Module::TestIM)
add_dependencies(testchttrans chttrans chttrans.conf.in-fmt copy-addon copy-testim)
add_test(NAME testchttrans COMMAND testchttrans)

add_subdirectory(addon)
add_executable(testpinyin testpinyin.cpp)
target_link_libraries(testpinyin Fcitx5::Core Fcitx5::Module::TestFrontend)
add_dependencies(testpinyin pinyin pinyinhelper copy-addon copy-im)
add_test(NAME testpinyin COMMAND testpinyin)
add_executable(testtable testtable.cpp)
target_link_libraries(testtable Fcitx5::Core Fcitx5::Module::TestFrontend)
add_dependencies(testtable table copy-addon copy-im)
add_test(NAME testtable COMMAND testtable)

add_executable(testcustomphrase testcustomphrase.cpp ../im/pinyin/customphrase.cpp)
target_compile_definitions(testcustomphrase PRIVATE "-DFCITX_CUSTOM_PHRASE_TEST")
target_link_libraries(testcustomphrase Fcitx5::Utils LibIME::Core)
add_test(NAME testcustomphrase COMMAND testcustomphrase)

add_executable(testsymboldictionary testsymboldictionary.cpp ../im/pinyin/symboldictionary.cpp)
target_link_libraries(testsymboldictionary Fcitx5::Utils LibIME::Core)
add_test(NAME testsymboldictionary COMMAND testsymboldictionary)

# Audio capture test
add_executable(testaudiocapture testaudiocapture.cpp ../im/voiceinput/audiocapture.cpp ../im/voiceinput/audiocapture.h)
target_link_libraries(testaudiocapture Fcitx5::Core pulse-simple pulse asound)
add_test(NAME testaudiocapture COMMAND testaudiocapture)

# Volcengine recognizer test
# Disabled: Requires full AsrClient implementation (WebSocket, network, etc.)
# TODO: Create mock implementation for unit testing
# add_executable(testvolcenginerecognizer testvolcenginerecognizer.cpp ../im/voiceinput/volcenginerecognizer.cpp ../im/voiceinput/volcenginerecognizer.h)
# target_link_libraries(testvolcenginerecognizer Fcitx5::Core curl)
# Also link websocket dependencies for AsrClient support
# if(TARGET PkgConfig::WEBSOCKETPP)
#     target_link_libraries(testvolcenginerecognizer PkgConfig::WEBSOCKETPP)
#     target_include_directories(testvolcenginerecognizer PRIVATE ${WEBSOCKETPP_INCLUDE_DIRS})
# endif()
# if(TARGET PkgConfig::NLOHMANN_JSON)
#     target_link_libraries(testvolcenginerecognizer PkgConfig::NLOHMANN_JSON)
#     target_include_directories(testvolcenginerecognizer PRIVATE ${NLOHMANN_JSON_INCLUDE_DIRS})
# endif()
# Link boost_iostreams for gzip support, and OpenSSL for SSL/TLS
# target_link_libraries(testvolcenginerecognizer boost_iostreams ssl crypto)
# add_test(NAME testvolcenginerecognizer COMMAND testvolcenginerecognizer)

# Voice input integration test
add_executable(testvoiceinputintegration testvoiceinputintegration.cpp)
target_link_libraries(testvoiceinputintegration Fcitx5::Core)
add_test(NAME testvoiceinputintegration COMMAND testvoiceinputintegration)

# Table-Voice integration test
add_executable(testtablevoiceintegration testtablevoiceintegration.cpp)
target_link_libraries(testtablevoiceintegration Fcitx5::Core)
add_test(NAME testtablevoiceintegration COMMAND testtablevoiceintegration)
