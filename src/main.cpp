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
#include <algorithm>
#include <vector>

using namespace cv;
using namespace std;

template<typename T>
T clamp(T val, T lo, T hi) { return max(lo, min(val, hi)); }

struct Config
	{
	int windowSize;
	float thresholdScalar;
	int iterations;
	int verbose;
	bool noGui;
	int codec;
	const char* codecName;
	const char* outputExt;

	Config();
	};

// Commonly used video codecs
namespace codecs
	{
	constexpr unsigned int fourcc(char a, char b, char c, char d)
		{
		return static_cast<unsigned int>(a)
			| (static_cast<unsigned int>(b) << 8)
			| (static_cast<unsigned int>(c) << 16)
			| (static_cast<unsigned int>(d) << 24);
		}

	struct Entry { const char* name; int fourcc; };

	constexpr Entry list[] =
		{
		{ "DIVX", static_cast<int>(fourcc('D','I','V','X')) },
		{ "MJPG", static_cast<int>(fourcc('M','J','P','G')) },
		{ "MPEG", static_cast<int>(fourcc('M','P','E','G')) },
		{ "MP4V", static_cast<int>(fourcc('M','P','4','V')) },
		{ "H264", static_cast<int>(fourcc('H','2','6','4')) },
		{ "X264", static_cast<int>(fourcc('X','2','6','4')) },
		{ "AVC1", static_cast<int>(fourcc('a','v','c','1')) },
		{ "WMV2", static_cast<int>(fourcc('W','M','V','2')) },
		};

	constexpr int defaultIndex = 0; // DIVX
	}

inline Config::Config()
	: windowSize(400)
	, thresholdScalar(1.0f)
	, iterations(2)
	, verbose(0)
	, noGui(false)
	, codec(codecs::list[codecs::defaultIndex].fourcc)
	, codecName(codecs::list[codecs::defaultIndex].name)
	, outputExt("avi")
	{}

// Argument parsing helpers
void setWindowSize(Config& cfg, const char* value)
	{
	cfg.windowSize = clamp(atoi(value), 50, 5000);
	cout << "Window size set to " << cfg.windowSize << "." << endl;
	}

void setThreshold(Config& cfg, const char* value)
	{
	cfg.thresholdScalar = clamp(static_cast<float>(atof(value)), 0.05f, 2.0f);
	cout << "Threshold set to " << cfg.thresholdScalar << "." << endl;
	}

void setIterations(Config& cfg, const char* value)
	{
	cfg.iterations = clamp(atoi(value), 1, 5);
	cout << "Iterations set to " << cfg.iterations << "." << endl;
	}

void setVerbose(Config& cfg, const char* value)
	{
	cfg.verbose = clamp(atoi(value), 0, 1);
	cout << "Verbose set to " << cfg.verbose << "." << endl;
	}

bool setCodec(Config& cfg, const char* value)
	{
	string name = value;
	transform(name.begin(), name.end(), name.begin(),
			  [](unsigned char c) { return static_cast<char>(toupper(c)); });

	for (const auto& e : codecs::list)
		{
		if (name == e.name)
			{
			cfg.codec = e.fourcc;
			cfg.codecName = e.name;
			cout << "Codec set to " << cfg.codecName << "." << endl;
			return true;
			}
		}

	cerr << "Unknown codec: " << value << endl;
	return false;
	}

bool setFormat(Config& cfg, const char* value)
	{
	string fmt = value;
	transform(fmt.begin(), fmt.end(), fmt.begin(),
			  [](unsigned char c) { return static_cast<char>(tolower(c)); });

	if (fmt == "avi" || fmt == "mp4")
		{
		cfg.outputExt = (fmt == "avi") ? "avi" : "mp4";
		cout << "Output format set to " << cfg.outputExt << "." << endl;
		return true;
		}

	cerr << "Unknown format: " << value << " (use avi or mp4)" << endl;
	return false;
	}

void printUsage(const char* progPath)
	{
	// Extract just the filename from the path
	string path = progPath;
	size_t pos = path.find_last_of("/\\");
	string progName = (pos != string::npos) ? path.substr(pos + 1) : path;

	cout << progName << ":\n"
		<< "\tStabilizes and crops videos of aircraft against reasonably cloud free skies.\n\n"
		<< "\tUsage: " << progName << " filename [windowSize] [threshold] [iterations] [verbose] [-nogui]\n"
		<< "\t   or: " << progName << " filename [-w size] [-t thresh] [-i iter] [-v] [-c codec] [-f fmt] [-nogui]\n\n"
		<< "\tOptions:\n"
		<< "\t  -w, -window      Window size in pixels (default 400)\n"
		<< "\t  -t, -threshold   Threshold scalar (default 1.0, try 0.5 if jittery)\n"
		<< "\t  -i, -iterations  Dilation iterations (default 2, try 3-4 if jittery)\n"
		<< "\t  -v, -verbose     Enable debug output\n"
		<< "\t  -c, -codec       Video codec: DIVX*, MJPG, MPEG, MP4V, H264, X264, AVC1, WMV2\n"
		<< "\t  -f, -format      Output format: avi (default), mp4\n"
		<< "\t  -nogui           Disable GUI windows (batch mode)\n\n"
		<< "\tOutput: filename_mcrop.{avi|mp4}\n"
		<< "\tCopyright 2017 by Jim Bourke.\n";
	}

