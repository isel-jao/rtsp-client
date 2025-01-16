#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <boost/asio.hpp>

using namespace cv;
using namespace std;
using namespace boost::asio;

// Define the RTSP URL and port as constants
const string CAMERA_RTSP_URL = "rtsp://197.230.172.128:553/user=admin_password=VoX4wnHy_channel=1_stream=0.sdp?real_stream";
const int port = 3000;
const int frameRate = 30;

// Shared frame and synchronization
Mat sharedFrame;
mutex frameMutex;
condition_variable frameAvailable;

// Function to continuously read frames from the camera
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

        // this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS
        this_thread::sleep_for(std::chrono::milliseconds(1000 / frameRate));
    }
}

// Function to handle a single client connection
void handleClient(ip::tcp::socket socket)
{
    try
    {
        // Get client info
        string clientAddress = socket.remote_endpoint().address().to_string();
        cout << "New client connected: " << clientAddress << endl;

        // HTTP Header
        string header =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
        socket.send(buffer(header));

        vector<uchar> buf;
        vector<int> params = {IMWRITE_JPEG_QUALITY, 90};

        while (true)
        {
            Mat frame;

            // Wait for a new frame
            {
                unique_lock<mutex> lock(frameMutex);
                frameAvailable.wait(lock, []
                                    { return !sharedFrame.empty(); });
                sharedFrame.copyTo(frame);
            }

            // Encode frame as JPEG
            imencode(".jpg", frame, buf, params);

            // Write frame boundary and data
            string boundary = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + to_string(buf.size()) + "\r\n\r\n";
            socket.send(buffer(boundary));
            socket.send(buffer(reinterpret_cast<const char *>(buf.data()), buf.size()));
            socket.send(buffer("\r\n"));

            this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS
        }
    }
    catch (std::exception &e)
    {
        cerr << "Client connection error: " << e.what() << endl;
    }

    // Log client disconnection
    cout << "Client disconnected." << endl;
}

// Function to stream MJPEG to multiple browsers
void streamToMultipleBrowsers(VideoCapture &cap)
{
    try
    {
        io_service io_service;
        ip::tcp::acceptor acceptor(io_service, ip::tcp::endpoint(ip::tcp::v4(), port));
        cout << "MJPEG Stream Server started at http://localhost:" << port << endl;

        for (;;)
        {
            ip::tcp::socket socket(io_service);
            acceptor.accept(socket); // Wait for a new client connection

            // Get client info
            string clientAddress = socket.remote_endpoint().address().to_string();
            cout << "New client connected: " << clientAddress << endl;

            // Start a new thread to handle this client
            thread clientThread(handleClient, move(socket));
            clientThread.detach(); // Detach the thread to allow it to run independently
        }
    }
    catch (std::exception &e)
    {
        cerr << "Server error: " << e.what() << endl;
    }
}

int main()
{
    VideoCapture cap(CAMERA_RTSP_URL);

    if (!cap.isOpened())
    {
        cerr << "Error: Cannot open RTSP stream from " << CAMERA_RTSP_URL << endl;
        return -1;
    }

    // Start the frame capture in a separate thread
    thread captureThread(captureFrames, ref(cap));

    // Start the server to stream to multiple browsers
    streamToMultipleBrowsers(cap);

    // Wait for the capture thread to finish
    captureThread.join();

    return 0;
}
