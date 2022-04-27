#ifndef CURLCLASS_HPP
#define CURLCLASS_HPP

#include <stdio.h>
#include <string>
#include <iostream>
#include <vector>
#include "curl/curl.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/reader.h"
#include <glog/logging.h>
#include <opencv2/imgcodecs.hpp>
#include <mutex>
#include <thread>
#include <queue>
#include <chrono>
#include <map>

using namespace rapidjson;
using namespace std;
using namespace cv;

/*
 * Struct to save every violation event data.
 */
struct violationData
{
    vector<Mat> imgs;
    string json;
};


inline size_t write_func(void *ptr, size_t size, size_t nmemb, string *s)
{
    s->append(static_cast<char *>(ptr), size*nmemb);
    return size*nmemb;
}


/*****
 * Singleton class implementation of http post, using thread to post communication data.
 *****/
class ViolationUploader
{
    ViolationUploader(string ip_param, string port_param)
    {
        
        ip_address = ip_param;
        port_num = port_param;
        image_upload_url = "http://" + ip_address + ":" + port_num +"/api/file/upload";
        json_upload_url = "http://" + ip_address + ":" + port_num +"/api/record/insert";
        heartbeat_url = "http://" + ip_address + ":" + port_num + "/data/road/add";
        curls_init();
    }
    ~ViolationUploader() {}

private:
    // static ViolationUploader* pinstance_;
    // static mutex mutex_instance;
    // static mutex mutex_queue;
    queue<violationData *> info_q;
    string ip_address;
    string port_num;
    string image_upload_url;
    string json_upload_url;
    string heartbeat_url;
    CURL *image_curl;
    CURL *json_curl;
    CURL *heartbeat_curl;

    void curls_init();

public:
    // ViolationUploader(ViolationUploader &other) = delete;
    

    // void operator=(const ViolationUploader &) = delete;
    static ViolationUploader *GetInstance(const string &ip_param, const string &port_param);

    vector<string> postImages(const vector<Mat> &imgs);
    void postHeartbeat(const string &road_data);
    void postJsonData(const string &json_data);
    void postJson(string &json_incomp, const vector<string> &ids);

    void collectInfo(violationData *data_p);
    void postInfo();

};

void ThreadUploader(const string &, const string &);
// void GetNodeConfig(const string &ip_param, const string &port_param, const string &node_id);
bool GetNodeConfig(const string &ip_param, const string &port_param, const string &node_id, string &s);
bool ParseNodeConfig(string &s, vector<Point> &bike_area, vector<Point> &car_area, vector<Point> &warning_area1, vector<Point> &warning_area2);


#endif
