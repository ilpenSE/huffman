#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#define BW_IMPLEMENTATION
#include "bw.h"
#define STR_IMPLEMENTATION
#include "str.h"
#include <errno.h>

#define DEBUG_PRINT 0

typedef struct Node Node;

String read_entire_file(const char *file_name);
size_t write_entire_file(const char *file_name, const String *content);

void print_tree(const Node *n, int depth /* 0 */, char edge);

typedef struct {
  char ch;
  int count;
} Freq;

// Pure nodes define count, ch is '\0' only leafs has meaningful ch
// But use left != NULL && right != NULL to check if it's leaf or not.
struct Node {
  Freq data;
  Node *left;
  Node *right;
};

typedef struct {
  char code[256];
  size_t code_length;
} Code;

typedef struct {
  Freq freqs[256]; // frequency table use symbol_count+1 to iterate it
  String data; // bit data (huffman's output)
  size_t orig_count; // original message's count
  uint8_t symbol_count; // real symbol_count - 1
} EncodedData;

Node *node_create(Freq payload)
{
  Node *n = malloc(sizeof(Node));
  n->data = payload;
  n->left = NULL;
  n->right = NULL;
  return n;
}

int freq_sorter(const void *lp, const void *rp)
{
  const Freq *x = lp;
  const Freq *y = rp;
  if (x->count != y->count) return (x->count > y->count) - (x->count < y->count);
  return (x->ch > y->ch) - (x->ch < y->ch);
}

size_t calculate_freqs(const char *input, size_t input_size, Freq *freqs)
{
  int freq_table[256] = {0};
  for (size_t i = 0; i < input_size; i++) freq_table[(unsigned char)input[i]]++;

  size_t n = 0;
  for (int i = 0; i < 256; i++) {
    int f = freq_table[i];
    if (f > 0) {
      freqs[n].ch = (char)i;
      freqs[n].count = f;
      n++;
    }
  }

  qsort(freqs, n, sizeof(*freqs), freq_sorter);
  return n;
}

Node *pop_min(const Freq *freqs, size_t *fcur, size_t fn, Node **nodes, size_t *ncur, size_t nn)
{
  if (*fcur < fn && (*ncur >= nn || freqs[*fcur].count <= nodes[*ncur]->data.count)) {
    return node_create(freqs[(*fcur)++]);
  } else {
    return nodes[(*ncur)++];
  }
}

// returns root of the tree
Node *build_tree(const Freq *freqs, size_t n, Node **nodes)
{
  size_t fcur = 0, ncur = 0, nn = 0;
  if (n == 1) {
    nodes[nn++] = node_create(freqs[0]);
  } else {
    while ((n - fcur) + (nn - ncur) > 1) {
      Node *left = pop_min(freqs, &fcur, n, nodes, &ncur, nn);
      Node *right = pop_min(freqs, &fcur, n, nodes, &ncur, nn);

      Node *merged = node_create((Freq){.count=(left->data.count + right->data.count)});
      merged->left = left;
      merged->right = right;
      nodes[nn++] = merged;
    }
  }

  return nodes[nn - 1];
}

// current_code should have enough space allocated
void generate_codes(Node *node, Code *codes, char *current_code, size_t code_length)
{
  if (!node) return;

  if (!node->left && !node->right) { // Leaf
    if (code_length == 0) code_length = 1;
    Code c = {.code_length=code_length};
    memcpy(c.code, current_code, code_length);
    codes[(unsigned char)node->data.ch] = c;
    return;
  }

  current_code[code_length] = '0';
  generate_codes(node->left, codes, current_code, code_length + 1);
  current_code[code_length] = '1';
  generate_codes(node->right, codes, current_code, code_length + 1);
}

// frees in postorder
void free_tree(Node *root)
{
  if (!root) return;
  free_tree(root->left);
  free_tree(root->right);
  free(root);
}