bool parseArgs(int argc, char** argv, Config& cfg, vector<string>& filenames)
	{
	if (argc < 2) { printUsage(argv[0]); return false; }

	// Check for flag-based arguments
	bool hasFlags = false;
	for (int i = 1; i < argc; ++i)
		if (argv[i][0] == '-' && string(argv[i]) != "-nogui")
			{ hasFlags = true; break; }

	if (!hasFlags)
		{
		// Legacy positional mode (single file only)
		filenames.push_back(argv[1]);
		int optArgc = argc;
		if (optArgc >= 3 && string(argv[optArgc-1]) == "-nogui")
			{ cfg.noGui = true; --optArgc; }

		if (optArgc >= 3) setWindowSize(cfg, argv[2]);
		if (optArgc >= 4) setThreshold(cfg, argv[3]);
		if (optArgc >= 5) setIterations(cfg, argv[4]);
		if (optArgc >= 6) setVerbose(cfg, argv[5]);
		return true;
		}

	// Flag-based mode (supports multiple files)
	for (int i = 1; i < argc; ++i)
		{
		string arg = argv[i];
		if (arg[0] != '-')
			{ filenames.push_back(arg); continue; }

		if (arg == "-nogui") cfg.noGui = true;
		else if ((arg == "-w" || arg == "-window") && i+1 < argc) setWindowSize(cfg, argv[++i]);
		else if ((arg == "-t" || arg == "-threshold") && i+1 < argc) setThreshold(cfg, argv[++i]);
		else if ((arg == "-c" || arg == "-codec") && i+1 < argc)
			{
			if (!setCodec(cfg, argv[++i])) return false;
			}
		else if ((arg == "-f" || arg == "-format") && i+1 < argc)
			{
			if (!setFormat(cfg, argv[++i])) return false;
			}
		else if ((arg == "-i" || arg == "-iterations") && i+1 < argc) setIterations(cfg, argv[++i]);
		else if (arg == "-v" || arg == "-verbose")
			{
			if (i+1 < argc && argv[i+1][0] != '-') setVerbose(cfg, argv[++i]);
			else { cfg.verbose = 1; cout << "Verbose set to 1." << endl; }
			}
		else if (arg == "-h" || arg == "-help") { printUsage(argv[0]); return false; }
		else { cerr << "Unknown option: " << arg << endl; return false; }
		}

	if (filenames.empty()) { printUsage(argv[0]); return false; }
	return true;
	}

// Find the centroid of the largest contour (aircraft) in the frame
Point findAircraftCentroid(const Mat& frame, const Config& cfg, Mat& hsv, Mat& edges,
                           Mat& frameMorph, const Mat& kernel,
                           vector<vector<Point>>& contours, int& largestIndex)
	{
	// Convert to HSV and extract Value channel
	cvtColor(frame, hsv, COLOR_BGR2HSV);
	Mat channels[3];
	split(hsv, channels);
	Mat& frameGray = channels[2];

	if (cfg.verbose && !cfg.noGui) imshow("FrameGray", frameGray);

	// Calculate Otsu threshold for Canny edge detection
	double otsuThresh = threshold(frameGray, edges, 0, 255, THRESH_BINARY | THRESH_OTSU);
	if (cfg.verbose && !cfg.noGui) imshow("Otsu", edges);

	// Edge detection
	Canny(frameGray, edges, otsuThresh * cfg.thresholdScalar, otsuThresh * cfg.thresholdScalar * 0.5);
	if (cfg.verbose && !cfg.noGui) imshow("Edges", edges);

	// Dilate to close edges
	dilate(edges, frameMorph, kernel, Point(-1, -1), cfg.iterations);
	if (cfg.verbose && !cfg.noGui) imshow("Dilation", frameMorph);

	// Find contours
	vector<Vec4i> hierarchy;
	findContours(frameMorph, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);

	// Default to frame center
	Point centroid(frame.cols / 2, frame.rows / 2);
	largestIndex = -1;

	if (contours.empty()) return centroid;

	// Find largest contour
	double largestArea = 0;
	for (size_t i = 0; i < contours.size(); ++i)
		{
		double area = contourArea(contours[i]);
		if (area > largestArea)
			{ largestArea = area; largestIndex = static_cast<int>(i); }
		}

	// Calculate centroid from moments
	if (largestIndex >= 0)
		{
		Moments mu = moments(contours[largestIndex]);
		if (mu.m00 != 0.0)
			centroid = Point(static_cast<int>(mu.m10 / mu.m00), static_cast<int>(mu.m01 / mu.m00));
		}

	return centroid;
	}

