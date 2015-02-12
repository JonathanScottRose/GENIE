import lu_new::*;

module cpu_pipe_ctrl #
(
	parameter IS_BPU = 1
)
(
	input clk,
	input reset,
	
	// Control from da boss
	input i_go,
	input i_flush,
	input [1:0] i_mode,
	output logic o_done,
	input i_whichpage,
	
	// Read counter control
	output logic o_k_reset,
	output logic o_k_inc,
	input i_k_done,
	
	output logic o_i_reset,
	output logic o_i_inc,
	output logic o_i_load_k1,
	input i_i_done,
	
	output logic o_j_reset,
	output logic o_j_inc,
	output logic o_j_load_k,
	output logic o_j_load_k1,
	input i_j_done,
	
	// Pipeline control
	output logic o_valid,
	output logic o_norm,
	output logic o_recip,
	output logic o_wr_top,
	output logic o_wr_left,
	output logic o_wr_cur,
	output logic o_whichpage,
	input i_pipe_empty
);


t_cpu_comp_mode i_mode_enum;
assign i_mode_enum = t_cpu_comp_mode'(i_mode);

// Counter to throttle reciprocal operation (it's multicycled)
logic[1:0] recip_cycle;

logic recip_cycle_reset, recip_cycle_inc, recip_cycle_on;
always_ff @ (posedge clk or posedge reset) begin
	if (reset) begin
		recip_cycle <= '0;
	end
	else begin
		if (recip_cycle_reset) recip_cycle <= '0;
		else if (recip_cycle_inc) recip_cycle <= recip_cycle + 2'd1;
	end
end

