/*
The MIT License (MIT)

Copyright (c) 2013-2014 winlin

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <srs_app_tb_http_hooks.hpp>

#include <srs_kernel_error.hpp>
#include <srs_protocol_rtmp.hpp>
#include <srs_app_st_socket.hpp>
#include <srs_app_http.hpp>
#include <srs_app_json.hpp>
#include <srs_app_http_client.hpp>

using namespace std;

#define TB_IM_METHOD_CHECK_USER_INFO "checkUserInfo"
#define TB_IM_METHOD_NOTIFY_STREAM_STATUS "notifyStreamStatus"
#define TB_IM_CMD_CHECK_USER_INFO "107120"
#define TB_IM_CMD_NOTIFY_STREAM_STATUS "107121"

SrsTbHttpHooks::SrsTbHttpHooks()
{
}

SrsTbHttpHooks::~SrsTbHttpHooks()
{
}

template<class T>
void SrsTbHttpHooks::append_param(std::stringstream& s, const char* key, const T& value, bool with_amp) {
    s << key << "=" << value;
    if (with_amp) {
        s << "&";
    }
}

int SrsTbHttpHooks::get_res_data(const string &res, int& error, SrsJsonObject*& data) {
    int ret = ERROR_SUCCESS;

    if (res.empty()) {
        ret = ERROR_HTTP_DATA_INVLIAD;
        srs_error("http hook on_publish validate failed. "
            "res=%s, ret=%d", res.c_str(), ret);
        return ret;
    }

    SrsJsonAny* http_res_tmp = NULL;

    char *res_str = new char[res.length()];
    strncpy(res_str, res.c_str(), res.length());

    if (!(http_res_tmp = SrsJsonAny::loads(res_str)))  {
        ret = ERROR_HTTP_DATA_INVLIAD;
        srs_error("http hook on_publish json parse failed. "
            "res=%s, ret=%d", res.c_str(), ret);
        srs_freep(http_res_tmp);
        srs_freep(res_str);
        return ret;
    }
    srs_freep(res_str);

    if (!http_res_tmp->is_object()) {
        ret = ERROR_HTTP_DATA_INVLIAD;
        srs_error("http hook on_publish json not an object. "
            "res=%s, ret=%d", res.c_str(), ret);
        srs_freep(http_res_tmp);
        return ret;
    }
    SrsJsonObject* http_res = http_res_tmp->to_object();

    SrsJsonAny* _error = http_res->get_property("error");
    if (!_error || !_error->is_integer()) {
        ret = ERROR_HTTP_DATA_INVLIAD;
        srs_error("http hook on_publish no error field. "
            "res=%s, ret=%d", res.c_str(), ret);
        srs_freep(http_res_tmp);
        return ret;
    }
    error = (int)(_error->to_integer());

    SrsJsonAny* _data = http_res->get_property("data");
    if (!_data || !_data->is_object()) { // ok, no data field
        data = NULL;
    } else {
        data = _data->to_object();
    }

    return ret;
}

int SrsTbHttpHooks::on_publish(string url, int client_id, string ip, SrsRequest* req) {
    int ret = ERROR_SUCCESS;

    SrsHttpUri uri;
    if ((ret = uri.initialize(url)) != ERROR_SUCCESS) {
        srs_error("http uri parse on_publish url failed. "
            "client_id=%d, url=%s, ret=%d", client_id, url.c_str(), ret);
        return ret;
    }

    std::stringstream ss;
    append_param(ss, "method", TB_IM_METHOD_CHECK_USER_INFO);
    append_param(ss, "cmd", TB_IM_CMD_CHECK_USER_INFO);
    // append_param(ss, "group_id", req->client_info->group_id);
    append_param(ss, "group_id", 1);
    //append_param(ss, "user_id", req->client_info->user_id);
    append_param(ss, "user_id", 2);
    append_param(ss, "identity", "publisher");
    //append_param("publishToken", req->publish_token);
    append_param(ss, "publishToken", "test");
    //append_param("client_type", req->client_type);
    append_param(ss, "client_type", 3);
    append_param(ss, "net_type", 0, false);
    std::string postdata = ss.str();
    std::string res;

    SrsHttpClient http;
    if ((ret = http.post(&uri, postdata, res)) != ERROR_SUCCESS) {
        srs_error("http post on_publish uri failed. "
            "client_id=%d, url=%s, request=%s, response=%s, ret=%d",
            client_id, url.c_str(), postdata.c_str(), res.c_str(), ret);
        return ret;
    }

    int error = 0;
    SrsJsonObject* data = NULL;
    if (get_res_data(res, error, data) != ERROR_SUCCESS) {
        ret = ERROR_HTTP_DATA_INVLIAD;
        srs_error("http post on_publish parse result failed. "
            "client_id=%d, url=%s, request=%s, response=%s, ret=%d",
            client_id, url.c_str(), postdata.c_str(), res.c_str(), ret);
        srs_freep(data);
        return ret;
    }
    SrsJsonAny* accept = NULL;
    if (error != 0 || !(accept = data->get_property("accept")) || !accept->is_integer()) {
        ret = ERROR_HTTP_DATA_INVLIAD;
        srs_error("http post on_publish parse result failed. "
            "client_id=%d, url=%s, request=%s, response=%s, ret=%d",
            client_id, url.c_str(), postdata.c_str(), res.c_str(), ret);
        srs_freep(data);
        return ret;
    }
    if (accept->to_integer() == 0LL) {
        ret = ERROR_HTTP_ON_PUBLISH_AUTH_FAIL;
        srs_error("http post on_publish authentification check failed. "
            "client_id=%d, url=%s, request=%s, response=%s, ret=%d",
            client_id, url.c_str(), postdata.c_str(), res.c_str(), ret);
        srs_freep(data);
        return ret;
    }

    SrsJsonAny* net_type = NULL;
    if ((net_type = data->get_property("net_type")) && net_type->is_integer()) {
        // TODO: write net_type to req
    }

    srs_freep(data);

    srs_trace("http hook on_publish success. "
        "client_id=%d, url=%s, request=%s, response=%s, ret=%d",
        client_id, url.c_str(), postdata.c_str(), res.c_str(), ret);

    return ret;
}

int SrsTbHttpHooks::on_close(string url, int client_id, string ip, SrsRequest* req) {
    int ret = ERROR_SUCCESS;

    SrsHttpUri uri;
    if ((ret = uri.initialize(url)) != ERROR_SUCCESS) {
        srs_error("http uri parse on_publish url failed. "
            "client_id=%d, url=%s, ret=%d", client_id, url.c_str(), ret);
        return ret;
    }

    std::stringstream ss;
    append_param(ss, "method", TB_IM_METHOD_NOTIFY_STREAM_STATUS);
    append_param(ss, "cmd", TB_IM_CMD_NOTIFY_STREAM_STATUS);
    // append_param(ss, "group_id", req->client_info->group_id);
    append_param(ss, "group_id", 1);
    //append_param(ss, "user_id", req->client_info->user_id);
    append_param(ss, "user_id", 2);
    //append_param(ss, "identity", req->client_info->identity, true);
    append_param(ss, "identity", 1);
    append_param(ss, "status", "close", false);
    std::string postdata = ss.str();
    std::string res;

    SrsHttpClient http;
    if ((ret = http.post(&uri, postdata, res)) != ERROR_SUCCESS) {
        srs_error("http post on_publish uri failed. "
            "client_id=%d, url=%s, request=%s, response=%s, ret=%d",
            client_id, url.c_str(), postdata.c_str(), res.c_str(), ret);
        return ret;
    }

    int error = 0;
    SrsJsonObject* data = NULL;
    if (get_res_data(res, error, data) != ERROR_SUCCESS) {
        ret = ERROR_HTTP_DATA_INVLIAD;
        srs_error("http post on_publish parse result failed. "
            "client_id=%d, url=%s, request=%s, response=%s, ret=%d",
            client_id, url.c_str(), postdata.c_str(), res.c_str(), ret);
        return ret;
    }
    SrsJsonAny* accept = NULL;
    if (error != 0 || !(accept = data->get_property("accept")) || !accept->is_integer()) {
        ret = ERROR_HTTP_DATA_INVLIAD;
        srs_error("http post on_publish parse result failed. "
            "client_id=%d, url=%s, request=%s, response=%s, ret=%d",
            client_id, url.c_str(), postdata.c_str(), res.c_str(), ret);
        return ret;
    }
    if (accept->to_integer() == 0LL) {
        ret = ERROR_HTTP_ON_PUBLISH_AUTH_FAIL;
        srs_error("http post on_publish authentification check failed. "
            "client_id=%d, url=%s, request=%s, response=%s, ret=%d",
            client_id, url.c_str(), postdata.c_str(), res.c_str(), ret);
        return ret;
    }

    SrsJsonAny* net_type = NULL;
    if ((net_type = data->get_property("net_type")) && net_type->is_integer()) {
        // TODO: write net_type to req
    }

    srs_trace("http hook on_publish success. "
        "client_id=%d, url=%s, request=%s, response=%s, ret=%d",
        client_id, url.c_str(), data.c_str(), res.c_str(), ret);

    return ret;
}

