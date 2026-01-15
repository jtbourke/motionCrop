// MotionCrop.cpp:
//
// This program analyzes video of aircraft against the sky.
// Given good source video it creates a cropped video with the aircraft in the center of the frame.
// 
// Copyright Jim Bourke, April 28, 2017
// Released under the terms of the GNU Public license
//
// Uses OpenCV for image processing.
//

#include "stdafx.h"
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/photo.hpp"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>

using namespace cv;
using namespace std;

const int defaultWindowSize = 400;
const float defaultThresholdScalar = 1.0;
const int defaultIterations = 2;
const int defaultVerbose = 0;

int windowSize = defaultWindowSize;
float thresholdScalar = defaultThresholdScalar;
int iterations = defaultIterations;
int verbose = defaultVerbose;

int main( int argc, char** argv )
	{

	if ( argc < 2 )
		{
		cout << argv[0] << ":" << endl;
		cout << "\tStabilizes and crops videos of aircraft against reasonably cloud free skies." << endl << endl;
		cout << "\tUsage: " << argv[0] << " filename [windowSize] [threshold] [iterations] [verbose]" << endl;
		cout << "\tOutput written to filename_mcrop.avi" << endl << endl;
		cout << "\twindowSize is optional and defaults to " << defaultWindowSize << " pixels." << endl << endl;
		cout << "\tthreshold is optional and defaults to " << defaultThresholdScalar << ".  Try .5 if the output is jittery." << endl << endl;
		cout << "\titerations is optional and defaults to " << defaultThresholdScalar << ".  Try 1 to save time.  Try 3 or 4 if the output is jittery." << endl << endl;
		cout << "\tdebug is optional and defaults to " << defaultVerbose << ".  Try 1 if you are trying to diagnose a problem." << endl << endl;
		cout << "\tTIPS:" << endl << "\t\tIf the output is still jittery try sending the output back through the program for another run." << endl;
		cout << "\t\tAbort process using ESC." << endl << endl;
		cout << "\tCopyright 2017 by Jim Bourke." << endl;
		return -1;
		}
	if ( argc >= 3 )
		{
		windowSize = strtol( argv[2], NULL, 10 );
		windowSize = max( windowSize, 50 );
		windowSize = min( windowSize, 5000 );
		cout << "Window size set to " << windowSize << "." << endl;
		if ( argc >= 4 )
			{
			thresholdScalar = float(strtod( argv[3], NULL ));
			thresholdScalar = max( thresholdScalar, 0.05f );
			thresholdScalar = min( thresholdScalar, 2.0f );
			cout << "Threshold set to " << thresholdScalar << "." << endl;
			if ( argc >= 5 )
				{
				iterations =  strtol( argv[4], NULL, 10 ) ;
				iterations = max( iterations, 1 );
				iterations = min( iterations, 5 );
				cout << "Iterations set to " << iterations << "." << endl;
				if ( argc >= 6 )
					{
					verbose = strtol( argv[4], NULL, 10 );
					verbose = max( iterations, 0 );
					verbose = min( iterations, 1 );
					cout << "Verbose set to " << verbose << "." << endl;
					}
				}
			}
		}
	
	string filename = argv[1];

	VideoCapture cap( filename );

	if ( !cap.isOpened() )  // check if we succeeded
		{
		cout << "Cannot open the video file. \n";
		return -1;
		}

	cout << "MotionCrop by Jim Bourke." << endl;
	cout << "Window Size set to " << windowSize << "x" << windowSize << endl;

	int length = int( cap.get( CAP_PROP_FRAME_COUNT ) );
	cout << "Number of frames: " << length << "\n";

	string outputFile = filename.substr( 0, filename.find_last_of( '.' ) ) + "_mcrop.avi";

	VideoWriter output_cap;

	// Tried several codecs.  Kept the one that seemed to work all the time.  Found through trial and error!
	//
	//	int codec = VideoWriter::fourcc( 'I', 'P', 'D', 'V' );  // select desired codec (must be available at runtime)
	// int codec = VideoWriter::fourcc( 'M', 'J', 'P', 'G' );  // select desired codec (must be available at runtime)
	//	int codec = VideoWriter::fourcc( 'W', 'M', 'V', '2' );  // select desired codec (must be available at runtime)
	//	int codec = VideoWriter::fourcc( 'D', 'I', 'V', '3' );		// for DivX MPEG - 4 codec
	//	int codec = VideoWriter::fourcc( 'M', 'P', '4', '2' );		// for MPEG - 4 codec
	int codec = VideoWriter::fourcc( 'D', 'I', 'V', 'X' );		// for DivX codec
	//int codec = VideoWriter::fourcc( 'H', '2', '6', '4' );		// for DivX codec
	//	int codec = VideoWriter::fourcc( 'P', 'I', 'M', '1' );		// for MPEG - 1 codec
	//	int codec = VideoWriter::fourcc( 'I', '2', '6', '3' );		// for ITU H.263 codec
	//	int codec = VideoWriter::fourcc( 'M', 'P', 'E', 'G' );		// for MPEG - 1 codec
	// int codec = VideoWriter::fourcc( 'M', 'P', 'V', '4' );		// for MPEG - 1 codec

	// Create output file

	output_cap.open( outputFile, codec, cap.get( CAP_PROP_FPS ), Size( windowSize, windowSize ), true );
	// check if we succeeded
	if ( !output_cap.isOpened() ) {
		cerr << "Could not open the output video file for write\n";
		return -1;
		}

	// Create windows
	namedWindow( "Motion Crop", WINDOW_AUTOSIZE );
	namedWindow( "Original", WINDOW_AUTOSIZE );

	Mat frameGray;
	Mat channels[3];

	cout << "Processing " << cap.get( CAP_PROP_FRAME_COUNT ) << " frames." << endl;

	Mat frame;
	Mat buffer;
	Mat hsv;
	Mat hsvSplit[3];
	Mat edges;
	Mat frameMorph;
	Mat roi;

	while ( 1 ) 
		{
		cout << ".";

		if (( (int)cap.get( CAP_PROP_POS_FRAMES ) % 10 ==0) && (cap.get(CAP_PROP_POS_FRAMES) > 1))
			cout << (int)cap.get( CAP_PROP_POS_FRAMES );

		//Mat frame;
		if ( !cap.read( frame ) ) // if not success, break loop
										  // read() decodes and captures the next frame.
			{
			break;
			}

		// Pad original image so we won't go out of bounds when we crop.  This creates stretching at the edges which is weird but better than the alternative.
		copyMakeBorder( frame, buffer, windowSize, windowSize,
							 windowSize, windowSize, BORDER_REPLICATE );

		// Extract hsv and keep value
		cvtColor( frame, hsv, COLOR_BGR2HSV );
		split( hsv, hsvSplit );
		frameGray = hsvSplit[2];
		if (verbose) imshow( "FrameGray", frameGray );
		
		// Calculate OTSU threshold which is useful for Canny edge detection.
		double otsuThreshold = cv::threshold(frameGray, edges, 0, 255, THRESH_BINARY | THRESH_OTSU	);
		if ( verbose ) imshow( "Otsu", edges );
		otsuThreshold *= thresholdScalar;

		// perform edge detection
		Canny( frameGray, edges, otsuThreshold , otsuThreshold*0.5 );
		if ( verbose ) imshow( "Edges", edges );

		// dilate the edges to close them.
		Mat kernel = getStructuringElement( MORPH_RECT,
																  Size( (2 * 2) + 1, (2 * 2) + 1 ) );
		dilate( edges, frameMorph , kernel, Point( -1, -1 ), iterations );
		if (verbose) imshow( "Dilation", frameMorph );

		/// Find contours
		vector<vector<Point> > contours;
		vector<Vec4i> hierarchy;
		findContours( frameMorph, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE );

		int ctrX = 0;
		int ctrY = 0;
		if ( contours.size() > 0 )
			{
			int largest_area = 0;
			int largest_contour_index = 0;

			for ( int i = 0; i < contours.size(); i++ ) // iterate through each contour.
				{
				double a = contourArea( contours[i], false );  //  Find the area of contour
				if ( a > largest_area ) {
					largest_area = (int) a;
					largest_contour_index = i;                //Store the index of largest contour
					}
				if ( verbose ) drawContours( frame, contours, i, Scalar( 0, 128, 0 ), 1, 8, vector<Vec4i>(), 0, Point() );
				}

			Moments mu;
			mu = moments( contours[largest_contour_index], false );

			ctrX = (int) (mu.m10 / mu.m00);
			ctrY = (int) (mu.m01 / mu.m00);

			// Draw markers to show user what is going on.
			Scalar color = Scalar(0,255,0);
			drawContours( frame, contours, largest_contour_index, color, 2, 8, vector<Vec4i>(), 0, Point() );
			color = Scalar( 0,0,255 );
			circle( frame, Point( ctrX, ctrY ), 9, color, 3);
			}

		// crop
		roi = buffer( Rect( windowSize + ctrX - windowSize / 2, windowSize + ctrY - windowSize / 2, windowSize, windowSize) );

		// Write frame
		output_cap.write( roi );

		// Display
		imshow( "Original", frame );
		imshow( "Motion Crop", roi );

		if ( waitKey( 30 ) == 27 ) // Wait for 'esc' key press to exit
			{
			break;
			}
		}
	return 0;
	}