// Draw debug visualization on frame
void drawDebugOverlay(Mat& frame, const vector<vector<Point>>& contours, int largestIndex,
                      Point centroid, bool verbose)
	{
	if (verbose)
		for (size_t i = 0; i < contours.size(); ++i)
			drawContours(frame, contours, static_cast<int>(i), Scalar(0, 128, 0), 1);

	if (largestIndex >= 0)
		{
		drawContours(frame, contours, largestIndex, Scalar(0, 255, 0), 2);
		circle(frame, centroid, 9, Scalar(0, 0, 255), 3);
		}
	}

bool processVideo(const string& filename, const Config& cfg)
	{
	VideoCapture cap(filename);
	if (!cap.isOpened())
		{ cerr << "Cannot open video file: " << filename << endl; return false; }

	int totalFrames = static_cast<int>(cap.get(CAP_PROP_FRAME_COUNT));
	cout << "Processing: " << filename << " (" << totalFrames << " frames)" << endl;

	string outputFile = filename.substr(0, filename.find_last_of('.')) + "_mcrop." + cfg.outputExt;
	VideoWriter output(outputFile, cfg.codec,
	                   cap.get(CAP_PROP_FPS), Size(cfg.windowSize, cfg.windowSize));
	if (!output.isOpened())
		{ cerr << "Cannot open output file: " << outputFile << endl; return false; }

	Mat frame, buffer, hsv, edges, frameMorph;
	Mat kernel = getStructuringElement(MORPH_RECT, Size(5, 5));
	vector<vector<Point>> contours;

	int frameNum = 0;
	int lastPercent = -1;

	while (cap.read(frame))
		{
		++frameNum;
		int percent = (totalFrames > 0) ? (frameNum * 100 / totalFrames) : 0;
		if (percent != lastPercent)
			{
			cout << "\r  " << percent << "% (" << frameNum << "/" << totalFrames << ")   " << flush;
			lastPercent = percent;
			}

		// Pad frame to handle edge cropping
		copyMakeBorder(frame, buffer, cfg.windowSize, cfg.windowSize,
		               cfg.windowSize, cfg.windowSize, BORDER_REPLICATE);

		// Find aircraft centroid
		int largestIndex;
		Point centroid = findAircraftCentroid(frame, cfg, hsv, edges, frameMorph, kernel,
		                                      contours, largestIndex);

		// Draw debug overlay
		drawDebugOverlay(frame, contours, largestIndex, centroid, cfg.verbose);

		// Crop around centroid
		Rect cropRect(cfg.windowSize + centroid.x - cfg.windowSize/2,
		              cfg.windowSize + centroid.y - cfg.windowSize/2,
		              cfg.windowSize, cfg.windowSize);
		Mat roi = buffer(cropRect);

		output.write(roi);

		if (!cfg.noGui)
			{
			imshow("Original", frame);
			imshow("Motion Crop", roi);
			if (waitKey(30) == 27) break;
			}
		}

	cout << "\r  100% (" << frameNum << "/" << totalFrames << ") -> " << outputFile << endl;
	return true;
	}

int main(int argc, char** argv)
	{
	Config cfg;
	vector<string> filenames;
	if (!parseArgs(argc, argv, cfg, filenames)) return -1;

	cout << "MotionCrop by Jim Bourke.\n"
		<< "Window Size: " << cfg.windowSize << "x" << cfg.windowSize << endl;

	if (!cfg.noGui)
		{
		namedWindow("Motion Crop", WINDOW_AUTOSIZE);
		namedWindow("Original", WINDOW_AUTOSIZE);
		}

	int succeeded = 0;
	int failed = 0;

	for (const auto& filename : filenames)
		{
		if (processVideo(filename, cfg))
			++succeeded;
		else
			++failed;
		}

	if (filenames.size() > 1)
		cout << "\nCompleted: " << succeeded << " succeeded, " << failed << " failed." << endl;

	return (failed > 0) ? 1 : 0;
	}
