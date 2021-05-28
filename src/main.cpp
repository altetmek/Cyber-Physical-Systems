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
const int min_pixels = 55;

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
    if (dArea > min_pixels)
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
                // std::cout << "lambda: groundSteering = " << gsr.groundSteering() << std::endl;
            };

            od4.dataTrigger(opendlv::proxy::GroundSteeringRequest::ID(), onGroundSteeringRequest);

            std::ofstream info_file;
            std::string info_file_name = "info.csv";

            // creating a file to store pixels count (if verbose)
            if (VERBOSE)
            {
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
            }

            // variables for verdict descision
            int is_final = 0;

            // Variables in which decoded images can be stored
            Mat frame_HSV, yellow_threshold, blue_threshold;

            // Variables for the number of detected conne pixels
            int yellow_cones_detected = 0;
            int blue_cones_detected = 0;

            // Sync values for cone placement (left/right cone colour)
            int total_sync_frames = 5;
            int sync_frames = total_sync_frames;

            // Counts the number of syncronisation frames where the colours are on the opposite sides.
            int disagree_frames = 0;

            // Variables for calculating steering magnitudes (default is left_blue)
            int cone_placement_verdict = 0;
            double steering_verdict = 0;

            double straight = -0.049;

            double left = 0.11;
            double hard_left = 0.14545;

            double right = -0.11;
            double hard_right = -0.14545;

            // Cone set-ups
            int left_yellow = -1;
            int left_blue = 1;

            // Values for cropping of image
            int offset_x = 30;
            int offset_y = 267;

            int cross_size = 60;
            int white_cross_colour = 255;

            // To keep track of mean location of each cone colour
            double mean_yellow_x = -1;
            double mean_yellow_y = -1;
            double mean_blue_x = -1;
            double mean_blue_y = -1;

            // creating variables to store pixel counts
            int blue_pixels;
            int yellow_pixels;

            // ------------------------------------------------------
            // Debug variables
            // ------------------------------------------------------

            // Variable to log debug info to the screen
            std::string correct_turn_string;

            // Each frame's actual turn magnitude will be available here
            double actual_ground_steering = 0;

            // If our calculated maginitude is acceptably accurate will be stored here
            int correct_turn = 0;

            // How many lines of debug info do we want to put on screen
            int onScreenDebugInfoLineCount = 7;

            // Variables to calculate our algorithm's accuracy
            int total_frames = 0;
            int correct_frames = 0;

            // ------------------------------------------------------
            // Debug variables
            // ------------------------------------------------------

            // Endless loop; end the program by pressing Ctrl-C.
            while (od4.isRunning())
            {
                // Turning decsion initialisation
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
                long int time_stamp = toMicroseconds(sharedMemory->getTimeStamp().second);

                // If you want to access the latest received ground steering, don't forget to lock the mutex:
                actual_ground_steering = 0;
                {
                    std::lock_guard<std::mutex> lck(gsrMutex);
                    actual_ground_steering = gsr.groundSteering();
                }

                sharedMemory->unlock();

                // Crop the image to ignore the 'sky'
                cv::Rect roi;
                roi.x = offset_x;
                roi.y = offset_y;
                roi.width = img.size().width - (offset_x);
                roi.height = img.size().height - (offset_y);

                cv::Mat crop = img(roi);

                // Drawing a black circle over the car's cables (So that we do not see them)
                cv::circle(crop, cv::Point(crop.cols / 2, ((crop.rows / 3) * 2) + 60), 100, cv::Scalar(0, 0, 0), CV_FILLED, 8, 0);

                // Re-initialise variables to store pixels count
                blue_pixels = -1;
                yellow_pixels = -1;

                // Convert from BGR to HSV colourspace
                cv::cvtColor(crop, frame_HSV, COLOR_BGR2HSV);

                // Detect the cones based on HSV Range Values
                // Yellow
                cv::inRange(frame_HSV, Scalar(yellow_low_H, yellow_low_S, yellow_low_V), Scalar(yellow_high_H, yellow_high_S, yellow_high_V), yellow_threshold);

                // Blue
                cv::inRange(frame_HSV, Scalar(blue_low_H, blue_low_S, blue_low_V), Scalar(blue_high_H, blue_high_S, blue_high_V), blue_threshold);

                //morphological closing (removes small holes from the foreground)
                dilate(yellow_threshold, yellow_threshold, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
                erode(yellow_threshold, yellow_threshold, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

                //morphological closing (removes small holes from the foreground)
                dilate(blue_threshold, blue_threshold, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
                erode(blue_threshold, blue_threshold, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

                // Counting yellow pixels
                yellow_pixels = cv::countNonZero(yellow_threshold);

                // Counting blue pixels
                blue_pixels = cv::countNonZero(blue_threshold);

                // Determine if we have seen enough of each colour to consider having identified atleast one cone
                yellow_cones_detected = yellow_pixels > min_pixels ? 1 : 0;
                blue_cones_detected = blue_pixels > min_pixels ? 1 : 0;

                // Re-initialise each cone co-ordinate
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
                    // We can see only blue cones, so we must turn away from them
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
                    // We can see yellow blue cones, so we must turn away from them
                    // by default blue cones are on the left
                    steering_verdict = hard_left;
                    is_final = 1;
                }

                // Over the first total_sync_frames frames, we determine which cones are on which side
                if (sync_frames > 0)
                {
                    // If we can see cones of both color, otherwise this frame can not be used
                    if (yellow_cones_detected && blue_cones_detected)
                    {
                        // Reduce the remaining sync_frames
                        --sync_frames;
                        int temp_verdict = 0;

                        // Which cones are on the left/right in current frame
                        if (mean_blue_x < mean_yellow_x)
                        {
                            temp_verdict = left_blue;
                        }
                        else
                        {
                            temp_verdict = left_yellow;
                        }

                        // If this is the first syncronisation frame
                        if (cone_placement_verdict == 0)
                        {
                            // Assume this frame's left/right is correct
                            cone_placement_verdict = temp_verdict;
                        }

                        else
                        {
                            // If this frame's verdict disagrees with the main verdict, we count it for a later vote
                            if (temp_verdict != cone_placement_verdict)
                            {
                                ++disagree_frames;
                            }
                        }
                    }
                }

                // Final descision, once we have no remaining sync frames
                else if (sync_frames == 0)
                {
                    --sync_frames;

                    // If the majority of the the sync frames disagree with the initial descision, we change our descision
                    if (disagree_frames >= total_sync_frames / 2)
                    {
                        cone_placement_verdict = -cone_placement_verdict;
                    }

                    // Since we assume that the left ones are blue, if they are yellow, we need to update our values accordingly
                    if (cone_placement_verdict == left_yellow)
                    {
                        // Invert all of these values (left/right are wrong)
                        left = left * cone_placement_verdict;
                        right = right * cone_placement_verdict;
                        hard_left = hard_left * cone_placement_verdict;
                        hard_right = hard_right * cone_placement_verdict;
                        straight = straight * cone_placement_verdict;
                    }
                }

                // Log the turn descision to console
                std::cout << "group_17;" << time_stamp << ";" << steering_verdict << std::endl;

                // Display debugging information if the VERBOSE flag was given
                if (VERBOSE)
                {
                    // Count the total number of frames, for calculating the total accuracy
                    ++total_frames;
                    correct_turn_string = "false";
                    correct_turn = 0;

                    // Generate strings to display the number of each cone colour pixels on the screen
                    std::string yellow_pixels_count = std::to_string(yellow_pixels);
                    std::string blue_pixels_count = std::to_string(blue_pixels);

                    // If yellow cones were detected, mark the median position on the image
                    if (yellow_cones_detected)
                    {
                        // Warnings for conversion to 'int' from 'double' considered, is of no consequence here
                        cv::drawMarker(yellow_threshold, cv::Point(mean_yellow_x, mean_yellow_y), white_cross_colour, MARKER_CROSS, cross_size, 1);
                        cv::drawMarker(crop, cv::Point(mean_yellow_x, mean_yellow_y), cv::Scalar(0, 255, 255), MARKER_CROSS, cross_size, 1);
                    }

                    // If blue cones were detected, mark the median position on the image
                    if (blue_cones_detected)
                    {
                        // Warnings for conversion to 'int' from 'double' considered, is of no consequence here
                        cv::drawMarker(blue_threshold, cv::Point(mean_blue_x, mean_blue_y), white_cross_colour, MARKER_CROSS, cross_size, 1);
                        cv::drawMarker(crop, cv::Point(mean_blue_x, mean_blue_y), cv::Scalar(255, 0, 0), MARKER_CROSS, cross_size, 1);
                    }

                    // ------------------------------------------------------
                    // Determine if we have computed a valid turn angle
                    if (actual_ground_steering == 0)
                    {
                        if ((steering_verdict == straight))
                        {
                            correct_turn = 1;
                        }
                    }
                    else if (actual_ground_steering > 0)
                    {
                        if ((steering_verdict <= actual_ground_steering && steering_verdict > actual_ground_steering / 2) || (steering_verdict >= actual_ground_steering && steering_verdict < actual_ground_steering * 1.5))
                        {
                            correct_turn = 1;
                        }
                    }
                    else
                    {
                        if ((steering_verdict >= actual_ground_steering && steering_verdict < actual_ground_steering / 2) || (steering_verdict <= actual_ground_steering && steering_verdict > actual_ground_steering * 1.5))
                        {
                            correct_turn = 1;
                        }
                    }

                    if (correct_turn)
                    {
                        correct_turn_string = "das RITE DAS RIITTTTTEEE";
                        ++correct_frames;
                    }
                    // ------------------------------------------------------

                    // Assemble debug info to print to screen

                    std::string debugDisplayText[onScreenDebugInfoLineCount] = {
                        "YellowPixels: " + yellow_pixels_count,                                                            //1
                        "BluePixels: " + blue_pixels_count,                                                                //2
                        "Verdict: " + std::to_string(steering_verdict),                                                    //3
                        "ActualTurn: " + std::to_string(actual_ground_steering),                                           //4
                        "ValidTurn: " + correct_turn_string,                                                               //5
                        cone_placement_verdict < 0 ? "ConeColourOnLeft: Yellow" : "ConeColourOnLeft: Blue",                //6
                        "RunningAccuracy: " + std::to_string(((double)correct_frames / (double)total_frames) * 100) + "%", //7
                    };

                    // Keep track of where we are printing on the screen, due to OpenCV not supporting \n characters
                    int y = 20;
                    int dy = 15;

                    // Print debug info to the screen
                    for (int i = 0; i < onScreenDebugInfoLineCount; i++)
                    {
                        cv::putText(crop, debugDisplayText[i], cv::Point(30, y), 1, 1, cv::Scalar(200, 200, 200), 1, 1, false);
                        y += dy;
                    }

                    // Displaying pixels numbers on the windows for each cone colour
                    cv::putText(yellow_threshold, yellow_pixels_count, cv::Point(30, 20), 1, 1, 127, 1, 1, false);
                    cv::putText(blue_threshold, blue_pixels_count, cv::Point(30, 20), 1, 1, 127, 1, 1, false);
                    // END print debug info on screen

                    // Log pixel counts to file
                    info_file.open(info_file_name, ios::app);
                    info_file
                        << time_stamp
                        << ","
                        << actual_ground_steering
                        << ","
                        << steering_verdict
                        << ","
                        << blue_pixels_count
                        << ","
                        << yellow_pixels_count
                        << ","
                        << mean_blue_x
                        << ","
                        << mean_blue_y
                        << ","
                        << mean_yellow_x
                        << ","
                        << mean_yellow_y
                        << "\n";
                    info_file.close();

                    cv::imshow("Yellow Cones", yellow_threshold);
                    cv::imshow("Blue Cones", blue_threshold);
                    cv::imshow("Debug Info", crop);
                    cv::waitKey(1);
                }
            }
        }
        retCode = 0;
    }
    return retCode;
}
