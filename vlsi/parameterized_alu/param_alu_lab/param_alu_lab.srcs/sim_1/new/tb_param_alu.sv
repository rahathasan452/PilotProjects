`timescale 1ns / 1ps

module tb_param_alu;

    // Change TB_WIDTH to 8, 16, 32, or 64 to verify parameterization
    localparam int TB_WIDTH = 8;
    localparam int SHIFT_W  = (TB_WIDTH > 1) ? $clog2(TB_WIDTH) : 1;

    // DUT Signals
    logic [TB_WIDTH-1:0] a;
    logic [TB_WIDTH-1:0] b;
    logic [3:0]          alu_op;
    logic [TB_WIDTH-1:0] result;
    logic                zero;
    logic                carry;
    logic                overflow;
    logic                negative;

    // Test Tracking Counters
    int total_tests = 0;
    int pass_count  = 0;
    int fail_count  = 0;

    // Instantiate Device Under Test (DUT)
    param_alu #(
        .WIDTH(TB_WIDTH)
    ) dut (
        .a        (a),
        .b        (b),
        .alu_op   (alu_op),
        .result   (result),
        .zero     (zero),
        .carry    (carry),
        .overflow (overflow),
        .negative (negative)
    );

    // Self-Checking Golden Reference Task
    task automatic verify_vector(
        input logic [TB_WIDTH-1:0] in_a,
        input logic [TB_WIDTH-1:0] in_b,
        input logic [3:0]          in_op,
        input string               test_name
    );
        logic [TB_WIDTH-1:0] exp_res;
        logic                exp_z, exp_c, exp_v, exp_n;
        logic [TB_WIDTH:0]   tmp_wide;
        logic [SHIFT_W-1:0]  sh;

        begin
            a      = in_a;
            b      = in_b;
            alu_op = in_op;
            #10; // Allow combinational logic to settle

            exp_res = '0;
            exp_c   = 1'b0;
            exp_v   = 1'b0;
            sh      = in_b[SHIFT_W-1:0];

            case (in_op)
                4'b0000: begin // ADD
                    tmp_wide = {1'b0, in_a} + {1'b0, in_b};
                    exp_res  = tmp_wide[TB_WIDTH-1:0];
                    exp_c    = tmp_wide[TB_WIDTH];
                    exp_v    = (in_a[TB_WIDTH-1] == in_b[TB_WIDTH-1]) &&
                               (exp_res[TB_WIDTH-1] != in_a[TB_WIDTH-1]);
                end
                4'b0001: begin // SUB
                    tmp_wide = {1'b0, in_a} - {1'b0, in_b};
                    exp_res  = tmp_wide[TB_WIDTH-1:0];
                    exp_c    = tmp_wide[TB_WIDTH];
                    exp_v    = (in_a[TB_WIDTH-1] != in_b[TB_WIDTH-1]) &&
                               (exp_res[TB_WIDTH-1] != in_a[TB_WIDTH-1]);
                end
                4'b0010: exp_res = in_a & in_b;
                4'b0011: exp_res = in_a | in_b;
                4'b0100: exp_res = in_a ^ in_b;
                4'b0101: exp_res = ~(in_a | in_b);
                4'b0110: begin // SLL
                    exp_res = in_a << sh;
                    if (sh != '0) exp_c = in_a[TB_WIDTH - sh];
                end
                4'b0111: begin // SRL
                    exp_res = in_a >> sh;
                    if (sh != '0) exp_c = in_a[sh - 1];
                end
                4'b1000: begin // SRA
                    exp_res = $signed(in_a) >>> sh;
                    if (sh != '0) exp_c = in_a[sh - 1];
                end
                4'b1001: exp_res = ($signed(in_a) < $signed(in_b)) ? {{(TB_WIDTH-1){1'b0}}, 1'b1} : '0;
                4'b1010: exp_res = (in_a < in_b) ? {{(TB_WIDTH-1){1'b0}}, 1'b1} : '0;
                4'b1011: exp_res = in_b;
                default: exp_res = '0;
            endcase

            exp_z = (exp_res == '0);
            exp_n = exp_res[TB_WIDTH-1];

            total_tests++;

            if ((result   === exp_res) &&
                (zero     === exp_z)   &&
                (carry    === exp_c)   &&
                (overflow === exp_v)   &&
                (negative === exp_n)) begin
                pass_count++;
            end else begin
                fail_count++;
                $error("[FAIL] %s | op=%b a=0x%0h b=0x%0h | GOT res=0x%0h Z=%b C=%b V=%b N=%b | EXP res=0x%0h Z=%b C=%b V=%b N=%b",
                       test_name, in_op, in_a, in_b,
                       result, zero, carry, overflow, negative,
                       exp_res, exp_z, exp_c, exp_v, exp_n);
            end
        end
    endtask

    // Main Stimulus Sequence
    initial begin
        logic [TB_WIDTH-1:0] max_pos;
        logic [TB_WIDTH-1:0] min_neg;
        logic [TB_WIDTH-1:0] all_ones;

        max_pos  = {1'b0, {(TB_WIDTH-1){1'b1}}}; // e.g., 0x7F (+127) for 8-bit
        min_neg  = {1'b1, {(TB_WIDTH-1){1'b0}}}; // e.g., 0x80 (-128) for 8-bit
        all_ones = {TB_WIDTH{1'b1}};             // e.g., 0xFF (-1 / 255) for 8-bit

        $display("=== Starting Self-Checking ALU Verification (WIDTH = %0d) ===", TB_WIDTH);

        // 1. Directed Arithmetic & Flag Corner Cases
        verify_vector('0,       '0,       4'b0000, "ADD Zero + Zero");
        verify_vector(max_pos,  'd1,      4'b0000, "ADD Signed Positive Overflow");
        verify_vector(all_ones, 'd1,      4'b0000, "ADD Unsigned Carry & Zero");
        verify_vector(min_neg,  min_neg,  4'b0000, "ADD Signed Negative Overflow + Carry");

        verify_vector('d25,     'd25,     4'b0001, "SUB Equal Operands -> Zero");
        verify_vector('d0,      'd1,      4'b0001, "SUB Unsigned Borrow & Negative");
        verify_vector(min_neg,  'd1,      4'b0001, "SUB Signed Negative Underflow");
        verify_vector(max_pos,  all_ones, 4'b0001, "SUB Positive Minus Negative Overflow");

        // 2. Directed Bitwise Logic Tests
        verify_vector(8'hAA,    8'h55,    4'b0010, "AND Alternating Bits -> Zero");
        verify_vector(8'hAA,    8'h55,    4'b0011, "OR Alternating Bits -> All Ones");
        verify_vector(all_ones, all_ones, 4'b0100, "XOR Identical -> Zero");
        verify_vector('0,       '0,       4'b0101, "NOR Zeros -> All Ones");

        // 3. Directed Shift Tests
        verify_vector(8'h81,    'd1,      4'b0110, "SLL Shift Out MSB -> Carry");
        verify_vector(8'h81,    'd1,      4'b0111, "SRL Zero Fill MSB");
        verify_vector(min_neg,  'd3,      4'b1000, "SRA Sign Extension");

        // 4. Directed Comparison & Pass-Through Tests
        verify_vector(all_ones, 'd1,      4'b1001, "SLT Signed (-1 < +1 == 1)");
        verify_vector(all_ones, 'd1,      4'b1010, "SLTU Unsigned (255 < 1 == 0)");
        verify_vector('d12,     8'h5A,    4'b1011, "PASS_B Forward Operand B");

        // 5. Randomized Regression across all valid opcodes
        for (int i = 0; i < 500; i++) begin
            logic [TB_WIDTH-1:0] rand_a, rand_b;
            logic [3:0]          rand_op;
            rand_a  = TB_WIDTH'($urandom());
            rand_b  = TB_WIDTH'($urandom());
            rand_op = 4'($urandom_range(0, 11));
            verify_vector(rand_a, rand_b, rand_op, $sformatf("RANDOM_TEST_%0d", i));
        end

        // Final Summary Banner
        $display("==========================================================");
        $display("ALU Verification Complete: %0d Total | %0d PASSED | %0d FAILED",
                 total_tests, pass_count, fail_count);
        $display("==========================================================");

        if (fail_count == 0)
            $display("STATUS: ALL TESTS PASSED SUCCESSFULLY!");
        else
            $fatal(1, "STATUS: VERIFICATION FAILED WITH %0d ERRORS!", fail_count);

        $finish;
    end

endmodule