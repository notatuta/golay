//
// Golay error correction code example
//
#include <cstring>
#include <limits.h>
#include <random>
#include <stdio.h>
#include "golay.h"

int main(int argc, char* argv[])
{
  GolayCode gc;
  if (argc != 2) {
    printf("Usage:\n\n\tgolay filename\n\n"
"Adds extended Golay code checksum bits to the file, then splits it in 8\n"
"slices named filename.Golay_A, filename.Golay_B, ..., filename_Golay_H.\n\n"
"If filename ends in \".Golay_A\", performs the decoding and recreates the\n"
"original file from the slices.\n\n"
"Up to 3 bit errors in a 24 bit codeword can be corrected. Each slice has\n"
"3 bits from any codeword, therefore the original file can be restored\n"
"even when one of the 8 slices is completely missing, as long as there are\n"
"no other errors.\n\n"
"When called without arguments, shows usage and runs a self-test.\n\n");
    unsigned not_decoded_ct[11] = {};
    unsigned decoded_ok_ct[11] = {};
    unsigned decoded_wrong_ct[11] = {};
    //
    // Self-test: encode some random 12-bit words, add errors, count
    // how many errors are corrected or detected
    //
    for (int i = 0; i < 10000; i++) {
      for (int j = 0; j < 11; j++) {
        unsigned x = rand() & 0xfff;
        auto y = gc.encode(x);
        unsigned errors = 0;
        for (int k = 0; k < j; ) {
          unsigned bit = 1 << (rand() % 24);
          if ((errors & bit) == 0) {
            errors |= bit;
            k++;
          }
        }
        auto z = gc.decode(y ^ errors);
        if (z < 0) {
          not_decoded_ct[j]++;
        } else if ((unsigned)z == x) {
          decoded_ok_ct[j]++;
        } else {
          decoded_wrong_ct[j]++;
        }
        if ((unsigned)z != x && j < 4) {
          printf("Self-test FAILED\nOriginal:    0x%03x\nTransmitted: 0x%06x\nError bits:  0x%06x\nReceived:    0x%06x\n", 
            x, y, errors, y ^ errors);
          if (z < 0) {
            printf("Nothing decoded\n");
          } else {
            printf("Decoded:     0x%03x\n", z);
          }
          return -1;
        }
      }
    }
    printf("Self-test summary statistics:\n\n"
           "Flipped\tDecoded\tNot\tDecoded\n"
           "bits\tok\tdecoded\twrong\n"
           "-------\t-------\t-------\t-------\n");
    for (int j = 0; j < 11; j++) {
      printf("%d\t%u\t%u\t%u\n", j, decoded_ok_ct[j], not_decoded_ct[j], decoded_wrong_ct[j]);
    }
    printf("\nProcessed %d twelve-bit codewords, corrected %d codewords, %d uncorrectable codewords\n"
           "\nSelf-test PASSED\n", 
            gc.processed_codewords_, gc.corrected_codewords_, gc.uncorrectable_codewords_);
    return 0;
  }
  char filename[PATH_MAX + 1] = {};
  strncpy(filename, argv[1], PATH_MAX);
  int len = strlen(filename);
  bool encode = true;
  if (len > 8) {
    if (strncmp(filename + len - 8, ".Golay_", 7) == 0) {
      len -= 8;
      filename[len] = '\0';
      encode = false;
    }
  }
  int retcode = 0;
  printf("%s %s\n", encode ? "Encoding" : "Decoding", filename);
  FILE* f = fopen(filename, encode ? "rb" : "wb");
  if (f) {
    if (PATH_MAX - len < 8) {
      printf("File name is too long, should be less than %d\n", PATH_MAX - 8);
    } else {
      char* p = filename + len;
      *p++ = '.';
      *p++ = 'G';
      *p++ = 'o';
      *p++ = 'l';
      *p++ = 'a';
      *p++ = 'y';
      *p++ = '_';
      FILE* slice[8] = {};
      int good_slice_ct = 0;
      for (int i = 0; i < 8; i++) {
        *p = 'A' + i;
        slice[i] = fopen(filename, encode ? "wb" : "rb");
        if (slice[i]) {
          good_slice_ct++;
        } else {
          printf("Can't open file %s\n", filename);
        }
      }
      if (encode && (good_slice_ct < 8)) {
        printf("Could not open all 8 output files to encode\n");
        retcode = -4;
      } else if (!encode && (good_slice_ct < 7)) {
        printf("Need 7 or 8 files to decode, found only %d\n", good_slice_ct);
        retcode = -5;
      } else {
        //
        // Process original file in 12 byte (96 bit) long blocks.
        // After adding 96 checksum bits, one such block corresponds to 24 bytes (192 bits).
        // Split those bits across 8 files such that each file contains 3 bits of any codeword.
        // Extended Golay code can correct up to 3 errors, therefore the data can be recovered 
        // after losing one of eight files, provided there are no other errors.
        // 
        // The approach below is brute force for ease of reading: zero all bits in target buffer,
        // then loop over all bits in source buffer. If the source buffer bit is set, then set 
        // corresponding bit in the target buffer. This sidesteps endianess and other bit 
        // twiddling complexities.
        //
        unsigned char twelvebytes[12];
        unsigned char eighttriplets[8][3];
        bool go_on = true;
        if (encode) {
          do {
            size_t ct = fread(&twelvebytes[0], 1, 12, f);
            if (ct < 12) {
              if (feof(f)) {
                // 
                // PKCS #7 padding: add between 1 and 12 bytes
                //
                // 0x01
                // 0x02 0x02
                // 0x03 0x03 0x03
                // ...
                //
                int pad = 12 - ct;
                for (int i = ct; i < 12; i++) {
                  twelvebytes[i] = pad;
                }
              } else {
                printf("Error reading file %s\n", argv[1]);
              }
              go_on = false;
            }
            for (int k = 0; k < 8; k++) {
              for (int t = 0; t < 3; t++) {
                eighttriplets[k][t] = 0;
              }
            }
            for (int i = 0; i < 8; i++) {
              int x = 0;
              for (int j = 0; j < 12; j++) {
                if (twelvebytes[j] & (1 << i)) {
                  x |= (1 << j);
                }
              }
              int codeword = gc.encode(x);
              for (int k = 0; k < 8; k++) {
                for (int t = 0; t < 3; t++) {
                  if (codeword & (1 << (k * 3 + t))) {
                    eighttriplets[k][t] |= (1 << i);
                  }
                }
              }
            }
            for (int i = 0; i < 8; i++) {
              if (fwrite(&eighttriplets[i], 1, 3, slice[i]) != 3) {
                printf("Error writing slice %c\n", 'A' + i);
                go_on = false;
                retcode = -3;
              }
            }
          } while (go_on); // end encode loop 
        } else { // decode
          bool first_block = true;
          bool last_block = false;
          do {
            for (int k = 0; k < 8; k++) {
              for (int t = 0; t < 3; t++) {
                eighttriplets[k][t] = 0;
              }
            }
            for (int i = 0; i < 8; i++) {
              if (!slice[i]) {
                // Don't try to read missing file
              } else if (fread(&eighttriplets[i], 1, 3, slice[i]) != 3) {
                last_block = true;
              }
            }
            if (first_block) {
              first_block = false;
            } else {
              size_t ct = 12;
              if (last_block) { // Remove padding
                int pad = twelvebytes[11];
                if (pad < 1 || pad > 12) {
                  printf("Unexpected padding byte %d, ignoring\n", pad);
                } else {
                  ct -= pad;
                }
              }
              size_t written = fwrite(&twelvebytes[0], 1, ct, f);
              if (written < ct) {
                printf("Error writing file %s\n", argv[1]);
              }
            }
            if (!last_block) {
              for (int j = 0; j < 12; j++) {
                twelvebytes[j] = 0;
              }
              for (int i = 0; i < 8; i++) {
                int codeword = 0;
                for (int k = 0; k < 8; k++) {
                  for (int t = 0; t < 3; t++) {
                    if (eighttriplets[k][t] & (1 << i)) {
                      codeword |= (1 << (k * 3 + t));
                    }
                  }
                }
                int x = gc.decode(codeword);
                if (x < 0) {
                  retcode = -7; // Uncorrectable error
                } else {
                  for (int j = 0; j < 12; j++) {
                    if (x & (1 << j)) {
                      twelvebytes[j] |= (1 << i);
                    }
                  }
                }
              }
            }
          } while (!last_block); // end of decode loop 
          printf("Processed %d twelve-bit codewords, corrected %d codewords, %d uncorrectable codewords\n", 
            gc.processed_codewords_, gc.corrected_codewords_, gc.uncorrectable_codewords_);
        } // end decode
      }
      for (int i = 0; i < 8; i++) {
        if (slice[i]) {
          fclose(slice[i]);
        }
      }
    }
    fclose(f);
  } else {
    printf("Cannot open file %s\n", argv[1]);
    retcode = -2;
  }
  return retcode;
}
