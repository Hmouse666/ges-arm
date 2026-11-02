#----------------------摄像头接口时钟---------------------------
#72M
create_clock -period 40.000 -name cam_pclk [get_ports cam_pclk]
set_property CLOCK_DEDICATED_ROUTE FALSE [get_nets cam_pclk_IBUF]


#HDMI
set_property -dict {PACKAGE_PIN J20 IOSTANDARD TMDS_33} [get_ports {TMDS_0_tmds_data_p[2]}]
set_property -dict {PACKAGE_PIN K19 IOSTANDARD TMDS_33} [get_ports {TMDS_0_tmds_data_p[1]}]
set_property -dict {PACKAGE_PIN G19 IOSTANDARD TMDS_33} [get_ports {TMDS_0_tmds_data_p[0]}]
set_property -dict {PACKAGE_PIN J18 IOSTANDARD TMDS_33} [get_ports TMDS_0_tmds_clk_p]



#----------------------摄像头接口---------------------------
set_property -dict {PACKAGE_PIN P14 IOSTANDARD LVCMOS33} [get_ports cam_rst_n]
set_property -dict {PACKAGE_PIN V15 IOSTANDARD LVCMOS33} [get_ports cam_pwdn]
set_property -dict {PACKAGE_PIN R14 IOSTANDARD LVCMOS33} [get_ports {cam_data[0]}]
set_property -dict {PACKAGE_PIN U13 IOSTANDARD LVCMOS33} [get_ports {cam_data[1]}]
set_property -dict {PACKAGE_PIN V13 IOSTANDARD LVCMOS33} [get_ports {cam_data[2]}]
set_property -dict {PACKAGE_PIN U15 IOSTANDARD LVCMOS33} [get_ports {cam_data[3]}]
set_property -dict {PACKAGE_PIN U14 IOSTANDARD LVCMOS33} [get_ports {cam_data[4]}]
set_property -dict {PACKAGE_PIN W13 IOSTANDARD LVCMOS33} [get_ports {cam_data[5]}]
set_property -dict {PACKAGE_PIN V12 IOSTANDARD LVCMOS33} [get_ports {cam_data[6]}]
set_property -dict {PACKAGE_PIN Y14 IOSTANDARD LVCMOS33} [get_ports {cam_data[7]}]
set_property -dict {PACKAGE_PIN T12 IOSTANDARD LVCMOS33} [get_ports cam_href]
set_property -dict {PACKAGE_PIN W14 IOSTANDARD LVCMOS33} [get_ports cam_pclk]
set_property -dict {PACKAGE_PIN U12 IOSTANDARD LVCMOS33} [get_ports cam_vsync]
#cam_scl:
set_property -dict {PACKAGE_PIN T10 IOSTANDARD LVCMOS33} [get_ports {emio_sccb_tri_io[0]}]
#cam_sda:
set_property -dict {PACKAGE_PIN T11 IOSTANDARD LVCMOS33} [get_ports {emio_sccb_tri_io[1]}]

set_property PULLUP true [get_ports {emio_sccb_tri_io[1]}]


set_property PACKAGE_PIN K16 [get_ports key1]
set_property PACKAGE_PIN L14 [get_ports key2]
set_property IOSTANDARD LVCMOS33 [get_ports key1]
set_property IOSTANDARD LVCMOS33 [get_ports key2]



# RGB LCD pixel clock, data enable, backlight and reset
set_property -dict {PACKAGE_PIN P19 IOSTANDARD LVCMOS33} [get_ports LCD_OUT_CLK_0]

