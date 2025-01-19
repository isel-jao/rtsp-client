import React, { useEffect, useRef } from "react";

interface MJPEGStreamProps extends React.HTMLProps<HTMLImageElement> {
  url: string;
  retryInterval?: number;
  maxRetries?: number;
}

type StreamState = "loading" | "connected" | "error";

const MJPEGStream = ({
  url,
  retryInterval = 5000,
  maxRetries = 5,
  ...props
}: MJPEGStreamProps) => {
  const videoRef = useRef<HTMLImageElement>(null);
  const [state, setState] = React.useState<StreamState>("loading");
  const retryCountRef = useRef(0);
  const intervalRef = useRef<number>(null);

  const connectStream = () => {
    if (videoRef.current) {
      videoRef.current.src = url;
    }
  };

  const handleManualRetry = () => {
    if (state !== "loading") {
      setState("loading");
    }
    retryCountRef.current = 0;
    connectStream();
    startRetryInterval();
  };

  const startRetryInterval = () => {
    // Clear any existing interval
    if (intervalRef.current) {
      clearInterval(intervalRef.current);
    }

    // Setup retry interval
    intervalRef.current = setInterval(() => {
      if (retryCountRef.current >= maxRetries) {
        // Stop retrying and show error
        setState("error");
        if (intervalRef.current) {
          clearInterval(intervalRef.current);
        }
        return;
      }

      if (videoRef.current) {
        videoRef.current.src = "";
        setTimeout(() => {
          connectStream();
        }, 100);
      }

      retryCountRef.current = retryCountRef.current + 1;
    }, retryInterval);
  };

  // Clear interval when stream connects successfully
  useEffect(() => {
    if (state === "connected" && intervalRef.current) {
      clearInterval(intervalRef.current);
      retryCountRef.current = 0;
    }
  }, [state]);

  useEffect(() => {
    if (state !== "loading") {
      setState("loading");
    }
    retryCountRef.current = 0;

    // Initial connection
    connectStream();
    startRetryInterval();

    // Cleanup
    return () => {
      if (intervalRef.current) {
        clearInterval(intervalRef.current);
      }
      if (videoRef.current) {
        videoRef.current.src = "";
      }
    };
  }, [url, retryInterval, maxRetries]);

  return (
    <div className="relative">
      <img
        ref={videoRef}
        onLoad={() => setState("connected")}
        onError={() => {
          if (state !== "loading") {
            setState("loading");
          }
          console.error("Error loading stream");
        }}
        {...props}
        className={`${props.className} ${state === "loading" ? "opacity-50" : ""}`}
      />
      {state === "loading" && (
        <div className="absolute inset-0 flex items-center justify-center bg-black bg-opacity-20">
          <div className="flex flex-col items-center">
            <div className="h-8 w-8 animate-spin rounded-full border-b-2 border-white"></div>
            <p className="mt-2 text-white">
              Connecting... ({retryCountRef.current}/{maxRetries})
            </p>
          </div>
        </div>
      )}
      {state === "error" && (
        <div className="absolute inset-0 flex items-center justify-center bg-black bg-opacity-50">
          <div className="text-center text-white">
            <p className="text-xl">Connection Failed</p>
            <p className="mt-2">
              Unable to connect to stream after {maxRetries} attempts
            </p>
            <button
              onClick={handleManualRetry}
              className="mt-4 rounded-lg bg-blue-500 px-4 py-2 text-white hover:bg-blue-600"
            >
              Retry Connection
            </button>
          </div>
        </div>
      )}
    </div>
  );
};

export default function DevPage() {
  return (
    <main>
      <div className="container py-container">
        <h1>Dev Page</h1>
        <div className="mt-12 grid grid-cols-2 gap-12">
          <MJPEGStream
            url="http://localhost:3000/1"
            className="aspect-video rounded-lg border"
            retryInterval={3000}
            maxRetries={5}
          />
          <MJPEGStream
            url="http://localhost:3000/1"
            className="aspect-video rounded-lg border"
          />
          <MJPEGStream
            url="http://localhost:3000/3"
            className="aspect-video rounded-lg border"
          />
          <MJPEGStream
            url="http://localhost:3000/4"
            className="aspect-video rounded-lg border"
          />
        </div>
      </div>
    </main>
  );
}
