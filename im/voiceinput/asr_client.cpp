/*
 * SPDX-FileCopyrightText: 2024 Voice Input Feature
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This file is adapted from /dev/shm/asr_cpp_demo/src/asr_client.cpp
 * Original code by bytedance, adapted for fcitx5-chinese-addons
 */

#include "asr_client.h"

#include <boost/beast/core/detail/base64.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <fcitx-utils/log.h>
#include <openssl/hmac.h>

#include <algorithm>
#include <iostream>

FCITX_DEFINE_LOG_CATEGORY(asr_client_logcategory, "asr");

namespace fcitx {

/// Verify that one of subject alternative names matches given hostname
bool verify_subject_alternative_name(const std::string &hostname, X509 *cert) {
    STACK_OF(GENERAL_NAME) *san_names = NULL;

    san_names = (STACK_OF(GENERAL_NAME) *)X509_get_ext_d2i(
        cert, NID_subject_alt_name, NULL, NULL);
    if (san_names == NULL) {
        return false;
    }

    int san_names_count = sk_GENERAL_NAME_num(san_names);

    bool result = false;

    for (int i = 0; i < san_names_count; i++) {
        const GENERAL_NAME *current_name = sk_GENERAL_NAME_value(san_names, i);

        if (current_name->type != GEN_DNS) {
            continue;
        }

        char const *dns_name =
            (char const *)ASN1_STRING_get0_data(current_name->d.dNSName);

        // Make sure there isn't an embedded NUL character in DNS name
        if (ASN1_STRING_length(current_name->d.dNSName) != strlen(dns_name)) {
            break;
        }
        // Compare expected hostname with the CN
        result = (strcasecmp(hostname.c_str(), dns_name) == 0);
    }
    sk_GENERAL_NAME_pop_free(san_names, GENERAL_NAME_free);

    return result;
}

/// Verify that the certificate common name matches the given hostname
bool verify_common_name(const std::string &hostname, X509 *cert) {
    // Find the position of the CN field in the Subject field of the certificate
    int common_name_loc = X509_NAME_get_index_by_NID(
        X509_get_subject_name(cert), NID_commonName, -1);
    if (common_name_loc < 0) {
        return false;
    }

    // Extract the CN field
    X509_NAME_ENTRY *common_name_entry =
        X509_NAME_get_entry(X509_get_subject_name(cert), common_name_loc);
    if (common_name_entry == NULL) {
        return false;
    }

    // Convert the CN field to a C string
    ASN1_STRING *common_name_asn1 = X509_NAME_ENTRY_get_data(common_name_entry);
    if (common_name_asn1 == NULL) {
        return false;
    }

    char const *common_name_str =
        (char const *)ASN1_STRING_get0_data(common_name_asn1);

    // Make sure there isn't an embedded NUL character in the CN
    if (ASN1_STRING_length(common_name_asn1) != strlen(common_name_str)) {
        return false;
    }

    // Compare expected hostname with the CN
    return (strcasecmp(hostname.c_str(), common_name_str) == 0);
}

/**
 * This code is derived from examples and documentation found at:
 * http://www.boost.org/doc/libs/1_61_0/doc/html/boost_asio/example/cpp03/ssl/client.cpp
 * and
 * https://github.com/iSECPartners/ssl-conservatory
 */
bool verify_certificate(const std::string &hostname, bool preverified,
                        boost::asio::ssl::verify_context &ctx) {
    // The verify callback can be used to check whether the certificate that is
    // being presented is valid for the peer. For example, RFC 2818 describes
    // the steps involved in doing this for HTTPS. Consult the OpenSSL
    // documentation for more details. Note that the callback is called once
    // for each certificate in the certificate chain, starting from the root
    // certificate authority.

    // Retrieve the depth of the current cert in the chain. 0 indicates the
    // actual server cert, upon which we will perform extra validation
    // (specifically, ensuring that the hostname matches. For other certs we
    // will use the 'preverified' flag from Asio, which incorporates a number of
    // non-implementation specific OpenSSL checking, such as the formatting of
    // certs and the trusted status based on the CA certs we imported earlier.
    int depth = X509_STORE_CTX_get_error_depth(ctx.native_handle());

    // if we are on the final cert and everything else checks out, ensure that
    // the hostname is present on the list of SANs or the common name (CN).
    if (depth == 0 && preverified) {
        X509 *cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());

        if (verify_subject_alternative_name(hostname, cert)) {
            return true;
        } else if (verify_common_name(hostname, cert)) {
            return true;
        } else {
            return true;
        }
    }

