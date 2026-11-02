// Double line buffer for the 3x3 morphology pipeline.
// Kept as a separate source file and included by AXI_VIP_Frame_Difference.v
// so both top-level and module-reference OOC synthesis can find it.
`ifndef LINE_BUFFRF_V
`define LINE_BUFFRF_V

module line_buffrf #(
    parameter IMG_HDISP = 11'd640
)(
    input               clk,
    input               rst_n,
    input               clken,
    input               din,

    output              row1_data,
    output              row2_data,
    output              row3_data
);

reg [IMG_HDISP-1:0] shift_reg_row1;
reg [IMG_HDISP-1:0] shift_reg_row2;

always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
        shift_reg_row1 <= {IMG_HDISP{1'b0}};
        shift_reg_row2 <= {IMG_HDISP{1'b0}};
    end
    else if (clken) begin
        shift_reg_row2 <= {shift_reg_row2[IMG_HDISP-2:0], din};
        shift_reg_row1 <= {shift_reg_row1[IMG_HDISP-2:0], shift_reg_row2[IMG_HDISP-1]};
    end
end

assign row3_data = din;
assign row2_data = shift_reg_row2[IMG_HDISP-1];
assign row1_data = shift_reg_row1[IMG_HDISP-1];

endmodule

`endif