// ipm.cpp:
// Find the perspective mapping transformation from the ground floor to the
// camera, and store all the parameters to a file

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <atomic>
#include <unistd.h>

using namespace cv;

void loadCoefficients(const std::string& filename,
                      cv::Mat& camera_matrix,
                      cv::Mat& dist_coeffs)
{
  cv::FileStorage fs( filename, cv::FileStorage::READ );
  if (!fs.isOpened())
  {
    throw std::runtime_error("Could not open file " + filename);
  }
  fs["camera_matrix"] >> camera_matrix;
  fs["distortion_coefficients"] >> dist_coeffs;
  fs.release();
}


// Defintion of the function pickNPoints and the callback mouseCallback.
// The function pickNPoints is used to display a window with a background
// image, and to prompt the user to select n points on this image.
cv::Mat result, bg_img;
int idx = 0;
std::string name;
std::atomic<bool> done;
int n;

void mouseCallback(int event, int x, int y, int, void* p)
{
  if (event != EVENT_LBUTTONDOWN || done.load()) return;

  if (result.rows <= idx) throw std::runtime_error("Something went wrong");
  result.at<float>((int)idx, 0) = x;
  result.at<float>((int)idx++, 1) = y;
  cv::circle(bg_img, {x,y}, 20, Scalar(0,0,255), -1);
  imshow(name.c_str(), bg_img);

  if (idx >= n) {
    usleep(500*1000);
    done.store(true);
  }
}

cv::Mat pickNPoints(int n0, const Mat& img)
{
  result = cv::Mat(n0, 2, CV_32F);
  idx = 0;
  bg_img = img.clone();
  name = "Pick " + std::to_string(n0) + " points";
  imshow(name.c_str(), bg_img);
  namedWindow(name.c_str());
  n = n0;

  done.store(false);

  setMouseCallback(name.c_str(), &mouseCallback, nullptr);
  while (!done.load()) {
    cv::waitKey(500);
  }

  destroyWindow(name.c_str());
  return result;
}


// Example of function to determine the perspective transformation of a
// rectangle on the ground plane (with manual intervention from the user, that
// is required to select the 4 corner points of the rectangle, starting from the
// top-left corner and proceeding in clockwise order, and the origin (top-left)
// and scale of the transformed top view image).
// Since the real size of the rectangle is known (width: 1m, height: 1.5m),
// the fucntion returns also the pixel_scale, i.e. the size (in mm) of each
// pixel in the top view image
Mat findTransform(const std::string& calib_image_name,
                  const cv::Mat& camera_matrix,
                  const cv::Mat& dist_coeffs,
                  double& pixel_scale)
{
  Mat calib_image, original_image = imread(calib_image_name);

  if (original_image.empty())
  {
    throw std::runtime_error("Could not open image " + calib_image_name);
  }

  undistort(original_image, calib_image, camera_matrix, dist_coeffs);

  cv::Mat corner_pixels = pickNPoints(4, calib_image);

  // generate and show a black image, corresponding to the top-view camera, and
  // ask the user to select the origin and scale of the resulting image
  cv::Mat screen_pixels = Mat(calib_image.rows, calib_image.cols, CV_8UC3, cv::Scalar(0,0,0));
  cv::Mat dst_pixels = pickNPoints(2, screen_pixels);

  float origin_x = dst_pixels.at<float>(0, 0),
        origin_y = dst_pixels.at<float>(0, 1);

  float delta_x = dst_pixels.at<float>(1, 0)-dst_pixels.at<float>(0, 0);
  float delta_y = dst_pixels.at<float>(1, 1)-dst_pixels.at<float>(0, 1);

  float delta_x_mm = 1000;
  float delta_y_mm = 1500;

  float scale_x = delta_x/delta_x_mm;
  float scale_y = delta_y/delta_y_mm;
  float scale = std::min(scale_x, scale_y);

  pixel_scale = 1./scale;
  delta_x = scale*delta_x_mm;
  delta_y = scale*delta_y_mm;

  cv::Mat transf_pixels = (cv::Mat_<float>(4,2) << origin_x, origin_y,
                                                   origin_x+delta_x, origin_y,
                                                   origin_x+delta_x, origin_y+delta_y,
                                                   origin_x, origin_y+delta_y);

  cv::Mat transf = cv::getPerspectiveTransform(corner_pixels, transf_pixels);
  cv::Mat unwarped_frame, concat;
  warpPerspective(calib_image, unwarped_frame, transf, calib_image.size());
  cv::hconcat(calib_image, unwarped_frame, concat);
  imshow("Unwarping", concat);

  waitKey(0);
  return transf;
}

// Store all the parameters to a file, for a later use, using the FileStorage
// class methods
void storeAllParameters(const std::string& filename,
                        const cv::Mat& camera_matrix,
                        const cv::Mat& dist_coeffs,
                        double pixel_scale,
                        const Mat& persp_transf)
{
  cv::FileStorage fs( filename, cv::FileStorage::WRITE );
  fs << "camera_matrix" << camera_matrix
     << "dist_coeffs" << dist_coeffs
     << "pixel_scale" << pixel_scale
     << "persp_transf" << persp_transf;
  fs.release();
}

void run() {
  cv::Mat camera_matrix, dist_coeffs;
  loadCoefficients("../config/intrinsic_calibration.xml", camera_matrix, dist_coeffs);

  double pixel_scale;
  Mat persp_transf = findTransform("persp_transf.jpg", camera_matrix, dist_coeffs, pixel_scale);
  std::cout << "Pixel Scale: " << pixel_scale << "mm" << std::endl;

  storeAllParameters("../config/fullCalibration.yml", camera_matrix, dist_coeffs, pixel_scale, persp_transf);
}

int main() {
  run();
  return 0;
}
