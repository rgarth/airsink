# AirSink

An AirPlay 2 audio sink implementation that receives audio streams from AirPlay 2 sources and saves them as FLAC files with metadata.

## Features

- AirPlay 2 server implementation
- RTSP protocol support with AirPlay 2 endpoints
- Audio streaming via RTP
- FLAC file output with metadata
- Authentication and pairing support
- mDNS/Bonjour service discovery

## Dependencies

- OpenSSL
- libevent
- FFmpeg (libavcodec, libavformat, libavutil)
- ALSA
- Avahi (for mDNS)
- json-c (for JSON parsing)

## Building

```bash
make clean
make
```

## Usage

```bash
./airsink [options]
```

Options:
- `-p port` - Specify port number (default: 7000)
- `-o directory` - Specify output directory (default: current directory)
- `-v` - Enable verbose logging
- `-h` - Show help message

## AirPlay 2 Protocol

This implementation supports the AirPlay 2 protocol which includes:

- `/pair-setup` - Device pairing setup
- `/pair-verify` - Device pairing verification  
- `/fp-setup` - FairPlay DRM setup
- `/stream` - Audio streaming endpoint
- Standard RTSP methods (OPTIONS, ANNOUNCE, SETUP, RECORD, etc.)

## Project Structure

```
airsink/
├── src/
│   ├── rtsp/         # RTSP server implementation
│   ├── rtp/          # RTP stream handling
│   ├── audio/        # Audio pipeline
│   ├── auth/         # Authentication/pairing
│   ├── mdns/         # mDNS/Bonjour advertisement
│   └── control/      # Volume/metadata/remote control
├── include/          # Public headers
├── lib/             # Internal headers
├── test/            # Test programs
├── Makefile         # Build system
└── README.md
```

## License

MIT License 