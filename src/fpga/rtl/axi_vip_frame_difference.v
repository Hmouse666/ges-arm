// =========================================================================





//手势特征识别及其跟踪模块  输入RGB888数据格式，输出axi数据流



// =========================================================================







module AXI_VIP_Frame_Difference(
    input               clk          ,   
    input               rst_n        ,

    // AXI_ST Slave 接口0，来自摄像头 

    input [23:0]        s0_axis_tdata   , // RGB888 像素输入 [23:16]=R, [15:8]=G, [7:0]=B
    input               s0_axis_tvalid  ,//数据有效信号
    output              s0_axis_tready  ,//握手
    input               s0_axis_tuser   , // 帧起始 SOF
    input               s0_axis_tlast   , // 帧结束 EOL



    input               key1            ,
    input               key2            ,
    // AXI_ST Master 接口，输出给后级显示通路 


    output reg [23:0]   m_axis_tdata    , // YCbCr444 像素输出 [23:16]=Y, [15:8]=Cb, [7:0]=Cr
    output reg          m_axis_tvalid   ,
    input               m_axis_tready   ,
    output reg          m_axis_tuser    ,
    output reg          m_axis_tlast    ,

    input                lcd_bl_en  ,//ps传输的背光拉高信号
    
    
    output wire [31:0]       ps_debug_data1 , //输出给ps的特征数据1
    output wire [10:0]       ps_debug_data2 , //输出给ps的特征数据2
    output wire [10:0]       zero_cnt_debug        , //零游程计数调试输出
    output reg               cool_state_d2          ,
    output reg   [3:0]        final_finger_num_d2    ,
    output wire              LCD_BL_1               ,
    output wire              LCD_RST_1
    
);


// 未使用的从机通道保持接收丢弃，主通道就绪信号直连
assign s1_axis_tready = 1'b1;
// ---------------------------------------------------------------------------
// LCD backlight handshake
//   BL stays low until the PS has prepared the frame buffer and the VDMA has
//   really read the first frame out of DDR and sent it to the LCD.
//   Trigger: first SOF (m_axis_tuser with valid data), then a short settle
//   time (~0.2 s at 100 MHz) before the backlight is switched on.
// ---------------------------------------------------------------------------


assign  LCD_BL_1 = lcd_bl_en ;
assign LCD_RST_1 = 1'b1;
assign s0_axis_tready = m_axis_tready;

// 握手信号
wire s0_axis_handshake = s0_axis_tvalid & s0_axis_tready;


// =========================================================================



// 1. RGB888 转 YCbCr444 计算（内部消耗 3 个时钟周期）



// =========================================================================



wire [7:0] s0_img_y;
wire [7:0] s0_img_cb;
wire [7:0] s0_img_cr;

RGB888_YCbCr444 S0_RGB888_YCbCr444 (
    .clk          (clk),
    .rst_n        (rst_n),
    
    .in_img_red   (s0_axis_tdata[23:16]),
    .in_img_green (s0_axis_tdata[15:8]),
    .in_img_blue  (s0_axis_tdata[7:0]),

    .out_img_Y    (s0_img_y),
    .out_img_Cb   (s0_img_cb),
    .out_img_Cr   (s0_img_cr)

);



//手掌阈值参数设置

localparam [7:0] Y_MIN  = 8'd60;    
localparam [7:0] Y_MAX  = 8'd180;

localparam [7:0] CB_MIN = 8'd80;   
localparam [7:0] CB_MAX = 8'd150; 
 
localparam [7:0] CR_MIN = 8'd130;  
localparam [7:0] CR_MAX = 8'd170;



//手掌肤色判断
reg skin_flag;
always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
        skin_flag <= 1'b0;
    end
    else begin
        if ((s0_img_cb >= CB_MIN) && (s0_img_cb <= CB_MAX) &&
            (s0_img_cr >= CR_MIN) && (s0_img_cr <= CR_MAX)) begin
            
            skin_flag <= 1'b1;  // 判定为手部肤色
            
        end
        else begin

            skin_flag <= 1'b0;  // 判断为背景

        end
    end
end


// =========================================================================

// 2. 统一多级时钟移位链 (共 9 级)

// =========================================================================
reg  finsh_skin_flag;//去除噪声后的值


reg [8:0] valid_shift;

reg [8:0] user_shift;

reg [8:0] last_shift;

reg [2:0] skin_shift;

reg [1:0] num_shift;


