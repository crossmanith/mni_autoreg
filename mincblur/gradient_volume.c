/* ----------------------------- MNI Header -----------------------------------
@NAME       : gradient3D_volume.c
@INPUT      : ifd  - a file pointer to an opened file, containing the
                     <file>_reals.mnc data
	      data - a pointer to a volume_struct of data, so that the
	             header can be used to create the new files.
	      outfile - name of the base filename to store the <name>_blur.mnc
	      ndim - =2, do blurring in the x and y directions only,
                     =3, blur in all three directions.
@OUTPUT     : creates and stores the partial dirivitives of the volumetric data
              stored in the file pointed to by ifd
@RETURNS    : status variable - OK or ERROR.
@DESCRIPTION: 
@METHOD     : 
@GLOBALS    : 
@CALLS      : stuff from volume_support.c and libmni.a
@CREATED    : Wed Jun 23 09:04:34 EST 1993  Louis Collins 
                 from code originally written for blur_grad working on .iff files.
@MODIFIED   : 
---------------------------------------------------------------------------- */
#include <def_mni.h>
#include <volume_support.h>
#include <recipes.h>


public Status gradient3D_volume(FILE *ifd, 
				Volume data, 
				char *outfile, 
				int ndim)
{ 
  float 
    *fdata,			/* floating point storage for blurred volume */
    *f_ptr,			/* pointer to fdata */
    tmp,
    max_val, 
    min_val,
    
    *dat_vector,		/* temp storage of original row, col or slice vect. */
    *dat_vecto2,		/* storage of result of dat_vector*kern                 */
    *kern,			/* convolution kernel                               */
    val;			/* temp storage, to take value from dat_vector, and     */
				/*  place it back into data->voxels                 */
  int				
    i, total_voxels,		
    vector_size_data,		/* original size of row, col or slice vector        */
    array_size_pow2;		/* actual size of vector/kernel data used in FFT    */
				/* routines - needs to be a power of two            */
  int   
    data_offset;		/* offset required to place original data (size n)  */
				/*  into array (size m=2^n) so that data is centered*/

  register int 
    slice_limit,
    row,col,slice,		/* counters to access original data                 */
    k,				/* counters to access kernel                        */
    vindex;			/* counter to access vector and vecto2              */

  int 
    voxel_size, slice_size,	/* size of each data step - in bytes              */
    row_size, col_size;
                
  FILE 
    *f,				/* file used to print out debug info about kernel(s)*/
    *ofp;			/* file used tp write out dx,dy or dz volume        */
  char
    full_outfilename[256];	/* name of output file */

  progress_struct 
    progress;			/* used to monitor progress of calculations         */

  Status 
    status;
  
  int
    sizes[3];			/* number of rows, cols and slices */
  Real
    steps[3];			/* size of voxel step from center to center in x,y,z */

  /*---------------------------------------------------------------------------------*/
  /*             start by setting up the raw data.                                   */
  /*---------------------------------------------------------------------------------*/

  get_volume_sizes(data, sizes);          /* rows,cols,slices */
  get_volume_separations(data, steps);
  
  slice_size = sizes[Y] * sizes[X];    /* sizeof one slice  */
  col_size   = sizes[Y];               /* sizeof one column */
  row_size   = sizes[X];               /* sizeof one row    */
  voxel_size = sizeof(float);                           
  
  total_voxels = sizes[Y]*sizes[X]*sizes[Z];
  
  ALLOC(fdata, total_voxels);
  f_ptr = fdata;

         /* read in data of input file. */

  set_file_position(ifd,(long)0);
  status = io_binary_data(ifd,READ_FILE, fdata, sizeof(float), total_voxels);




  /*--------------------------------------------------------------------------------------*/
  /*                get ready to start up the transformation.                             */
  /*--------------------------------------------------------------------------------------*/
  
  initialize_progress_report( &progress, FALSE, sizes[Z] + sizes[Z] + sizes[X] + 1,
			     "Blurring volume" );


  /* note data is stored by rows (along x), then by cols (along y) then slices (along z) */
  

  /*--------------------------------------------------------------------------------------*/
  /*                start with rows - i.e. the d/dx volume                                */
  /*--------------------------------------------------------------------------------------*/
  
  /*-----------------------------------------------------------------------------*/
  /*             determine   size of data structures needed                      */
  
  vector_size_data = sizes[X]; 
  
  /*             array_size_pow2 will hold the size of the arrays for FFT convolution,
		 remember that ffts require arrays 2^n in length                          */
  
  array_size_pow2  = next_power_of_two(vector_size_data);

  dat_vector = vector(0,2*array_size_pow2+1); /* allocate 2*, since each point is a    */
  dat_vecto2 = vector(0,2*array_size_pow2+1); /* complex number for FFT, and the plus 1*/
  kern       = vector(0,2*array_size_pow2+1); /* is for Num. Rec. FFT routines         */
  
  /*    1st calculate kern array for FT of 1st derivitive */
  
  make_kernel_FT(kern,array_size_pow2);
  
  /*    calculate offset for original data to be placed in vector            */
  
  data_offset = (array_size_pow2-sizes[X])/2;
  
  max_val = -100000000.0;
  min_val =  100000000.0;


  /*    2nd now convolve this kernel with the rows of the dataset            */  
  
  if (ndim<3)
    slice_limit = 1;
  else
    slice_limit = sizes[Z];
  
  for (slice = 0; slice < slice_limit; slice++) {      /* for each slice */
    
    for (row = 0; row < sizes[Y]; row++) {           /* for each row   */
      
      f_ptr = fdata + slice*slice_size + row*sizes[X];
      memset(dat_vector,0,(2*array_size_pow2+1)*sizeof(float));
      
      for (col=0; col< sizes[X]; col++) {        /* extract the row */
	dat_vector[1 +2*(col+data_offset)  ] = *f_ptr++;
      }
      
      four1(dat_vector,array_size_pow2,1);
      muli_vects(dat_vecto2,dat_vector,kern,array_size_pow2);
      four1(dat_vecto2,array_size_pow2,-1);
      
      f_ptr = fdata + slice*slice_size + row*sizes[X];
      for (col=0; col< sizes[X]; col++) {        /* put the row back */
	
	vindex = 1 + 2*(col+data_offset);
       *f_ptr = dat_vecto2[vindex]/array_size_pow2;


	if (max_val<*f_ptr) max_val = *f_ptr;
	if (min_val>*f_ptr) min_val = *f_ptr;

	f_ptr++;
      }
      
      
    }
    update_progress_report( &progress, slice+1 );
  }
  
  free_vector(dat_vector, 0,2*array_size_pow2+1); 
  free_vector(dat_vecto2, 0,2*array_size_pow2+1); 
  free_vector(kern      , 0,2*array_size_pow2+1); 
    
/*
 *
 *  FINISH THIS FILE WRITE:
 *
 *
*/

  f_ptr = fdata;
  
  out_data->fp_min = min_val;
  out_data->fp_max = max_val;
  
  printf ("dx: min = %f, max = %f\n",min_val, max_val);
  
  printf("Making byte volume..." );
  for (i=0; i < total_voxels; ++i) {
    
    *c_ptr = get_uchar_DATA(*f_ptr,out_data);
    
      c_ptr++;
    f_ptr++;
    
  }
  printf("done\n");
  
  
  sprintf(full_outfilename,"%s_dx.iff",outfile);
  status = write_new_iff_file(out_data,full_outfilename);
  if (status != OK) 
    PRINT("problems writing dx gradient data...");
  
  
  /*--------------------------------------------------------------------------------------*/
  /*                 now do cols - i.e. the d/dy volume                                   */
  /*--------------------------------------------------------------------------------------*/
  
  /*-----------------------------------------------------------------------------*/
  /*             determine   size of data structures needed                      */
  

  set_file_position(ifd,0);
  status = io_binary_data(ifd,READ_FILE, fdata, sizeof(float), total_voxels);
  
  f_ptr = fdata;

  vector_size_data = sizes[Y];
  
  /*             array_size_pow2 will hold the size of the arrays for FFT convolution,
		 remember that ffts require arrays 2^n in length                          */
  
  array_size_pow2  = next_power_of_two(vector_size_data);
  
  dat_vector = vector(0,2*array_size_pow2+1); /* allocate 2*, since each point is a    */
  dat_vecto2 = vector(0,2*array_size_pow2+1); /* complex number for FFT, and the plus 1*/
  kern       = vector(0,2*array_size_pow2+1); /* is for Num. Rec. FFT routines         */
  
  /*    1st calculate kern array for FT of 1st derivitive */
  
  make_kernel_FT(kern,array_size_pow2);
  
  /*    calculate offset for original data to be placed in vector            */
  
  data_offset = (array_size_pow2-sizes[Y])/2;
  
  /*    2nd now convolve this kernel with the rows of the dataset            */
  
  max_val = -100000000.0;
  min_val =  100000000.0;

  if (ndim<3)
    slice_limit = 1;
  else
    slice_limit = sizes[Z];
  
  for (slice = 0; slice < slice_limit; slice++) {      /* for each slice */
    
    for (col = 0; col < sizes[X]; col++) {           /* for each col   */
      
      /*	 f_ptr = fdata + slice*slice_size + row*sizeof(float); */
      
      f_ptr = fdata + slice*slice_size + col;
      
      
      memset(dat_vector,0,(2*array_size_pow2+1)*sizeof(float));
      
      for (row=0; row< sizes[Y]; row++) {        /* extract the col */
	dat_vector[1 +2*(row+data_offset) ] = *f_ptr;
	f_ptr += row_size;
      }
      
      
      four1(dat_vector,array_size_pow2,1);
      muli_vects(dat_vecto2,dat_vector,kern,array_size_pow2);
      four1(dat_vecto2,array_size_pow2,-1);
      
      f_ptr = fdata + slice*slice_size + col;
      for (row=0; row< sizes[Y]; row++) {        /* put the col back */
	
	vindex = 1 + 2*(row+data_offset);
	
	*f_ptr = dat_vecto2[vindex]/array_size_pow2;
	
	if (max_val<*f_ptr) max_val = *f_ptr;
	if (min_val>*f_ptr) min_val = *f_ptr;

	f_ptr += row_size;
	
	
      }
      
    }
    update_progress_report( &progress, slice+sizes[Z]+1 );
    
  }
  
  free_vector(dat_vector, 0,2*array_size_pow2+1); 
  free_vector(dat_vecto2, 0,2*array_size_pow2+1); 
  free_vector(kern      , 0,2*array_size_pow2+1); 
  
/*
 *
 *  FINISH THIS FILE WRITE:
 *
 *
*/

  f_ptr = fdata;
  c_ptr = out_data->voxels;
  
  out_data->fp_min = min_val;
  out_data->fp_max = max_val;
  
  printf ("dy: min = %f, max = %f\n",min_val, max_val);
  
  printf("Making byte volume..." );
  for (i=0; i < total_voxels; ++i) {
    
    *c_ptr = get_uchar_DATA(*f_ptr,out_data);
    
    c_ptr++;
    f_ptr++;
    
  }
  printf("done\n");
  
  sprintf(full_outfilename,"%s_dy.iff",outfile);
   status = write_new_iff_file(out_data,full_outfilename);
  if (status != OK) 
    PRINT("problems writing dy gradient data...");
  
  
  
  /*--------------------------------------------------------------------------------------*/
  /*                 now do slices - i.e. the d/dz volume                                 */
  /*--------------------------------------------------------------------------------------*/
  
  /*-----------------------------------------------------------------------------*/
  /*             determine   size of data structures needed                      */
  
  set_file_position(ifd,0);
  status = io_binary_data(ifd,READ_FILE, fdata, sizeof(float), total_voxels);
  f_ptr = fdata;
  
  vector_size_data = sizes[Z];

  /*             array_size_pow2 will hold the size of the arrays for FFT convolution,
		 remember that ffts require arrays 2^n in length                          */
  
  array_size_pow2  = next_power_of_two(vector_size_data);
  
  dat_vector = vector(0,2*array_size_pow2+1); /* allocate 2*, since each point is a    */
  dat_vecto2 = vector(0,2*array_size_pow2+1); /* complex number for FFT, and the plus 1*/
  kern       = vector(0,2*array_size_pow2+1); /* is for Num. Rec. FFT routines         */
  
  if (ndim>2) {
    
    /*    1st calculate kern array for FT of 1st derivitive */
    
    make_kernel_FT(kern,array_size_pow2);
    
    /*    calculate offset for original data to be placed in vector            */
    
    data_offset = (array_size_pow2-sizes[Z])/2;
    
    /*    2nd now convolve this kernel with the slices of the dataset            */
    
    max_val = -100000000.0;
    min_val =  100000000.0;
    
    
    for (col = 0; col < sizes[X]; col++) {      /* for each column */
      
      for (row = 0; row < sizes[Y]; row++) {           /* for each row   */
	
	if (data->bytes_per_voxel==1) {
	  /*	    f_ptr = fdata + col*col_size + row*sizeof(float); */
	  f_ptr = fdata + col*col_size + row;
	  
	  memset(dat_vector,0,(2*array_size_pow2+1)*sizeof(float));
	  
	  for (slice=0; slice< sizes[Z]; slice++) {        /* extract the slice vector */
	    dat_vector[1 +2*(slice+data_offset) ] = *f_ptr;
	    f_ptr += slice_size;
	  }
	  
	  four1(dat_vector,array_size_pow2,1);
	  muli_vects(dat_vecto2,dat_vector,kern,array_size_pow2);
	  four1(dat_vecto2,array_size_pow2,-1);
	  
	  f_ptr = fdata + col*col_size + row;
	  
	  for (slice=0; slice< sizes[Z]; slice++) {        /* put the vector back */
	    
	    vindex = 1 + 2*(slice+data_offset);
	    
	    *f_ptr = dat_vecto2[vindex]/array_size_pow2;
	    
	    if (max_val<*f_ptr) max_val = *f_ptr;
	    if (min_val>*f_ptr) min_val = *f_ptr;
	    
	    f_ptr += slice_size;
	  }
	}
	
	
      }
      update_progress_report( &progress, col + 2*sizes[Z] + 1 );
      
    }
    
  }  /* if ndim */
  else {
    max_val = 0.0;
    min_val = 0.0;
    
    for (col = 0; col < sizes[X]; col++) {      /* for each column */
      for (row = 0; row < sizes[Y]; row++) {           /* for each row   */
	*f_ptr = 0.0;
	f_ptr++;
      }
    }
  }
  
  printf ("dz: min = %f, max = %f\n",min_val, max_val);
  
  free_vector(dat_vector, 0,2*array_size_pow2+1); 
  free_vector(dat_vecto2, 0,2*array_size_pow2+1); 
  free_vector(kern      , 0,2*array_size_pow2+1); 
  
  
/* set up the correct info to copy the data back out in mnc */


/*
  *
  *
  *   FINISH THESE WRITE ROUTINES:
  *
  *
  *
*/

   f_ptr = fdata;
   c_ptr = out_data->voxels;

   out_data->fp_min = min_val;
   out_data->fp_max = max_val;

   printf ("dz: min = %f, max = %f\n",min_val, max_val);

   printf("Making byte volume..." );
   for (i=0; i < total_voxels; ++i) {

      if (ndim>2)
	 *c_ptr = get_uchar_DATA(*f_ptr,out_data);
      else
	 *c_ptr = get_uchar_DATA(0.0,out_data);
	 

      c_ptr++;
      f_ptr++;

   }
   printf("done\n");

   sprintf(full_outfilename,"%s_dz.iff",outfile);
   status = write_new_iff_file(out_data,full_outfilename);
   if (status != OK) 
      PRINT("problems writing dz gradient data...");


  terminate_progress_report( &progress );

  FREE(fdata);

  
}