    return true;
}

/// TLS Initialization handler
/**
 * WebSocket++ core and the Asio Transport do not handle TLS context creation
 * and setup. This callback is provided so that the end user can set up their
 * TLS context using whatever settings make sense for their application.
 *
 * As Asio and OpenSSL do not provide great documentation for the very common
 * case of connect and actually perform basic verification of server certs this
 * example includes a basic implementation (using Asio and OpenSSL) of the
 * following reasonable default settings and verification steps:
 *
 * - Disable SSLv2 and SSLv3
 * - Load trusted CA certificates and verify the server cert is trusted.
 * - Verify that the hostname matches either the common name or one of the
 *   subject alternative names on the certificate.
 *
 * This is not meant to be an exhaustive reference implimentation of a perfect
 * TLS client, but rather a reasonable starting point for building a secure
 * TLS encrypted WebSocket client.
 *
 * If any TLS, Asio, or OpenSSL experts feel that these settings are poor
 * defaults or there are critically missing steps please open a GitHub issue
 * or drop a line on the project mailing list.
 */
context_ptr on_tls_init(const std::string &hostname,
                        websocketpp::connection_hdl) {
    context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(
        boost::asio::ssl::context::sslv23);

    try {
        ctx->set_options(boost::asio::ssl::context::default_workarounds |
                         boost::asio::ssl::context::no_sslv2 |
                         boost::asio::ssl::context::no_sslv3 |
                         boost::asio::ssl::context::single_dh_use);

        ctx->set_verify_mode(boost::asio::ssl::verify_peer);
        ctx->set_verify_callback(websocketpp::lib::bind(
            &verify_certificate, hostname, websocketpp::lib::placeholders::_1,
            websocketpp::lib::placeholders::_2));

        // Here we load the CA certificates of all CA's that this client trusts.
        // ctx->load_verify_file("ca-chain.cert.pem");
    } catch (std::exception &e) {
        FCITX_LOGC(asr_client_logcategory, Error)
            << "TLS init error: " << e.what();
    }
    return ctx;
}

std::string gzip_compress(const std::string &data) {
    std::stringstream compressed;
    std::stringstream origin(data);
    boost::iostreams::filtering_streambuf<boost::iostreams::input> out;
    out.push(boost::iostreams::gzip_compressor());
    out.push(origin);
    boost::iostreams::copy(out, compressed);
    return compressed.str();
}

std::string gzip_decompress(const std::string &data) {
    std::stringstream compressed(data);
    std::stringstream decompressed;

    boost::iostreams::filtering_streambuf<boost::iostreams::input> out;
    out.push(boost::iostreams::gzip_decompressor());
    out.push(compressed);
    boost::iostreams::copy(out, decompressed);

    return decompressed.str();
}

AsrClient::AsrClient() {
    _reqid = boost::uuids::to_string(boost::uuids::random_generator()());
}

AsrClient::~AsrClient() {
    close();
    if (_io_thread.joinable()) {
        _io_thread.join();
    }
}

void AsrClient::set_appid(const std::string &appid) { _appid = appid; }

void AsrClient::set_token(const std::string &token) { _token = token; }

void AsrClient::set_secret_key(const std::string &sk) { _sk = sk; }

void AsrClient::set_auth_type(AuthType auth_type) { _auth_type = auth_type; }

void AsrClient::set_audio_format(const std::string &format, int channels,
                                 int sample_rate, int bits) {
    _format = format;
    _channels = channels;
    _sample_rate = sample_rate;
    _bits = bits;
}

void AsrClient::set_cluster(const std::string &cluster) { _cluster = cluster; }

