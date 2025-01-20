FROM debian:latest

# Install dependencies
RUN apt-get update -y
RUN apt-get install libopencv-dev ffmpeg libboost-all-dev libasio-dev g++ -y

# Set the working directory
WORKDIR /app

# Copy the src folder to the container
COPY src /app/src

# Compile the code
RUN g++  src/main.cpp `pkg-config --cflags --libs opencv4` -lboost_system -lpthread -o program  

# expose the ports
EXPOSE 8080 3000

# Run the application
CMD ["./program"]