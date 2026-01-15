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
#include <iostream>
#include <stdio.h>
#include <stdlib.h>

using namespace cv;
using namespace std;

const int defaultWindowSize = 400;
const float defaultThresholdScalar = 1.0;
const int defaultIterations = 2;
const int defaultVerbose = 0;

struct Config
	{
	int windowSize = defaultWindowSize;
	float thresholdScalar = defaultThresholdScalar;
	int iterations = defaultIterations;
	int verbose = defaultVerbose;
	bool noGui = false;
	};

void printUsage(const char* progName)
	{
	cout << progName << ":" << endl;
	cout << "\tStabilizes and crops videos of aircraft against reasonably cloud free skies." << endl << endl;
	cout << "\tPositional usage (backwards compatible):" << endl;
	cout << "\t  " << progName << " filename [windowSize] [threshold] [iterations] [verbose] [-nogui]" << endl << endl;
	cout << "\tFlag-based usage:" << endl;
	cout << "\t  " << progName << " filename [-w windowSize] [-t threshold] [-i iterations] [-v [level]] [-nogui]" << endl << endl;
	cout << "\tOutput is written to filename_mcrop.avi" << endl << endl;
	cout << "\tOptions:" << endl;
	cout << "\t  -w, -window      Window size in pixels (default " << defaultWindowSize << ")" << endl;
	cout << "\t  -t, -threshold   Threshold scalar (default " << defaultThresholdScalar << ", try 0.5 if output is jittery)" << endl;
	cout << "\t  -i, -iterations  Number of dilation iterations (default " << defaultIterations << ", try 3 or 4 if output is jittery)" << endl;
	cout << "\t  -v, -verbose     Verbose/debug output (default " << defaultVerbose << "; -v or -v 1 to enable)" << endl;
	cout << "\t  -nogui           Disable all OpenCV windows (batch mode)" << endl << endl;
	cout << "\tTIPS:" << endl;
	cout << "\t  If the output is still jittery try sending the output back through the program for another run." << endl;
	cout << "\t  Abort process using ESC when GUI is enabled." << endl << endl;
	cout << "\tCopyright 2017 by Jim Bourke." << endl;
	}

bool parseArgs( int argc, char** argv, Config& cfg, string& filename )
	{
	if ( argc < 2 )
		{
		printUsage( argv[0] );
		return false;
		}

	// Detect whether any named options (other than -nogui) are present.
	bool hasNamedOptions = false;
	for ( int i = 1; i < argc; ++i )
		{
		string arg = argv[i];
		if ( !arg.empty() && arg[0] == '-' && arg != "-nogui" )
			{
			hasNamedOptions = true;
			break;
			}
		}

	// Legacy positional mode: filename [windowSize] [threshold] [iterations] [verbose] [-nogui]
	if ( !hasNamedOptions )
		{
		filename = argv[1];

		// Support optional -nogui flag as the last argument.
		int optArgc = argc;
		if ( optArgc >= 3 && string( argv[optArgc - 1] ) == "-nogui" )
			{
			cfg.noGui = true;
			--optArgc;
			}

		if ( optArgc >= 3 )
			{
			cfg.windowSize = strtol( argv[2], NULL, 10 );
			cfg.windowSize = max( cfg.windowSize, 50 );
			cfg.windowSize = min( cfg.windowSize, 5000 );
			cout << "Window size set to " << cfg.windowSize << "." << endl;
			if ( optArgc >= 4 )
				{
				cfg.thresholdScalar = float(strtod( argv[3], NULL ));
				cfg.thresholdScalar = max( cfg.thresholdScalar, 0.05f );
				cfg.thresholdScalar = min( cfg.thresholdScalar, 2.0f );
				cout << "Threshold set to " << cfg.thresholdScalar << "." << endl;
				if ( optArgc >= 5 )
					{
					cfg.iterations =  strtol( argv[4], NULL, 10 ) ;
					cfg.iterations = max( cfg.iterations, 1 );
					cfg.iterations = min( cfg.iterations, 5 );
					cout << "Iterations set to " << cfg.iterations << "." << endl;
					if ( optArgc >= 6 )
						{
						cfg.verbose = strtol( argv[5], NULL, 10 );
						cfg.verbose = max( cfg.verbose, 0 );
						cfg.verbose = min( cfg.verbose, 1 );
						cout << "Verbose set to " << cfg.verbose << "." << endl;
						}
					}
				}
			}

		return true;
		}

	// Named option mode: filename plus flags like -w, -t, -i, -v, -nogui
	bool haveFilename = false;

	for ( int i = 1; i < argc; ++i )
		{
		string arg = argv[i];

		if ( !arg.empty() && arg[0] == '-' )
			{
			if ( arg == "-nogui" )
				{
				cfg.noGui = true;
				}
			else if ( (arg == "-w" || arg == "-window") && i + 1 < argc )
				{
				cfg.windowSize = strtol( argv[++i], NULL, 10 );
				cfg.windowSize = max( cfg.windowSize, 50 );
				cfg.windowSize = min( cfg.windowSize, 5000 );
				cout << "Window size set to " << cfg.windowSize << "." << endl;
				}
			else if ( (arg == "-t" || arg == "-threshold") && i + 1 < argc )
				{
				cfg.thresholdScalar = float(strtod( argv[++i], NULL ));
				cfg.thresholdScalar = max( cfg.thresholdScalar, 0.05f );
				cfg.thresholdScalar = min( cfg.thresholdScalar, 2.0f );
				cout << "Threshold set to " << cfg.thresholdScalar << "." << endl;
				}
			else if ( (arg == "-i" || arg == "-iterations") && i + 1 < argc )
				{
				cfg.iterations = strtol( argv[++i], NULL, 10 );
				cfg.iterations = max( cfg.iterations, 1 );
				cfg.iterations = min( cfg.iterations, 5 );
				cout << "Iterations set to " << cfg.iterations << "." << endl;
				}
			else if ( arg == "-v" || arg == "-verbose" )
				{
				// Allow optional numeric level after -v; default to 1 if omitted.
				if ( i + 1 < argc && argv[i + 1][0] != '-' )
					{
					cfg.verbose = strtol( argv[++i], NULL, 10 );
					}
				else
					{
					cfg.verbose = 1;
					}

				cfg.verbose = max( cfg.verbose, 0 );
				cfg.verbose = min( cfg.verbose, 1 );
				cout << "Verbose set to " << cfg.verbose << "." << endl;
				}
			else if ( arg == "-h" || arg == "-help" )
				{
				printUsage( argv[0] );
				return false;
				}
			else
				{
				cerr << "Unknown option: " << arg << endl;
				return false;
				}
			}
		else
			{
			if ( !haveFilename )
				{
				filename = arg;
				haveFilename = true;
				}
			else
				{
				cerr << "Unexpected extra argument: " << arg << endl;
				return false;
				}
			}
		}

	if ( !haveFilename )
		{
		printUsage( argv[0] );
		return false;
		}

	return true;
	}

