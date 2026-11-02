`timescale 1ns / 1ps
`ifndef KEY_V
`define KEY_V
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 2026/09/21 19:47:42
// Design Name: 
// Module Name: key1
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module key(
    input   sys_clk,
    input   sys_rst_n,
    input   key,
    output  reg key_out
    );
 parameter  CNT_MAX = 20'd100_0000;    //消抖时间20ms
 reg    key_d0;
 reg    key_d1;
 reg    [19:0] CNT;
 
 
//对按键端口的数据延迟两个时钟周期
always @ (posedge sys_clk or negedge sys_rst_n) begin
    if(!sys_rst_n) begin
        key_d0 <= 1'b1;
        key_d1 <= 1'b1;
        
    end
    else begin
        key_d0 <= key;
        key_d1 <= key_d0;
    end 
end

 always @ (posedge sys_clk or negedge sys_rst_n) begin
        if(!sys_rst_n)begin
                CNT     <= 20'b0;
        end
        else begin           
            if(key_d0 != key_d1)  begin
                CNT  <= CNT_MAX;
            end
            else if(CNT > 20'd0 ) begin
                CNT  <= CNT -1'b1 ;
            end  
            else  begin
                CNT  <= 20'b0;
            end 
         end              
 end
 
 always @ (posedge sys_clk or negedge sys_rst_n) begin
        if(!sys_rst_n)begin
            key_out <= 1'b1;
        end
        else begin
            if(CNT <= 1'b0) begin 
                key_out <= key_d1 ;
            end
            else begin
                key_out <= key_out;
            end
        end       
end
  
endmodule


`endif