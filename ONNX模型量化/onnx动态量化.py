from onnxruntime.quantization import QuantType, quantize_dynamic

# 模型路径
model_fp32 = 'models/arcface.onnx'
model_quant_dynamic = 'models/arcface_quant_dynamic.onnx'

# 动态量化
quantize_dynamic(
    model_input=model_fp32,  # 输入模型
    model_output=model_quant_dynamic,  # 输出模型
    weight_type=QuantType.QUInt8,  # 参数类型 Int8 / UInt8
    optimize_model=True  # 是否优化模型
)