#include "MNN/Interpreter.hpp"
#include <memory>
#include <string>
#include<iostream>
#include <fstream>
using namespace std;
using namespace MNN;  // 使用MNN的命名空间


int test_load_mnn_model() {
	string model_path = "C:/Users/zhangzuo/Desktop/best.mnn";

	// 加载模型和配置参数
	shared_ptr<Interpreter> net = shared_ptr<Interpreter>(Interpreter::createFromFile(model_path.c_str()));
	ScheduleConfig config;
	config.numThread = 2;
	config.backupType = MNN_FORWARD_CPU;
	BackendConfig backendConfig;
	backendConfig.memory = BackendConfig::Memory_Normal;  // 内存
	backendConfig.power = BackendConfig::Power_Normal;    // 功耗
	backendConfig.precision = BackendConfig::PrecisionMode::Precision_Low;   // 精度
	config.backendConfig = &backendConfig;
	Session* session = net->createSession(config);

	// input和output是参于session的计算的输入和输出
	std::string input_name = "images";
	Tensor* input = net->getSessionInput(session, input_name.c_str());
	std::string output_name = "output";
	Tensor* output = net->getSessionOutput(session, output_name.c_str());

	// 转换自己mnn的输入维度
	net->resizeTensor(input, { 1, 384, 384 });
	net->resizeSession(session);

	Tensor* input_tensor = Tensor::create<float>(input->shape(), NULL, Tensor::CAFFE);
	// 将输入一一赋值
	for (int i = 0; i < input_tensor->elementSize(); i++) {
		input_tensor->host<float>()[i] = 0.5;
	}
	// 将赋值的tensorcopy给input用户session计算
	input->copyFromHostTensor(input_tensor);
	// input->printShape();

	// 运行会话
	net->runSession(session);

	// 创建output一样大小和格式的tensor来copy数据
	Tensor output_tensor(output, output->getDimensionType());
	output->copyToHostTensor(&output_tensor);
	// output->print();

	for (int i = 0; i < output_tensor.elementSize(); i++) {
		cout << output_tensor.host<int>()[i] << endl;
	}

	net->releaseModel();
	net->releaseSession(session);
	return 0;
}
