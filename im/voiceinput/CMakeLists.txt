# Find websocketpp and nlohmann-json
find_package(PkgConfig REQUIRED)
pkg_check_modules(WEBSOCKETPP IMPORTED_TARGET websocketpp)
pkg_check_modules(NLOHMANN_JSON IMPORTED_TARGET nlohmann_json)

set(VOICEINPUT_SOURCES
    voiceinput.cpp
    voiceinput.h
    audiocapture.cpp
    audiocapture.h
    volcenginerecognizer.cpp
    volcenginerecognizer.h
    asr_client.cpp
    asr_client.h
)

add_fcitx5_addon(voiceinput ${VOICEINPUT_SOURCES})
target_link_libraries(voiceinput Fcitx5::Core Fcitx5::Config)
target_link_libraries(voiceinput pulse-simple pulse asound curl)
if(TARGET PkgConfig::WEBSOCKETPP)
    target_link_libraries(voiceinput PkgConfig::WEBSOCKETPP)
    target_include_directories(voiceinput PRIVATE ${WEBSOCKETPP_INCLUDE_DIRS})
endif()
if(TARGET PkgConfig::NLOHMANN_JSON)
    target_link_libraries(voiceinput PkgConfig::NLOHMANN_JSON)
    target_include_directories(voiceinput PRIVATE ${NLOHMANN_JSON_INCLUDE_DIRS})
endif()
# Also link boost_iostreams for gzip support, and OpenSSL for SSL/TLS
target_link_libraries(voiceinput boost_iostreams ssl crypto)
install(TARGETS voiceinput DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5")