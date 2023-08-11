-include $(POLARIS_TOP)/build/Makefile.param

ifeq ($(POLARIS_PF_SOC),VC0768)
    PLAT_FL += -D__CONFIG_V7
endif

#TARGET  = algorithemService
TARGET = cameraservice

DCVI_INCLUDE=$(POLARIS_INSTALL_CHIP_LIB_INC)/dcvi
SYS_INCLUDE=$(POLARIS_INSTALL_CHIP_LIB_INC)/syslib
INI_INCLUDE=$(POLARIS_INSTALL_CHIP_LIB_INC)/iniparserlib/
BINDER_INCLUDE=$(POLARIS_INSTALL_CHIP_LIB_INC)/vbinderlib/
VO_INCLUDE=$(POLARIS_INSTALL_CHIP_LIB_INC)/volib/
HDMI_INCLUDE=$(POLARIS_INSTALL_CHIP_LIB_INC)/hdmi/
VPSS_INCLUDE=$(POLARIS_INSTALL_CHIP_LIB_INC)/vpss/
VENC_INCLUDE=$(POLARIS_INSTALL_CHIP_LIB_INC)/vcodeclib/

INCDIR = -I.
INCDIR += -I..
INCDIR += -I./common
INCDIR += -I./modelDms
INCDIR += -I./modelDms/opencv4
INCDIR += -I./modelDms/include

INCDIR += -I$(RR_TOP)/common/include/
INCDIR += -I$(RR_TOP)/common/include/binder
INCDIR += -I$(RR_TOP)/common/include/binder/include 
INCDIR += -I$(RR_TOP)/common/include/mcuprotocol
INCDIR += -I$(RR_TOP)/common/include/settings
INCDIR += -I$(RR_TOP)/common/include/tdrservice
INCDIR += -I$(RR_TOP)/common/sysroot/include

INCDIR += -I$(DCVI_INCLUDE) 
INCDIR += -I$(PLAT_FL)
INCDIR += -I$(SYS_INCLUDE)
INCDIR += -I$(INI_INCLUDE)
INCDIR += -I$(BINDER_INCLUDE)
INCDIR += -I$(VO_INCLUDE) 
INCDIR += -I$(HDMI_INCLUDE)
INCDIR += -I$(VPSS_INCLUDE)
INCDIR += -I$(VENC_INCLUDE)

SRCDIRS   += modelDms

OBJDIR=obj
#ifneq ($(OBJDIR), $(wildcard $(OBJDIR)))
#$(shell mkdir -p $(OBJDIR))
#endif
CPPFILES  = $(foreach dir, $(SRCDIRS), $(wildcard $(dir)/*.cpp))
CPPFILES += main.cpp

CFILES = ./common/sample_comm_dcvi.c
CFILES += ./common/sample_comm_vo.c
CFILES += ./common/de_hdmi.c
CFILES += ./common/sample_common_vpss.c
CFILES += ./common/sample_common_vpss_param.c
CFILES += ./common/sample_comm_venc.c
CFILES += ./common/sample_comm_string.c
CFILES += ./common/sample_common_region.c
CFILES += ./common/loadbmp.c
CFILES += $(wildcard ./*.c)

CPPOBJS = $(patsubst %.cpp, %.o, $(CPPFILES))
COBJS = $(patsubst %.c, %.o, $(CFILES))

OBJSNDIR = $(patsubst %, %, $(notdir $(COBJS)))
#OBJSNDIR = $(patsubst %, $(OBJDIR)/%, $(notdir $(OBJS)))

CXXFLAGS  = -pipe -std=gnu++11 -W -D_REENTRANT -fPIC $(DEFINES) -DBINDER_IPC_32BIT=1

#CFLAGS += -Wall -fPIC -g -DBINDER_IPC_32BIT=1
CFLAGS+= -g -rdynamic  -funwind-tables -ffunction-sections -Wall -Wextra -pthread -ldl -lm
CFLAGS+= -Wextra -ftree-vectorize -mfpu=neon -mcpu=cortex-a7 -ffast-math -mfloat-abi=hard 

LDFLAGS = -L$(POLARIS_IMG_SYS_DIR)/lib
LDFLAGS += -L$(POLARIS_IMG_SYS_DIR)/lib_a
LDFLAGS += -L$(RR_TOP)/middleware/libs
LDFLAGS += -L$(POLARIS_IMG_SYS_DIR)/app/HawkDeploy/lib
LDFLAGS += -L$(RR_TOP)/framework/services/cameraservice
LDFLAGS += -liniparser -lsys -lvo -lbinder -l_dcvi -lmpi_vpss -lvreg -lVimRecordSdk -lvpu_vcodec -lhdmi -l:libasound.a -lLCv10
LDFLAGS += -lhawksdk -lnetsdk -lkit -lcore -lcom -lui -lcaritlv -lulink -lvsl -lvgw -ljson-c -lvsec_lib -lZXSvacDec
LDFLAGS += -llbinder -lrrlist -lrrhb -lrrpipe
LDFLAGS += -lrrsettingsclient -lrrmcuprotocolclient
LDFLAGS += -lrrplatform

CXXLIB = -L$(RR_TOP)/external/3party/lib
CXXLIB += -L$(RR_TOP)/external/3party/libs
CXXLIB += -L$(RR_TOP)/external/3party/outlib
CXXLIB += -L./modelDms
CXXLIB += -lopencv_features2d \
         -lopencv_highgui \
         -lopencv_calib3d \
         -lopencv_core \
         -lopencv_flann \
         -lopencv_imgcodecs \
         -lopencv_imgproc \
         -lopencv_ml \
         -lopencv_photo \
         -lopencv_stitching \
         -lopencv_video \
         -lopencv_videoio \
	 -lZXWAlg \
	 -lDMS_interface_Library \
	 -lADAS_interface_Library

.PHONY: all
all: $(TARGET)
	@cp -f $(TARGET) $(RR_TOP)/middleware/apps/	

#.PHONY: clean install
%.o: %.c
	$(CC) -c $< -o $@ $(INCDIR) $(CFLAGS) $(LDFLAGS) $(CXXLIB) 
%.o: %.cpp
	$(CXX) -c $< -o $@ $(INCDIR) $(CFLAGS) $(CXXFLAGS) $(LDFLAGS)

$(TARGET): $(CPPOBJS) $(COBJS)
	$(CXX) $^ $(CFLAGS) $(CXXFLAGS) $(CXXLIB) $(LDFLAGS) -o $(TARGET) 

install:
	@cp -f $(TARGET) /home/wujianhe/wrkSpace/rootfs/app/
clean:
#	@echo CFLAGS:$(CFLAGS)
	@rm -f $(TARGET) $(COBJS) $(CPPOBJS)
	@rm -f $(RR_TOP)/middleware/apps/$(TARGET)
