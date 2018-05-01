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
                printf("found image: %s\n", fpath);
                break;
            }
        }
    }

    /* tell ftw to continue */
    return 0;
}




int main() {
  //grab all video files
  ftw("/home/sprenger/Videos/FPV/2018-04/09 (copy)", fileCallback, 16);


  cv::Mat frame;
  for (string currentFile : videoFiles) {

    cv::VideoCapture cap(currentFile); // open the default camera
    if (!cap.isOpened())  {// check if we succeeded
      cout << "Unable to open file " << currentFile << endl;
      continue;
     }

    //work through frames, only analyze every so often
    char frameSkipCounter = 0;
    const char frameSkipCount = 10;

    // grayscale frame will decrease, color will increase
    // when score reaches scoreMAX or -scoreMAX we will set currentMode accordingly
    char score = 3;
    const char scoreMAX = 3;

    bool currentMode = true; //true = in color section of video, false = grayscale part

    //how many times a video has been split, also current index of split file / filename
    short videoSplitCount = 0;


    while(true) {
      frameSkipCounter++;
      if (frameSkipCounter > frameSkipCount) {
        frameSkipCounter = 0;
        if (!cap.retrieve(frame)) {
          //todo: finish current color segment, video has ended here
          break;
        }

        //average frame color
        cv::Scalar avg = cv::mean(frame);
        double minimum = min(min(avg[0], avg[1]), avg[2]);
        double maximum = max(max(avg[0], avg[1]), avg[2]);
        if (abs(minimum-maximum)<1) {
          //grayscale frame
          score--;
        } else {
          //color frame
          score++;
        }
      }
      if (score > scoreMAX) {
        score = scoreMAX;
      }
      if (score < -scoreMAX) {
        score = -scoreMAX;
      }

      if (score == scoreMAX && !currentMode) {
        //transition from grayscale part to color part
        //start split

        currentMode = true;
      }

      if (score == -scoreMAX && currentMode) {
        //transition from color part to grayscale part
        //finish split

        currentMode = false;
      }



      cap.grab();
    }
    cout << "Done. " << currentFile << endl;
  }



  return 0;
}