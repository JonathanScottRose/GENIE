package lu_new;

	parameter BSIZE = 64;
	parameter BSIZEBITS = $clog2(BSIZE-1);
	parameter BWORDS = BSIZE*BSIZE;
	parameter BWORDSBITS = $clog2(BWORDS-1);
	parameter BWORDSMEM = BWORDS / 8;
	parameter BWORDSMEMBITS = $clog2(BWORDSMEM-1);
	
	parameter LANES = BSIZE / 8;
	parameter LANESBITS = $clog2(LANES-1);
	
	parameter NET_DWIDTH = 256;
	
	parameter MAX_DIM = 16384;
	parameter MAX_BDIM = MAX_DIM / BSIZE;
	parameter MAX_BDIMBITS = $clog2(MAX_BDIM-1);
	
	parameter CACHE_DWIDTH = 256;
	parameter CACHE_AWIDTH = 2*BSIZEBITS;

	typedef enum
	{
		MODE_1,
		MODE_2,
		MODE_3,
		MODE_4
	} t_cpu_comp_mode;
	
	typedef struct packed
	{
		logic cur;
		logic left;
		logic top;
	} t_buftrio;
endpackage


