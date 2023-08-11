#ifndef __DE_HDMI_H__
#define __DE_HDMI_H__

void LT8618SXB_I2C_Write_Byte( unsigned char RegAddr, unsigned char data );     // IIC write operation
unsigned int LT8618SXB_I2C_Read_Byte( unsigned char RegAddr );            // IIC read operation
int de_hdmi_init( int hdmi_mode );
VIM_S32 i2c_init(VIM_S8 * name, VIM_S32 slave_addr) ;
VIM_S32 i2cv_write(VIM_S8 * name, VIM_S32 slave_addr, VIM_S32 reg_addr, VIM_S32 reg_val, VIM_S32 addr_width, VIM_S32 val_width, VIM_S32 msb_flag);
VIM_S32 i2cv_read(VIM_S8 * name, VIM_S32 slave_addr, VIM_U32 reg_addr, VIM_U32 *reg_val, VIM_S32 addr_width, VIM_S32 val_width, VIM_S32 msb_flag);

#endif

