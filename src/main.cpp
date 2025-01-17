#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>

using namespace cv;
using namespace std;
using namespace boost::asio;

class Camera
{
private:
    static std::map<std::string, Camera *> instances;
    static std::mutex instanceMutex;

    VideoCapture cap;
    std::string rtspUrl;
    int frameRate;
    bool streaming;
    std::vector<ip::tcp::socket> clients;
    // std::set<ip::tcp::socket> clients;
    std::thread captureThread;
    std::mutex clientMutex;
    std::condition_variable clientCond;
    Mat sharedFrame;

public:
    static Camera &getInstance(const std::string &rtspUrl, int frameRate);
    void addClient(ip::tcp::socket socket);
    void removeClient(ip::tcp::socket socket);
    void startStreaming();
    void stopStreaming();
    void streamToClient(ip::tcp::socket socket);

private:
    Camera(const std::string &rtspUrl, int frameRate);
    Camera(const Camera &) = delete;
    Camera &operator=(const Camera &other);
    ~Camera();

    void captureFrames();
};

std::map<std::string, Camera *> Camera::instances;
std::mutex Camera::instanceMutex;

Camera::Camera(const std::string &rtspUrl, int frameRate)
    : rtspUrl(rtspUrl), frameRate(frameRate), streaming(false)
{
    cap.open(rtspUrl);
    if (!cap.isOpened())
    {
        cerr << "Error: Cannot open RTSP stream from " << rtspUrl << endl;
        throw std::runtime_error("Cannot open RTSP stream.");
    }
}

Camera &Camera::getInstance(const std::string &rtspUrl, int frameRate)
{
    std::lock_guard<std::mutex> lock(instanceMutex);
    if (instances.find(rtspUrl) == instances.end())
    {
        std::cout << "Creating new Camera instance" << std::endl;
        instances[rtspUrl] = new Camera(rtspUrl, frameRate);
    }
    else
    {
        std::cout << "Camera instance already exists" << std::endl;
    }
    return *instances[rtspUrl];
}

Camera::~Camera()
{
    streaming = false;
    if (captureThread.joinable())
    {
        captureThread.join();
    }
    cap.release();
}

void Camera::captureFrames()
{
    Mat frame;
    while (streaming && cap.read(frame))
    {
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            if (!clients.empty())
            {
                frame.copyTo(sharedFrame);
            }
        }
        this_thread::sleep_for(std::chrono::milliseconds(1000 / frameRate));
    }
}

void Camera::addClient(ip::tcp::socket socket)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    clients.push_back(std::move(socket));

    if (!streaming)
    {
        streaming = true;
        captureThread = std::thread(&Camera::captureFrames, this);
    }
}

void Camera::removeClient(ip::tcp::socket socket)
{
    std::lock_guard<std::mutex> lock(clientMutex);
    clients.erase(std::remove_if(clients.begin(), clients.end(),
                                 [&socket](ip::tcp::socket &s)
                                 { return s.native_handle() == socket.native_handle(); }),
                  clients.end());
    if (clients.empty() && streaming)
    {
        streaming = false;
        clientCond.notify_all();
    }
}

void Camera::streamToClient(ip::tcp::socket socket)
{
    try
    {
        std::vector<uchar> buf;
        std::vector<int> params = {IMWRITE_JPEG_QUALITY, 90};
        Mat frame;

        // Send MJPEG headers
        string header =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
        socket.send(buffer(header));

        while (streaming)
        {
            std::lock_guard<std::mutex> lock(clientMutex);
            if (sharedFrame.empty())
            {
                continue;
            }
            sharedFrame.copyTo(frame);

            // Encode frame as JPEG
            if (!imencode(".jpg", frame, buf, params))
            {
                cerr << "Error encoding frame to JPEG." << endl;
                continue;
            }

            // Send frame boundary and data
            string boundary = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + to_string(buf.size()) + "\r\n\r\n";
            socket.send(buffer(boundary));
            socket.send(buffer(reinterpret_cast<const char *>(buf.data()), buf.size()));
            socket.send(buffer("\r\n"));

            this_thread::sleep_for(std::chrono::milliseconds(1000 / frameRate));
        }
    }
    catch (std::exception &e)
    {
        cerr << "Client connection error: " << e.what() << endl;
    }
}

void handleClient(ip::tcp::socket socket)
{
    try
    {
        // Read HTTP request
        boost::asio::streambuf requestBuffer;
        boost::asio::read_until(socket, requestBuffer, "\r\n\r\n");
        string requestData = string(boost::asio::buffer_cast<const char *>(requestBuffer.data()));

        // Parse the request to get the rtspUrl
        // For example, if the path is /camera/rtsp://example.com/stream
        size_t start = requestData.find("GET ");
        std::cout << "Request Data: " << requestData << std::endl;
        if (start == string::npos)
        {
            return;
        }
        start += 4;
        size_t end = requestData.find(" HTTP/1.", start);
        if (end == string::npos)
        {
            return;
        }
        string path = requestData.substr(start, end - start);
        boost::trim(path);

        // Extract rtspUrl from path
        // Assuming path is in the form "/camera/rtsp://example.com/stream"
        if (path.find("/camera/") != 0)
        {
            return;
        }
        string rtspUrl = path.substr(8);
        std::cout << "RTSP URL: " << rtspUrl << std::endl;

        // Get Camera instance
        Camera &cam = Camera::getInstance(rtspUrl, 30);

        // Add client
        cam.addClient(std::move(socket));

        // Stream to client
        cam.streamToClient(std::move(socket));
    }
    catch (std::exception &e)
    {
        cerr << "Client connection error: " << e.what() << endl;
    }
}

