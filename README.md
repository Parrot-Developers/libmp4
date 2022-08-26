# libmp4 - MP4 file library

libmp4 is a C library to handle MP4 files (ISO base media file format, see ISO/IEC 14496-12).
It is mainly targeted to be used with videos produced by Parrot Drones (Bebop, Bebop2, Disco, etc.).

# On Windows
- `cd libmp4`
- `cmake -G "Visual Studio 15 2017 Win64" -B _build`
- `cd _build`
- Open `libmp4.sln`
- libmp4.lib is generated
## For Running tests: 
- Put a series of jpeg frames in `libmp4/data/frames/out_%03d.jpg` format. 
- `_build/Debug/muxerjpeg ../data/output_video.mp4 crash_at_frame_number`
- This will generate a `data/muxerjpeg.mp4` file with mp4v codec. 
- Do similar thing for h264 video with sample h264 encoded frames.