EncodedData huffman_encode(const char *input, size_t input_size)
{
#if DEBUG_PRINT == 1
  printf("[DEBUG] Input: '%.*s' (%zu bits)\n", (int)input_size, input, input_size*8);
#endif

  // Calculating frequencies
  Freq freqs[256];
  size_t nfreqs = calculate_freqs(input, input_size, freqs);
#if DEBUG_PRINT == 1
  printf("\n[DEBUG] Frequencies:\n");
  for (size_t i = 0; i < nfreqs; i++) {
    printf("'%c' -> %d\n", freqs[i].ch, freqs[i].count);
  }
#endif

  // Building the tree
  Node *nodes[256];
  Node *root = build_tree(freqs, nfreqs, nodes);
#if DEBUG_PRINT == 1
  printf("\n[DEBUG] Generated tree:\n");
  print_tree(root, 0, ' ');
#endif

  // Code generation
  char code_buf[256] = {0};
  code_buf[0] = '0';
  Code codes[256] = {0};
  generate_codes(root, codes, code_buf, 0);
#if DEBUG_PRINT == 1
  printf("\n[DEBUG] Codes:\n");
  for (size_t i = 0; i < 256; i++) {
    Code *c = &codes[i];
    if (c->code_length == 0) continue;
    printf("'%c' -> %.*s\n", (char)i, (int)c->code_length, c->code);
  }
  putchar('\n');
#endif

  // Serialization
  BitWriter bw = bw_new(input_size);
  for (size_t i = 0; i < input_size; i++) {
    Code *c = &codes[(unsigned char)input[i]];
    for (size_t j = 0; j < c->code_length; j++) {
      bw_put(&bw, c->code[j] == '1');
    }
  }

#if DEBUG_PRINT == 1
  // Print output in binary
  printf("[DEBUG] Output: ");
  for (size_t i = 0; i < bw.pos; i++) {
    printf("%d", bw_get(&bw, i));
  }
  size_t orig_bits = input_size * 8;
  printf(" (%zu bits) (original was %zu bits)\n", bw.pos, orig_bits);
#endif

  free_tree(root);
  String str = {.data=(char *)bw.buf, .count=bw_count(&bw), .capacity=bw.capacity};
  EncodedData ed = {.data=str, .orig_count=input_size, .symbol_count=(uint8_t)(nfreqs-1)};
  memcpy(ed.freqs, freqs, nfreqs * sizeof(Freq));
  return ed;
}

/*
  File format:
  [magic and version] 4 bytes
  [symbol count = n]  1 byte (stored like n-1 to fit 1 byte)
  [n amount of Freq: ch (1), count (4)]
  [original length]  8 bytes (size_t)
  [bit data...]
 */
size_t write_encoded_data_to_file(const char *file_path, EncodedData *ed)
{
  FILE *f = fopen(file_path, "wb");
  if (!f) {
    fprintf(stderr, "ERROR: Failed to open file: %s: %s\n", file_path, strerror(errno));
    return 0;
  }

  size_t written = 0;
  written += fwrite("\0ET1", 1, 4, f);

  String buffer = {0};
  str_append(&buffer, (char)ed->symbol_count);

  for (size_t i = 0; i < (size_t)ed->symbol_count+1; i++) {
    Freq f = ed->freqs[i];
    str_append(&buffer, f.ch);
    str_concat(&buffer, (StringView){.data=(const char *)&f.count, .count=sizeof(f.count)});
  }

  str_concat(&buffer, (StringView){.data=(const char *)&ed->orig_count, .count=sizeof(ed->orig_count)});

  written += fwrite(buffer.data, 1, buffer.count, f);
  buffer.count = 0;
  str_free(&buffer);

  written += fwrite(ed->data.data, 1, ed->data.count, f);

  fclose(f);
  return written;
}

