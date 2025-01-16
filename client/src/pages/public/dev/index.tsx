import React, { useEffect, useRef } from "react";

// const MJPEGStream = () => {
//   const videoRef = useRef(null);

//   useEffect(() => {
//     const videoElement = videoRef.current;

//     if (videoElement) {
//       // Set the source of the video element to the MJPEG stream URL
//       videoElement.src = 'http://localhost:3000'; // Assuming the server is running locally
//     }

//     return () => {
//       if (videoElement) {
//         videoElement.src = ''; // Stop the stream when the component is unmounted
//       }
//     };
//   }, []);

//   return (
//     <div>
//       <h1>MJPEG Stream</h1>
//       <img ref={videoRef} alt="MJPEG Stream" />
//     </div>
//   );
// };

interface MJPEGStreamProps extends React.HTMLProps<HTMLImageElement> {
  url: string;
}

const MJPEGStream = ({ url, ...props }: MJPEGStreamProps) => {
  const videoRef = useRef<HTMLImageElement>(null);
  const [state, setState] = React.useState("loading");

  useEffect(() => {
    const videoElement = videoRef.current;

    if (videoElement) {
      // Set the source of the video element to the MJPEG stream URL
      videoElement.src = url;
    }

    return () => {
      if (videoElement) {
        videoElement.src = ""; // Stop the stream when the component is unmounted
      }
    };
  }, [url]);

  return <img ref={videoRef} {...props} />;
};

export default function DevPage() {
  return (
    <main>
      <div className="container py-container">
        <h1>Dev Page</h1>
        <div className="mt-12 grid grid-cols-2 gap-12">
          <MJPEGStream
            url="http://localhost:3000"
            className="aspect-video rounded-lg border"
          />
          <MJPEGStream
            url="http://localhost:3000"
            className="aspect-video rounded-lg border"
          />
          <MJPEGStream
            url="http://localhost:3000"
            className="aspect-video rounded-lg border"
          />
          <MJPEGStream
            url="http://localhost:3000"
            className="aspect-video rounded-lg border"
          />
        </div>
      </div>
    </main>
  );
}
