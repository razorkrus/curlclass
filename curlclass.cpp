#include "curlclass.hpp"

// ViolationUploader* ViolationUploader::pinstance_{nullptr};
// mutex ViolationUploader::mutex_instance;
// mutex ViolationUploader::mutex_queue;

ViolationUploader* pinstance_{nullptr};
mutex mutex_instance;
mutex mutex_queue;


void ViolationUploader::curls_init()
{
    curl_global_init(CURL_GLOBAL_ALL);

    image_curl = curl_easy_init();
    if (image_curl)
    {
        curl_easy_setopt(image_curl, CURLOPT_URL, image_upload_url.c_str());
        curl_easy_setopt(image_curl, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(image_curl, CURLOPT_TCP_KEEPIDLE, 120L);
        curl_easy_setopt(image_curl, CURLOPT_TCP_KEEPINTVL, 60L);

        curl_easy_setopt(image_curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(image_curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(image_curl, CURLOPT_CONNECTTIMEOUT, 10L);

    }

    json_curl = curl_easy_init();
    if (json_curl)
    {
        curl_easy_setopt(json_curl, CURLOPT_URL, json_upload_url.c_str());
        curl_easy_setopt(json_curl, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(json_curl, CURLOPT_TCP_KEEPIDLE, 120L);
        curl_easy_setopt(json_curl, CURLOPT_TCP_KEEPINTVL, 60L);

        curl_easy_setopt(json_curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(json_curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(json_curl, CURLOPT_CONNECTTIMEOUT, 10L);
        
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type:application/json;charset=UTF-8");
        curl_easy_setopt(json_curl, CURLOPT_HTTPHEADER, headers);
    }

    heartbeat_curl = curl_easy_init();
    if (heartbeat_curl)
    {
        curl_easy_setopt(heartbeat_curl, CURLOPT_URL, heartbeat_url.c_str());
        curl_easy_setopt(heartbeat_curl, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(heartbeat_curl, CURLOPT_TCP_KEEPIDLE, 120L);
        curl_easy_setopt(heartbeat_curl, CURLOPT_TCP_KEEPINTVL, 60L);

        curl_easy_setopt(heartbeat_curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(heartbeat_curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(heartbeat_curl, CURLOPT_CONNECTTIMEOUT, 10L);
        
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type:application/json;charset=UTF-8");
        curl_easy_setopt(heartbeat_curl, CURLOPT_HTTPHEADER, headers);
    }

}


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

    curl_mime *form = nullptr;
    curl_mimepart *field = nullptr;

    CURLcode res;
    vector<string> r;

    if (image_curl){
        form = curl_mime_init(image_curl);

        field = curl_mime_addpart(form);
        curl_mime_name(field, "module");
        curl_mime_data(field, "10", CURL_ZERO_TERMINATED);

        field = curl_mime_addpart(form);
        curl_mime_name(field, "isDir");
        curl_mime_data(field, "False", CURL_ZERO_TERMINATED);

        int i = 1;
        for (auto m : imgs){
            field = curl_mime_addpart(form);
            curl_mime_name(field, "file");
            vector<uchar> vec_img;
            imencode(".jpg", m, vec_img);
            string file_name = to_string(i++) + ".jpg";
            curl_mime_filename(field, file_name.c_str());
            curl_mime_data(field, reinterpret_cast<const char*>(vec_img.data()), vec_img.size());
        }
        curl_easy_setopt(image_curl, CURLOPT_MIMEPOST, form);

        curl_easy_setopt(image_curl, CURLOPT_WRITEFUNCTION, write_func);
        string s;
        curl_easy_setopt(image_curl, CURLOPT_WRITEDATA, &s);

        lOG(INFO) << "Posting images to backend!" << endl;
        res = curl_easy_perform(image_curl);
        
        // free curl_mime object
        curl_mime_free(form);

        if (res!=CURLE_OK){
            LOG(ERROR)  << "Inside function" << __func__ 
                        << ", curl_easy_perform() faild: " << curl_easy_strerror(res) 
                        << endl;
        }
        else{
            Document doc;
            doc.Parse(s.c_str(), s.size());
            const Value& a = doc["data"];
            for (SizeType i=0; i < a.Size(); i++){
                r.push_back(a[i]["id"].GetString());
            }
        }
    }

    LOG(INFO) << "Function postImages end!" << endl;

    return r;
}

void ViolationUploader::postHeartbeat(const string &road_data)
{
    CURLcode res;

    if (heartbeat_curl)
    {
        curl_easy_setopt(heartbeat_curl, CURLOPT_POSTFIELDS, road_data.c_str());
        
        DLOG(INFO) << "Posting heartbeat to backend!" << endl;
        res = curl_easy_perform(heartbeat_curl);

        LOG_IF(ERROR, res!=CURLE_OK)    << "Inside function" << __func__ 
                                        << ", curl_easy_perform() faild: " << curl_easy_strerror(res) 
                                        << endl;
    }
}

void ViolationUploader::postJsonData(const string &json_data)
{
    LOG(INFO) << "Function postJsonData begin!" << endl;

    CURLcode res;

    if (json_curl){
        curl_easy_setopt(json_curl, CURLOPT_POSTFIELDS, json_data.c_str());

        LOG(INFO) << "Posting json data to backend!" << endl;
        res = curl_easy_perform(json_curl);

        LOG_IF(ERROR, res!=CURLE_OK)    << "Inside function" << __func__ 
                                        << ", curl_easy_perform() faild: " << curl_easy_strerror(res) 
                                        << endl;
    }

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


bool GetNodeConfig(const string &ip_param, const string &port_param, const string &node_id, string &s)
{
    CURL *curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if (curl){
        string get_url = "http://" + ip_param + ":" + port_param +"/data/road/info/" + node_id;

        curl_easy_setopt(curl, CURLOPT_URL, get_url.c_str());
        
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_func);

        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            LOG(ERROR) << "curl_easy_perform() failed: " << curl_easy_strerror(res) << endl;
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return false;
        }
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    return true;
}

bool ParseNodeConfig(string &s, vector<bool> violation_type, vector<Point> &bike_area, vector<Point> &car_area, vector<Point> &warning_area1, vector<Point> &warning_area2)
{
    Document doc;
    doc.Parse(s.c_str(), s.size());
    if (doc["data"].HasMember("nodesList") && doc["data"]["nodesList"][0].HasMember("config"))
    {
        string t = doc["data"]["nodesList"][0]["config"].GetString();
        Document d;
        d.Parse(t.c_str(), t.size());
        LOG(INFO) << "d is object: " << d.IsObject() << endl;
        LOG(INFO) << "d has BIKE_AREA: " << d.HasMember("BIKE_AREA") << endl;
        LOG(INFO) << "bike_area is array: " << d["BIKE_AREA"].IsArray() << endl;

        // Processing AREA paramters
        vector<string> keys = {"BIKE_AREA","CAR_AREA", "WARNING_AREA_1", "WARNING_AREA_2"};
        map<string, vector<Point>> AREA;
        AREA.insert(make_pair(keys[0], bike_area));
        AREA.insert(make_pair(keys[1], car_area));
        AREA.insert(make_pair(keys[2], warning_area1));
        AREA.insert(make_pair(keys[3], warning_area2));

        for (auto k:keys)
        {
            if (d.HasMember(k.c_str()))
            {
                LOG(INFO) << "====== Printing " << k << " ======" << endl;
                
                const Value &a = d[k.c_str()];
                for (SizeType i = 0; i < a.Size(); i++){
                    string tt = a[i].GetString();
                    tt = tt.substr(1, tt.size()-2);
                    auto index = tt.find(",");
                    int x = stoi(tt.substr(0, index));
                    int y = stoi(tt.substr(index+2, tt.size()-1));
                    LOG(INFO) << x << " " << y << endl;
                    AREA[k].push_back(Point(x, y));
                }                 
            }
        }  
        bike_area = AREA["BIKE_AREA"];
        car_area = AREA["CAR_AREA"];
        warning_area1 = AREA["WARNING_AREA_1"];
        warning_area2 = AREA["WARNING_AREA_2"];

        // Processing violation type parameters
        if (d.HasMember("VIOLATION_TYPE"))
        {
            const Value &a = d["VIOLATION_TYPE"];
            for (SizeType i = 0; i < a.Size(); i++)
            {
                string tt = a[i].GetString();
                if (tt == "True")
                    violation_type.push_back(true);
                else if (tt = "False")
                    violation_type.push_back(false);
            }
        }
    }
    else
    {
        LOG(ERROR) << "No config data found! Please check config existence from front-end." << endl;
        return false;
    }
    return true;
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



