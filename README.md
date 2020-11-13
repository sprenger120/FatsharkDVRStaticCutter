## Overview 

Small openCV application that will go through a folder (and subfolders) of .avi files and for every file cut out the static. Originally done for batch processing of Fatshark FPV google's DVR recordings.
Works most of times but has trouble with bad reception parts where the video goes grayscale. 

## Dependencies

Should work with openCV 2 and 3. Requires ffmpeg, cmake and libopencv-dev installed

## Build 

Only tested with linux. (Get creative for windows.)

```
# in repository folder
mkdir build
cmake -S . -B build
cd build
make -j 8
cd ..
```

## Use

```
build/dvrsnippy path-to-your-files
```
