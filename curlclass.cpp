#include "curlclass.hpp"


// ViolationUploader* ViolationUploader::pinstance_{nullptr};
// mutex ViolationUploader::mutex_instance;
// mutex ViolationUploader::mutex_queue;

ViolationUploader* pinstance_{nullptr};
mutex mutex_instance;
mutex mutex_queue;

ViolationUploader *ViolationUploader::GetInstance(const string &ip_param, const string &port_param)
{
    lock_guard<mutex> lock(mutex_instance);
    if (pinstance_ == nullptr)
    {
        pinstance_ = new ViolationUploader(ip_param, port_param);
    }
    return pinstance_;
}

vector<string> ViolationUploader::postImages(const vector<Mat> &imgs)
{
    /*
     * Post image files to backend server, and return the response id for each image.
     */

    LOG(INFO) << "Function postImages begin!" << endl;

    LOG_IF(ERROR, imgs.size()!=3) << "imgs.size() is not 3! Its value is: " << imgs.size() << endl;

    CURL *curl;
    CURLcode res;

    curl_mime *form = nullptr;
    curl_mimepart *field = nullptr;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    vector<string> r;

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
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);

        curl_easy_setopt(curl, CURLOPT_URL, image_upload_url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_func);
        string s;
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);

        res = curl_easy_perform(curl);

        if (res!=CURLE_OK){
            LOG(ERROR) << "Inside function" << __func__ << ", curl_easy_perform() faild: " << curl_easy_strerror(res) << endl;
        }
        else{
            Document doc;
            doc.Parse(s.c_str(), s.size());
            const Value& a = doc["data"];
            for (SizeType i=0; i < a.Size(); i++){
                r.push_back(a[i]["id"].GetString());
            }
        }

        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();

    LOG(INFO) << "Function postImages end!" << endl;

    return r;
}

void ViolationUploader::postJsonData(const string &json_data)
{

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

        LOG_IF(ERROR, res!=CURLE_OK) << "Inside function" << __func__ << ", curl_easy_perform() faild: " << curl_easy_strerror(res) << endl;

        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();

    LOG(INFO) << "Function postJsonData end!" << endl;
}

void ViolationUploader::postJson(string &json_incomp, const vector<string> &ids)
{

    LOG(INFO) << "Function postJson begin!" << endl;

    LOG_IF(ERROR, ids.size()!=3) << "ids.size() is not 3! Its value is: " << ids.size() << endl;

    Document doc;
    doc.Parse(json_incomp.c_str(), json_incomp.size());

    if (!ids.empty())
    {
        map<int, string> numWords;
        numWords[1] = "One";
        numWords[2] = "Two";
        numWords[3] = "Three";

        Value k, v;
        int i = 1;
        for (auto id: ids){
            string t = "img" + numWords[i];
            k.SetString(t.c_str(), t.size(), doc.GetAllocator());
            v.SetString(id.c_str(), id.size(), doc.GetAllocator());
            doc.AddMember(k, v, doc.GetAllocator());
            i++;
        }
    }
    
    StringBuffer s;
    Writer<StringBuffer> writer(s);

    doc.Accept(writer);
    string json_comp = s.GetString();

    postJsonData(json_comp);

    LOG(INFO) << "Function postJson end!" << endl;
}


void ViolationUploader::collectInfo(violationData *data_p)
{
    lock_guard<mutex> lock(mutex_queue);
    info_q.push(data_p);

    LOG(INFO) << "violationData pointer pushed to queue!" << endl;
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
                LOG(INFO) << "t->imgs.size() is: " << t->imgs.size();

                vector<string> respon_ids = postImages(t->imgs);
                postJson(t->json, respon_ids);
                delete t;
            }
            else{
                LOG(ERROR) << "Queue top is nullptr!" << endl;
            }
        }
    }

}


/*
 * Function to be passed to thread.
 * Initialize a ViolationUploader inside, and then start uploading constantly.
 */ 
void ThreadUploader(const string &ip_param, const string &port_param)
{
    ViolationUploader* uploader = ViolationUploader::GetInstance(ip_param, port_param);
    uploader->postInfo();
}

void GetNodeConfig(const string &ip_param, const string &port_param, const string &node_id)
{
    CURL *curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if (curl){
        string get_url = "http://" + ip_param + ":" + port_param +"/data/road/info/" + node_id;

        curl_easy_setopt(curl, CURLOPT_URL, get_url.c_str());
        
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_func);
        string s;
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            LOG(ERROR) << "curl_easy_perform() failed: " << curl_easy_strerror(res) << endl;
        }
        else
        {
            // LOG(INFO) << "s is: " << s << endl;
            Document doc;
            doc.Parse(s.c_str(), s.size());
            // LOG(INFO) << "config is: " << doc["data"]["config"].GetString() << endl;
            // LOG(INFO) << " data is string: " << doc["data"].IsString() << endl;
            // LOG(INFO) << " config is string: " << doc["data"]["config"].IsString() << endl;
            if (doc["data"].HasMember("config"))
            {
                string t = doc["data"]["config"].GetString();
                Document d;
                d.Parse(t.c_str(), t.size());

                LOG(INFO) << "d is object: " << d.IsObject() << endl;
                LOG(INFO) << "d has BIKE_AREA: " << d.HasMember("BIKE_AREA") << endl;
                LOG(INFO) << "bike_area is array: " << d["BIKE_AREA"].IsArray() << endl;

                // if (d.HasMember("BIKE_AREA"))
                // {
                //     const Value &a = d["BIKE_AREA"];
                //     for (SizeType i = 0; i < a.Size(); i++)
                //         LOG(INFO) << a[i].GetString() << endl;
                // }
                vector<string> keys = {"BIKE_AREA","CAR_AREA", "WARNING_AREA_1", "WARNING_AREA_2"};

                for (auto k:keys)
                {
                    if (d.HasMember(k.c_str()))
                            {
                                LOG(INFO) << "====== Printing " << k << " ======" << endl;
                                const Value &a = d[k.c_str()];
                                for (SizeType i = 0; i < a.Size(); i++){
                                    // LOG(INFO) << a[i].GetString() << endl;
                                    string tt = a[i].GetString();
                                    tt = tt.substr(1, tt.size()-2);
                                    auto index = tt.find(",");
                                    int x = stoi(tt.substr(0, index));
                                    int y = stoi(tt.substr(index+2, tt.size()-1));
                                    LOG(INFO) << x << " " << y << endl;
                                    // LOG(INFO) << stoi(t.substr(0, index)) << " " << stoi(tt.substr(index+2, tt.size()-1)) << endl;
                                    // LOG(INFO) << tt.substr(1, tt.size()-2) << endl;
                                }
                                    
                            }
                }
            }
            else
            {
                LOG(ERROR) << "No config data found! Please check config existence from front-end." << endl;
            }
        }
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
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