always @(posedge clk or negedge rst_n) begin

    if (!rst_n) begin

        valid_shift <= 9'b0;

        user_shift  <= 9'b0;

        last_shift  <= 9'b0;
        skin_shift  <= 3'b0;
        
        num_shift   <= 2'b0;
        

    end

    else begin

        // 把握手有效信号一路往后推 6 拍

        valid_shift    <= {valid_shift[7:0], s0_axis_handshake};

        user_shift     <= {user_shift[7:0],  s0_axis_handshake & s0_axis_tuser};

        last_shift     <= {last_shift[7:0],  s0_axis_handshake & s0_axis_tlast};
        //把输出肤色信号与二值化的信号对齐，延后3拍
        skin_shift     <=  {skin_shift[1:0], finsh_skin_flag};
        
    end

end



// 提取各级生命周期的控制信号

wire dly4_valid = valid_shift[3]; // 第 4 拍有效信号

wire dly5_valid = valid_shift[4]; // 第 5 拍有效信号

wire dly6_valid = valid_shift[5]; // 第 6 拍有效信号
wire dly6_user  = user_shift[5];  // 第 6 拍帧头
wire dly6_last  = last_shift[5];  // 第 6 拍行尾

wire dly7_valid = valid_shift[6]; // 第 7 拍有效信号

wire dly8_valid = valid_shift[7]; // 第 8 拍有效信号

wire dly9_valid = valid_shift[8]; // 第 9 拍有效信号
wire dly9_user  = user_shift[8];  // 第 9 拍帧头
wire dly9_last  = last_shift[8];  // 第 9拍行尾

wire skin_dly9  = skin_shift[2];  //去噪后第九拍的信号
// =========================================================================

// 3. 行缓存：接收第 4 拍的 skin_flag，缓存3行

// =========================================================================

wire row1_data;

wire row2_data;

wire row3_data;

localparam	[10:0]	IMG_HDISP 	= 11'd640;	//图像分辨率
localparam	[10:0]	IMG_VDISP 	= 11'd480;	//



line_buffrf #(

    .IMG_HDISP(IMG_HDISP)

)s0_line_buffrf(

    .clk        (clk),

    .rst_n      (rst_n),

    .clken      (dly4_valid), // 【精准对齐】给第 4 拍使能

    .din        (skin_flag),  // 第 4 拍产出的肤色信号

    

    .row1_data  (row1_data),

    .row2_data  (row2_data),

    .row3_data  (row3_data)            

);





// =========================================================================

// 4. 3x3 滑窗矩阵：由于 line_buffer 是 0 延迟，直接使用 dly4_valid 采样

// =========================================================================

reg p11, p12, p13;

reg p21, p22, p23;

reg p31, p32, p33;



always @(posedge clk or negedge rst_n) begin

     if(!rst_n) begin

        {p11, p12, p13} <= 3'b0;

        {p21, p22, p23} <= 3'b0;

        {p31, p32, p33} <= 3'b0;

     end

     else if (dly4_valid) begin // 第 5 拍上升沿抓入

        {p11, p12, p13} <= {p12, p13, row1_data};

        {p21, p22, p23} <= {p22, p23, row2_data};

        {p31, p32, p33} <= {p32, p33, row3_data};

     end

     else begin

        {p11, p12, p13} <= {p11, p12, p13};

        {p21, p22, p23} <= {p21, p22, p23};

        {p31, p32, p33} <= {p31, p32, p33};

     end

end





// =========================================================================

// 5.形态学去噪 (腐蚀膨胀运算)
// =========================================================================


reg box_skin_flag;   // 计算边框的信号 
wire [3:0] pixel_sum; //像素累加计数器

assign pixel_sum = p11 + p12 + p13 + 
                   p21 + p22 + p23 + 
                   p31 + p32 + p33;

