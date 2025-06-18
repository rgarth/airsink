# AirSink

An AirPlay audio sink implementation that receives audio streams from AirPlay sources and saves them as FLAC files with metadata.

## Features

- AirPlay server implementation
- RTSP protocol support
- Audio streaming via RTP
- FLAC file output with metadata
- Authentication and pairing support

## Dependencies

- OpenSSL
- libevent
- FFmpeg (libavcodec, libavformat, libavutil)
- ALSA

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

```bash
./airsink [options]
```

## Project Structure

```
airplay/
├── src/
│   ├── rtsp/         # RTSP server implementation
│   ├── rtp/          # RTP stream handling
│   ├── audio/        # Audio pipeline
│   ├── auth/         # Authentication/pairing
│   └── control/      # Volume/metadata/remote control
├── include/          # Public headers
├── lib/             # Internal headers
├── test/            # Test programs
├── CMakeLists.txt   # Build system
└── README.md
```

## License

MIT License 