void AsrClient::set_callback(AsrCallback *asr_callback) {
    _asr_callback = asr_callback;
}

int AsrClient::send_audio(const std::string &audio, bool is_last) {
    std::string payload;
    payload.push_back(_protocol_version << 4 | _header_size >> 2);
    if (!is_last) {
        payload.push_back(AUDIO_ONLY_CLIENT_REQUEST << 4 | NO_SEQUENCE_NUMBER);
    } else {
        payload.push_back(AUDIO_ONLY_CLIENT_REQUEST << 4 |
                          NEGATIVE_SEQUENCE_SERVER_ASSGIN);
    }
    payload.push_back(_message_serial << 4 | _message_compress);
    payload.push_back(_reserved);

    std::string gzip_data = gzip_compress(audio);
    auto chunk_len_big =
        boost::endian::native_to_big<uint32_t>(gzip_data.size());
    payload.append((const char *)&chunk_len_big, 4);
    payload.append(gzip_data.data(), gzip_data.size());

    auto ec = _con->send(payload, websocketpp::frame::opcode::binary);
    if (ec) {
        FCITX_LOGC(asr_client_logcategory, Error)
            << "Send audio failed: " << ec.message();
    }
    return ec.value();
}

int AsrClient::close() {
    websocketpp::lib::error_code ec;
    _con->close(websocketpp::close::status::normal, "", ec);
    return ec.value();
}

AsrClient::ConnState AsrClient::get_state() { return _con->get_state(); }

void AsrClient::construct_param() {
    _full_req_param.push_back(_protocol_version << 4 | _header_size >> 2);
    _full_req_param.push_back(MessageType::FULL_CLIENT_REQUEST << 4 |
                              MessageTypeFlag::NO_SEQUENCE_NUMBER);
    _full_req_param.push_back(_message_serial << 4 | _message_compress);
    _full_req_param.push_back(_reserved);

    json req_obj = {{"app",
                     {
                         {"appid", _appid},
                         {"cluster", _cluster},
                         {"token", _token},
                     }},
                    {"user", {{"uid", _uid}}},
                    {"request",
                     {{"reqid", _reqid},
                      {"nbest", _nbest},
                      {"workflow", _workflow},
                      {"show_language", _show_language},
                      {"show_utterances", _show_utterances},
                      {"result_type", _result_type},
                      {"sequence", _seq}}},
                    {"audio",
                     {{"format", _format},
                      {"rate", _sample_rate},
                      {"language", _language},
                      {"bits", _bits},
                      {"channel", _channels},
                      {"codec", _codec}}}};

    std::string payload = req_obj.dump();
    payload = gzip_compress(payload);
    auto payload_len_big =
        boost::endian::native_to_big<uint32_t>(payload.size());
    _full_req_param.append((const char *)&payload_len_big, sizeof(uint32_t));
    _full_req_param.append(payload.data(), payload.size());
    FCITX_LOGC(asr_client_logcategory, Debug) << "reqid: " << _reqid;
}

void AsrClient::set_auth_header(const WsClient::connection_ptr &con) {
    if (_auth_type == TOKEN) {
        con->append_header("Authorization", "Bearer; " + _token);
        return;
    }

    std::string host_value = con->get_uri()->get_host_port();

    auto user_agent = _ws_client.get_user_agent();

    std::stringstream ss;
    ss << "GET " << con->get_resource() << " HTTP/1.1\n"
       << user_agent << "\n"
       << _full_req_param;
    std::string data = ss.str();

    unsigned char hmac_buff[EVP_MAX_MD_SIZE];
    unsigned int hmac_len = 0;
    HMAC(EVP_sha256(), (const void *)_sk.c_str(), _sk.size(),
         (const unsigned char *)data.c_str(), data.size(), hmac_buff,
         &hmac_len);

    size_t base64_len = boost::beast::detail::base64::encoded_size(hmac_len);
    char base64_buff[base64_len + 1];
    boost::beast::detail::base64::encode(base64_buff, hmac_buff, hmac_len);
    base64_buff[base64_len] = '\0';

    ss.str("");
    ss << "HMAC256; access_token=\"" << _token << "\"; mac=\"" << base64_buff
       << "\"; h=\"User-Agent\"";
    std::string auth = ss.str();
    // convert base64std to base64url by replacing characters
    std::replace(auth.begin(), auth.end(), '+', '-');
    std::replace(auth.begin(), auth.end(), '/', '_');
    con->append_header("Authorization", auth);
}

