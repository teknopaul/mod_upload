#include "multipart_parser.h"
#include <stdio.h>
#include <string.h>

/**
 * TEST broken content, too much meta data
 */
int main() {
 
  char * test_data_00 = "-----------------------------204250758710714497121462520684\n";

  char test_data_excess[META_BUFFER_SIZE + 2];
  memset(test_data_excess, 'a', META_BUFFER_SIZE + 1);
  test_data_excess[META_BUFFER_SIZE + 1] = 0;

  char * test_data_01 = " Content-disposition : form-data; name=\"hidden\"\n";
  char * test_data_02 = "\n";
  char * test_data_03 = "data\n";
  char * test_data_04 = "-----------------------------204250758710714497121462520684\n";
  char * test_data_05 = "Content-Disposition: form-data; name=\"file\"; filename=\"data.dat\"\n";
  char * test_data_06 = "Content-Type: text/plain\n";
  char * test_data_07 = "\n";
  char test_data_08[13]; // binary data
  test_data_08[0] = 'a';
  test_data_08[1] = '\n';
  test_data_08[2] = '-';
  test_data_08[3] = '-';
  test_data_08[4] = '-';
  test_data_08[5] = 'x';
  test_data_08[6] = 'x';
  test_data_08[7] = 'x';
  test_data_08[8] = 'x';
  test_data_08[9] = 'a';
  test_data_08[10] = 'b';
  test_data_08[11] = 'c';
  test_data_08[12] = 0;
  char * test_data_09 = "\n";
  char * test_data_10 = "-----------------------------204250758710714497121462520684\n";

 fprintf (stdout,"\nRunning unit test, too much meta data (should fail)\n");

  char test_data[4096];
  test_data[0] = 0;
  strcat(test_data, test_data_00);
  strcat(test_data, test_data_excess );
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
