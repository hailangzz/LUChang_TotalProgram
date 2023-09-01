from onnxruntime.quantization import QuantType, quantize_dynamic

# ģ��·��
model_fp32 = 'models/arcface.onnx'
model_quant_dynamic = 'models/arcface_quant_dynamic.onnx'

# ��̬����
quantize_dynamic(
    model_input=model_fp32,  # ����ģ��
    model_output=model_quant_dynamic,  # ���ģ��
    weight_type=QuantType.QUInt8,  # �������� Int8 / UInt8
    optimize_model=True  # �Ƿ��Ż�ģ��
)