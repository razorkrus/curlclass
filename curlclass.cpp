#include "curlclass.hpp"


// ViolationUploader* ViolationUploader::pinstance_{nullptr};
// mutex ViolationUploader::mutex_instance;
// mutex ViolationUploader::mutex_queue;

ViolationUploader* pinstance_{nullptr};
mutex mutex_instance;
mutex mutex_queue;

ViolationUploader *ViolationUploader::GetInstance(string &ip_param){
    lock_guard<mutex> lock(mutex_instance);
    if (pinstance_ == nullptr)
    {
        pinstance_ = new ViolationUploader(ip_param);
    }
    return pinstance_;
}

vector<string> ViolationUploader::postImages(vector<Mat> &imgs){
    /*
        * Post image files to backend server, and return the response id for each image.
        */
    LOG(INFO) << "Function postImages begin!" << endl;
    CURL *curl;
    CURLcode res;

cout << "imgs size is: " << imgs.size() << endl;

    curl_mime *form = nullptr;
    curl_mimepart *field = nullptr;
cout << "------------------------------------------ 1" << endl;
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    vector<string> r;
cout << "------------------------------------------ 2" << endl;
    if (curl){
        form = curl_mime_init(curl);

        field = curl_mime_addpart(form);
        curl_mime_name(field, "module");
        curl_mime_data(field, "10", CURL_ZERO_TERMINATED);

        field = curl_mime_addpart(form);
        curl_mime_name(field, "isDir");
        curl_mime_data(field, "False", CURL_ZERO_TERMINATED);

        for (auto m : imgs){
            field = curl_mime_addpart(form);
            curl_mime_name(field, "file");
            vector<uchar> vec_img;
            imencode(".jpg", m, vec_img);
            curl_mime_filename(field, "001.jpg"); // need to change later to add a suitable file name
            curl_mime_data(field, reinterpret_cast<const char*>(vec_img.data()), vec_img.size());
        }
cout << "------------------------------------------ 3" << endl;
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
        curl_easy_setopt(curl, CURLOPT_URL, image_upload_url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_func);
        string s;
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);

        res = curl_easy_perform(curl);
        LOG_IF(ERROR, res!=CURLE_OK) << "curl_easy_perform() faild: " << curl_easy_strerror(res) << endl;

        Document doc;
        doc.Parse(s.c_str(), s.size());
        const Value& a = doc["data"];
        for (SizeType i=0; i < a.Size(); i++){
            r.push_back(a[i]["id"].GetString());
        }

        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    LOG(INFO) << "Function postImages end!" << endl;
    return r;
    
}

void ViolationUploader::postJsonData(string &json_data){
    LOG(INFO) << "Function postJsonData begin!" << endl;

    CURL *curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if (curl){
        curl_easy_setopt(curl, CURLOPT_URL, json_upload_url.c_str());
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type:application/json;charset=UTF-8");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
        res = curl_easy_perform(curl);
        LOG_IF(ERROR, res!=CURLE_OK) << "curl_easy_perform() faild: " << curl_easy_strerror(res) << endl;
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
    LOG(INFO) << "Function postJsonData begin!" << endl;

}

void ViolationUploader::postJson(string &json_incomp, vector<string> &ids){
    LOG(INFO) << "Function postJson begin!" << endl;

    Document doc;
    doc.Parse(json_incomp.c_str(), json_incomp.size());
cout << "-----------------------------1" << endl;
    // for (auto id : ids){
    //     Value v;
    //     v.SetString(id.c_str(), id.size(), doc.GetAllocator());
    //     doc.AddMember("imgOne", v, doc.GetAllocator());
    // }
cout << ids.size() << endl;
    Value v;
    v.SetString(ids[0].c_str(), ids[0].size(), doc.GetAllocator());
    doc.AddMember("imgOne", v, doc.GetAllocator());
    cout << "-----------------------------2" << endl;
    // v.SetString(ids[1].c_str(), ids[1].size(), doc.GetAllocator());
    // doc.AddMember("imgTwo", v, doc.GetAllocator());
    // cout << "-----------------------------3" << endl;
    // v.SetString(ids[2].c_str(), ids[2].size(), doc.GetAllocator());
    // doc.AddMember("imgThree", v, doc.GetAllocator());
    StringBuffer s;
    Writer<StringBuffer> writer(s);
    cout << "-----------------------------4" << endl;
    doc.Accept(writer);
    string json_comp = s.GetString();
    cout << "-----------------------------5" << endl;
    postJsonData(json_comp);
    LOG(INFO) << "Function postJson end!" << endl;
}


void ViolationUploader::collectInfo(violationData *data_p){
    lock_guard<mutex> lock(mutex_queue);
    info_q.push(data_p);
}

void ViolationUploader::postInfo()
{
    int i = 0;
    while(true)
    {
        if (info_q.empty()){
            i += 1;
            this_thread::sleep_for(chrono::milliseconds(1000));
            if (i % 10 == 0){
                LOG(INFO) << "HTTP queue idle for 10 seconds" << endl;
                i = 0;
            }
        }
        else{
            violationData *t = nullptr;
            {
                lock_guard<mutex> lock(mutex_queue);
                t = info_q.front();
                info_q.pop();
            }
            if (t != nullptr)
            {
                cout << "t->imgs is: " << t->imgs.size() << endl;
                vector<string> respon_ids = postImages(t->imgs);
                postJson(t->json, respon_ids);
                delete t;
            }
            else{
                cout << "t is nULL !!!!!" << endl;
            }
        }
    }

}


/*
 * Function to be passed to thread.
 * Initialize a ViolationUploader inside, and then start uploading constantly.
 */ 
void ThreadUploader()
{
    string ip_address = "192.168.0.91";
    ViolationUploader* uploader = ViolationUploader::GetInstance(ip_address);
    uploader->postInfo();
}




/* 
 * main function used to test.
 */

// int main(int argc, char* argv[]){
//     thread th(ThreadUploader);
//     th.detach();

//     while (true){

//     }
//     /*
//      * There after should have some running code processing and analyzing video stream.
//      * And use the ->collectInfo interface to put results inside the message queue.
//     */

   

// }