EncodedData parse_encoded_data_from_file(StringView content)
{
  EncodedData ret = {.data=STR_INVALID};

  // Magic parsing
  StringView magic = sv_chop_left(&content, 4);
  if (!sv_eq(magic, SVL("\0ET1"))) {
    fprintf(stderr, "ERROR: Magic doesn't match!\n");
    goto exit;
  }

  // Symbol count parsing
  StringView symcount_sv = sv_chop_left(&content, 1);
  if (SV_ISINVALID(symcount_sv)) {
    fprintf(stderr, "ERROR: Failed to parse symbol count!\n");
    goto exit;
  }
  ret.symbol_count = (uint8_t)(*symcount_sv.data);

  // Freq array parsing
  for (size_t i = 0; i < ret.symbol_count + 1; i++) {
    StringView freq_ch_sv = sv_chop_left(&content, 1);
    if (SV_ISINVALID(freq_ch_sv)) {
      fprintf(stderr, "ERROR: Failed to parse character of freq (%zu)\n", i);
      goto exit;
    }

    StringView freq_count_sv = sv_chop_left(&content, 4);
    if (SV_ISINVALID(freq_count_sv)) {
      fprintf(stderr, "ERROR: Failed to parse count of freq (%zu)\n", i);
      goto exit;
    }

    char ch = *freq_ch_sv.data;
    int count = *((int *)freq_count_sv.data);
    ret.freqs[i] = (Freq){.ch=ch, .count=count};
  }

  // Original count parsing
  StringView orig_len_sv = sv_chop_left(&content, 8);
  if (SV_ISINVALID(orig_len_sv)) {
    fprintf(stderr, "ERROR: Failed to parse original count!\n");
    goto exit;
  }
  ret.orig_count = *((size_t *)orig_len_sv.data);

  // Bit data parsing
  String bit_data = str_from_sv(content);
  if (STR_ISINVALID(bit_data)) {
    fprintf(stderr, "ERROR: Failed to parse bit data!\n");
    goto exit;
  }
  ret.data = bit_data;

exit:
  return ret;
}

String huffman_decode(const EncodedData *ed)
{
  int symbol_count = ed->symbol_count + 1;
  Node *nodes[256];
  Node *root = build_tree(ed->freqs, symbol_count, nodes);
#if DEBUG_PRINT == 1
  printf("\n[DEBUG] Tree:\n");
  print_tree(root, 0, ' ');
#endif

  String res = str_new(ed->orig_count);
  BitReader br = {.buf=(const bw_byte_t *)ed->data.data, .pos=ed->data.count * 8};
  size_t cur = 0;
  Node *next = root;

  while (cur < br.pos) {
    if (res.count >= ed->orig_count) break;

    if (!next->left && !next->right) {
      str_append(&res, next->data.ch);
      next = root;
    } else {
      bw_bit_t bit = br_get(&br, cur);
      if (bit == 0) next = next->left;
      else next = next->right;
      cur++;
    }
  }

  return res;
}

const char *shift_argv(int *argc, char ***argv)
{ return *argc == 0 ? NULL : ((*argc)--, *(*argv)++); }

void print_usage(const char *program_name, FILE *f)
{
  fprintf(f, "Usage: %s <input_file> [...OPTIONS]\n", program_name);
  fprintf(f, "Options:\n");
  fprintf(f, "    --encode | -e: Do encoding. (By default)\n");
  fprintf(f, "    --decode | -d: Do decoding.\n");
  fprintf(f, "    --output | -o <file_path>: Specify output file of operation. "
    "('output.bin' or 'output.txt' depending on operation by default)\n");
  fprintf(f, "    --help | -h: Print this help message.\n");
}

