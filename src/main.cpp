#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <boost/asio.hpp>
#include "crow_all.h"
using namespace cv;
using namespace std;
using namespace boost::asio;

// Define the RTSP URL and port as constants
// const string CAMERA_RTSP_URL = "rtsp://197.230.172.128:553/user=admin_password=VoX4wnHy_channel=1_stream=0.sdp?real_stream";
const string CAMERA_RTSP_URL = "rtsp://192.168.11.201:554/user=admin_password=VoX4wnHy_channel=1_stream=0.sdp?real_stream";
cv::Mat sharedFrame;
mutex frameMutex;
condition_variable frameAvailable;
const int port = 3000;
const int frameRate = 30;

class Camera;

class Camera
{
private:
    string _rtspUrl;

    static std::map<string, Camera *> _instances;

    Camera(string rtspUrl) : _rtspUrl(rtspUrl) {}

public:
    static Camera *getInstance(string rtspUrl)
    {
        if (_instances.find(rtspUrl) == _instances.end())
        {
            _instances[rtspUrl] = new Camera(rtspUrl);
        }
        return _instances[rtspUrl];
    }
};

string getCodec(const cv::VideoCapture *cap)
{
    // Get codec information
    int fourcc = static_cast<int>(cap->get(cv::CAP_PROP_FOURCC));

    // Convert FOURCC to readable format
    char fourcc_str[5];
    for (int i = 0; i < 4; i++)
    {
        fourcc_str[i] = fourcc & 0xFF;
        fourcc >>= 8;
    }
    fourcc_str[4] = '\0';
    return fourcc_str;
}

void captureFrames(VideoCapture &cap)
{
    while (true)
    {
        Mat frame;
        cap >> frame; // Capture a frame

        if (frame.empty())
        {
            cerr << "Error: Received empty frame. Stopping capture." << endl;
            break;
        }

        // Lock the mutex and update the shared frame
        {
            lock_guard<mutex> lock(frameMutex);
            frame.copyTo(sharedFrame);
        }

        // Notify waiting threads that a new frame is available
        frameAvailable.notify_all();

        this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS
    }
}

int main(int argc, char **argv)
{
    // Open RTSP stream
    cv::VideoCapture cap;
    cap.open(CAMERA_RTSP_URL);

    if (!cap.isOpened())
    {
        std::cout << "Error opening RTSP stream" << std::endl;
        return -1;
    }

    // Get codec information
    std::string codec = getCodec(&cap);
    std::cout << "RTSP stream opened successfully" << std::endl;
    std::cout << "Codec: " << codec << std::endl;
    cap.release();

    // Start the frame capture in a separate thread
    // captureFrames(cap); // This will block the main thread
    // thread captureThread(captureFrames, ref(cap));

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
