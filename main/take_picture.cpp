/**
 * This example takes a picture every 5s and print its size on serial monitor.
 */

// =============================== SETUP ======================================

// 1. Board setup (Uncomment):
// #define BOARD_WROVER_KIT
#define BOARD_ESP32CAM_AITHINKER

// This is really only necessary if we want to change camera settings, the defaults work -- Talia 2/28/24 
/**
 * 2. Kconfig setup
 *
 * If you have a Kconfig file, copy the content from
 *  https://github.com/espressif/esp32-camera/blob/master/Kconfig into it.
 * In case you haven't, copy and paste this Kconfig file inside the src directory.
 * This Kconfig file has definitions that allows more control over the camera and
 * how it will be initialized.
 */

// This is actually taken care of by putting "esp_psram" in our main/CMakeLists.txt PRIV_REQUIRES list -- Talia 2/28/24 
/**
 * 3. Enable PSRAM on sdkconfig:
 * CONFIG_ESP32_SPIRAM_SUPPORT=y
 *
 * More info on
 * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/kconfig.html#config-esp32-spiram-support
 */

// ================================ CODE ======================================
#define ESP32 1
#undef EPS
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#define EPS 192

#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>
#include "sdkconfig.h"
#include <iostream>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <esp_err.h>
#include <esp_spiffs.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// C++ includes
#include <iostream>
#include <iomanip>
#include <cstdint>
#include <cstddef>

// support IDF 5.x
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

#include "esp_camera.h"

using namespace std;
using namespace cv;

// ESP32Cam (AiThinker) PIN Map
#ifdef BOARD_ESP32CAM_AITHINKER

#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1 //software reset will be performed
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

#endif

static const char *APP_TAG = "take_picture";
static const char *DATA_TAG = "image_data";

// https://github.com/espressif/esp32-camera/blob/master/driver/include/esp_camera.h
static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_GRAYSCALE, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_QVGA,    //QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates.

    .jpeg_quality = 12, //0-63, for OV series camera sensors, lower number means higher quality
    .fb_count = 1,       //When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
	.fb_location = CAMERA_FB_IN_PSRAM, // tried hack of storing pic in DRAM, but when opencv is being used, we don't have enough space for this to work.
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

static esp_err_t init_camera(void)
{
    // initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(APP_TAG, "Camera Init Failed");
        return err;
    }

    return ESP_OK;
}

Mat detect_edges(Mat img) {
    Mat edges;

    // Canny(img, edges, 100, 200);
    return img;
}

Mat dilate_img(Mat img) {
    Mat dilated;
    dilate(img, dilated, getStructuringElement(MORPH_RECT, Size(11, 11)), Point(-1, -1), 1);
    return dilated;
}

Mat calc_var_over_rows(Mat img) {
    Mat sum;
    reduce(img, sum, 1, REDUCE_SUM, CV_32S);
    return sum;
}

Mat gaussian_filter(int kernel_size, double sigma) {
    Mat kernel(kernel_size, 1, CV_64F);
    double sum = 0.0;
    for (int x = 0; x < kernel_size; x++) {
        kernel.at<double>(x, 0) = (1 / (std::sqrt(2 * M_PI) * sigma)) * std::exp(-((x - (kernel_size - 1) / 2) * (x - (kernel_size - 1) / 2)) / (2 * sigma * sigma));
        sum += kernel.at<double>(x, 0);
    }
    kernel /= sum;
    return kernel;
}

Mat smooth_data(Mat data, Mat kernel) {
    Mat smoothed_data;
    filter2D(data, smoothed_data, -1, kernel);
    return smoothed_data;
}

int calc_splitter(Mat data, int threshold = 10000) {
    for (int i = 0; i < data.rows; i++) {
        if (data.at<int>(i, 0) > threshold) {
            return i;
        }
    }
    return -1;
}

int run_canny(Mat raw_img) {
    cv::Mat edge_img = detect_edges(raw_img);
    cv::Mat dilated = dilate_img(edge_img);
    cv::Mat variation_over_rows = calc_var_over_rows(dilated);
    cv::Mat kernel = gaussian_filter(30, 10);
    cv::Mat smoothed_data = smooth_data(variation_over_rows, kernel);
    int splitter = calc_splitter(smoothed_data, 20000);
	return splitter;
}