int main( int argc, char** argv )
	{
	Config cfg;
	string filename;

	if ( !parseArgs( argc, argv, cfg, filename ) )
		{
		return -1;
		}

	int& windowSize = cfg.windowSize;
	float& thresholdScalar = cfg.thresholdScalar;
	int& iterations = cfg.iterations;
	int& verbose = cfg.verbose;
	bool noGui = cfg.noGui;

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
	if ( !noGui )
		{
		namedWindow( "Motion Crop", WINDOW_AUTOSIZE );
		namedWindow( "Original", WINDOW_AUTOSIZE );
		if ( verbose )
			{
			namedWindow( "FrameGray", WINDOW_AUTOSIZE );
			namedWindow( "Otsu", WINDOW_AUTOSIZE );
			namedWindow( "Edges", WINDOW_AUTOSIZE );
			namedWindow( "Dilation", WINDOW_AUTOSIZE );
			}
		}
	Mat frameGray;

	cout << "Processing " << cap.get( CAP_PROP_FRAME_COUNT ) << " frames." << endl;

	Mat frame;
	Mat buffer;
	Mat hsv;
	Mat hsvSplit[3];
	Mat edges;
	Mat frameMorph;
	Mat roi;
	Mat kernel = getStructuringElement( MORPH_RECT,
														 Size( (2 * 2) + 1, (2 * 2) + 1 ) );

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
		dilate( edges, frameMorph , kernel, Point( -1, -1 ), iterations );
		if (verbose) imshow( "Dilation", frameMorph );

		/// Find contours
		vector<vector<Point> > contours;
		vector<Vec4i> hierarchy;
		findContours( frameMorph, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE );

		int ctrX = frame.cols / 2;
		int ctrY = frame.rows / 2;
		if ( contours.size() > 0 )
			{
			int largest_area = 0;
			int largest_contour_index = 0;

			for ( size_t i = 0; i < contours.size(); ++i ) // iterate through each contour.
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

			if (mu.m00 != 0.0)
				{
				ctrX = (int) (mu.m10 / mu.m00);
				ctrY = (int) (mu.m01 / mu.m00);
				}

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
		if ( !noGui )
			{
			imshow( "Original", frame );
			imshow( "Motion Crop", roi );

			if ( waitKey( 30 ) == 27 ) // Wait for 'esc' key press to exit
				{
				break;
				}
			}
		}
	return 0;
	}