assign recip_cycle_on = (recip_cycle == '1);


// Register the whichpage and flush signals
logic whichpage;
logic flush;
always_ff @ (posedge clk) begin
	if (i_go) begin
		whichpage <= i_whichpage;
		flush <= i_flush;
	end
end

assign o_whichpage = whichpage;

// State
enum int unsigned
{
	S_IDLE,
	S_DONE,
	S_RECIP,
	S_MODE1_1,
	S_MODE1_2,
	S_MODE1_3,
	S_MODE1_4,
	S_MODE1_5,
	S_MODE1_6,
	S_MODE2_1,
	S_MODE2_2,
	S_MODE2_3,
	S_MODE2_4,
	S_MODE2_5,
	S_MODE3_1,
	S_MODE3_2,
	S_MODE3_3,
	S_MODE4
} state, nextstate;

always_ff @ (posedge clk or posedge reset) begin
	if (reset) state <= S_IDLE;
	else state <= nextstate;
end

//OVERVIEW of algorithm:
/* Consider a matrix K, with blocks of 64 denoted by each element
*
*   1333
*   2444  
*   2444
*   2444
*   
*   LU consists of three loops, with indexes k(outer), j (along a row), i (along a column).
*
*   The state machine below has covers four modes of operation, which is a function of block
*   position:
*
*   MODE 1: Top left-block: TOP=LEFT=CUR Blocks. (BPU ONLY) 
*   MODE 2: Left-most block: LEFT=CUR!=TOP block (BPU ONLY)
*   MODE 3: Top most block excluding the top left block (TOP=CUR!=LEFT)
*   MODE 4: LEFT!=CUR!=TOP 
*
*/



// The state machine
always_comb begin
	nextstate = state;
	o_done = '0;
	o_k_reset = '0; o_k_inc = '0;
	o_i_reset = '0; o_i_inc = '0; o_i_load_k1 = '0;
	o_j_reset = '0; o_j_inc = '0; o_j_load_k1 = '0; o_j_load_k = '0;
	o_valid = '0;
	o_norm = '0;
	o_recip = '0;
	o_wr_top = '0; o_wr_left = '0; o_wr_cur = '0;
	recip_cycle_reset = '0; recip_cycle_inc = '0;
	
	case (state)
		S_IDLE: begin
			o_k_reset = '1;
			o_j_reset = '1;
			o_i_reset = '1;
			recip_cycle_reset = '1;
			
			if (i_go) begin
				case (i_mode_enum)
					// IS_BPU is a compile time parameter true only for
					// one of the CPUs, so this should optimize away
					// half this state machine for the other CPUs.
					MODE_1: nextstate = IS_BPU? S_MODE1_1 : S_IDLE;
					MODE_2: nextstate = IS_BPU? S_MODE2_1 : S_IDLE;
					MODE_3: nextstate = S_MODE3_1;
					MODE_4: nextstate = S_MODE4;
					default: ;
				endcase
			end
		end

		// Done - optionally wait for pipeline to flush
		S_DONE: begin
			if (!flush || i_pipe_empty) begin
				o_done = '1;
				nextstate = S_IDLE;
			end
		end
		
		// Calculate reciprocal. These can be reused in Mode 2
		S_MODE1_1: begin
			recip_cycle_inc = '1;
			o_recip = '1;
			if (recip_cycle_on) begin
				o_valid = '1;
				nextstate = S_MODE1_2;
			end
		end
		//Set i to k; 
		S_MODE1_2: begin
			o_recip = '1;
			if (i_pipe_empty) begin
				o_i_load_k1 = '1;

                if (i_k_done) nextstate = S_DONE;
   				else nextstate = S_MODE1_3;
			end
		end
		//Normalize reamining columning; 
		S_MODE1_3: begin
			o_i_inc = '1;
			o_norm = '1;
			o_valid = '1;
			o_wr_top = '1;
			o_wr_left = '1;
			o_wr_cur = '1;
			if (i_i_done) nextstate = S_MODE1_4;
		end
		
		S_MODE1_4: begin
			o_norm = '1;
			if (i_pipe_empty) begin
				o_j_load_k1 = '1;
				o_i_load_k1 = '1;
				nextstate = S_MODE1_5;
			end
		end
		
		// Case 1 - remainder of computation
		S_MODE1_5: begin
			o_valid = '1;
			o_wr_top = '1;
			o_wr_left = '1;
			o_wr_cur = '1;
			o_i_inc = '1;
			if (i_i_done) begin
				o_j_inc = '1;
				o_i_load_k1 = '1;	 //overrides i_inc
				if (i_j_done) begin
					nextstate = S_MODE1_6;
				end
			end
		end
		//Initialize for next submatrix 
		S_MODE1_6: begin
			if (i_pipe_empty) begin
				o_k_inc = '1;
				o_j_load_k1 = '1; // j and k equal after this cycle
				recip_cycle_reset = '1;
				if (i_k_done) nextstate = S_DONE;
				else nextstate = S_MODE1_1;
			end
		end
		
		// Case 2 - normalization step
		S_MODE2_1: begin
			o_i_reset = '1;
			o_j_load_k = '1;
			nextstate = S_MODE2_2;
		end
		
		S_MODE2_2: begin
			o_valid = '1;
			o_norm = '1;
			o_wr_left = '1;
			o_wr_cur = '1;
			o_i_inc = '1;
			if (i_i_done) nextstate = S_MODE2_3;
		end
		
		S_MODE2_3: begin
			o_norm = '1;
			o_wr_left = '1;
			o_wr_cur = '1;
			if (i_pipe_empty) begin
				o_j_load_k1 = '1;
				o_i_reset = '1;
                if (i_k_done) nextstate = S_DONE; // norm only for last column
                else nextstate = S_MODE2_4;
			end
		end
		
		// Case 2 - remainder of computation
		S_MODE2_4: begin
			o_valid = '1;
			o_wr_left = '1;
			o_wr_cur = '1;
			o_i_inc = '1;
			if (i_i_done) begin
				o_j_inc = '1;
				o_i_reset = '1; // overrides i_inc
				if (i_j_done) begin
					if (i_k_done) nextstate = S_DONE;
					else nextstate = S_MODE2_5;
				end
			end
		end
		
		S_MODE2_5: begin
			if (i_pipe_empty) begin
				o_k_inc = '1;
				nextstate = S_MODE2_1;
			end
		end
	
		// Case 3 - reinitialize i = k+1
		S_MODE3_1: begin			
			o_i_load_k1 = '1;

            // skip remainder of loop when k==31
            if (i_k_done) nextstate = S_DONE;
            else nextstate = S_MODE3_2;
		end
		
		// Case 3 - combined main loop
		S_MODE3_2: begin
			o_valid = '1;
			o_wr_top = '1;
			o_wr_cur = '1;
			o_i_inc = '1;
			if (i_i_done) begin
				o_j_inc = '1; // resets to 0 due to this add when j_done
				o_i_load_k1 = '1; // overrides i_inc
				if (i_j_done) begin
					nextstate = S_MODE3_3;
				end
			end
		end
		
		S_MODE3_3: begin
			if (i_pipe_empty) begin
				o_k_inc = '1;
				nextstate = S_MODE3_1;
			end
		end
	
		// Case 4 - i and j reinitializations are not dependent on k here,
		// so it can all be done in one state with no gap cycles
		S_MODE4: begin
			o_valid = '1;
			o_wr_cur = '1;
			o_i_inc = '1;
			if (i_i_done) begin
				o_j_inc = '1;
				o_i_reset = '1;
				if (i_j_done) begin
					o_k_inc = '1;
					if (i_k_done) nextstate = S_DONE;
				end
			end
		end
	endcase
end

endmodule
