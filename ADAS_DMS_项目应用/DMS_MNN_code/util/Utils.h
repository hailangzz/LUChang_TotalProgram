/*
 * @Author: chenjingyu
 * @Date: 2023-06-20 12:29:31
 * @LastEditTime: 2023-08-22 10:24:23
 * @Description: utils module
 * @FilePath: \Mediapipe-MNN\source\Utils.h
 */
#pragma once

#include "TypeDefines.h"
#include "optional.h"
#include <cmath>
#include <utility>
#include <opencv2/core.hpp>

#if defined(ANDROID)
#include <android/log.h>
#define CVLOG_TAG "cvUtil"
#define CVLOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, CVLOG_TAG, __VA_ARGS__))
#define CVLOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, CVLOG_TAG, __VA_ARGS__))
#define CVLOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, CVLOG_TAG, __VA_ARGS__))
#else
#include "Logger.h"
#define CVLOG_TAG     "cvUtil"
#define CVLOGD(...)   LOG::debug(CVLOG_TAG, __VA_ARGS__)
#define CVLOGI(...)   LOG::info(CVLOG_TAG, __VA_ARGS__)
#define CVLOGE(...)   LOG::error(CVLOG_TAG, __VA_ARGS__)
#endif

namespace dms {
#ifndef M_PI
#define M_PI 3.14159265358979323846 // pi
#endif

#define MAX_(x, y) ((x) > (y) ? (x) : y)
#define MIN_(x, y) ((x) < (y) ? (x) : y)

// Indices within the partial landmarks.
constexpr int kNumPalmLandmarks = 21;
constexpr float kTargetPalmAngle = 90.0f;
constexpr int kNumFaceLandmarks = 478;
constexpr float kTargetFaceAngle = 0.0f;
constexpr int kNumPoseLandmarks = 39;
float ComputeRotation(const Point2f &src, const Point2f &dst);

std::vector<Point2f> getInputRegion(const ImageHead &in, RotateType type,
                                    int out_w, int out_h,
                                    bool keep_aspect = true);
std::vector<Point2f> getInputRegion(const ImageHead &in, RotateType type,
                                    const Rect &rect, int out_h, int out_w,
                                    float init_angle = 0.0f,
                                    float expand_scale = 1.0f,
                                    float offset_x_scale = 0.0f,
                                    float offset_y_scale = 0.0f);

std::vector<Point2f>
getInputRegion(const ImageHead &in, RotateType type,
               const ObjectInfo &object, float expand_scale = 1.0f);

float sigmoid(float x);

float getIouOfObjects(const ObjectInfo &a, const ObjectInfo &b);
void NMSObjects(std::vector<ObjectInfo> &objects, float iou_thresh);

float RotateTypeToAngle(RotateType type);

double YoloGetIOU(cv::Rect_<float> bb_test, cv::Rect_<float> bb_gt); //YoloDetector get iou value.
void YoloNMS(std::vector<BoxInfo>& result, float nms_threshold);

template <typename T>
optional<double> ComputeCosineSimilarity(const T& u, const T& v, int num_elements)
{
	if (num_elements <= 0) {
		return nullopt;
	}

	double norm_u = 0.0;
	double norm_v = 0.0;
	double dot_product = 0.0;

	for (int i = 0; i < num_elements; ++i) {
		norm_u += u[i] * u[i];
		norm_v += v[i] * v[i];
		dot_product += u[i] * v[i];
	}

	if (norm_u <= 0.0 || norm_v <= 0.0) {
		return nullopt;
	}

	return dot_product / std::sqrt(norm_u * norm_v);
}

Embedding buildFloatEmbedding(const std::vector<float> &values);
Embedding buildQuantizedEmbedding(const std::vector<int8_t> &values);
optional<double> cosineSimilarity(const Embedding &u, const Embedding &v);

float GetInverseL2Norm(const float *values, int size);

Embedding FillFloatEmbedding(const float *data, int size, bool l2_normalize);
Embedding FillQuantizedEmbedding(const float *data, int size,
                                 bool l2_normalize);

cv::Vec3f eulerAngles(const cv::Mat& R);

} // namespace dms

namespace util {
	class TimeMeasure {
	public:
		TimeMeasure(const char* tips) : mTips(tips) {
			mStart = double(cv::getTickCount());
			mPrev = mStart;
		}

		~TimeMeasure() {
			if (mTips != NULL) {
				_print(mTips, mStart);
			}
		}

		void print(const char* tips) {
			mPrev = _print(tips, mPrev);
		}

		void clear() {
			mPrev = double(cv::getTickCount());
		}

	protected:
		double _print(const char* tips, double start) {
			double currentTick = double(cv::getTickCount());
			double durationMS = (currentTick - start) * 1000 / cv::getTickFrequency();
			if (durationMS > 2.0f) {
				CVLOGD("%s: %0.1f ms\r\n", tips != NULL ? tips : "null", durationMS);
			}
			return currentTick;
		}

	protected:
		const char* mTips;
		double mStart;
		double mPrev;
	};

	class AvgTimeMeasure {
	public:
		AvgTimeMeasure(const char* name, int printCount = 10000) : mName(name), mPrintCount(printCount) {
			mCount = 0;
			mTotalTime = 0.0f;
		}

		void begin() {
			mStartTime = double(cv::getTickCount());
		}

		void end() {
			double current = double(cv::getTickCount());
			double duration = (current - mStartTime) / cv::getTickFrequency();
			mTotalTime += duration;
			mCount++;
			double avgTime = mTotalTime * 1000.0f / mCount;
			if (mCount % mPrintCount == 0) {
				CVLOGD("%s: avg time %f ms, call count %d\r\n", mName, avgTime, mCount);
				mTotalTime = 0.0f;
				mCount = 0;
			}
		}

	protected:
		const char* mName;
		int mCount;
		int mPrintCount;
		double mStartTime;
		double mTotalTime;
	};
};
