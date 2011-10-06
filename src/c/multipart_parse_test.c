#include "multipart_parser.h"
#include <stdio.h>
#include <string.h>


int main() {
 
  char * test_data_00 = "-----------------------------204250758710714497121462520684\n";
  char * test_data_01 = "Content-Disposition: form-data; name=\"hidden\"\n";
  char * test_data_02 = "\n";
  char * test_data_03 = "data\n";
  char * test_data_04 = "-----------------------------204250758710714497121462520684\n";
  char * test_data_05 = "Content-Disposition: form-data; name=\"file\"; filename=\"data.dat\"\n";
  char * test_data_06 = "Content-Type: text/plain\n";
  char * test_data_07 = "\n";
  char test_data_08[5]; // binary data
  test_data_08[0] = 1;
  test_data_08[1] = 2;
  test_data_08[2] = 3;
  test_data_08[3] = 4;
  test_data_08[4] = 0;
  char * test_data_09 = "\n";
  char * test_data_10 = "-----------------------------204250758710714497121462520684\n";

 fprintf (stdout,"Running unit test \n");

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

 // fprintf (stdout,"setup test data %s \n", test_data);

  int i;
  int ret;
  for (i = 0 ; i < strlen(test_data) ; i++) {
	ret = process_char(ctx, test_data[i]);
    if (ret != 0 ) {
	  fprintf (stdout,"failed [%d]\n", ret );
    }
  }
  fprintf (stdout,"file_name [%s]\n", ctx->file_name	);
  return ret;
};
