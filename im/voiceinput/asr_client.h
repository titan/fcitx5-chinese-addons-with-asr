/*
 * SPDX-FileCopyrightText: 2024 Voice Input Feature
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This file is adapted from /dev/shm/asr_cpp_demo/include/asr_client.h
 * Original code by bytedance, adapted for fcitx5-chinese-addons
 */

#ifndef _VOICEINPUT_ASR_CLIENT_H_
#define _VOICEINPUT_ASR_CLIENT_H_

#include <condition_variable>
#include <mutex>
#include <nlohmann/json.hpp>
#include <thread>
#include <websocketpp/client.hpp>
#include <websocketpp/common/asio.hpp>
#include <websocketpp/config/asio_client.hpp>

namespace fcitx {

class AsrClient;

class AsrCallback {
public:
    // This message handler will be invoked once for each websocket connection
    // open.
    virtual void on_open(AsrClient *asr_client) = 0;

    // This message handler will be invoked once for each incoming message.
    virtual void on_message(AsrClient *asr_client, std::string msg) = 0;

    virtual void on_error(AsrClient *asr_client, std::string msg) = 0;

    virtual void on_close(AsrClient *asr_client) = 0;
};

typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context>
    context_ptr;

context_ptr on_tls_init(const std::string &hostname,
                        websocketpp::connection_hdl);

class AsrClient {
public:
    typedef websocketpp::client<websocketpp::config::asio_tls_client> WsClient;
    // pull out type of messages sent by our config
    typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

    using json = nlohmann::json;

    enum AudioType : uint8_t { LOCAL = 1, URL = 2 };

    enum AuthType : uint8_t { TOKEN = 1, SIGNATURE = 2 };

    // connecting = 0, open = 1, closing = 2, closed = 3
    using ConnState = websocketpp::session::state::value;

    AsrClient();

    AsrClient(const AsrClient &) = delete;

    AsrClient &operator=(const AsrClient &) = delete;

    ~AsrClient();

    void set_appid(const std::string &appid);

    void set_token(const std::string &token);

    void set_auth_type(AuthType auth_type);

    // set secret key when using signature auth
    void set_secret_key(const std::string &sk);

    void set_audio_format(const std::string &format, int channels,
                          int sample_rate, int bits);

    void set_cluster(const std::string &cluster);

    void set_callback(AsrCallback *asr_callback);

    /*
     * async connect
     */
    int connect();

    // sync connect
    bool sync_connect(int timeout = 5);

    /*
     * params:
     *  audio: audio segment
     *  is_last: is last audio segment
     */
    int send_audio(const std::string &audio, bool is_last);

    int close();

    ConnState get_state();

    void set_reqeust_handle(long handle);

    long get_reqeust_handle();

    void set_connected();

    bool get_connect_status();

private:
    static void
    io_thread(std::unique_ptr<websocketpp::lib::asio::io_context> io_context);

    // This message handler will be invoked once for each websocket connection
    // open.
    void on_open(websocketpp::connection_hdl hdl);

    // This message handler will be invoked once for each incoming message.
    void on_message(websocketpp::connection_hdl, message_ptr msg);

    void on_error(websocketpp::connection_hdl hdl);

    void on_close(websocketpp::connection_hdl hdl);

    void construct_param();

    void set_auth_header(const WsClient::connection_ptr &con);

    int send_params(const websocketpp::connection_hdl &hdl);

    int parse_response(const message_ptr &msg, std::string &payload_msg);

private:
    enum MessageType : uint8_t {
        FULL_CLIENT_REQUEST = 0b0001,
        AUDIO_ONLY_CLIENT_REQUEST = 0b0010,
        FULL_SERVER_RESPONSE = 0b1001,
        SERVER_ACK = 0b1011,
        ERROR_MESSAGE_FROM_SERVER = 0b1111
    };

    enum MessageTypeFlag : uint8_t {
        NO_SEQUENCE_NUMBER = 0b0000,
        POSITIVE_SEQUENCE_CLIENT_ASSGIN = 0b0001,
        NEGATIVE_SEQUENCE_SERVER_ASSGIN = 0b0010,
        NEGATIVE_SEQUENCE_CLIENT_ASSIGN = 0b0011
    };

    enum MessageSerial : uint8_t {
        NO_SERIAL = 0b0000,
        JSON = 0b0001,
        CUSTOM_SERIAL = 0b1111
    };

    enum MessageCompress : uint8_t {
        NO_COMPRESS = 0b0000,
        GZIP = 0b0001,
        CUSTOM_COMPRESS = 0b1111
    };

    std::string _url{"wss://openspeech.bytedance.com/api/v2/asr"};
    std::string _full_req_param;

    std::string _reqid;
    int32_t _seq{1};

    std::string _appid;
    std::string _token;
    std::string _sk;
    AuthType _auth_type{TOKEN};

    std::string _cluster{""};

    std::string _uid{"fcitx5-asr"};
    std::string _workflow{"audio_in,resample,partition,vad,fe,decode"};
    int _nbest{1};
    bool _show_language{false};
    bool _show_utterances{false};
    std::string _result_type{"full"};
    std::string _language{"zh-CN"};

    AudioType _audio_type{LOCAL};
    std::string _format{"wav"};
    int _sample_rate{16000};
    int _bits{16};
    int _channels{1};
    std::string _codec{"raw"};
    bool _recv_last_msg{false};

    uint8_t _protocol_version{0b0001};
    uint8_t _header_size{4};
    MessageType _message_type{MessageType::FULL_CLIENT_REQUEST};
    MessageTypeFlag _message_type_flag{MessageTypeFlag::NO_SEQUENCE_NUMBER};
    MessageSerial _message_serial{MessageSerial::JSON};
    MessageCompress _message_compress{MessageCompress::GZIP};
    uint8_t _reserved{0};

    std::mutex _mutex;
    std::condition_variable _cv;
    bool _connected_notify{false};
    bool _connected{false};
    bool _use_sync_connect{false};

    std::thread _io_thread;

    WsClient _ws_client;
    WsClient::connection_ptr _con;

    AsrCallback *_asr_callback{nullptr};
    long requesthandle{0};
    bool isconnected{false};
};

} // namespace fcitx

#endif // _VOICEINPUT_ASR_CLIENT_H_
