#include "AdasProgram.h"

int main()
{
    YolopNet *pyolop = new YolopNet();
	pyolop->YolopLoadModel("./yolop-384-640_8bit.npumodel");	
	S_MODELPARAM model_param;
	
	std::string image_folder_path = "./images";
	//获取所有待检测图片的路径信息
    std::vector<std::string> jpg_files;
	GetAbsoluteFiles(image_folder_path,jpg_files);
	
	for (const std::string& jpgFilePath : jpgFiles) {
		
		cv::Mat test_img = cv::imread(jpgFilePath);
		cv::resize(test_img, test_img, cv::Size(640, 384));
		pyolop->detect_yolop_input(test_img, 640, 384, 3, "input0");
		printf("finish detect_yolop_input() \n");
        pyolop->detect_yolop_inference(4, NodeNames, 32);
		printf("finish detect_yolop_inference() \n");
		std::vector<zxw_object_info_p> objs;  //检测到的车辆框
		cv::Mat mark_images = cv::Mat::zeros(384, 640,CV_8UC1);
	}
}