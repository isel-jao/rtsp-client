#include "main.hpp"

using namespace cv;
using namespace std;
using namespace boost::asio;

// Define the RTSP URL and port as constants
// const string CAMERA_RTSP_URL = "rtsp://197.230.172.128:553/user=admin_password=VoX4wnHy_channel=1_stream=0.sdp?real_stream";
// const string CAMERA_RTSP_URL = "rtsp://192.168.11.201:554/user=admin_password=VoX4wnHy_channel=1_stream=0.sdp?real_stream";
// rtsp://197.230.172.128:555/user=admin_password=VoX4wnHy_channel=1_stream=0.sdp?real_stream

std::map<std::string, Camera *> Camera::instances;
std::mutex Camera::mutex;

const int port = 3000;

int main(int argc, char **argv)
{

    crow::SimpleApp app;

    CROW_ROUTE(app, "/")
    ([]
     {
        crow::response res;
        res.add_header("Content-Type", "application/json");
        res.body = "{\"message\": \"Welcome to the RTSP stream server\"}";
        return res; });

    CROW_ROUTE(app, "/<string>/<string>").methods("GET"_method)([](string host, string credentials)
                                                                {
        
        std::string rtspUrl = "rtsp://" + host +"/" +  credentials;
        crow::response res;
        Camera *camera = Camera::getInstance(rtspUrl);
        res.add_header("Content-Type", "application/json");
        res.body = "{\"rtsp\": \"" + rtspUrl + "\"}";
        return res; });

    app.port(port).multithreaded().run();

    // captureThread.join();
    return 0;
}
