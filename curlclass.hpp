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
struct violationData{
    vector<Mat> imgs;
    string json;
};


inline size_t write_func(void *ptr, size_t size, size_t nmemb, string *s){
    s->append(static_cast<char *>(ptr), size*nmemb);
    return size*nmemb;
}


/*****
 * Singleton class implementation of http post, using thread to post communication data.
 *****/
class ViolationUploader
{
    ViolationUploader(string ip_param){
        
        ip_address = ip_param;
        image_upload_url = "http://" + ip_address + ":8093/api/file/upload";
        json_upload_url = "http://" + ip_address + ":8093/api/record/insert";
    }
    ~ViolationUploader() {}

private:
    // static ViolationUploader* pinstance_;
    // static mutex mutex_instance;
    // static mutex mutex_queue;
    queue<violationData *> info_q;
    string ip_address;
    string image_upload_url;
    string json_upload_url;


public:
    // ViolationUploader(ViolationUploader &other) = delete;
    

    // void operator=(const ViolationUploader &) = delete;

    vector<string> postImages(const vector<Mat> &imgs);
    void postJsonData(const string &json_data);
    void postJson(string &json_incomp, const vector<string> &ids);


    static ViolationUploader *GetInstance(const string &ip_param);

    void collectInfo(violationData *data_p);
    void postInfo();

};

void ThreadUploader();



#endif
