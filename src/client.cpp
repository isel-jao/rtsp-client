#include "main.hpp"

using namespace cv;
using namespace std;
using namespace boost::asio;

Client::Client(ClientType clientType, ip::tcp::socket &socket, Camera *camera) : clientType(clientType), socket(socket), camera(camera)
{
}

void Client::startStream()
{
    // do nothing
}

void Client::stopStream()
{
    socket.close();
}

Client::~Client()
{
    stopStream();
}
