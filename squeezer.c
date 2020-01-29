/*
 * Squeezer (https://github.com/nardinan/squeezer)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#define OK 0
#define KO 1
#define BUFFER_SIZE 1024 /* has to be at least (LIMIT_DICTIONARY * 2) */
#define CHARACTER_OFFSET 128 /* the first encoded character starts from zero, while the last entry of the ASCII table is 127 */
#define LIMIT_DICTIONARY 128 /* the maximum number of pairs we can keep */
typedef struct s_occurrency {
  unsigned char byte_0, byte_1;
  unsigned int occurrency;
  size_t index;
  struct s_occurrency *next, *previous;
} s_occurrency;
#pragma pack(push, 1)
typedef struct s_file_hader_entry {
  uint8_t byte_0, byte_1;
} s_file_header_entry;
#pragma pack(pop)
struct s_occurrency *f_update_occurrency_table(unsigned char byte_0, unsigned char byte_1, struct s_occurrency *root) {
  struct s_occurrency *current = root, *previous = NULL, *legit_previous;
  size_t index = 0;
  while (current) {
    if ((current->byte_0 == byte_0) && (current->byte_1 == byte_1)) {
      ++current->occurrency;
      /* we need to move the node back until the previous node is not root or is not bigger than this one */
      while ((current->previous) && (current->occurrency > current->previous->occurrency)) {
        legit_previous = current->previous;
        if ((legit_previous->next = current->next)) {
          legit_previous->next->previous = legit_previous;
        }
        if (!(current->previous = legit_previous->previous)) {
          root = current;
        } else {
          legit_previous->previous->next = current;
        }
        current->next = legit_previous;
        /* since we're here, we can easily use 'index': we we'll not need it anymore at the end of the loop */
        index = current->index;
        current->index = legit_previous->index;
        legit_previous->index = index;
        legit_previous->previous = current;
      }
      break;
    }
    previous = current;
    current = current->next;
    ++index;
  }
  if (!current) {
    if ((current = (struct s_occurrency *)malloc(sizeof(struct s_occurrency)))) {
      current->byte_0 = byte_0;
      current->byte_1 = byte_1;
      if (!(current->previous = previous)) {
        root = current;
      } else {
        previous->next = current;
      }
      current->index = index;
      current->next = NULL;
      current->occurrency = 1;
    } else {
      fprintf(stderr, "seems impossible to allocate more memory (we're asking for %zu bytes)\n", 
          sizeof(struct s_occurrency));
      exit(1);
    }
  }
  return root;
}
int f_create_occurrency_table(const char *full_path, struct s_occurrency **root) {
  int stream, result = OK;
  ssize_t read_bytes;
  unsigned char buffer[BUFFER_SIZE], last_entry_previous_buffer = CHARACTER_OFFSET;
  unsigned int index;
  if ((stream = open(full_path, O_RDONLY)) != -1) {
    /* when we read the file we need to take in consideration the fact that the last entry of the previous buffer and the 
     * first entry of the new buffer should be part of the statistics */
    while ((read_bytes = read(stream, buffer, BUFFER_SIZE)) > 0) {
      if (read_bytes > 0) {
        if (last_entry_previous_buffer != CHARACTER_OFFSET) {
          *root = f_update_occurrency_table(last_entry_previous_buffer, buffer[0], *root);
        }
        last_entry_previous_buffer = buffer[(read_bytes - 1)];
      }
      for (index = 1; index < read_bytes; ++index) {
        *root = f_update_occurrency_table(buffer[index - 1], buffer[index], *root);
      }
    }
    close(stream);
  } else {
    fprintf(stderr, "impossible to access the file '%s'\n", full_path);
    result = KO;
  }
  return result;
}
void f_destroy_occurrency_table(struct s_occurrency **root) {
  struct s_occurrency *current_entry = *root, *next_entry;
  while (current_entry) {
    next_entry = current_entry->next;
    free(current_entry);
    current_entry = next_entry;
  }
  *root = NULL;
}
struct s_occurrency *f_find_entry(unsigned char byte_0, unsigned char byte_1, struct s_occurrency *root, ssize_t dictionary_limitation) {
  struct s_occurrency *current_node = root, *result = NULL;
  size_t dictionary_index = 0;
  while (((dictionary_limitation < 0) || (dictionary_index < (size_t)dictionary_limitation)) && (current_node)) {
    if ((current_node->byte_0 == byte_0) && (current_node->byte_1 == byte_1)) {
      result = current_node;
      break;
    }
    current_node = current_node->next;
    ++dictionary_index;
  }
  return result;
}
int f_encode_file(const char *input_path, const char *output_path, double *score) {
  struct s_occurrency *root = NULL, *current_entry;
  int result = f_create_occurrency_table(input_path, &root), input_stream, output_stream;
  ssize_t read_bytes, write_bytes = 0, // write_bytes contains the number of byte int the output_buffer
          total_read_bytes = 0, total_written_bytes = 0;
  unsigned char input_buffer[BUFFER_SIZE], output_buffer[BUFFER_SIZE], last_entry_previous_buffer = CHARACTER_OFFSET;
  unsigned int index, dictionary_index = 0, offset_buffer = 0;
  if (result != KO) {
    if ((input_stream = open(input_path, O_RDONLY)) != -1) {
      if ((output_stream = open(output_path, (O_WRONLY | O_CREAT | O_TRUNC), 0644)) != -1) {
        current_entry = root;
        /* if the entries are not LIMIT_DICTIONARY, we need to pad the output */
        while ((current_entry) && (dictionary_index < LIMIT_DICTIONARY)) {
          output_buffer[write_bytes++] = current_entry->byte_0;
          output_buffer[write_bytes++] = current_entry->byte_1;
          current_entry = current_entry->next;
          ++dictionary_index;
        }
        /* here we need to pad the rest of the buffer if the dictionary is not big enough (we need to consider that the header of the file is going
         * to have the very same size: LIMIT_DICTIONARY * 2 bytes) */
        memset(&(output_buffer[write_bytes]), 0, ((LIMIT_DICTIONARY - dictionary_index) * 2));
        write_bytes += ((LIMIT_DICTIONARY - dictionary_index) * 2);
        while ((read_bytes = read(input_stream, (input_buffer + offset_buffer), (BUFFER_SIZE - offset_buffer))) > 0) {
          if (last_entry_previous_buffer != CHARACTER_OFFSET) {
            input_buffer[0] = last_entry_previous_buffer;
            last_entry_previous_buffer = CHARACTER_OFFSET;
            ++read_bytes;
          }
          total_read_bytes += read_bytes;
          if (read_bytes > 1) {
            index = 1;
            while (index < read_bytes) {
              if (write_bytes > (BUFFER_SIZE - 2)) {
                /* if we don't have enough space to store the next couple of bytes we need to dump the content */
                write(output_stream, output_buffer, write_bytes);
                total_written_bytes += write_bytes;
                write_bytes = 0;
              }
              if ((current_entry = f_find_entry(input_buffer[index - 1], input_buffer[index], root, LIMIT_DICTIONARY))) {
                /* nice! We got an entry in out dictionary, so we need to write it down in the target file. If index is the latest entry, we can 
                 * void the last_entry_previous_buffer entry because has been already compressed */
                output_buffer[write_bytes++] = (unsigned char)(current_entry->index + CHARACTER_OFFSET);
                if ((index += 2) == read_bytes) {
                  /* woops! We just jumped the latest entry; we should mark it as last entry of the previous previous buffer, so we'll push it into the
                   * head of the next buffer */
                  last_entry_previous_buffer = output_buffer[(read_bytes - 1)];
                }
              } else {
                output_buffer[write_bytes++] = input_buffer[index - 1];
                if ((++index) == read_bytes) {
                  /* woops! We just jumped the latest entry; we should mark it as last entry of the previous buffer, so we'll push it into the head of 
                   * the next buffer */
                  last_entry_previous_buffer = output_buffer[(read_bytes - 1)];
                }
              }
            }
          } else {
            last_entry_previous_buffer = input_buffer[0];
          }
          if (last_entry_previous_buffer != CHARACTER_OFFSET) {
            offset_buffer = 1;
          } else {
            offset_buffer = 0;
          }
        }
        if (last_entry_previous_buffer != CHARACTER_OFFSET) {
          output_buffer[write_bytes++] = last_entry_previous_buffer;
        }
        /* if the buffer is not empty, I'm afraid we have to dump the content */
        if (write_bytes > 0) {
          write(output_stream, output_buffer, write_bytes);
          total_written_bytes += write_bytes;
        }
        *score = ((double)total_written_bytes/(double)total_read_bytes);
        close(output_stream);
      } else {
        fprintf(stderr, "impossible to access the output file '%s' because %s\n", output_path, strerror(errno));
        result = KO;
      }
      close(input_stream);
    } else {
      fprintf(stderr, "impossible to access the input file '%s' because %s\n", input_path, strerror(errno));
      result = KO;
    }
    f_destroy_occurrency_table(&root);
  }
  return result;
}
int f_decode_file(const char *input_path, const char *output_path) {
  static char dictionary_buffer[LIMIT_DICTIONARY * 2];
  int result = OK, input_stream, output_stream;
  ssize_t read_bytes, write_bytes = 0, // write_bytes contains the number of byte int the output_buffer
          total_read_bytes = 0, total_written_bytes = 0;
  unsigned char input_buffer[BUFFER_SIZE], output_buffer[BUFFER_SIZE];
  unsigned int index;
  if ((input_stream = open(input_path, O_RDONLY)) > 0) {
    if ((output_stream = open(output_path, (O_WRONLY | O_CREAT | O_TRUNC), 0644)) != -1) {
      if ((read_bytes = read(input_stream, dictionary_buffer, (LIMIT_DICTIONARY * 2))) == (LIMIT_DICTIONARY * 2)) {
        while ((read_bytes = read(input_stream, input_buffer, BUFFER_SIZE)) > 0) {
          total_read_bytes += read_bytes;
          for (index = 0; index < read_bytes; ++index) {
            if (write_bytes > (BUFFER_SIZE - 2)) {
              /* if we don't have enough space to store the next couple of bytes we need to dump the content */
              write(output_stream, output_buffer, write_bytes);
              total_written_bytes += write_bytes;
              write_bytes = 0;
            }
            if (input_buffer[index] >= CHARACTER_OFFSET) {
              output_buffer[write_bytes++] = dictionary_buffer[((input_buffer[index] - CHARACTER_OFFSET) * 2)];
              output_buffer[write_bytes++] = dictionary_buffer[((input_buffer[index] - CHARACTER_OFFSET) * 2) + 1];
            } else {
              output_buffer[write_bytes++] = input_buffer[index];
            }
          }
        }
        /* is the buffer empty? */
        if (write_bytes > 0) {
          write(output_stream, output_buffer, write_bytes);
          total_written_bytes += write_bytes;
        }
      } else {
        fprintf(stderr, "impossible to find the dictionary in the header of the compressed file '%s' (we read %zu instead of %d)\n", 
            input_path, read_bytes, (LIMIT_DICTIONARY * 2));
        result = KO;
      }
      close(output_stream);
    } else {
      fprintf(stderr, "impossible to access the output file '%s' because %s\n", output_path, strerror(errno));
      result = KO;
    }
    close(input_stream);
  } else {
    fprintf(stderr, "impossible to access the input file '%s' because %s\n", input_path, strerror(errno));
    result = KO;
  }
  return result;
}
int main(int argc, char *argv[]) {
  double compression_score;
  int require_explanation = 1, result = 0;
  if (argc == 4) {
    if (strcmp(argv[1], "enc") == 0) {
      result = f_encode_file(argv[2], argv[3], &compression_score);
      printf("The compression score (final size VS initial size) is %.02f\n", compression_score);
      require_explanation = 0;
    } else if (strcmp(argv[1], "dec") == 0) {
      result = f_decode_file(argv[2], argv[3]);
      require_explanation = 0;
    }
  }
  if (require_explanation) {
    printf("Compress/Decompress text files using a retarded dictionary generated with the highest number of occurrencies in the source file\n");
    printf("Use:\n");
    printf("%s enc <uncompressed file> <where to save the compressed file>\n", argv[0]);
    printf("%s dec <compressed file> <where to save the uncompressed file>\n", argv[0]);
  }
  return 0;
}


