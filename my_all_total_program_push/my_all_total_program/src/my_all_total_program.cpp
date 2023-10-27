// my_all_total_program.cpp: 定义应用程序的入口点。

#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <onnxruntime_cxx_api.h>
#include "pfld.h"
#include "utils.h"
#include "ssrnet.h"
#include "fsanet.h"
#include "constants.h"
#include "FaceParsingBiSeNet.h"
#include "arcface.h"
#include "test.h"
#include "hello.h"
#include "ncnn/net.h"


#include "pfld_ncnn.h"




using namespace std;


void draw_landmarks_inplace(cv::Mat& mat, Landmarks& landmarks)
{
	if (landmarks.points.empty() || !landmarks.flag) return;
	for (const auto& p : landmarks.points)
		cv::circle(mat, p, 2, cv::Scalar(0, 255, 0), -1);
}


static void test_onnxruntime()
{

	std::string onnx_path = "F:/lite.ai.toolkit/hub/onnx/cv/pfld-106-v3.onnx";
	std::string test_img_path = "F:/lite.ai.toolkit/examples/lite/resources/test_lite_pfld.png";
	std::string save_img_path = "./test_pfld_onnx.jpg";

	
	ortcv::PFLD *pfld = new ortcv::PFLD(onnx_path);
	Landmarks landmarks;
	cv::Mat img_bgr = cv::imread(test_img_path);
	pfld->detect(img_bgr, landmarks);

	draw_landmarks_inplace(img_bgr, landmarks);

	cv::imwrite(save_img_path, img_bgr);

	std::cout << "ONNXRuntime Version Done! Detected Landmarks Num: "
		<< landmarks.points.size() << std::endl;

	delete pfld;
	
}

void draw_age_inplace(cv::Mat& mat_inplace, Age& age)
{
	if (!age.flag) return;
	const unsigned int offset = static_cast<unsigned int>(
		0.1f * static_cast<float>(mat_inplace.rows));
	std::string age_text = "Age:" + std::to_string(age.age).substr(0, 4);
	std::string interval_text = "Interval" + std::to_string(age.age_interval[0])
		+ "_" + std::to_string(age.age_interval[1]);
	std::string interval_prob = "Prob:" + std::to_string(age.interval_prob).substr(0, 4);
	cv::putText(mat_inplace, age_text, cv::Point2i(10, offset),
		cv::FONT_HERSHEY_SIMPLEX, 0.6f, cv::Scalar(0, 255, 0), 2);
	cv::putText(mat_inplace, interval_text, cv::Point2i(10, offset * 2),
		cv::FONT_HERSHEY_SIMPLEX, 0.6f, cv::Scalar(255, 0, 0), 2);
	cv::putText(mat_inplace, interval_prob, cv::Point2i(10, offset * 3),
		cv::FONT_HERSHEY_SIMPLEX, 0.6f, cv::Scalar(0, 0, 255), 2);
}

static void ssrnet_onnxruntime()
{

	std::string onnx_path = "F:/lite.ai.toolkit/hub/onnx/cv/ssrnet.onnx";
	std::string test_img_path = "F:/lite.ai.toolkit/examples/lite/resources/test_lite_ssrnet.jpg";
	std::string save_img_path = "./test_lite_ssrnet_result.jpg";


	ortcv::SSRNet *ssrnet = new ortcv::SSRNet(onnx_path);
	Age age;
	cv::Mat img_bgr = cv::imread(test_img_path);
	ssrnet->detect(img_bgr, age);
	draw_age_inplace(img_bgr, age);
	cv::imwrite(save_img_path, img_bgr);

	delete ssrnet;

}

void draw_axis_inplace(cv::Mat& mat_inplace,
	const EulerAngles& euler_angles,
	float size, int thickness)
{
	if (!euler_angles.flag) return;

	const float pitch = euler_angles.pitch * _PI / 180.f;
	const float yaw = -euler_angles.yaw * _PI / 180.f;
	const float roll = euler_angles.roll * _PI / 180.f;

	const float height = static_cast<float>(mat_inplace.rows);
	const float width = static_cast<float>(mat_inplace.cols);

	const int tdx = static_cast<int>(width / 2.0f);
	const int tdy = static_cast<int>(height / 2.0f);

	// X-Axis pointing to right. drawn in red
	const int x1 = static_cast<int>(size * std::cos(yaw) * std::cos(roll)) + tdx;
	const int y1 = static_cast<int>(
		size * (std::cos(pitch) * std::sin(roll)
			+ std::cos(roll) * std::sin(pitch) * std::sin(yaw))
		) + tdy;
	// Y-Axis | drawn in green
	const int x2 = static_cast<int>(-size * std::cos(yaw) * std::sin(roll)) + tdx;
	const int y2 = static_cast<int>(
		size * (std::cos(pitch) * std::cos(roll)
			- std::sin(pitch) * std::sin(yaw) * std::sin(roll))
		) + tdy;
	// Z-Axis (out of the screen) drawn in blue
	const int x3 = static_cast<int>(size * std::sin(yaw)) + tdx;
	const int y3 = static_cast<int>(-size * std::cos(yaw) * std::sin(pitch)) + tdy;

	cv::line(mat_inplace, cv::Point2i(tdx, tdy), cv::Point2i(x1, y1), cv::Scalar(0, 0, 255), thickness);
	cv::line(mat_inplace, cv::Point2i(tdx, tdy), cv::Point2i(x2, y2), cv::Scalar(0, 255, 0), thickness);
	cv::line(mat_inplace, cv::Point2i(tdx, tdy), cv::Point2i(x3, y3), cv::Scalar(255, 0, 0), thickness);
}