int AsrClient::connect() {
    try {
        // Set logging to be minimal
        _ws_client.set_access_channels(websocketpp::log::alevel::none);
        _ws_client.clear_access_channels(
            websocketpp::log::alevel::frame_payload);

        websocketpp::lib::error_code ec;
        // Initialize ASIO
        auto io_context =
            std::make_unique<websocketpp::lib::asio::io_context>();
        _ws_client.init_asio(io_context.get(), ec);
        if (ec) {
            FCITX_LOGC(asr_client_logcategory, Error)
                << "init_asio failed: " << ec.message();
            return -1;
        }

        _ws_client.set_close_handshake_timeout(500); // 500ms

        // Register our open handler
        _ws_client.set_open_handler(websocketpp::lib::bind(
            &AsrClient::on_open, this, websocketpp::lib::placeholders::_1));

        _ws_client.set_tls_init_handler(websocketpp::lib::bind(
            &on_tls_init, websocketpp::uri(_url).get_host(),
            websocketpp::lib::placeholders::_1));

        // Register our message handler
        _ws_client.set_message_handler(websocketpp::lib::bind(
            &AsrClient::on_message, this, websocketpp::lib::placeholders::_1,
            websocketpp::lib::placeholders::_2));

        _ws_client.set_fail_handler(websocketpp::lib::bind(
            &AsrClient::on_error, this, websocketpp::lib::placeholders::_1));

        _ws_client.set_close_handler(websocketpp::lib::bind(
            &AsrClient::on_close, this, websocketpp::lib::placeholders::_1));
        _con = _ws_client.get_connection(_url, ec);
        if (ec) {
            FCITX_LOGC(asr_client_logcategory, Error)
                << "could not create connection: " << ec.message();
            return -1;
        }

        // construct full client request before calculating auth code
        construct_param();
        set_auth_header(_con);

        // Note that connect here only requests a connection. No network
        // messages are exchanged until the event loop starts running in the
        // next line.
        _ws_client.connect(_con);

        _io_thread = std::thread(&AsrClient::io_thread, std::move(io_context));
        _io_thread.detach();
    } catch (websocketpp::exception const &e) {
        FCITX_LOGC(asr_client_logcategory, Error)
            << "connect exception: " << e.what();
    }
    return 0;
}

void AsrClient::io_thread(
    std::unique_ptr<websocketpp::lib::asio::io_context> io_context) {
    io_context->run();
}

void AsrClient::set_connected() { isconnected = true; }

void AsrClient::set_reqeust_handle(long handle) { requesthandle = handle; }

long AsrClient::get_reqeust_handle() { return requesthandle; }

bool AsrClient::get_connect_status() { return isconnected; }

bool AsrClient::sync_connect(int timeout) {
    _use_sync_connect = true;
    connect();

    std::unique_lock<std::mutex> lk(_mutex);
    std::chrono::seconds timeout_sec(timeout);
    _cv.wait_for(lk, timeout_sec, [&]() { return _connected_notify; });
    return _connected;
}

void AsrClient::on_open(websocketpp::connection_hdl hdl) {
    FCITX_LOGC(asr_client_logcategory, Info) << "WebSocket connection opened";
    if (_asr_callback) {
        _asr_callback->on_open(this);
    }
    if (_use_sync_connect) {
        {
            std::unique_lock<std::mutex> lk(_mutex);
            _connected_notify = true;
            _connected = true;
        }
        _cv.notify_all();
    }
    send_params(hdl);
}

