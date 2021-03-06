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

#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

using namespace cv;
using namespace std;

const double MIN_AREA_SIZE = 100;
static const int W_0      = 300;
static const int H_0      = 0;
static const int OFFSET_W = 10;
static const int OFFSET_H = 100;

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

Mat findTransform(const cv::Mat& calib_image,
                  const cv::Mat& camera_matrix,
                  const cv::Mat& rectangular_points,
                  const cv::Mat& dist_coeffs,
                  double& pixel_scale)
{
  cv::Mat corner_pixels = rectangular_points.clone(); //pickNPoints(4, calib_image);
  cv::Mat dst_pixels(2,2,CV_32F);

  dst_pixels.at<float>(0, 0) = 0;
  dst_pixels.at<float>(0, 1) = 0;

  dst_pixels.at<float>(1, 0) = calib_image.cols;
  dst_pixels.at<float>(1, 1) = calib_image.rows;


  //scale transform point accordingly 
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
  //cv::hconcat(calib_image, unwarped_frame, concat);
  //imshow("Unwarping", unwarped_frame);
  //imwrite("img1.jpg",unwarped_frame);
  // return unwarped_frame;
  waitKey(0);
  return transf;
}

Mat find_unwarped_img(const cv::Mat& calib_image,
                  const cv::Mat& camera_matrix,
                  const cv::Mat& rectangular_points,
                  const cv::Mat& dist_coeffs,
                  double& pixel_scale)
{
  cv::Mat corner_pixels = rectangular_points.clone(); //pickNPoints(4, calib_image);
  cv::Mat dst_pixels(2,2,CV_32F);

  dst_pixels.at<float>(0, 0) = 0;
  dst_pixels.at<float>(0, 1) = 0;

  dst_pixels.at<float>(1, 0) = calib_image.cols;
  dst_pixels.at<float>(1, 1) = calib_image.rows;


  //scale transform point accordingly 
  float origin_x = dst_pixels.at<float>(0, 0),
        origin_y = dst_pixels.at<float>(0, 1);

  float delta_x = dst_pixels.at<float>(1, 0)-dst_pixels.at<float>(0, 0);
  float delta_y = dst_pixels.at<float>(1, 1)-dst_pixels.at<float>(0, 1);

  float delta_x_mm = 1000;
  float delta_y_mm = 1500;

  float scale_x = delta_x/delta_x_mm;
  float scale_y = delta_y/delta_y_mm;//
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
  //cv::hconcat(calib_image, unwarped_frame, concat);
  namedWindow( "Unwarping", WINDOW_NORMAL ); // Create a window for display.

  imshow("Unwarping", unwarped_frame);
  waitKey(0);
  //imwrite("img1.jpg",unwarped_frame);
  return unwarped_frame;
  
  //return transf;
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



void processImage(cv::Mat& img, cv::Mat& rectangular_points)
{
  // Load image from file

  if(img.empty()) {
    throw std::runtime_error("Failed to open the file ");
  }
  
  // Display original image
  cv::imshow("Original", img);
  cv::moveWindow("Original", W_0, H_0);

  // Convert color space from BGR to HSV
  cv::Mat hsv_img;
  cv::cvtColor(img, hsv_img, cv::COLOR_BGR2HSV);
  
  // Display HSV image
  //cv::imshow("HSV", hsv_img);
  //cv::moveWindow("HSV", W_0+img.cols+OFFSET_W, H_0);

  // Find black regions (filter on saturation and value)
  cv::Mat black_mask;
  cv::inRange(hsv_img, cv::Scalar(0, 0, 0), cv::Scalar(180, 255, 100), black_mask);  
  cv::imshow("BLACK_filter", black_mask);
  cv::moveWindow("BLACK_filter", W_0+2*(img.cols+OFFSET_W), H_0+img.rows+OFFSET_H);

  // Wait keypress
  cv::waitKey(0);

  // Find contours
  std::vector<std::vector<cv::Point>> contours, contours_approx;
  std::vector<cv::Point> approx_curve;
  cv::Mat contours_img;

  // Process black mask
  cv::findContours(black_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE); // find external contours of each blob
  contours_img = img.clone();
  drawContours(contours_img, contours, -1, cv::Scalar(40,190,40), 1, cv::LINE_AA);
  std::cout << "N. contours: " << contours.size() << std::endl;
  for (int i=0; i<contours.size(); ++i)
  {
    if (contours[i].size() > 200){
    std::cout << (i+1) << ") Contour size: " << contours[i].size() << std::endl;
    approxPolyDP(contours[i], approx_curve, 20, true); // fit a closed polygon (with less vertices) to the given contour,
                                                      // with an approximation accuracy (i.e. maximum distance between 
                                                     // the original and the approximated curve) of 3
    if(approx_curve.size()==4){
    contours_approx = {approx_curve};
    drawContours(contours_img, contours_approx, -1, cv::Scalar(0,170,220), 5, cv::LINE_AA);
    std::cout << "   Approximated contour size: " << approx_curve.size() << std::endl;

    rectangular_points.at<float>(0,0) = approx_curve[0].x;
    rectangular_points.at<float>(0,1) = approx_curve[0].y;
    
    rectangular_points.at<float>(1,0) = approx_curve[3].x;
    rectangular_points.at<float>(1,1) = approx_curve[3].y;

    rectangular_points.at<float>(2,0) = approx_curve[2].x;
    rectangular_points.at<float>(2,1) = approx_curve[2].y;

    rectangular_points.at<float>(3,0) = approx_curve[1].x;
    rectangular_points.at<float>(3,1) = approx_curve[1].y;
    }
    }
  }
  std::cout << std::endl;
  cv::imshow("Original", contours_img);
  cv::waitKey(0);
}

void rotate_row(cv::Mat& rectangular_points){
    cv::Mat temp(1,2,CV_32F);
    rectangular_points.row(0).copyTo(temp.row(0));
    rectangular_points.row(1).copyTo(rectangular_points.row(0));
    rectangular_points.row(2).copyTo(rectangular_points.row(1));
    rectangular_points.row(3).copyTo(rectangular_points.row(2));
    temp.row(0).copyTo(rectangular_points.row(3));
}


std::vector<cv::Point> blue_rect_calc(cv::Mat& img)
{
  // // Load image from file
  // //cv::Mat img = cv::imread(filename);
  // if(img.empty()) {
  //   throw std::runtime_error("Failed to open the file " + filename);
  // }
  // Convert color space from BGR to HSV
  cv::Mat hsv_img;
  cv::cvtColor(img, hsv_img, cv::COLOR_BGR2HSV);
  std::vector<cv::Point> approx_curve;
  // Find blue regions
  cv::Mat blue_mask;
  cv::inRange(hsv_img, cv::Scalar(100, 50, 55), cv::Scalar(115, 255, 255), blue_mask);

  // Find contours
  std::vector<std::vector<cv::Point>> contours, contours_approx;
  cv::Mat contours_img;

  // Process blue mask
  contours_img = img.clone();
  cv::findContours(blue_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
  drawContours(contours_img, contours, -1, cv::Scalar(40,190,40), 1, cv::LINE_AA);
  std::cout << "N. contours: " << contours.size() << std::endl;
  for (int i=0; i<contours.size(); ++i)
  {    
    if(contours[i].size()>4){
  
    approxPolyDP(contours[i], approx_curve, 40, true);
    contours_approx = {approx_curve};

    drawContours(contours_img, contours_approx, -1, cv::Scalar(255,255,0), 3, cv::LINE_AA);
  
      if (approx_curve.size()==4){
          return approx_curve;
        }

    }
  }
  return approx_curve;
  std::cout << std::endl;
}

bool isitinside (std::vector<cv::Point> approx_curve_insidebox,Mat img)
{
int setvaluex = img.cols/4;
int setvaluey = img.rows/2;
  for (int i = 0 ;i< approx_curve_insidebox.size(); ++i){
    if (approx_curve_insidebox[i].x > setvaluex || approx_curve_insidebox[i].y < setvaluey){
      return false; 
    }
  }  
  return true;
}

bool checkbluebox(cv::Mat& filename){
  //std::vector<cv::Point> approx_curve;
  std::vector<cv::Point> approx_curve_insidebox; 
  approx_curve_insidebox = blue_rect_calc(filename);

  if(approx_curve_insidebox.size()!=0)
    return isitinside(approx_curve_insidebox,filename);
  else
    return false; 
}

void processNumbers(cv::Mat img)
{
  
  // Convert color space from BGR to HSV
  cv::Mat hsv_img;
  cv::cvtColor(img, hsv_img, cv::COLOR_BGR2HSV);
  
  // Find green regions
  cv::Mat green_mask;
  cv::inRange(hsv_img, cv::Scalar(40, 60, 119), cv::Scalar(88, 249, 255), green_mask);

  // Apply some filtering
  cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size((1*2) + 1, (1*2)+1));
  cv::dilate(green_mask, green_mask, kernel);
  cv::erode(green_mask, green_mask, kernel);
  
  // Display image
  cv::imshow("GREEN_filter", green_mask);  
  
  // Find contours
  std::vector<std::vector<cv::Point>> contours, contours_approx;
  std::vector<cv::Point> approx_curve;
  cv::Mat contours_img;

  contours_img = img.clone();
  cv::findContours(green_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);  
  
  std::vector<cv::Rect> boundRect(contours.size());
  for (int i=0; i<contours.size(); ++i)
  {
    double area = cv::contourArea(contours[i]);
    if (area < MIN_AREA_SIZE) continue; // filter too small contours to remove false positives
    approxPolyDP(contours[i], approx_curve, 2, true);
    contours_approx = {approx_curve};
    drawContours(contours_img, contours_approx, -1, cv::Scalar(0,170,220), 3, cv::LINE_AA);
    boundRect[i] = boundingRect(cv::Mat(approx_curve)); // find bounding box for each green blob
  }
  cv::imshow("Original", contours_img);
  cv::waitKey(0);
  
  
  cv::Mat green_mask_inv, filtered(img.rows, img.cols, CV_8UC3, cv::Scalar(255,255,255));
  cv::bitwise_not(green_mask, green_mask_inv); // generate binary mask with inverted pixels w.r.t. green mask -> black numbers are part of this mask
  
  cv::imshow("Numbers", green_mask_inv);
  cv::waitKey(0);

  // Create Tesseract object
  tesseract::TessBaseAPI *ocr = new tesseract::TessBaseAPI();
  // Initialize tesseract to use English (eng) 
  ocr->Init(NULL, "eng");
  // Set Page segmentation mode to PSM_SINGLE_CHAR (10)
  ocr->SetPageSegMode(tesseract::PSM_SINGLE_CHAR);
  // Only digits are valid output characters
  ocr->SetVariable("tessedit_char_whitelist", "0123456789");
  
  img.copyTo(filtered, green_mask_inv);   // create copy of image without green shapes

  kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size((2*2) + 1, (2*2)+1));
  
  // For each green blob in the original image containing a digit
  for (int i=0; i<boundRect.size(); ++i)
  {
    cv::Mat processROI(filtered, boundRect[i]); // extract the ROI containing the digit
    
    if (processROI.empty()) continue;
    
    cv::resize(processROI, processROI, cv::Size(200, 200)); // resize the ROI
    cv::threshold( processROI, processROI, 80, 255, 0 ); // threshold and binarize the image, to suppress some noise
    
    // Apply some additional smoothing and filtering
    cv::erode(processROI, processROI, kernel);
    cv::GaussianBlur(processROI, processROI, cv::Size(5, 5), 2, 2);
    cv::erode(processROI, processROI, kernel);
    
    // Show the actual image passed to the ocr engine
    cv::imshow("ROI", processROI);
    
     // Set image data
    ocr->SetImage(processROI.data, processROI.cols, processROI.rows, 3, processROI.step);
    
    // Run Tesseract OCR on image and print recognized digit
    std::cout << "Recognized digit: " << std::string(ocr->GetUTF8Text()) << std::endl;
    
    cv::waitKey(0);
  }
  
  ocr->End(); // destroy the ocr object (release resources)

}
cv::Mat processRGB(cv::Mat img)
{

  // Convert color space from BGR to HSV
  cv::Mat hsv_img;
  cv::cvtColor(img, hsv_img, cv::COLOR_BGR2HSV);
  
  // Find red regions: h values around 0 (positive and negative angle: [0,15] U [160,179])
  cv::Mat red_mask_low, red_mask_high, red_mask;
  cv::inRange(hsv_img, cv::Scalar(10, 0, 38), cv::Scalar(19, 250, 229), red_mask_low);
  cv::inRange(hsv_img, cv::Scalar(160, 10, 10), cv::Scalar(179, 255, 255), red_mask_high);
  cv::addWeighted(red_mask_low, 1.0, red_mask_high, 1.0, 0.0, red_mask); // combine together the two binary masks

  // Find blue regions
  cv::Mat blue_mask;
  cv::inRange(hsv_img, cv::Scalar(90, 50, 55), cv::Scalar(115, 255, 255), blue_mask);
  
  // Find green regions
  cv::Mat green_mask;
  //cv::inRange(hsv_img, cv::Scalar(40, 60, 119), cv::Scalar(88, 249, 255), green_mask);
  cv::inRange(hsv_img, cv::Scalar(32, 65, 45), cv::Scalar(57,215 , 200), green_mask);
  //cv::inRange(hsv_img, cv::Scalar(40, 45, 42), cv::Scalar(57,160 , 200), green_mask);


  // Find black regions (filter on saturation and value)
  cv::Mat black_mask;
  cv::inRange(hsv_img, cv::Scalar(100, 50, 55), cv::Scalar(115, 255, 255), black_mask);  


  // Wait keypress
  cv::waitKey(0);

  // Find contours
  std::vector<std::vector<cv::Point>> contours, contours_approx;
  std::vector<cv::Point> approx_curve;
  cv::Mat contours_img;

  // Process red mask
  contours_img = img.clone();
  cv::findContours(red_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
  
  std::cout << "N. contours: " << contours.size() << std::endl;
  for (int i=0; i<contours.size(); ++i)
  {
    if (contours[i].size() > 50) { 
    std::cout << (i+1) << ") Contour size: " << contours[i].size() << std::endl;
    approxPolyDP(contours[i], approx_curve, 10, true);
    contours_approx = {approx_curve};
    drawContours(contours_img, contours_approx, -1, cv::Scalar(0,170,220), 3, cv::LINE_AA);
    std::cout << "   Approximated contour size: " << approx_curve.size() << std::endl;
    }   
  }
  std::cout << std::endl;

  // Process blue mask
  cv::findContours(blue_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
 
  std::cout << "N. contours: " << contours.size() << std::endl;
  for (int i=0; i<contours.size(); ++i)
  {
    if(contours[i].size()>4){
    std::cout << (i+1) << ") Contour size: " << contours[i].size() << std::endl;
    approxPolyDP(contours[i], approx_curve, 10, true);
    contours_approx = {approx_curve};
    drawContours(contours_img, contours_approx, -1, cv::Scalar(255,0,0), 3, cv::LINE_AA);
    std::cout << "   Approximated contour size: " << approx_curve.size() << std::endl;
    }
  }
  std::cout << std::endl;

  // Process green mask
  cv::findContours(green_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
  
  std::cout << "N. contours: " << contours.size() << std::endl;
  for (int i=0; i<contours.size(); ++i)
  {
    if (contours[i].size() > 30) 
  {
    std::cout << (i+1) << ") Contour size: " << contours[i].size() << std::endl;
    approxPolyDP(contours[i], approx_curve, 3, true);
    contours_approx = {approx_curve};
    drawContours(contours_img, contours_approx, -1, cv::Scalar(250,170,220), 3, cv::LINE_AA);
    std::cout << "   Approximated contour size: " << approx_curve.size() << std::endl;
    }
  }
  std::cout << std::endl;
  cv::imshow("Original", contours_img);
  
  cv::waitKey(0);
  return contours_img;

}
cv::Mat cropImage(cv::Mat& img){
  Rect rectangle = Rect(0,0,684,1024);
  Mat crop_img = img(rectangle);
  return crop_img;
}
int main(int argc, char* argv[])
{
  cv::Mat rectangular_points(4,2,CV_32F);
  cv::Mat camera_matrix, dist_coeffs;
  cv::Mat frame, frameUndist,persp_transf,unwarped_img;
  double pixel_scale;

  frame = cv::imread(argv[1], 1);//reading file
  
  loadCoefficients("../config/intrinsic_calibration.xml", camera_matrix, dist_coeffs);//loading camera co-efficients
  
  undistort(frame, frameUndist, camera_matrix, dist_coeffs, cv::getOptimalNewCameraMatrix(camera_matrix, dist_coeffs, frame.size(),0));//undistorting the image
  
  processImage(frameUndist,rectangular_points);//finding the black boundary points from the image
  
  for(int i =0;i<4;i++){
    cout<< "interation"<<i<<endl;
    unwarped_img = find_unwarped_img(frameUndist, camera_matrix,rectangular_points, dist_coeffs, pixel_scale);//unwrapping the image using 
    if(checkbluebox(unwarped_img)){cout<<"value of the check is true"<<endl;break;}//to check if the black box is on left
    rotate_row(rectangular_points);
  }
  persp_transf = findTransform(frameUndist, camera_matrix,rectangular_points, dist_coeffs, pixel_scale);
  std::cout << "Pixel Scale: " << pixel_scale << "mm" << std::endl;
  cv::Mat cropimage = cropImage(unwarped_img);
  storeAllParameters("../config/fullCalibration.yml", camera_matrix, dist_coeffs, pixel_scale, persp_transf);
  imwrite("abc.jpg", cropimage);
  processNumbers(processRGB(cropimage));
  return 0;
}