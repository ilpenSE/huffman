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

#define DEBUG_PRINT 1

typedef struct Node Node;

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

Node *pop_min(Freq *freqs, size_t *fcur, size_t fn, Node **nodes, size_t *ncur, size_t nn)
{
  if (*fcur < fn && (*ncur >= nn || freqs[*fcur].count <= nodes[*ncur]->data.count)) {
    return node_create(freqs[(*fcur)++]);
  } else {
    return nodes[(*ncur)++];
  }
}

// returns root of the tree
Node *build_tree(Freq *freqs, size_t n, Node **nodes)
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
  printf("Input: '%.*s' (%zu bits)\n", (int)input_size, input, input_size*8);
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
  printf("Output: ");
  for (size_t i = 0; i < bw.pos; i++) {
    printf("%d", bw_get(&bw, i));
  }
  size_t orig_bits = input_size * 8;
  printf(" (%zu bits) (original was %zu bits)\n", bw.pos, orig_bits);
#endif

  free_tree(root);
  String str = {.data=(char *)bw.buf, .count=bw_count(&bw), .capacity=bw.capacity};
  EncodedData ret = {.data=str, .orig_count=input_size, .symbol_count=(uint8_t)(nfreqs-1)};
  memcpy(ret.freqs, freqs, nfreqs * sizeof(Freq));
  return ret;
}

/*
  File format:
  [magic and version] 4 bytes
  [symbol count = n]  1 byte (stored like n-1 to fit 1 byte)
  [n amount of Freq: ch (1), count (4)]
  [original length]  8 bytes (size_t)
  [bit data...]
 */
bool write_encoded_data_to_file(const char *file_path, EncodedData *ed)
{
  FILE *f = fopen(file_path, "wb");
  if (!f) {
    fprintf(stderr, "ERROR: Failed to open file: %s: %s\n", file_path, strerror(errno));
    return false;
  }

  size_t written = 0;
  written += fwrite("\0ET1", 1, 4, f);

  String buffer = {0};
  str_append(&buffer, (char)ed->symbol_count);

  for (size_t i = 0; i < (size_t)ed->symbol_count+1; i++) {
    Freq f = ed->freqs[i];
    printf("%c -> %d\n", f.ch, f.count);
    str_append(&buffer, f.ch);
    str_concat(&buffer, (StringView){.data=(const char *)&f.count, .count=sizeof(f.count)});
  }

  str_concat(&buffer, (StringView){.data=(const char *)&ed->orig_count, .count=sizeof(ed->orig_count)});

  written += fwrite(buffer.data, 1, buffer.count, f);
  buffer.count = 0;
  str_free(&buffer);

  written += fwrite(ed->data.data, 1, ed->data.count, f);

  fclose(f);
  printf("Wrote %zu bytes to %s\n", written, file_path);
  return true;
}

int main(void)
{
  const char *input = "lossless";
  size_t input_size = strlen(input);

  EncodedData encoded_data = huffman_encode(input, input_size);
  printf("Output in bytes: ");
  for (size_t i = 0; i < encoded_data.data.count; i++) {
    if (i != 0) putchar(' ');
    printf("%02X", (unsigned char)encoded_data.data.data[i]);
  }
  putchar('\n');

  if (!write_encoded_data_to_file("output.bin", &encoded_data)) return false;

  str_free(&encoded_data.data);
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