set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[23]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[22]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[21]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[20]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[19]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[18]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[17]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[16]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[15]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[14]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[13]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[12]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[11]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[10]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[9]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[8]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[7]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[6]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[5]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[4]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[3]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[2]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[1]}]
set_property IOSTANDARD LVCMOS33 [get_ports {LCD_data_0[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports LCD_EN]
set_property IOSTANDARD LVCMOS33 [get_ports LCD_hsync_0]
set_property IOSTANDARD LVCMOS33 [get_ports LCD_vsync_0]
set_property PACKAGE_PIN N18 [get_ports LCD_hsync_0]
set_property PACKAGE_PIN T20 [get_ports LCD_vsync_0]
set_property PACKAGE_PIN U20 [get_ports LCD_EN]
set_property PACKAGE_PIN W18 [get_ports {LCD_data_0[0]}]
set_property PACKAGE_PIN W19 [get_ports {LCD_data_0[1]}]
set_property PACKAGE_PIN R16 [get_ports {LCD_data_0[2]}]
set_property PACKAGE_PIN R17 [get_ports {LCD_data_0[3]}]
set_property PACKAGE_PIN W20 [get_ports {LCD_data_0[4]}]
set_property PACKAGE_PIN V20 [get_ports {LCD_data_0[5]}]
set_property PACKAGE_PIN P18 [get_ports {LCD_data_0[6]}]
set_property PACKAGE_PIN N17 [get_ports {LCD_data_0[7]}]
set_property PACKAGE_PIN V17 [get_ports {LCD_data_0[8]}]
set_property PACKAGE_PIN V18 [get_ports {LCD_data_0[9]}]
set_property PACKAGE_PIN T17 [get_ports {LCD_data_0[10]}]
set_property PACKAGE_PIN R18 [get_ports {LCD_data_0[11]}]
set_property PACKAGE_PIN Y18 [get_ports {LCD_data_0[12]}]
set_property PACKAGE_PIN Y19 [get_ports {LCD_data_0[13]}]
set_property PACKAGE_PIN P15 [get_ports {LCD_data_0[14]}]
set_property PACKAGE_PIN P16 [get_ports {LCD_data_0[15]}]
set_property PACKAGE_PIN V16 [get_ports {LCD_data_0[16]}]
set_property PACKAGE_PIN W16 [get_ports {LCD_data_0[17]}]
set_property PACKAGE_PIN T14 [get_ports {LCD_data_0[18]}]
set_property PACKAGE_PIN T15 [get_ports {LCD_data_0[19]}]
set_property PACKAGE_PIN Y17 [get_ports {LCD_data_0[20]}]
set_property PACKAGE_PIN Y16 [get_ports {LCD_data_0[21]}]
set_property PACKAGE_PIN T16 [get_ports {LCD_data_0[22]}]
set_property PACKAGE_PIN U17 [get_ports {LCD_data_0[23]}]

set_property IOSTANDARD LVCMOS33 [get_ports LCD_BL_1_0]
set_property IOSTANDARD LVCMOS33 [get_ports LCD_RST_1_0]
set_property PACKAGE_PIN M20 [get_ports LCD_BL_1_0]
set_property PACKAGE_PIN L17 [get_ports LCD_RST_1_0]

#---------------------- capacitive touch panel (I2C over EMIO GPIO) ----------
#touch_scl  R19 -> emio_sccb_tri_io[2]
set_property -dict {PACKAGE_PIN R19 IOSTANDARD LVCMOS33} [get_ports {emio_sccb_tri_io[2]}]
#touch_sda  P20 -> emio_sccb_tri_io[3]
set_property -dict {PACKAGE_PIN P20 IOSTANDARD LVCMOS33} [get_ports {emio_sccb_tri_io[3]}]
#touch_rst_n M19 -> emio_sccb_tri_io[4]
set_property -dict {PACKAGE_PIN M19 IOSTANDARD LVCMOS33} [get_ports {emio_sccb_tri_io[4]}]
#touch_int   U19 -> emio_sccb_tri_io[5]
set_property -dict {PACKAGE_PIN U19 IOSTANDARD LVCMOS33} [get_ports {emio_sccb_tri_io[5]}]

set_property PULLUP true [get_ports {emio_sccb_tri_io[2]}]
set_property PULLUP true [get_ports {emio_sccb_tri_io[3]}]
set_property PULLUP true [get_ports {emio_sccb_tri_io[5]}]
# keep the backlight off while the FPGA is not configured
set_property PULLDOWN true [get_ports LCD_BL_1_0]
