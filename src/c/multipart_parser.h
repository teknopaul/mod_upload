
#define META_BUFFER_SIZE 2048
#define DATA_BUFFER_SIZE 2048
#define UNKNOWN_BUFFER_SIZE 2048
#define MAX_FILE_NAME_SIZE 256

/**
 * Parsere state, also holds data read and buffered
 * memset() the whole struct to 0 before use
 * positive states are 1's
 */
typedef struct parse_ctx {
  const char* boundary;  // e.g. -----------------------------204250758710714497121462520684	
  int boundary_len;
  char file_name[MAX_FILE_NAME_SIZE];
  void * user_data; // can be filled with anything, is filed with an apr file handle
  int written;  // amound of file data written

  char meta_buffer[META_BUFFER_SIZE];
  int meta_pos;
  int reading_file;
  int last_was_newline; // last char read was \n  double \n indicates end of meta data
// buffered file data
  char data_buffer[DATA_BUFFER_SIZE]; // 2 extra bytes for potential trailing CRLF
  int data_pos;
// buffer for boundary matching, used for buffereng data that appears to be a boundy but we don't know
// untill the whole boundary length is read
  char unknown_buffer[UNKNOWN_BUFFER_SIZE];
  int unknown_pos;
// states
  int reading_meta; // reading meta data (and boundary)
  int reading_data; // reading form data
  int reading_unknown; // not sure which (hit a new line in the form data need to check the boundary match)
} parse_ctx ;

int mpp_process_char(parse_ctx *ctx, char c);
int mpp_finish(parse_ctx *ctx);
