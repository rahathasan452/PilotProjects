`timescale 1ns / 1ps

module param_alu #(
    parameter int WIDTH = 32
) (
    input  logic [WIDTH-1:0] a,
    input  logic [WIDTH-1:0] b,
    input  logic [3:0]       alu_op,
    output logic [WIDTH-1:0] result,
    output logic             zero,
    output logic             carry,
    output logic             overflow,
    output logic             negative
);

    // Number of bits required to represent shift distances from 0 to WIDTH-1
    localparam int SHIFT_W = (WIDTH > 1) ? $clog2(WIDTH) : 1;

    // Opcode definitions
    localparam logic [3:0] OP_ADD    = 4'b0000;
    localparam logic [3:0] OP_SUB    = 4'b0001;
    localparam logic [3:0] OP_AND    = 4'b0010;
    localparam logic [3:0] OP_OR     = 4'b0011;
    localparam logic [3:0] OP_XOR    = 4'b0100;
    localparam logic [3:0] OP_NOR    = 4'b0101;
    localparam logic [3:0] OP_SLL    = 4'b0110;
    localparam logic [3:0] OP_SRL    = 4'b0111;
    localparam logic [3:0] OP_SRA    = 4'b1000;
    localparam logic [3:0] OP_SLT    = 4'b1001;
    localparam logic [3:0] OP_SLTU   = 4'b1010;
    localparam logic [3:0] OP_PASS_B = 4'b1011;

    // Extended (WIDTH+1)-bit intermediates for clean carry/borrow extraction
    logic [WIDTH:0]     add_ext;
    logic [WIDTH:0]     sub_ext;
    logic [SHIFT_W-1:0] shamt;

    assign shamt   = b[SHIFT_W-1:0];
    assign add_ext = {1'b0, a} + {1'b0, b};
    assign sub_ext = {1'b0, a} - {1'b0, b};

    always_comb begin
        // Default assignments prevent latch inference
        result   = '0;
        carry    = 1'b0;
        overflow = 1'b0;

        unique case (alu_op)
            OP_ADD: begin
                result   = add_ext[WIDTH-1:0];
                carry    = add_ext[WIDTH];
                overflow = (a[WIDTH-1] == b[WIDTH-1]) && (result[WIDTH-1] != a[WIDTH-1]);
            end

            OP_SUB: begin
                result   = sub_ext[WIDTH-1:0];
                carry    = sub_ext[WIDTH]; // 1 indicates unsigned borrow (a < b)
                overflow = (a[WIDTH-1] != b[WIDTH-1]) && (result[WIDTH-1] != a[WIDTH-1]);
            end

            OP_AND: begin
                result = a & b;
            end

            OP_OR: begin
                result = a | b;
            end

            OP_XOR: begin
                result = a ^ b;
            end

            OP_NOR: begin
                result = ~(a | b);
            end

            OP_SLL: begin
                result = a << shamt;
                if (shamt != '0) begin
                    carry = a[WIDTH - shamt];
                end
            end

            OP_SRL: begin
                result = a >> shamt;
                if (shamt != '0) begin
                    carry = a[shamt - 1];
                end
            end

            OP_SRA: begin
                result = $signed(a) >>> shamt;
                if (shamt != '0) begin
                    carry = a[shamt - 1];
                end
            end

            OP_SLT: begin
                result = ($signed(a) < $signed(b)) ? {{(WIDTH-1){1'b0}}, 1'b1} : '0;
            end

            OP_SLTU: begin
                result = (a < b) ? {{(WIDTH-1){1'b0}}, 1'b1} : '0;
            end

            OP_PASS_B: begin
                result = b;
            end

            default: begin
                result   = '0;
                carry    = 1'b0;
                overflow = 1'b0;
            end
        endcase
    end

    // Status flags derived directly from final result
    assign zero     = (result == '0);
    assign negative = result[WIDTH-1];

endmodule