int main(int argc, char **argv)
{
  const char *program_name = shift_argv(&argc, &argv);
  bool encode = true;
  const char *input_file = NULL;
  const char *output_file = NULL;

  const char *arg_cstr;
  while ((arg_cstr = shift_argv(&argc, &argv)) != NULL) {
    StringView arg = SV(arg_cstr);
    if (sv_eq(arg, SVL("--help")) || sv_eq(arg, SVL("-h"))) {
      print_usage(program_name, stdout);
      return 0;
    } else if (sv_eq(arg, SVL("-e")) || sv_eq(arg, SVL("--encode"))) {
      encode = true;
    } else if (sv_eq(arg, SVL("-d")) || sv_eq(arg, SVL("--decode"))) {
      encode = false;
    } else if (sv_eq(arg, SVL("-o")) || sv_eq(arg, SVL("--output"))) {
      const char *oarg = shift_argv(&argc, &argv);
      if (!oarg) {
        fprintf(stderr, "ERROR: Output file is required!\n");
        print_usage(program_name, stderr);
        return 1;
      }
      output_file = oarg;
    } else if (*arg.data == '-') {
      fprintf(stderr, "ERROR: Unrecognized option: "SV_FMT"\n", SV_ARG(arg));
      print_usage(program_name, stderr);
      return 1;
    } else {
      if (input_file != NULL) {
        fprintf(stderr, "ERROR: Multiple input files has been passed, there can be only one!\n");
        fprintf(stderr, "       Previous: %s, now: "SV_FMT"\n", input_file, SV_ARG(arg));
        return 1;
      }
      input_file = arg_cstr;
    }
  }

  if (!output_file) {
    output_file = encode ? "output.bin" : "output.txt";
  }

  if (!input_file) {
    fprintf(stderr, "ERROR: No input files.\n");
    print_usage(program_name, stderr);
    return 1;
  }

  String input = read_entire_file(input_file);
  if (STR_ISINVALID(input)) {
    fprintf(stderr, "ERROR: Failed to read file: %s: %s\n", input_file, strerror(errno));
    return 1;
  }
  size_t orig_bytes = input.count;

  if (encode) {
    EncodedData ed = huffman_encode(input.data, input.count);
    str_free(&input);

    size_t written = write_encoded_data_to_file(output_file, &ed);
    printf("%s (%zu bytes) encoded into %s (%zu bytes)\n", input_file, orig_bytes, output_file, written);
    str_free(&ed.data);
  } else {
    EncodedData ed = parse_encoded_data_from_file(SVS(input));
    str_free(&input);
    if (STR_ISINVALID(ed.data)) {
      fprintf(stderr, "ERROR: Could not parse encoded data from file!\n");
      return 1;
    }
    String decoded = huffman_decode(&ed);
    str_free(&ed.data);

    size_t w = write_entire_file(output_file, &decoded);
    if (w == 0) {
      fprintf(stderr, "ERROR: Could not write to file: %s: %s\n", output_file, strerror(errno));
      str_free(&decoded);
      return 1;
    }
    printf("%s (%zu bytes) decoded into %s (%zu bytes)\n", input_file, orig_bytes, output_file, w);

    str_free(&decoded);
  }
}

static void print_node(const Node *n)
{
  if (!n->left && !n->right) printf("%c:%d", n->data.ch, n->data.count);
  else                       printf("(%d)", n->data.count);
}

void print_tree(const Node *n, int depth, char edge)
{
  if (!n) return;
  print_tree(n->right, depth + 1, '1');
  for (int i = 0; i < depth; i++) printf("      ");
  if (depth > 0) printf("%c--", edge);
  print_node(n);
  printf("\n");
  print_tree(n->left, depth + 1, '0');
}

size_t write_entire_file(const char *file_name, const String *content)
{
  FILE *f = fopen(file_name, "wb");
  if (!f) return 0;
  size_t w = fwrite(content->data, sizeof(*content->data), content->count, f);
  fclose(f);
  return w;
}

String read_entire_file(const char *file_name)
{
  FILE *f = fopen(file_name, "rb");
  if (!f) return STR_INVALID;
  if (fseek(f, 0, SEEK_END) != 0) return STR_INVALID;
  long n = ftell(f);
  if (n < 0) return STR_INVALID;
  size_t file_size = (size_t)n;
  if (fseek(f, 0, SEEK_SET) != 0) return STR_INVALID;

  String res = str_new(file_size + 1);

  size_t bytes_read = fread(res.data, 1, file_size, f);
  if (bytes_read == 0) {
    if (feof(f) || ferror(f)) return STR_INVALID;
  }
  res.count = bytes_read;

  fclose(f);
  return res;
}
