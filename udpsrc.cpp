/***************************************
@Copyright(C) All rights reserved.
@Author DENG Ganglin
@Date   2020.08
@File   udpsrc.cpp
***************************************/

#include "opencv2/opencv.hpp"
#include <cstdio>

using namespace cv;
using namespace std;

int main(int argc, char** argv)
{
	VideoCapture cap("udpsrc port=5033 ! application/x-rtp,encoding-name=H264 ! rtpjitterbuffer latency=0 ! rtph264depay ! avdec_h264 ! videoconvert ! appsink");

	if( !cap.isOpened())
	{
		cout<<"Fail"<<endl;
		return -1;
	}

	namedWindow("frame",1);
	for(;;)
	{
		Mat frame(1280,720, CV_8UC3, Scalar(0,0,255));
		cap >> frame; // get a new frame from camera
		imshow("frame", frame);
		if(waitKey(30) >= 0) break;
	}
	return 0;
}