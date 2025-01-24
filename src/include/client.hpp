#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "main.hpp"

using namespace cv;
using namespace std;
using namespace boost::asio;

enum ClientType
{
    MJPEG,
    RTSP,
    HLS,
    DASH,
};

class Client
{
private:
    const ClientType clientType;
    ip::tcp::socket &socket;
    Camera *camera;

    Client(ClientType clientType, ip::tcp::socket &socket, Camera *camera);
    ~Client();

public:
    virtual void startStream() = 0;
    virtual void stopStream() = 0;
    friend class Camera;
};

#endif