// a bad algorithm courtesy of chat GPT
Point run_threshold(Mat raw_img) {
	// Apply thresholding
	threshold(raw_img, raw_img, 128, 255, THRESH_BINARY);

	// Find contours in the thresholded image
	vector<vector<Point>> contours;
	findContours(raw_img, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

	// Find the contour with the highest top point
	Point highestPoint(0, 0);
	double highestY = DBL_MAX; // Initialize with a large value

	for (const auto& contour : contours) {
		for (const auto& point : contour) {
			if (point.y < highestY) {
				highestY = point.y;
				highestPoint = point;
			}
		}
	}
	return highestPoint;
}

// this is necessary for defining the main function in C++ rather than C
//  https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/cplusplus.html#developing-in-c
extern "C" {
void app_main(void);
}

void app_main(void)
{
	// https://docs.espressif.com/projects/esp-idf/en/v4.3/esp32/api-reference/system/log.html#_CPPv417esp_log_level_setPKc15esp_log_level_t
	// set (max) log level for tags
	// we're using log-level verbose for data and log level info for normal
	// then it should be pretty easy to filter between the two when running the monitor
	esp_log_level_set(DATA_TAG, ESP_LOG_VERBOSE);
	esp_log_level_set(APP_TAG, ESP_LOG_INFO);
	if(ESP_OK != init_camera()) {
		return;
	}

	while (1)
	{
		ESP_LOGI(APP_TAG, "Taking picture...");
		camera_fb_t *pic = esp_camera_fb_get();

		// use pic->buf to access the image
		ESP_LOGI(APP_TAG, "Picture taken! Its size was: %zu bytes", pic->len);
		ESP_LOGI(APP_TAG, "height: %zu ", pic->height);
		ESP_LOGI(APP_TAG, "width: %zu ", pic->width);

		// NOTE: if you change any log statements with DATA_TAG, viewer.py may have to be updated 
		ESP_LOGV(DATA_TAG, "Picture Start");

		// https://github.com/espressif/esp-idf/blob/v5.2.1/components/log/include/esp_log_internal.h
		// write the image data to serial monitor
		// buf len is uint16_t, so in theory, its max value is 65535. we will use this as a chunk size to iteratively write the buffer
		uint16_t max_buf_len = 65535;
		size_t offset = 0; // offset = 0
		while ((pic->len - offset) > max_buf_len) {
			ESP_LOG_BUFFER_HEX_LEVEL(DATA_TAG, pic->buf + offset, max_buf_len, ESP_LOG_VERBOSE);
			offset += max_buf_len;
		}
		ESP_LOG_BUFFER_HEX_LEVEL(DATA_TAG, pic->buf + offset, pic->len - offset, ESP_LOG_VERBOSE);

		// for PIXFORMAT_GRAYSCALE, pic size = height*width, so each pixel is a byte
		// presumably an 8-bit unsigned 1 channel opencv data type (8UC1) is fine
		Mat raw_img(pic->height, pic->width, CV_8UC1, pic->buf);

		// Iterate over matrix elements and compare with buffer
		for (int i = 0; i < raw_img.rows; ++i) {
			for (int j = 0; j < raw_img.cols; ++j) {
				// matrix index is column first
				uchar matrixValue = raw_img.at<uchar>(i, j);
				uchar bufferValue = pic->buf[i * raw_img.cols + j];
				if (matrixValue != bufferValue) {
					ESP_LOGW(APP_TAG, "Value mismatch between pic buffer and opencv matrix at position %zu, %zu", i, j);
				}
			}
		}

		// run edge detection algorithm

		// using opencv Canny function leads to issues with our specific build of open cv (on Windows 11)
		// https://github.com/joachimBurket/esp32-opencv/issues/2
		// we'd have to rebuild opencv for this to work
		// or use docker
		// or hope that it works on Mac OS
		// int canny_y = run_canny(raw_img);
		// ESP_LOGI(APP_TAG, "canny edges highest y=%zu", canny_y);

		// this threshold algorithm is not functional, but it runs without raising errors 
		// we're just using it temporarily for testing purposes
		Point highestPoint = run_threshold(raw_img);

		ESP_LOGI(DATA_TAG, "Highest point is (x=%zu, y=%zu)", highestPoint.x, highestPoint.y);
		ESP_LOGV(DATA_TAG, "Picture End");

		esp_camera_fb_return(pic);

		vTaskDelay(5000 / portTICK_RATE_MS);
	}
}