always @ (posedge clk or negedge rst_n) begin
    if(!rst_n) begin
        finsh_skin_flag <= 1'b0;
        box_skin_flag   <= 1'b0;
    end
    else if (dly5_valid) begin
        
        // 【轨道1】：屏幕显示的二值化图像 (宽松，只要 >= 4 就强行填白，修补手掌漏洞)
        if (pixel_sum >= 4'd4) begin
            finsh_skin_flag <= 1'b1; 
        end
        else begin
            finsh_skin_flag <= 1'b0;
        end

        // 【轨道2】：画框专用的高抗噪信号 (防噪点拉扯框)
        if (pixel_sum >= 4'd8) begin
            box_skin_flag <= 1'b1;
        end
        else begin
            box_skin_flag <= 1'b0;
        end
        
    end
    else begin
        finsh_skin_flag <= 1'b0;
        box_skin_flag   <= 1'b0;
    end
end
//计算去噪后的像素坐标
reg         [10:0] x_cnt;    
reg         [10:0] y_cnt;    

always @ (posedge clk or negedge rst_n) begin
    if(!rst_n) begin
        x_cnt <= 10'd0;
        y_cnt <= 10'd0;
    end
    else if(dly6_valid) begin
        if (dly6_last)begin
             x_cnt <= 11'd0;   
             y_cnt <= y_cnt + 1'b1;   
        end
        else if(dly6_user) begin
            x_cnt <= 10'd0;       
            y_cnt <= 10'd0;
        end
        else begin
            x_cnt <= x_cnt + 1'b1;
        end       
    end
end  
// =========================================================================
//6.提取手掌手指最大矩形边框
// =========================================================================

reg [10:0] 	up_reg	 ;	  
reg [10:0] 	down_reg ;
reg [10:0] 	left_reg ;
reg [10:0] 	right_reg;

reg [10:0] 	out_up_reg	 ;
reg [10:0] 	out_down_reg ;
reg [10:0] 	out_left_reg ;
reg [10:0] 	out_right_reg;

reg			flag_reg ;
reg [18:0]  area_reg;

localparam STEP_LIMIT   = 11'd20;
localparam MIN_HAND_AREA = 19'd1000;
reg [4:0] skin_streak_cnt;

always @(posedge clk or negedge rst_n) begin
    if(!rst_n) begin
        skin_streak_cnt <= 5'd0;
    end
    else if(dly6_user || dly6_last) begin // 换行或换帧清零
        skin_streak_cnt <= 5'd0;
    end
    else if(dly6_valid) begin
        if(box_skin_flag) begin
            if(skin_streak_cnt < 5'd16)
                skin_streak_cnt <= skin_streak_cnt + 1'b1;
        end
        else begin
            skin_streak_cnt <= 5'd0; // 遇到背景像素立即清零打断
        end
    end
end

// Adaptive minimum skin-run length based on the tracked hand size.
// A distant hand has narrower fingers, so the fixed 8-pixel rule was
// dropping valid fingers and making an open hand look like a fist.
wire [10:0] tracked_hand_width =
    (out_right_reg > out_left_reg) ? (out_right_reg - out_left_reg) : 11'd0;
wire [4:0] min_skin_run =
    (tracked_hand_width == 11'd0)  ? 5'd8 :
    (tracked_hand_width < 11'd80)  ? 5'd4 :
    (tracked_hand_width < 11'd140) ? 5'd6 : 5'd8;
wire valid_skin_run = (skin_streak_cnt >= min_skin_run);

always @(posedge clk or negedge rst_n) begin
    if(!rst_n) begin
        up_reg    <= IMG_VDISP;
        down_reg  <= 11'd0;
        left_reg  <= IMG_HDISP;
        right_reg <= 11'd0;
        flag_reg  <= 1'b0;
        area_reg  <= 19'd0;
    end
    else if(dly6_user) begin             // 每一帧开头重新初始化
        up_reg    <= IMG_VDISP;
        down_reg  <= 11'd0;
        left_reg  <= IMG_HDISP;
        right_reg <= 11'd0;
        flag_reg  <= 1'b0;
        area_reg  <= 19'd0;
    end
    else if(box_skin_flag && dly6_valid) begin
        flag_reg <= 1'b1;                // 手掌标志置 1
        area_reg <= area_reg + 1'b1;

        if(valid_skin_run) begin
            // 真实手掌的左边界回退 8 个像素对齐起点（加防下溢）
            if(x_cnt >= 11'd8) begin
                if((x_cnt - 11'd8) < left_reg) left_reg <= x_cnt - 11'd8;
            end
            else begin
                left_reg <= 11'd0;
            end

            if(x_cnt > right_reg) right_reg <= x_cnt;
            if(y_cnt < up_reg)    up_reg    <= y_cnt;
            if(y_cnt > down_reg)  down_reg  <= y_cnt;
        end
    end
end
//帧尾添加限幅

reg [10:0] next_left, next_right, next_up, next_down;

always @(*) begin
    // 默认保持原值
    next_left  = out_left_reg;
    next_right = out_right_reg;
    next_up    = out_up_reg;
    next_down  = out_down_reg;

    if(flag_reg && (area_reg >= MIN_HAND_AREA)) begin
        // 左边界
        if(left_reg > out_left_reg + STEP_LIMIT)
            next_left = out_left_reg + STEP_LIMIT;
        else if((out_left_reg > STEP_LIMIT) && (left_reg < out_left_reg - STEP_LIMIT))
            next_left = out_left_reg - STEP_LIMIT;
        else
            next_left = left_reg;

        // 右边界
        if(right_reg > out_right_reg + STEP_LIMIT)
            next_right = out_right_reg + STEP_LIMIT;
        else if((out_right_reg > STEP_LIMIT) && (right_reg < out_right_reg - STEP_LIMIT))
            next_right = out_right_reg - STEP_LIMIT;
        else
            next_right = right_reg;

        // 上边界
        if(up_reg > out_up_reg + STEP_LIMIT)
            next_up = out_up_reg + STEP_LIMIT;
        else if((out_up_reg > STEP_LIMIT) && (up_reg < out_up_reg - STEP_LIMIT))
            next_up = out_up_reg - STEP_LIMIT;
        else
            next_up = up_reg;

        // 下边界
        if(down_reg > out_down_reg + STEP_LIMIT)
            next_down = out_down_reg + STEP_LIMIT;
        else if((out_down_reg > STEP_LIMIT) && (down_reg < out_down_reg - STEP_LIMIT))
            next_down = out_down_reg - STEP_LIMIT;
        else
            next_down = down_reg;
    end
end
//*****************************************************
//分出手掌手指部分
// 手指框 (上半部分: Y 从 top 到 split_y)
reg [10:0] finger_up;
reg [10:0] finger_down;
reg [10:0] finger_left;
reg [10:0] finger_right;

// 手掌框 (下半部分: Y 从 split_y 到 bottom)
reg [10:0] palm_up;
reg [10:0] palm_down;
reg [10:0] palm_left;
reg [10:0] palm_right;

reg hand_valid_flag; // 目标有效标志

always @(posedge clk or negedge rst_n) begin
    if(!rst_n) begin
        out_left_reg    <= 11'd0;
        out_right_reg   <= 11'd0;
        out_up_reg      <= 11'd0;
        out_down_reg    <= 11'd0;
        finger_left     <= 11'd0;
        finger_right    <= 11'd0;
        finger_up       <= 11'd0;
        finger_down     <= 11'd0;
        palm_left       <= 11'd0;
        palm_right      <= 11'd0;
        palm_up         <= 11'd0;
        palm_down       <= 11'd0;
        hand_valid_flag <= 1'b0;
    end
    else if(dly6_last && (y_cnt == IMG_VDISP - 1'b1)) begin
        if(flag_reg && (area_reg >= MIN_HAND_AREA)) begin
            hand_valid_flag <= 1'b1;

            // 同步更新总框
            out_left_reg    <= next_left;
            out_right_reg   <= next_right;
            out_up_reg      <= next_up;
            out_down_reg    <= next_down;

            // 使用 next_* 赋值防止 1 帧的滞后
            finger_left     <= next_left;
            finger_right    <= next_right;
            finger_up       <= next_up;
            finger_down     <= (next_up + next_down) >> 1;

            palm_left       <= next_left;
            palm_right      <= next_right;
            palm_up         <= (next_up + next_down) >> 1;
            palm_down       <= next_down;
        end
        else begin
            hand_valid_flag <= 1'b0;
        end
    end
end


// =========================================================================
// 7.手指计数 + 抗黑洞握拳最大连通域检测
// =========================================================================
reg [3:0]  current_finger_count; 
reg [3:0]  final_finger_num;     

reg        finger_active; 
reg [10:0] zero_cnt;   
reg [10:0] r_cnt;

assign zero_cnt_debug = zero_cnt;

reg [10:0] current_block_width;
reg [10:0] max_block_width;
reg        cool_state;

// 1. 修正几何坐标计算（★ 彻底修复切线飞出的 Bug）
// 横向切线：精准切在手指框正中间
wire [10:0] scan_line_y = (finger_up  + finger_down) >> 1  ; 

// 框体宽高
wire [10:0] box_width  = (out_right_reg > out_left_reg) ? (out_right_reg - out_left_reg) : 11'd0;
wire [10:0] box_height = (out_down_reg  > out_up_reg)   ? (out_down_reg  - out_up_reg)   : 11'd0;

// Dynamic gap tolerance: fill a gap until it reaches three quarters of
// the current finger width. Only a gap wider than that is a real gap.
localparam [10:0] MIN_GAP_TOLERANCE = 11'd2;
wire [10:0] current_skin_width =
    (current_block_width > zero_cnt) ? (current_block_width - zero_cnt) : 11'd1;
wire [10:0] gap_tolerance =
    (current_skin_width < 11'd4)
        ? MIN_GAP_TOLERANCE
        : (current_skin_width  >> 2);

always @(posedge clk or negedge rst_n) begin
    if(!rst_n) begin
        current_finger_count <= 4'd0;
        final_finger_num     <= 4'd0;
        finger_active        <= 1'b0;
        zero_cnt <= 11'd0;
        r_cnt                <= 11'd0;
        current_block_width  <= 11'd0;
        max_block_width      <= 11'd0;
        cool_state           <= 1'b0;
    end
    else if(dly6_valid) begin

        // ========================================================
        // 核心：只在扫描线这一行 (y_cnt == scan_line_y) 运行横向状态机
        // ========================================================
        if(y_cnt == scan_line_y) begin
            
            // 仅在手框内部统计
            if(x_cnt >= finger_left && x_cnt <= finger_right) begin
                if(box_skin_flag == 1'b1) begin
                    zero_cnt <= 11'd0; // 只要是肉色，阴影计数清零
                    
                    if(finger_active == 1'b0) begin
                        finger_active        <= 1'b1;
                        current_finger_count <= current_finger_count + 1'b1;
                        current_block_width  <= 11'd1;
                    end
                    else begin
                        current_block_width  <= current_block_width + 1'b1;
                    end
                end
                else begin // 遇到黑色/阴影
                    if(finger_active == 1'b1) begin
                        // 还没超出容忍阈值，先算作肉褶继续累加
                        if(zero_cnt <= gap_tolerance) begin
                            zero_cnt            <= zero_cnt + 1'b1;
                            current_block_width <= current_block_width + 1'b1;
                        end
                        else begin
                            // 确定断开了（是指缝或出框）
                            finger_active <= 1'b0;
                            zero_cnt <= 11'd0;

                            // 防下溢安全减法
                            if(current_block_width > zero_cnt) begin
                                if((current_block_width - zero_cnt) > max_block_width)
                                    max_block_width <= current_block_width - zero_cnt;
                            end
                        end
                    end
                end
            end

            // 扫描线到达手指右边界或行尾：强制结算并安全收尾
            if(x_cnt == finger_right || dly6_last) begin
                if(finger_active == 1'b1) begin
                    if(current_block_width > zero_cnt) begin
                        if((current_block_width - zero_cnt) > max_block_width)
                            max_block_width <= current_block_width - zero_cnt;
                    end
                end
                finger_active       <= 1'b0;
                zero_cnt <= 11'd0;
                current_block_width <= 11'd0;
            end
        end

        // ========================================================
        // 帧尾统一裁决（整帧图像扫描完毕）
        // ========================================================
        if(dly6_last && (y_cnt == IMG_VDISP - 1'b1)) begin
            final_finger_num <= current_finger_count;

            // 握拳判定核心：
            // 握拳时整个手聚拢，手指被桥接后，单块最大宽度往往占整个手宽的 50% 以上（box_width >> 1）
            // 并且检测出的独立手指数量极少（通常 <= 1）
            if((max_block_width > (box_width >> 1)) || (current_finger_count <= 4'd1 && max_block_width > (box_width * 3 >> 3))) begin
                cool_state <= 1'b1; // 握拳
            end
            else begin
                cool_state <= 1'b0; // 张手
            end

            // 清零计数器，为下一帧做准备
            current_finger_count <= 4'd0;
            current_block_width  <= 11'd0;
            max_block_width      <= 11'd0;
            r_cnt                <= 11'd0;
        end
    end
end
//同步信号，打两拍
reg [3:0] final_finger_num_d1; // 第一拍
reg       cool_state_d1;       //握拳状态一拍

// 时序逻辑实现打两拍
always @(posedge clk or negedge rst_n) begin
    if(!rst_n) begin
        final_finger_num_d1 <= 4'd0;
        final_finger_num_d2 <= 4'd0;
        cool_state_d1       <= 1'd0;
        cool_state_d2       <= 1'd0;
    end
    else begin
        // 像流水线一样，把数据一级一级往后传
        final_finger_num_d1 <= final_finger_num;    
        final_finger_num_d2 <= final_finger_num_d1; 
        cool_state_d1       <= cool_state;
        cool_state_d2       <= cool_state_d1;
    end
end
// =========================================================================
// 8.矩形框绘制
// =========================================================================

reg border_finger_flag; // 手指边框标志 (红)
reg border_palm_flag;   // 手掌边框标志 (绿)
reg center_flag;        //中心点坐标（红）
// x 取手掌左右边界的中间，y 取手指顶端和手掌底端的中间
wire [10:0] center_x = (palm_left + palm_right) >> 1;
wire [10:0] center_y = finger_up + 10 'd100 ;
wire [10:0] width= palm_right - palm_left;
always @ (posedge clk or negedge rst_n) begin
    if(!rst_n) begin
        border_finger_flag <= 1'b0;
        border_palm_flag   <= 1'b0;
    end
    else if(dly8_valid) begin
        if(hand_valid_flag)begin
            // 1. 手指边缘判断（红色）
            if( ((y_cnt == finger_up || y_cnt == finger_down) && (x_cnt >= finger_left && x_cnt <= finger_right)) ||
                ((x_cnt == finger_left || x_cnt == finger_right) && (y_cnt >= finger_up && y_cnt <= finger_down)) ) begin 
                
                border_finger_flag  <= 1'b1;
            end
            else begin 
                border_finger_flag  <= 1'b0; 
            end

            // 2. 手掌边框判断 (绿色) 
            if( ((y_cnt == palm_up || y_cnt == palm_down) && (x_cnt >= palm_left && x_cnt <= palm_right)) ||
                ((x_cnt == palm_left || x_cnt == palm_right) && (y_cnt >= palm_up && y_cnt <= palm_down)) ) begin
                
                border_palm_flag <= 1'b1; // 如果当前坐标踩到了线，拉高！
            end
            else begin
                border_palm_flag <= 1'b0;
            end
            // 3. 手掌中心判断 (红色 6x6 小方块)
            if ( (x_cnt + 3 >= center_x && x_cnt <= center_x + 3) &&
                 (y_cnt + 3 >= center_y && y_cnt <= center_y + 3) ) begin
                
                center_flag <= 1'b1; 
            end
            else begin
                center_flag <= 1'b0;
            end
        end
        else begin
            border_finger_flag <= 1'b0;
            border_palm_flag <= 1'b0;
        end
    end  
end

// =========================================================================
//9. 按键信号控制
// =========================================================================
wire     mode1;
wire     mode2;
key u_key1 (
    .sys_clk   (clk),
    .sys_rst_n (rst_n),
    .key       (key1),
    .key_out   ( mode1)
);
key u_key2 (
    .sys_clk   (clk),
    .sys_rst_n (rst_n),
    .key       (key2),
    .key_out   ( mode2)
);

// =========================================================================
//10. AXI4-Stream Master 接口寄存器输出
// =========================================================================

always @ (posedge clk or negedge rst_n ) begin
	if(!rst_n) begin
		 m_axis_tvalid   <= 1'd0;
         m_axis_tuser    <= 1'd0;
         m_axis_tlast    <= 1'd0;
         m_axis_tdata    <= 24'd0;
     
	end
    else begin
         m_axis_tvalid  <= dly9_valid;
         m_axis_tuser   <= dly9_user ;
         m_axis_tlast   <= dly9_last ;
         
        if(border_finger_flag) begin
            m_axis_tdata <= 24'hFF_00_00; // 纯红：手指边界 (RGB888: R=0xFF, G=0x00, B=0x00)
        end
        else if(border_palm_flag) begin
            m_axis_tdata <= 24'h00_FF_00; // 纯绿：手掌边界 (RGB888: R=0x00, G=0xFF, B=0x00)
        end
         else if(center_flag) begin
            m_axis_tdata <= 24'hFF_00_00; // 纯绿：手掌边界 (RGB888: R=0x00, G=0xFF, B=0x00)
        end
        else begin
            m_axis_tdata <= skin_dly9 ? 24'hFF_FF_FF : 24'h00_00_00;
        end
    end
end

assign ps_debug_data1 = {3'd0,mode1,mode2,cool_state_d2, final_finger_num_d2, center_y, center_x};
assign ps_debug_data2 = {width};
// ILA has been moved to the Block Design; do not instantiate it again here.

endmodule






// Include the line buffer here so the module-reference OOC synthesis sees it.

`ifndef LINE_BUFFRF_V

`include "../../../../new/line_buffer.v"

`endif
`ifndef KEY_V

`include "../../../../new/key.v"

`endif
