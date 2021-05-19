/*
 * Copyright (C) 2020  Christian Berger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// Include the single-file, header-only middleware libcluon to create high-performance microservices
#include "cluon-complete.hpp"
// Include the OpenDLV Standard Message Set that contains messages that are usually exchanged for automotive or robotic applications
#include "opendlv-standard-message-set.hpp"

// Include the GUI and image processing header files from OpenCV
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/videoio.hpp"

#include <ctime>
#include <iostream>
#include <fstream>

using namespace cv;
using namespace std;

typedef struct
{
    double x;
    double y;
} COORDINATE;

const int max_value_H = 360 / 2;
const int max_value = 255;
const String window_capture_name = "Image Capture";
const String window_detection_name = "Object Detection";

// Blue
int blue_low_H = 108, blue_low_S = 95, blue_low_V = 50;
int blue_high_H = 146, blue_high_S = 178, blue_high_V = 88;

// Yellow
int yellow_low_H = 0, yellow_low_S = 83, yellow_low_V = 112;
int yellow_high_H = 98, yellow_high_S = 193, yellow_high_V = 225;

// Default
int low_H = 0, low_S = 0, low_V = 0;
int high_H = max_value_H, high_S = max_value, high_V = max_value;

inline int64_t toMicroseconds(const cluon::data::TimeStamp &tp) noexcept
{
    return static_cast<int64_t>(tp.seconds()) * static_cast<int64_t>(1000 * 1000) + static_cast<int64_t>(tp.microseconds());
}

COORDINATE *meanCoOrdinate(Mat colour_threshold)
{
    COORDINATE *pos;
    pos = (COORDINATE *)malloc(sizeof(COORDINATE));
    pos->x = 0;
    pos->y = 0;

    //Calculate the moments of the thresholded image
    Moments oMoments = moments(colour_threshold);

    double dM01 = oMoments.m01;
    double dM10 = oMoments.m10;
    double dArea = oMoments.m00;

    // if the area <= 1000, I consider that the there are no object in the image and it's because of the noise, the area is not zero
    if (dArea > 100)
    {
        //calculate the position of the cones
        pos->x = dM10 / dArea;
        pos->y = dM01 / dArea;
    }

    return pos;
}

int32_t main(int32_t argc, char **argv)
{

    int32_t retCode{1};
    // Parse the command line parameters as we require the user to specify some mandatory information on startup.
    auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
    if ((0 == commandlineArguments.count("cid")) ||
        (0 == commandlineArguments.count("name")) ||
        (0 == commandlineArguments.count("width")) ||
        (0 == commandlineArguments.count("height")))
    {
        std::cerr << argv[0] << " attaches to a shared memory area containing an ARGB image." << std::endl;
        std::cerr << "Usage:   " << argv[0] << " --cid=<OD4 session> --name=<name of shared memory area> [--verbose]" << std::endl;
        std::cerr << "         --cid:    CID of the OD4Session to send and receive messages" << std::endl;
        std::cerr << "         --name:   name of the shared memory area to attach" << std::endl;
        std::cerr << "         --width:  width of the frame" << std::endl;
        std::cerr << "         --height: height of the frame" << std::endl;
        std::cerr << "Example: " << argv[0] << " --cid=253 --name=img --width=640 --height=480 --verbose" << std::endl;
    }
    else
    {
        // Extract the values from the command line parameters
        const std::string NAME{commandlineArguments["name"]};
        const uint32_t WIDTH{static_cast<uint32_t>(std::stoi(commandlineArguments["width"]))};
        const uint32_t HEIGHT{static_cast<uint32_t>(std::stoi(commandlineArguments["height"]))};
        const bool VERBOSE{commandlineArguments.count("verbose") != 0};

        // Attach to the shared memory.
        std::unique_ptr<cluon::SharedMemory> sharedMemory{new cluon::SharedMemory{NAME}};
        if (sharedMemory && sharedMemory->valid())
        {
            std::clog << argv[0] << ": Attached to shared memory '" << sharedMemory->name() << " (" << sharedMemory->size() << " bytes)." << std::endl;

            // Interface to a running OpenDaVINCI session where network messages are exchanged.
            // The instance od4 allows you to send and receive messages.
            cluon::OD4Session od4{static_cast<uint16_t>(std::stoi(commandlineArguments["cid"]))};

            opendlv::proxy::GroundSteeringRequest gsr;
            std::mutex gsrMutex;
            auto onGroundSteeringRequest = [&gsr, &gsrMutex](cluon::data::Envelope &&env) {
                // The envelope data structure provide further details, such as sampleTimePoint as shown in this test case:
                // https://github.com/chrberger/libcluon/blob/master/libcluon/testsuites/TestEnvelopeConverter.cpp#L31-L40
                std::lock_guard<std::mutex> lck(gsrMutex);
                gsr = cluon::extractMessage<opendlv::proxy::GroundSteeringRequest>(std::move(env));
                std::cout << "lambda: groundSteering = " << gsr.groundSteering() << std::endl;
            };

            od4.dataTrigger(opendlv::proxy::GroundSteeringRequest::ID(), onGroundSteeringRequest);

            // creating a file to store pixels count
            std::string info_file_name = "info.csv";
            std::ofstream info_file;
            info_file.open(info_file_name, ios::app);
            info_file << "time_stamp"
                      << ","
                      << "actual_ground_steering"
                      << ","
                      << "calculated_turn"
                      << ","
                      << "blue_pixels_count"
                      << ","
                      << "yellow_pixels_count"
                      << ","
                      << "blue_x"
                      << ","
                      << "blue_y"
                      << ","
                      << "yellow_x"
                      << ","
                      << "yellow_y"
                      << "\n";
            info_file.close();

            // Variables for steering values (default is left_blue)
            double steering_verdict = 0;

            double straight = -0.049;

            double left = 0.11;
            double hard_left = 0.14545;

            double right = -0.11;
            double hard_right = -0.14545;

            // Variable to print the correct turn
            std::string correct_turn;

            // variables for verdict desicion
            int is_final = 0;

            // Variables for the number of detected cones
            int yellow_cones_detected = 0;
            int blue_cones_detected = 0;

            // Sync values for cone placement
            int total_sync_frames = 5;
            int sync_frames = total_sync_frames;

            // Counts the cone placement changes detected
            int disagree_frames = 0;

            // Cone set-ups
            int left_yellow = -1;
            int left_blue = 1;

            int cone_placement_verdict = 0;

            // Keep it wherever it makes sense
            int offset_x = 30;
            int offset_y = 267;

            int cross_size = 60;
            int white_cross_colour = 255;

            double mean_yellow_x = -1;
            double mean_yellow_y = -1;
            double mean_blue_x = -1;
            double mean_blue_y = -1;

            // creating variables to store pixels count
            int blue_pixels;
            int yellow_pixels;

            int onScreenDebugInfoLineCount = 9;

            // Endless loop; end the program by pressing Ctrl-C.
            while (od4.isRunning())
            {
                // truning desicion
                is_final = 0;
                steering_verdict = straight;

                // OpenCV data structure to hold an image.
                cv::Mat img;

                // Wait for a notification of a new frame.
                sharedMemory->wait();

                // Lock the shared memory.
                sharedMemory->lock();
                {
                    // Copy the pixels from the shared memory into our own data structure.
                    cv::Mat wrapped(HEIGHT, WIDTH, CV_8UC4, sharedMemory->data());
                    img = wrapped.clone();
                }

                // time stamp
                int time_stamp = toMicroseconds(sharedMemory->getTimeStamp().second);

                // If you want to access the latest received ground steering, don't forget to lock the mutex:
                double actual_ground_steering = 0;
                {
                    std::lock_guard<std::mutex> lck(gsrMutex);
                    actual_ground_steering = gsr.groundSteering();
                }

                sharedMemory->unlock();

                cv::Rect roi;
                roi.x = offset_x;
                roi.y = offset_y;
                roi.width = img.size().width - (offset_x);
                roi.height = img.size().height - (offset_y);

                cv::Mat crop = img(roi);

                // Drawing a circle over the car's cables
                cv::circle(crop, cv::Point(crop.cols / 2, ((crop.rows / 3) * 2) + 60), 100, cv::Scalar(0, 0, 0), CV_FILLED, 8, 0);

                // creating variables to store pixels count
                blue_pixels = -1;
                yellow_pixels = -1;
                // int pixles_avarage;

                // TODO: Do something with the frame.
                // Example: Draw a red rectangle and display image.
                Mat frame_HSV, yellow_threshold, blue_threshold;

                // Convert from BGR to HSV colourspace
                cv::cvtColor(crop, frame_HSV, COLOR_BGR2HSV);

                // Detect the object based on HSV Range Values
                // Yellow
                cv::inRange(frame_HSV, Scalar(yellow_low_H, yellow_low_S, yellow_low_V), Scalar(yellow_high_H, yellow_high_S, yellow_high_V), yellow_threshold);

                //morphological closing (removes small holes from the foreground)
                dilate(yellow_threshold, yellow_threshold, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
                erode(yellow_threshold, yellow_threshold, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

                // Blue
                cv::inRange(frame_HSV, Scalar(blue_low_H, blue_low_S, blue_low_V), Scalar(blue_high_H, blue_high_S, blue_high_V), blue_threshold);

                //morphological closing (removes small holes from the foreground)
                dilate(blue_threshold, blue_threshold, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
                erode(blue_threshold, blue_threshold, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

                // Counting yellow pixels
                yellow_pixels = cv::countNonZero(yellow_threshold);

                // Counting blue pixels
                blue_pixels = cv::countNonZero(blue_threshold);

                yellow_cones_detected = yellow_pixels > 100 ? 1 : 0;
                blue_cones_detected = blue_pixels > 100 ? 1 : 0;

                mean_yellow_x = -1;
                mean_yellow_y = -1;
                mean_blue_x = -1;
                mean_blue_y = -1;

                if (yellow_cones_detected)
                {
                    COORDINATE *mean_yellow = meanCoOrdinate(yellow_threshold);
                    mean_yellow_x = mean_yellow->x;
                    mean_yellow_y = mean_yellow->y;
                    free(mean_yellow);
                }
                else if (blue_cones_detected)
                {
                    // by default blue cones are on the left
                    steering_verdict = hard_right;
                    is_final = 1;
                }
                if (blue_cones_detected)
                {
                    COORDINATE *mean_blue = meanCoOrdinate(blue_threshold);
                    mean_blue_x = mean_blue->x;
                    mean_blue_y = mean_blue->y;
                    free(mean_blue);
                }
                else if (yellow_cones_detected && !is_final)
                {
                    // by default blue cones are on the left
                    steering_verdict = hard_left;
                    is_final = 1;
                }

                // Sync_frames = 6
                // cone verdict = blue (-1)
                // temp = blue (-1)

                // Sync frames = 5
                // cone verdict = blue (-1)
                // temp = yellow (1)

                // Sync frames = 4
                // cone verdict = blue (-1)
                // temp = blue (-1)

                if (sync_frames > 0)
                {
                    // If we can see cones of both color
                    if (yellow_cones_detected && blue_cones_detected)
                    {
                        // TODO: Put this down?
                        --sync_frames;
                        int temp_verdict = 0;

                        // What is left/right in current frame
                        if (mean_blue_x < mean_yellow_x)
                        {
                            temp_verdict = left_blue;
                        }
                        else
                        {
                            temp_verdict = left_yellow;
                        }
                        if (cone_placement_verdict == 0)
                        {
                            cone_placement_verdict = temp_verdict;
                        }
                        else
                        {
                            if (temp_verdict != cone_placement_verdict)
                            {
                                ++disagree_frames;
                                // TODO: reset the cone verdict
                                // cone_placement_verdict == temp_verdict;
                            }
                        }
                    }
                }

                // Final descision
                // 6 = original sync frames
                else if (sync_frames == 0)
                {
                    --sync_frames;
                    if (disagree_frames >= total_sync_frames / 2)
                    {
                        cone_placement_verdict = -cone_placement_verdict;
                    }
                    if (cone_placement_verdict == left_yellow)
                    {
                        // each of the turns = cone_placement_verdict * turn
                        left = left * cone_placement_verdict;
                        right = right * cone_placement_verdict;
                        hard_left = hard_left * cone_placement_verdict;
                        hard_right = hard_right * cone_placement_verdict;
                        straight = straight * cone_placement_verdict;
                    }
                }

                // Display image on your screen.
                if (VERBOSE)
                {

                    // Generate strings to display the number of each cone colour pixels on the screen
                    std::string yellow_pixels_count = std::to_string(yellow_pixels);
                    std::string blue_pixels_count = std::to_string(blue_pixels);

                    // This can be replaced by cv::Scalar(255, 255, 255) for use in BGR

                    if (yellow_cones_detected)
                    {

                        cv::drawMarker(yellow_threshold, cv::Point(mean_yellow_x, mean_yellow_y), white_cross_colour, MARKER_CROSS, cross_size, 1);
                        cv::drawMarker(crop, cv::Point(mean_yellow_x, mean_yellow_y), cv::Scalar(0, 255, 255), MARKER_CROSS, cross_size, 1);
                    }
                    if (blue_cones_detected)
                    {

                        cv::drawMarker(blue_threshold, cv::Point(mean_blue_x, mean_blue_y), white_cross_colour, MARKER_CROSS, cross_size, 1);
                        cv::drawMarker(crop, cv::Point(mean_blue_x, mean_blue_y), cv::Scalar(255, 0, 0), MARKER_CROSS, cross_size, 1);
                    }

                    // Print debug info on screen

                    /*    std::string displayText[onScreenDebugInfoLineCount] = {
                            "YellowPixels: " + yellow_pixels_count,                  //1
                            "BluePixels: " + blue_pixels_count,                      //2
                            "Verdict: straight",                                     //3
                            "ActualTurn: " + std::to_string(actual_ground_steering), //4
                        }; */

                    // calculating if steering_verdict is accurate
                    correct_turn = "false";
                    if (actual_ground_steering == -0)
                    {
                        actual_ground_steering = 0;
                    }

                    if ((steering_verdict <= actual_ground_steering && steering_verdict > actual_ground_steering / 2) || (steering_verdict >= actual_ground_steering && steering_verdict < actual_ground_steering * 2) || ((actual_ground_steering == 0 && steering_verdict == 0.049) || (actual_ground_steering == 0 && steering_verdict == -0.049)))
                    {
                        correct_turn = "true";
                    }

                    // Print debug info for sync

                    std::string displaySyncText[onScreenDebugInfoLineCount] = {
                        "YellowPixels: " + yellow_pixels_count,                                             //1
                        "BluePixels: " + blue_pixels_count,                                                 //2
                        "Verdict: " + std::to_string(steering_verdict),                                     //3
                        "ActualTurn: " + std::to_string(actual_ground_steering),                            //4
                        "CorrectTurn: " + correct_turn,                                                     //5
                        cone_placement_verdict < 0 ? "ConeColourOnLeft: Yellow" : "ConeColourOnLeft: Blue", //6
                        "Left: " + std::to_string(left),                                                    //7
                        "HardLeft: " + std::to_string(hard_left),                                           //8
                        "Right: " + std::to_string(right),                                                  //9
                        "HardRight: " + std::to_string(hard_right),                                         //10
                    };

                    // std::string yellowPixelCountS = "YellowPixels: " + yellow_pixels_count;           //1
                    // std::string bluePixelCountS = "BluePixels: " + blue_pixels_count;                 //2
                    // std::string verdictS = "Verdict: straight";                                       //3
                    // std::string actualTurn = "ActualTurn: " + std::to_string(actual_ground_steering); //4

                    // std::string displayText[onScreenDebugInfoLineCount] = {
                    //     yellowPixelCountS,
                    //     bluePixelCountS,
                    //     verdictS,
                    //     actualTurn,
                    // };

                    int y = 20;
                    int dy = 15;
                    /* for (int i = 0; i < onScreenDebugInfoLineCount; i++)
                        {
                            cv::putText(crop, displayText[i], cv::Point(30, y), 1, 1, cv::Scalar(200, 200, 200), 1, 1, false);
                            y += dy;
                        } */

                    for (int i = 0; i < onScreenDebugInfoLineCount; i++)
                    {
                        cv::putText(crop, displaySyncText[i], cv::Point(30, y), 1, 1, cv::Scalar(200, 200, 200), 1, 1, false);
                        y += dy;
                    }

                    // Displaying pixels number on the screen
                    cv::putText(yellow_threshold, yellow_pixels_count, cv::Point(30, 20), 1, 1, 127, 1, 1, false);
                    cv::putText(blue_threshold, blue_pixels_count, cv::Point(30, 20), 1, 1, 127, 1, 1, false);
                    // END print debug info on screen

                    // creating a file to store pixels count
                    // std::ofstream info_file;
                    info_file.open(info_file_name, ios::app);
                    info_file << time_stamp << "," << actual_ground_steering << "," << steering_verdict << "," << blue_pixels_count << "," << yellow_pixels_count << "," << mean_blue_x << "," << mean_blue_y << "," << mean_yellow_x << "," << mean_yellow_y << "\n";
                    info_file.close();

                    cv::imshow("Yellow Cones", yellow_threshold);
                    cv::imshow("Blue Cones", blue_threshold);
                    cv::imshow("Default Cones", crop);
                    cv::waitKey(1);
                }
            }
        }
        retCode = 0;
    }
    return retCode;
}
