#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>
#include <ctime>
#include <string>
#include <stdio.h>
#include <ftw.h>
#include <fnmatch.h>
#include <algorithm>

using namespace std;

vector<string> videoFiles;

//https://stackoverflow.com/questions/983376/recursive-folder-scanning-in-c
static const char *filters[] = {
    "*.avi"
};

static int fileCallback(const char *fpath, const struct stat *sb, int typeflag) {
    /* if it's a file */
    if (typeflag == FTW_F) {
        int i;
        /* for each filter, */
        for (i = 0; i < sizeof(filters) / sizeof(filters[0]); i++) {
            /* if the filename matches the filter, */
            if (fnmatch(filters[i], fpath, FNM_CASEFOLD) == 0) {
                /* do something */
                videoFiles.push_back(string(fpath));
                //printf("found video: %s\n", fpath);
                break;
            }
        }
    }

    /* tell ftw to continue */
    return 0;
}

int frameNumberToSeconds(int frameNr)  {
  //fps for pal fatshark dvr recordings
  const double fps = 24.899158;
  return frameNr <= 0 ? 0 : (int)floor(double(frameNr) / fps);
}

typedef std::pair<string,string> Filename;

Filename splitFilename(const string& path) {
  size_t extensionStart = path.find_last_of(".");

  if (extensionStart == string::npos) {
    cout<<"unable to trim extension from filename\n";
    return Filename("","");
  }

  string pathWithoutExtension(path, 0, extensionStart) ;
  string extension(path, extensionStart+1);
  return Filename(pathWithoutExtension, extension);
};

string splitVideo(const string& path, int frameNrStart, int frameNrEnd, size_t splitID) {
  cout<<"split #"<<splitID<<" from "<<frameNumberToSeconds(frameNrStart)<<"s to "<<frameNumberToSeconds(frameNrEnd)<<"s\n";

  auto splt = splitFilename(path);
  string finishedFilename = splt.first + "_" + to_string(splitID) + "." +splt.second;

  stringstream command;

  command<<"ffmpeg -v quiet ";
  command<<"-y -i \""<<path<<"\" ";
  command<<"-vcodec copy -an ";
  command<<"-ss "<<frameNumberToSeconds(frameNrStart)<<" ";
  command<<"-t "<<frameNumberToSeconds(frameNrEnd - frameNrStart)<<" ";
  command<<" -sn \""<<finishedFilename<<"\"";


  string command_ = command.str();
  //cout<<command_<<"\n";
  system(command_.c_str());

  return finishedFilename;
}

int main(int argc,  char** argv) {
  if (argc < 2) {
    cout<<"Please supply directory of video files.\n";
    return 1;
  }


  //grab all video files
  ftw(argv[1], fileCallback, 16);


  cv::Mat frame;
  for (size_t i=0;i<videoFiles.size();++i) {
    const string& currentFile = videoFiles[i];

    cout<<"############################# File "<<i+1<<"/"<<videoFiles.size()<<"\n";

    cv::VideoCapture cap(currentFile, cv::CAP_FFMPEG);
    if (!cap.isOpened())  {// check if we succeeded
      cout << "Unable to open file " << currentFile << endl;
      continue;
     }

    //work through frames, only analyze every so often
    char frameSkipCounter = 0;
    const char frameSkipCount = 10;

    //processed frames
    int frameCounter = 0;

    //statistics
    int grayFramesCount = 0;

    // grayscale frame will decrease, color will increase
    // when score reaches scoreMAX or -scoreMAX we will set currentlyColorSegment accordingly
    //starting at -2 because initially we assume we are in a color segment and want to switch fast if we are not
    char score = -2;
    const char scoreMAX = 2;

    //true = in color section of video, false = grayscale part
    bool currentlyColorSegment = true;

    //frame indices of split start
    int splitStartFrameNr = 0;
    //how many frames will be added at the end / subtracted from the start when a split will be executed by ffmpeg
    const int splitExtensions = 20;


    vector<string> splitFileList;

    while(true) {
      ++frameSkipCounter;
      ++frameCounter;

      if (!currentlyColorSegment) {
        ++grayFramesCount;
      }

      //analyze if we have grayscale image or color
      if (frameSkipCounter > frameSkipCount) {
        //cout<<"analyzed: "<<frameNumberToSeconds(frameCounter)<<"s"<<"\n";
        frameSkipCounter = 0;
        if (!cap.retrieve(frame)) {
          if (currentlyColorSegment) {
            //video ended with color segment, finish split
            splitFileList.push_back(
                splitVideo(currentFile, splitStartFrameNr-splitExtensions, frameCounter+splitExtensions,
                    splitFileList.size()));
          }
          break;
        }

        //average frame color
        cv::Scalar avg = cv::mean(frame);
        double minimum = min(min(avg[0], avg[1]), avg[2]);
        double maximum = max(max(avg[0], avg[1]), avg[2]);

        //score system to have a bit of hysteresis when there is a short  gray frame burst from bad signal
        if (abs(minimum-maximum)<1) {
          //grayscale frame
          score--;
        } else {
          //color frame
          score++;
        }

        if (score > scoreMAX) {
          score = scoreMAX;
        }
        if (score < -scoreMAX) {
          score = -scoreMAX;
        }

        if (score == scoreMAX && !currentlyColorSegment) {
          //transition from grayscale part to color part
          //start split
          splitStartFrameNr = frameCounter;
          currentlyColorSegment = true;
        }
        if (score == -scoreMAX && currentlyColorSegment) {
          //transition from color part to grayscale part
          //finish split
          splitFileList.push_back(
              splitVideo(currentFile, splitStartFrameNr - splitExtensions, frameCounter + splitExtensions, splitFileList.size()));
          currentlyColorSegment = false;
        }
      }

      cap.grab();
    }
    cout<<"Split File into "<<splitFileList.size()
        <<" parts. Removed "<<frameNumberToSeconds(grayFramesCount)<<"s of noise frames\n";

    if (!splitFileList.empty()) {
      //concat files
      stringstream command;

      command << "ffmpeg -v quiet -y ";

      command << "-i \"concat:";
      command <<splitFileList[0];
      for (size_t j = 1; j<splitFileList.size(); ++j) {
        command << "|"<<splitFileList[j];
      }
      command << "\" ";

      command << "-c:v libx264 -preset slow -crf 27 -c:a copy ";
      auto splt = splitFilename(currentFile);
      command << splt.first << ".mkv";

      system(command.str().c_str());
      //cout << command.str() << "\n";

      //clean up temp. files
      for(const string& file: splitFileList) {
        remove(file.c_str());
      }
    }

    cout << "Finished " << currentFile << endl;
  }
  cout<<"Processed all "<<videoFiles.size()<<" files. \n";


  return 0;
}