static void fsanet_onnxruntime()
{



	std::string onnx_path = "F:/lite.ai.toolkit/hub/onnx/cv/fsanet-var.onnx";
	std::string test_img_path = "F:/lite.ai.toolkit/examples/lite/resources/test_lite_fsanet.jpg";
	std::string save_img_path = "./test_lite_fsanet_result.jpg";


	ortcv::FSANet * fsanet = new ortcv::FSANet(onnx_path);
	cv::Mat img_bgr = cv::imread(test_img_path);
	EulerAngles euler_angles;
	fsanet->detect(img_bgr, euler_angles);

	if (euler_angles.flag)
	{
		draw_axis_inplace(img_bgr, euler_angles,3.0,7);
		cv::imwrite(save_img_path, img_bgr);
		std::cout << "yaw:" << euler_angles.yaw << " pitch:" << euler_angles.pitch << " row:" << euler_angles.roll << std::endl;
	}
	delete fsanet;

}


static void FaceParsingBiSeNet_onnxruntime()
{



	std::string onnx_path = "F:/lite.ai.toolkit/hub/onnx/cv/face_parsing_512x512.onnx";
	std::string test_img_path = "F:/lite.ai.toolkit/examples/lite/resources/test_lite_face_parsing.png";
	std::string save_img_path = "./test_lite_face_parsing_result.png";


	ortcv::FaceParsingBiSeNet* face_parsing_bisenet =
		new ortcv::FaceParsingBiSeNet(onnx_path, 4); // 4 threads

	FaceParsingContent content;
	cv::Mat img_bgr = cv::imread(test_img_path);

	face_parsing_bisenet->detect(img_bgr, content);

	if (content.flag)
	{
		if (!content.merge.empty()) cv::imwrite(save_img_path, content.merge);
		std::cout << "ONNXRuntime Version FaceParsingBiSeNet Done!" << std::endl;
	}

	delete face_parsing_bisenet;

}


template<typename T> float cosine_similarity(
	const std::vector<T>& a, const std::vector<T>& b)
{
	float zero_vale = 0.f;
	if (a.empty() || b.empty() || (a.size() != b.size())) return zero_vale;
	const unsigned int _size = a.size();
	float mul_a = zero_vale, mul_b = zero_vale, mul_ab = zero_vale;
	for (unsigned int i = 0; i < _size; ++i)
	{
		mul_a += (float)a[i] * (float)a[i];
		mul_b += (float)b[i] * (float)b[i];
		mul_ab += (float)a[i] * (float)b[i];
	}
	if (mul_a == zero_vale || mul_b == zero_vale) return zero_vale;
	return (mul_ab / (std::sqrt(mul_a) * std::sqrt(mul_b)));
}

static void arcface_onnxruntime()
{

	std::string onnx_path = "F:/BaiduNetdiskDownload/ms1mv3_arcface_r100.onnx";
	std::string test_img_path0 = "F:/lite.ai.toolkit/examples/lite/resources/test_lite_faceid_0.png";
	std::string test_img_path1 = "F:/lite.ai.toolkit/examples/lite/resources/test_lite_faceid_1.png";
	std::string test_img_path2 = "F:/lite.ai.toolkit/examples/lite/resources/test_lite_faceid_2.png";


	auto* glint_arcface = new ortcv::GlintArcFace(onnx_path);

	FaceContent face_content0, face_content1, face_content2;
	cv::Mat img_bgr0 = cv::imread(test_img_path0);
	cv::Mat img_bgr1 = cv::imread(test_img_path1);
	cv::Mat img_bgr2 = cv::imread(test_img_path2);
	glint_arcface->detect(img_bgr0, face_content0);
	glint_arcface->detect(img_bgr1, face_content1);
	glint_arcface->detect(img_bgr2, face_content2);

	if (face_content0.flag && face_content1.flag && face_content2.flag)
	{
		float sim01 = cosine_similarity<float>(
			face_content0.embedding, face_content1.embedding);
		float sim02 = cosine_similarity<float>(
			face_content0.embedding, face_content2.embedding);
		std::cout << "Detected Sim01: " << sim01 << " Sim02: " << sim02 << std::endl;
	}

	delete glint_arcface;

}


static void test_pfld_ncnn()
{

	std::string param_path = "F:/lite.ai.toolkit/hub/ncnn/cv/pfld-106-v3.opt.param";
	std::string bin_path = "F:/lite.ai.toolkit/hub/ncnn/cv/pfld-106-v3.opt.bin";
	std::string test_img_path = "F:/lite.ai.toolkit/examples/lite/resources/test_lite_pfld.png";
	std::string save_img_path = "test_pfld_ncnn.jpg";

	ncnncv::NCNNPFLD * pfld =
		new ncnncv::NCNNPFLD(param_path, bin_path);

	Landmarks landmarks;
	cv::Mat img_bgr = cv::imread(test_img_path);
	pfld->detect(img_bgr, landmarks);

	draw_landmarks_inplace(img_bgr, landmarks);

	cv::imwrite(save_img_path, img_bgr);

	std::cout << "NCNN Version Done! Detected Landmarks Num: "
		<< landmarks.points.size() << std::endl;

	delete pfld;
}

int main()
{
	test_onnxruntime();
	ssrnet_onnxruntime();
	fsanet_onnxruntime();
	//FaceParsingBiSeNet_onnxruntime();
	arcface_onnxruntime();
	cout << "Hello CMake." << endl;
	print_test();
	hello();

	test_pfld_ncnn();
	return 0;
}
