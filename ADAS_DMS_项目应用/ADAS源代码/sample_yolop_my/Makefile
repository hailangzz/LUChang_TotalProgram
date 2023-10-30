-include $(POLARIS_TOP)/build/Makefile.param

COMMON_DIR:=$(PWD)/../comm/
SYSLIBINC += $(POLARIS_INSTALL_LIB)
CFLAGS += -I $(SYSLIBINC)
SYSLIBINC += -I $(COMMON_DIR)  -I $(POLARIS_INSTALL_CHIP_LIB_INC)/syslib/   -I $(POLARIS_INSTALL_ROOT)/vc0768_inc/lib/  -I $(POLARIS_INSTALL_ROOT)/vc0768_inc/lib/npu




COMM_SRC := $(wildcard $(COMMON_DIR)/*.cpp)
COMM_OBJ := $(COMM_SRC:%.cpp=%.o)
CC := $(CROSS_COMPILE)g++
STRIP=$(CROSS_COMPILE)strip
SRC  := $(wildcard *.cpp) 
OBJ  := $(SRC:%.cpp=%.o)
LDFLAGS += -L $(POLARIS_INSTALL_LIB) 
LDFLAGS += -lm -lpthread -lstdc++ -lpthread -lc 
LDFLAGS += -lnpu  -lnpu_alg

#opencv lib 
LDFLAGS += -lopencv_imgcodecs  -lopencv_imgproc -lopencv_tracking  -lopencv_video -lopencv_plot -lopencv_plot  -lopencv_plot -lopencv_flann -lopencv_features2d 
LDFLAGS += -lopencv_calib3d -lopencv_core  -lzlib  -llibpng  -llibjpeg-turbo




CFLAGS += -DARM_LINUX 
TARGET := $(OBJ:%.o=%)
TARGET := sample_yolop
all:  $(TARGET) $(OBJ) $(COMM_OBJ)

$(OBJ): %.o: %.cpp
	$(CC) -Wall -O3 -c $< -o $@ $(CFLAGS) $(INC)
$(COMM_OBJ): %.o:%.cpp
	$(CC) -Wall -c $< -o $@ $(INC) $(CFLAGS)
$(TARGET):  $(COMM_OBJ) $(OBJ)
	$(CC) -Wall -o $@ $^ $(CFLAGS) $(LDFLAGS) $(LIBS) 
	$(STRIP) $(TARGET)	 
install:all
	install -d $(POLARIS_INSTALL_APP)/npu_samples/sample_yolov5_face/
	install -d $(POLARIS_INSTALL_CHIP_LIB_INC)/npu/
	@cp -fav $(TARGET) $(POLARIS_INSTALL_APP)/npu_samples/sample_yolov5_face/
	@cp -fav ./model/ $(POLARIS_INSTALL_APP)/npu_samples/sample_yolov5_face/
	@cp -fav ./images/ $(POLARIS_INSTALL_APP)/npu_samples/sample_yolov5_face/
	@cp -fav ./result/ $(POLARIS_INSTALL_APP)/npu_samples/sample_yolov5_face//sample_yolov5_face/
	@cp -fav ./resource/ $(POLARIS_INSTALL_APP)/npu_samples/sample_yolov5_face/
	@cp -fav ./yolov5-face.html $(POLARIS_INSTALL_APP)/npu_samples/sample_yolov5_face/
	@cp -fav ./readme_yolov5_face.txt $(POLARIS_INSTALL_APP)/npu_samples/sample_yolov5_face/

clean:
	@rm -f $(TARGET)
	@rm -f $(OBJ)
	@rm -f $(COMM_OBJ)
