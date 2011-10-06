#include "multipart_parser.h"
#include <stdio.h>
#include <string.h>

/**
 *  test large data sets 2
 */
int main() {
 
  char * test_data_00 = "-----------------------------204250758710714497121462520684\r\n";
  char * test_data_01 = " Content-disposition:form-data;name=\"hidden\"\r\n";
  char * test_data_02 = "\r\n";
  char * test_data_03 = "data\r\n";
  char * test_data_04 = "-----------------------------204250758710714497121462520684\r\n";
  char * test_data_05 = "Content-Disposition:form-data;name=\"file\";filename=\"data.dat\"\r\n";
  char * test_data_06 = "Content-Type:text/plain\r\n";
  char * test_data_07 = "\r\n";
  char test_data_08[(DATA_BUFFER_SIZE * 4)]; // binary data
  memset(test_data_08, 'a', (DATA_BUFFER_SIZE * 4));
  test_data_08[(DATA_BUFFER_SIZE * 4) -1] = 0;
  char * test_data_09 = "\r\n";
  char * test_data_10 = "-----------------------------204250758710714497121462520684\r\n";

 fprintf (stdout,"\nRunning unit test, large data sets 2\n");

  char test_data[4096];
  test_data[0] = 0;
  strcat(test_data, test_data_00);
  strcat(test_data, test_data_01);
  strcat(test_data, test_data_02);
  strcat(test_data, test_data_03);
  strcat(test_data, test_data_04);
  strcat(test_data, test_data_05);
  strcat(test_data, test_data_06);
  strcat(test_data, test_data_07);
  strcat(test_data, test_data_08);
  strcat(test_data, test_data_09);
  strcat(test_data, test_data_10);

  // fprintf (stdout,"setup test data \n%s\n", test_data);

  parse_ctx sctx;
  parse_ctx *ctx = &sctx;
  memset( ctx, 0, sizeof(parse_ctx) );	
  ctx->boundary = "-----------------------------204250758710714497121462520684";
  ctx->boundary_len = strlen(ctx->boundary);
 // fprintf (stdout,"boundary_len %d \n", ctx->boundary_len);
 // fprintf (stdout,"setup test data %s \n", test_data);

  int i;
  int ret;
  for (i = 0 ; i < strlen(test_data) ; i++) {
	ret = mpp_process_char(ctx, test_data[i]);
    if (ret < 0 ) {
	  fprintf (stdout,"\nfailed [%d]\n", ret );
      return -1;
    }
  }
  if (ret == 0) {
	mpp_finish(ctx);
  }
  fprintf (stdout,"\nFinished file_name [%s] [%s]\n", ctx->file_name, ret == 0 ? "OK" : "FAIL" );
  return ret;
};
