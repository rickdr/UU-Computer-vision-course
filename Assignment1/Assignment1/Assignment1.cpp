// opencv-1.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>
#include <fstream>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include "Settings.cpp"

using namespace cv;
using namespace std;


int main(int argc, char** argv)
{
    //// Read the settings from default.xml
    Settings s;
    CommandLineParser parser = s.initParser(argc, argv);
    const string inputSettingsFile = parser.get<string>(0);
    FileStorage fs(inputSettingsFile, FileStorage::READ);
    if (!fs.isOpened())
    {
        std::cout << "Could not open the configuration file: \"" << inputSettingsFile << "\"" << endl;
        parser.printMessage();
        return -1;
    }
    fs["Settings"] >> s;
    fs.release();

    // Initialization of variables
    Mat image;
    Mat gray_image;
    int image_amount;

    Size board_size = Size(s.boardSize);

    vector<vector<Point3f>> overall_object_points;
    vector<vector<Point2f>> overall_image_points;
    Size overall_image_size;
    Mat overall_dist_coeffs;
    Mat overall_intrinsic = Mat(3, 3, CV_32FC1);
    vector<Mat> overall_rvecs;
    vector<Mat> overall_tvecs;

    vector<float> overall_errors;

    overall_intrinsic.ptr<float>(0)[0] = 1;
    overall_intrinsic.ptr<float>(1)[1] = 1;

    bool done = false;
    double best_rms = 10;
    int loop = 0;

    // While loop: iterates over all images leaving out image (1 + iteration number)
    while (done == false and loop < s.imageList.size())
    {
        std::cout << "Image set:" << loop << "\n";

        // Initialize lists of lists for image and object points
        vector<vector<Point3f>> object_points;
        vector<vector<Point2f>> image_points;
        vector<Point3f> objects;
        Size image_size;

        // Create the points on the board in 3d space, with the origin on the corner of square (0,0)
        for (int i = 0; i < board_size.height; i++)
            for (int j = 0; j < board_size.width; j++)
                objects.push_back(Point3f((float)j * s.squareSize, (float)i * s.squareSize, 0));

        // Get the image list from the settings file, and leave one out every iteration
        vector<String> images = s.imageList;
        images.erase(images.begin() + loop);
        for (int image_amount = 0; image_amount < images.size(); image_amount++)
        {
            //std::cout << "\r" << "Processing image: " << image_amount;
            image = imread(images[image_amount], IMREAD_COLOR);
            image_size = image.size();

            vector<Point2f> image_corners;
            cvtColor(image, gray_image, COLOR_BGR2GRAY);
            bool found_chessboard = findChessboardCorners(gray_image, board_size, image_corners);

            if (found_chessboard)
            {
                cornerSubPix(gray_image, image_corners, Size(11, 11), Size(-1, -1), TermCriteria(TermCriteria::EPS + TermCriteria::MAX_ITER, 30, 0.1));
                drawChessboardCorners(image, board_size, Mat(image_corners), found_chessboard);

                image_points.push_back(image_corners);
                object_points.push_back(objects);
            }
            //namedWindow("Image display");
            //imshow("window", image);
            //waitKey(0);

            if (waitKey(10) == 27)
            {
                break;
            }
        }

        Mat intrinsic = overall_intrinsic.clone();
        Mat dist_coeffs = overall_dist_coeffs.clone();
        vector<Mat> rvecs;
        vector<Mat> tvecs;

        // Run calibration with the parameters found in the current set and update high scores accordingly.
        double rms = calibrateCamera(object_points, image_points, image_size, intrinsic, dist_coeffs, rvecs, tvecs);
        if (rms < best_rms)
        {
            overall_object_points = object_points;
            overall_image_points = image_points;
            overall_image_size = image_size;
            overall_intrinsic = intrinsic;
            overall_dist_coeffs = dist_coeffs;
            overall_rvecs = rvecs;
            overall_tvecs = tvecs;
            best_rms = rms;
        }

        std::cout << "\r" << "Root mean square: " << rms << "\n";
        loop++;
    }
    std::cout << "Calibration completed." << "\n";
    std::cout << "Best rms: " << best_rms << "\n";
    std::cout << "Final intrinsic: " << overall_intrinsic << "\n";
    std::cout << "Final dist coeffs: " << overall_dist_coeffs << "\n";

    //// Video Capture

    VideoCapture camera(1);
    if (!camera.isOpened()) {
        cerr << "ERROR: Could not open camera" << endl;
        return 1;
    }

    // Create windows to display the images from the webcam before and after modification
    cv::namedWindow("Webcam", WINDOW_AUTOSIZE);
    cv::namedWindow("Undistorted", WINDOW_AUTOSIZE);

    // capture frame by frame from the webcam
    Mat frame, intrinsic, dist_coeffs, rvecs, tvecs, frame_undistorted;
    bool running = true;
    camera >> frame;
    while (running) {
        vector<vector<Point3f>> object_points;
        vector<Point3f> objects;
        vector<Point3f> cube_points_background;
        vector<Point3f> cube_points;

        vector<Point2f> frame_points;
        vector<Point2f> frame_points_background;
        vector<Point2f> frame_corners;

        undistort(frame, frame_undistorted, overall_intrinsic, overall_dist_coeffs);
        cvtColor(frame_undistorted, gray_image, COLOR_BGR2GRAY);

        // Create the points on the board in 3d space, with the origin on the corner of square (0,0)
        for (int i = 0; i < board_size.height; i++)
            for (int j = 0; j < board_size.width; j++)
                objects.push_back(Point3f((float)j * s.squareSize, (float)i * s.squareSize, 0));

        // Create the cube coordinates
        for (int i = 0; i < 3; i++)
        {
            for (int j = 0; j < 3; j++) {
                cube_points_background.push_back(Point3f((float)j * s.squareSize, (float)i * s.squareSize, 0));
                cube_points.push_back(Point3f((float)j * s.squareSize, (float)i * s.squareSize, (-(s.squareSize*2))));
            }
        }

        bool found_chessboard = findChessboardCorners(gray_image, board_size, frame_corners);
        if (found_chessboard)
        {
            solvePnP(objects, Mat(frame_corners), overall_intrinsic, dist_coeffs, rvecs, tvecs);
            projectPoints(cube_points_background, rvecs, tvecs, overall_intrinsic, dist_coeffs, frame_points_background);
            projectPoints(cube_points, rvecs, tvecs, overall_intrinsic, dist_coeffs, frame_points);

            // possible line colors
            vector<Scalar> colors = { Scalar(255,0,0), Scalar(0,255,0), Scalar(0,0,255) };

            // indexes of start and end points of lines to draw
            vector<tuple<int, int>> draw_points = { make_tuple(0,2), make_tuple(2,8), make_tuple(6,0), make_tuple(8,6) };

            for (size_t i = 0; i < draw_points.size(); i++)
            {
                Scalar color = colors[rand() % colors.size()];
                Scalar color_background = colors[rand() % colors.size()];
                Scalar color_dimension = colors[rand() % colors.size()];

                line(frame_undistorted, frame_points[get<0>(draw_points[i])], frame_points_background[get<0>(draw_points[i])], color, 3);
                line(frame_undistorted, frame_points_background[get<0>(draw_points[i])], frame_points_background[get<1>(draw_points[i])], color_background, 3);
                line(frame_undistorted, frame_points[get<0>(draw_points[i])], frame_points[get<1>(draw_points[i])], color_dimension, 3);
            }
        }

        imshow("Webcam", frame);
        imshow("Undistorted", frame_undistorted);

        camera >> frame;

        if (waitKey(10) == 27)
        {
            destroyWindow("Webcam");
            destroyWindow("Undistorted");
            running = false;
        }
    }
    camera.release();

    return 0;
}