int main()
{
    try
    {
        io_service io_service;
        ip::tcp::acceptor acceptor(io_service, ip::tcp::endpoint(ip::tcp::v4(), 3000));
        cout << "Server started at port 3000" << endl;

        while (true)
        {
            ip::tcp::socket socket(io_service);
            acceptor.accept(socket);
            thread clientThread(handleClient, move(socket));
            clientThread.detach();
        }
    }
    catch (std::exception &e)
    {
        cerr << "Server error: " << e.what() << endl;
    }
    return 0;
}
// Define the RTSP URL and port as constants
// const string CAMERA_RTSP_URL = "rtsp://197.230.172.128:553/user=admin_password=VoX4wnHy_channel=1_stream=0.sdp?real_stream";
// const int port = 3000;
// const int frameRate = 30;

// // Shared frame and synchronization
// Mat sharedFrame;
// mutex frameMutex;
// condition_variable frameAvailable;

// // Function to continuously read frames from the camera
// void captureFrames(VideoCapture &cap)
// {
//     while (true)
//     {
//         Mat frame;
//         cap >> frame; // Capture a frame

//         if (frame.empty())
//         {
//             cerr << "Error: Received empty frame. Stopping capture." << endl;
//             break;
//         }

//         // Lock the mutex and update the shared frame
//         {
//             lock_guard<mutex> lock(frameMutex);
//             frame.copyTo(sharedFrame);
//         }

//         // Notify waiting threads that a new frame is available
//         frameAvailable.notify_all();

//         // this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS
//         this_thread::sleep_for(std::chrono::milliseconds(1000 / frameRate));
//     }
// }

// // Function to handle a single client connection
// void handleClient(ip::tcp::socket socket)
// {
//     try
//     {
//         // Get client info
//         string clientAddress = socket.remote_endpoint().address().to_string();
//         cout << "New client connected: " << clientAddress << endl;

//         // HTTP Header
//         string header =
//             "HTTP/1.1 200 OK\r\n"
//             "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
//         socket.send(buffer(header));

//         vector<uchar> buf;
//         vector<int> params = {IMWRITE_JPEG_QUALITY, 90};

//         while (true)
//         {
//             Mat frame;

//             // Wait for a new frame
//             {
//                 unique_lock<mutex> lock(frameMutex);
//                 frameAvailable.wait(lock, []
//                                     { return !sharedFrame.empty(); });
//                 sharedFrame.copyTo(frame);
//             }

//             // Encode frame as JPEG
//             imencode(".jpg", frame, buf, params);

//             // Write frame boundary and data
//             string boundary = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + to_string(buf.size()) + "\r\n\r\n";
//             socket.send(buffer(boundary));
//             socket.send(buffer(reinterpret_cast<const char *>(buf.data()), buf.size()));
//             socket.send(buffer("\r\n"));

//             this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS
//         }
//     }
//     catch (std::exception &e)
//     {
//         cerr << "Client connection error: " << e.what() << endl;
//     }

//     // Log client disconnection
//     cout << "Client disconnected." << endl;
// }

// // Function to stream MJPEG to multiple browsers
// void streamToMultipleBrowsers(VideoCapture &cap)
// {
//     try
//     {
//         io_service io_service;
//         ip::tcp::acceptor acceptor(io_service, ip::tcp::endpoint(ip::tcp::v4(), port));
//         cout << "MJPEG Stream Server started at http://localhost:" << port << endl;

//         for (;;)
//         {
//             ip::tcp::socket socket(io_service);
//             acceptor.accept(socket); // Wait for a new client connection

//             // Get client info
//             string clientAddress = socket.remote_endpoint().address().to_string();
//             cout << "New client connected: " << clientAddress << endl;

//             // Start a new thread to handle this client
//             thread clientThread(handleClient, move(socket));
//             clientThread.detach(); // Detach the thread to allow it to run independently
//         }
//     }
//     catch (std::exception &e)
//     {
//         cerr << "Server error: " << e.what() << endl;
//     }
// }

// int main()
// {
//     VideoCapture cap(CAMERA_RTSP_URL);

//     if (!cap.isOpened())
//     {
//         cerr << "Error: Cannot open RTSP stream from " << CAMERA_RTSP_URL << endl;
//         return -1;
//     }

//     // Start the frame capture in a separate thread
//     thread captureThread(captureFrames, ref(cap));

//     // Start the server to stream to multiple browsers
//     streamToMultipleBrowsers(cap);

//     // Wait for the capture thread to finish
//     captureThread.join();

//     return 0;
// }
