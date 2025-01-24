#ifndef CAMERA_HPP
#define CAMERA_HPP

using namespace cv;
using namespace std;
using namespace boost::asio;

#include "main.hpp"

class Camera
{
private:
    const std::string rtspUrl;
    cv::Mat sharedFrame;
    std::mutex frameMutex;
    std::condition_variable frameAvailable;
    cv::VideoCapture cap;
    const int frameIntervale = 80;
    const int jpegQuality = 50;
    std::vector<Client *> clients;

private:
    Camera();
    Camera(const std::string &rtspUrl);
    Camera(const Camera &) = delete;
    Camera &operator=(const Camera &) = delete;

public:
    static std::map<std::string, Camera *> instances;
    static std::mutex mutex;
    static Camera *getInstance(const std::string &rtspUrl);
    static void releaseInstance(const std::string &rtspUrl);
    int captureFrames();
    void handleClient(ip::tcp::socket socket);
    < template <typename T>
        void addClient(ClientType clientType, T socket);
    void addClient(ClientType clientType, ip::tcp::socket socket);
    friend class Client;
};

#endif