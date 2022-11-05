//
// Extended Golay error correction code
//
#ifndef GOLAY_H_INCLUDED
#define GOLAY_H_INCLUDED

class GolayCode {
  public:
    int processed_codewords_ = 0;
    int corrected_codewords_ = 0;
    int uncorrectable_codewords_ = 0;

    // Takes 12 bits of data, appends 12 checksum bits, returns a 24 bit codeword
    unsigned inline encode(unsigned x) const
    {
      return ((x & 0xfff) << 12) | checksum_bits(x);
    }

    // Takes a 24 bit codeword, returns decoded 12 bits.
    // On unrecoverable error, returns -1.
    int decode(unsigned x)
    {
      processed_codewords_++;

      unsigned received_data = (x >> 12) & 0xfff;
      unsigned received_checksum = x & 0xfff;
      unsigned expected_checksum = checksum_bits(received_data);

      // Which checksum bits differ between what we expected and what we received 
      unsigned syndrome = expected_checksum ^ received_checksum;

      // Number of checksum bits that differ between what we expected and what we received 
      int weight = __builtin_popcount(syndrome);

      // When there are three or fewer errors in the checksum bits, then either there are no errors
      // in the data bits (and no correction is necessary), or there are at least 5 errors in the 
      // data bits (and we can't correct that many errors anyway, so return data bits as they are).
      if (weight <= 3) {
        if (weight) {
          corrected_codewords_++;  
        }
        return received_data;
      }

      // To find exactly one error in the data bits, flip each bit in turn and see if an error in 
      // that bit gets us within two bits of the received checksum. More errors would give a total 
      // error weight of 4 or more, which would be uncorrectable.
      for (int i = 0; i < 12; i++) {
        unsigned error_mask = 1 << (11 - i);
        unsigned coding_error = golay_matrix_[i];
        if (__builtin_popcount(syndrome ^ coding_error) <= 2) {
          corrected_codewords_++;
          return received_data ^ error_mask;
        }
      }

      // Check whether all (up to three) errors are in the data bits
      unsigned inverted_syndrome = checksum_bits(syndrome);
      int w = __builtin_popcount(inverted_syndrome);
      if (w <= 3) {
        corrected_codewords_++;
        return received_data ^ inverted_syndrome;
      }

      // Finally, try to find two errors in the data bits and one in the checksum bits
      for (int i = 0; i < 12; i++) {
        unsigned coding_error = golay_matrix_[i];
        if (__builtin_popcount(inverted_syndrome ^ coding_error) <= 2) {
          corrected_codewords_++;
          return received_data ^ inverted_syndrome ^ coding_error;
        }
      }

      // Give up
      uncorrectable_codewords_++;
      return -1;
    }

  private:
    static constexpr unsigned const golay_matrix_[] = {
      0x9f1, 0x4fa, 0x27d, 0x93e, 0xc9d, 0xe4e,
      0xf25, 0xf92, 0x7c9, 0x3e6, 0x557, 0xaab
    };

    unsigned inline checksum_bits(unsigned x) const
    {
      unsigned y = 0;
      for (int i = 0; i < 12; i++) {
        y = (y << 1) | __builtin_parity(x & golay_matrix_[i]);
      }
      return y;
    }
};
#endif
