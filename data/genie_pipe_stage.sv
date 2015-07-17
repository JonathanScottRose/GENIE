module genie_pipe_stage #
(
	parameter WIDTH = 1,
	parameter MLAB = 0,
    parameter NO_BUF = 0
)
(
	input i_clk,
	input i_reset,
	
	input [WIDTH-1:0] i_data,
	input i_valid,
	output logic o_ready,
	
	output logic [WIDTH-1:0] o_data,
	output logic o_valid,
	input i_ready
);

generate
if (NO_BUF) begin : nobuf
    logic pipe_enable;
    assign pipe_enable = !(o_valid && !i_ready);
    
    always_ff @ (posedge i_clk or posedge i_reset) begin
        if (i_reset) begin
            o_valid <= '0;
        end
        else begin
            if (pipe_enable) begin
                o_valid <= i_valid;
                o_data <= i_data;
            end
            
            o_ready <= i_ready;
        end
    end
end
else if (MLAB) begin : mlab
    (* ramstyle = "no_rw_check,MLAB" *)
    reg [WIDTH-1:0] mem [0 : 1];
    
    logic [1:0] rdptr, wrptr;
    
    logic empty;
    logic full;
    logic reading;
    logic writing;
    
    assign empty = rdptr == wrptr;
    assign full = rdptr[0] == wrptr[0] && rdptr[1] != wrptr[1];
    
    assign writing = i_valid && !full;
    assign reading = i_ready && !empty;
    
    always_ff @ (posedge i_clk or posedge i_reset) begin
        if (i_reset) begin
            rdptr <= '0;
            wrptr <= '0;
        end
        else begin
            if (writing) wrptr <= wrptr + 2'd1;
            if (reading) rdptr <= rdptr + 2'd1;
        end
    end
    
    always_ff @ (posedge i_clk) begin
        if (writing) mem[wrptr[0]] <= i_data;
    end
    
    assign o_data = mem[rdptr[0]];
    assign o_valid = !empty;
    assign o_ready = !full;
end
else begin : regs
	logic [WIDTH-1:0] i2o_saved_data;
	(* keep *) logic [WIDTH-1:0] i2o_data;
	logic i2o_valid;

    always_ff @ (posedge i_clk or posedge i_reset) begin
        if (i_reset) begin
            i2o_valid <= 1'b0;
        end
        else if (o_ready) begin
            i2o_valid <= i_valid;
            i2o_data <= i_data;
        end
    end
	
	always_ff @ (posedge i_clk or posedge i_reset) begin
		if (i_reset) begin
			i2o_saved_data <= 'bx;
		end
		else if (o_ready) begin
			i2o_saved_data <= i2o_data;
		end
	end

	logic blocked;
	always_ff @ (posedge i_clk or posedge i_reset) begin
		if (i_reset) begin
			blocked <= 1'b0;
		end
		else begin
			case (blocked)
				1'b0: begin
					if (i2o_valid && !i_ready) blocked <= 1'b1;
				end
				
				1'b1: begin
					if (i_ready) blocked <= 1'b0;
				end
			endcase
		end
	end

	always_comb begin
		case (blocked)
			1'b0: begin
				o_valid = i2o_valid;
				o_data = i2o_data;
				o_ready = 1'b1;
			end
			
			1'b1: begin
				o_valid = 1'b1;
				o_data = i2o_saved_data;
				o_ready = 1'b0;
			end
		endcase
	end
end
endgenerate


endmodule
