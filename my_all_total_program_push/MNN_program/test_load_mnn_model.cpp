#include "MNN/Interpreter.hpp"
#include <memory>
#include <string>
#include<iostream>
#include <fstream>
using namespace std;
using namespace MNN;  // ʹ��MNN�������ռ�


int test_load_mnn_model() {
	string model_path = "C:/Users/zhangzuo/Desktop/best.mnn";

	// ����ģ�ͺ����ò���
	shared_ptr<Interpreter> net = shared_ptr<Interpreter>(Interpreter::createFromFile(model_path.c_str()));
	ScheduleConfig config;
	config.numThread = 2;
	config.backupType = MNN_FORWARD_CPU;
	BackendConfig backendConfig;
	backendConfig.memory = BackendConfig::Memory_Normal;  // �ڴ�
	backendConfig.power = BackendConfig::Power_Normal;    // ����
	backendConfig.precision = BackendConfig::PrecisionMode::Precision_Low;   // ����
	config.backendConfig = &backendConfig;
	Session* session = net->createSession(config);

	// input��output�ǲ���session�ļ������������
	std::string input_name = "images";
	Tensor* input = net->getSessionInput(session, input_name.c_str());
	std::string output_name = "output";
	Tensor* output = net->getSessionOutput(session, output_name.c_str());

	// ת���Լ�mnn������ά��
	net->resizeTensor(input, { 1, 384, 384 });
	net->resizeSession(session);

	Tensor* input_tensor = Tensor::create<float>(input->shape(), NULL, Tensor::CAFFE);
	// ������һһ��ֵ
	for (int i = 0; i < input_tensor->elementSize(); i++) {
		input_tensor->host<float>()[i] = 0.5;
	}
	// ����ֵ��tensorcopy��input�û�session����
	input->copyFromHostTensor(input_tensor);
	// input->printShape();

	// ���лỰ
	net->runSession(session);

	// ����outputһ����С�͸�ʽ��tensor��copy����
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