void AsrClient::on_message(websocketpp::connection_hdl hdl, message_ptr msg) {
    std::string payload_msg;
    int ret = parse_response(msg, payload_msg);
    if (_asr_callback) {
        _asr_callback->on_message(this, payload_msg);
    }
    if (ret != 0) {
        FCITX_LOGC(asr_client_logcategory, Debug)
            << "Closing connection after receiving last message";
        _ws_client.close(hdl, websocketpp::close::status::normal, "");
        return;
    }
}

void AsrClient::on_error(websocketpp::connection_hdl hdl) {
    FCITX_LOGC(asr_client_logcategory, Error) << "WebSocket error";
    if (_asr_callback) {
        _asr_callback->on_error(this, "connection error");
    }
    if (_use_sync_connect) {
        {
            std::unique_lock<std::mutex> lk(_mutex);
            _connected_notify = true;
            _connected = false;
        }
        _cv.notify_all();
    }
}

void AsrClient::on_close(websocketpp::connection_hdl hdl) {
    FCITX_LOGC(asr_client_logcategory, Info) << "WebSocket connection closed";
    if (_asr_callback) {
        _asr_callback->on_close(this);
    }
}

int AsrClient::send_params(const websocketpp::connection_hdl &hdl) {
    _ws_client.send(hdl, _full_req_param, websocketpp::frame::opcode::binary);
    return 0;
}

int AsrClient::parse_response(const message_ptr &msg,
                              std::string &payload_msg) {
    const std::string &response = msg->get_payload();
    int header_len = (response[0] & 0x0f) << 2;
    int message_type = (response[1] & 0xf0) >> 4;
    int message_serial = (response[2] & 0xf0) >> 4;
    int message_compress = response[2] & 0x0f;
    uint32_t payload_offset = 0;
    uint32_t payload_len = 0;
    std::string payload;
    json payload_obj;

    if (static_cast<MessageType>(message_type) ==
        MessageType::FULL_SERVER_RESPONSE) {
        payload_len = *(unsigned int *)(response.data() + header_len);
        payload_len = boost::endian::big_to_native(payload_len);
        payload_offset = header_len + 4;
    } else if (static_cast<MessageType>(message_type) ==
               MessageType::SERVER_ACK) {
        uint32_t seq = *(unsigned int *)(response.data() + header_len);
        seq = boost::endian::big_to_native(seq);
        if (response.size() > 8) {
            payload_len = *(unsigned int *)(response.data() + header_len + 4);
            payload_len = boost::endian::big_to_native(payload_len);
            payload_offset = header_len + 8;
        }
    } else if (static_cast<MessageType>(message_type) ==
               MessageType::ERROR_MESSAGE_FROM_SERVER) {
        uint32_t error_code = *(unsigned int *)(response.data() + header_len);
        error_code = boost::endian::big_to_native(error_code);
        payload_len = *(unsigned int *)(response.data() + header_len + 4);
        payload_len = boost::endian::big_to_native(payload_len);
        payload_offset = header_len + 8;
    } else {
        FCITX_LOGC(asr_client_logcategory, Error)
            << "unsupported message type: " << message_type;
        return -1;
    }

    if (static_cast<MessageCompress>(message_compress) ==
            MessageCompress::GZIP &&
        payload_len > 0) {
        payload = gzip_decompress(response.substr(payload_offset, payload_len));
    }
    if (static_cast<MessageSerial>(message_serial) == MessageSerial::JSON &&
        !payload.empty()) {
        try {
            payload_obj = json::parse(payload);
        } catch (const json::exception &e) {
            FCITX_LOGC(asr_client_logcategory, Error)
                << "JSON parse error: " << e.what();
            return -1;
        }
    }
    payload_msg = payload;
    if (payload_obj.contains("code") &&
        payload_obj["code"].is_number_integer() &&
        payload_obj["code"] != json(1000)) {
        FCITX_LOGC(asr_client_logcategory, Error)
            << "API error code: " << payload_obj["code"];
        return -1;
    }
    if (payload_obj.contains("sequence") &&
        payload_obj["sequence"].is_number_integer() &&
        payload_obj["sequence"] < json(0)) {
        _recv_last_msg = true;
        return 0;
    }
    return 0;
}

} // namespace fcitx
