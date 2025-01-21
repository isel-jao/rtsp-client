#include "main.hpp"

using namespace cv;
using namespace std;
using namespace boost::asio;

std::map<std::string, Camera *> Camera::instances;
std::mutex Camera::mutex;

const int port = 3000;

void handle_client(ip::tcp::socket socket)
{
    try
    {
        // Read HTTP request
        std::vector<char> request(1024);
        size_t len = socket.receive(buffer(request));
        std::string req(request.data(), len);

        // Get client info
        string clientAddress = socket.remote_endpoint().address().to_string();
        cout << "New client connected: " << clientAddress << endl;

        // Parse request path
        size_t pathStart = req.find(" ") + 1;
        size_t pathEnd = req.find(" ", pathStart);
        if (pathStart == string::npos || pathEnd == string::npos)
        {
            throw std::runtime_error("Invalid HTTP request format");
        }

        std::string path = req.substr(pathStart, pathEnd - pathStart);
        std::string rtspUrl = "rtsp:/" + path;
        std::cout << "RTSP URL: " << rtspUrl << std::endl;

        try
        {
            Camera *camera = Camera::getInstance(rtspUrl);
            camera->handleClient(std::move(socket));
        }
        catch (const std::exception &e)
        {
            std::cerr << "Camera handling error: " << e.what() << std::endl;
            // Send error response to client
            std::string errorMsg = "Error processing RTSP stream\r\n";
            socket.write_some(buffer(errorMsg));
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Client handling error: " << e.what() << std::endl;
        try
        {
            // Try to send error response
            std::string errorResponse = "HTTP/1.1 500 Internal Server Error\r\n"
                                        "Content-Type: text/plain\r\n"
                                        "Connection: close\r\n"
                                        "\r\n"
                                        "Internal server error occurred";
            socket.write_some(buffer(errorResponse));
        }
        catch (...)
        {
            // Ignore any errors during error response
        }
    }
}

int main(int argc, char **argv)
{
    try
    {
        io_service io_service;
        ip::tcp::acceptor acceptor(io_service, ip::tcp::endpoint(ip::tcp::v4(), port));
        std::cout << "Stream server started on port " << port << std::endl;

        while (true)
        {
            ip::tcp::socket socket(io_service);
            acceptor.accept(socket);

            std::thread clientThread([socket = std::move(socket)]() mutable
                                     { handle_client(std::move(socket)); });
            clientThread.detach();
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
// http://localhost:3000/197.230.172.128:555/user=admin_password=VoX4wnHy_channel=1_stream=0.sdp?real_stream