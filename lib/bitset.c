#include "bitset.h"
#include <assert.h>
#define BITS_PER_WORD (sizeof(void*) * 8)
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define WORDS(set) (((set)->capacity + BITS_PER_WORD - 1) / BITS_PER_WORD)
#define CHECK(set) assert(do_check(set))
#define roundup(x,n) (((x)+((n)-1))&(~((n)-1)))

/** Implements set of integers as a bit vector */
struct bit_set {
    int capacity;  /*< Capacity of this set (in bits) */
    unsigned bits[1];   /*< Variable size array for members */
};

/** Checks that all redundant bits are zero.
    Redundant bits are those whose index is >= capacity.
*/
static int do_check(BitSet set) {
    int limit = set->capacity;
    int max = roundup(limit, BITS_PER_WORD);
    int i;
    for (i = limit; i < max; i++) {
        int pattern = 1 << (i & (BITS_PER_WORD - 1)); 
        if((set->bits[WORDS(set) - 1] & pattern) != 0)
            return 0;
    }
    return 1;
}

BitSet set_new(int capacity, Arena a) {
    int i, words = (capacity + BITS_PER_WORD - 1) / BITS_PER_WORD;
    BitSet set = gvmt_allocate(sizeof(int) * (words + 1), a);
    set->capacity = capacity;
    for(i = 0; i < words; i++) {
        set->bits[i] = 0;
    }
    CHECK(set);
    return set;
}

void set_add(BitSet set, int bit) {
    int word, pattern;
    assert(set);
    assert(bit >= 0);
    assert(bit < set->capacity);
    word = bit / BITS_PER_WORD; 
    pattern = 1 << (bit & (BITS_PER_WORD - 1));
    set->bits[word] |= pattern;
    CHECK(set);
}

void set_remove(BitSet set, int bit) {
    int word, pattern;
    assert(set);
    assert(bit >= 0);
    assert(bit < set->capacity);
    word = bit / BITS_PER_WORD; 
    pattern = 1 << (bit & (BITS_PER_WORD - 1));
    set->bits[word] &= (~pattern);
    CHECK(set);
}

int set_contains_function(BitSet set, int bit) {
    int word, pattern;
    assert(set);
    assert(bit >= 0);
    assert(bit < set->capacity);
    word = bit / BITS_PER_WORD; 
    pattern = 1 << (bit & (BITS_PER_WORD - 1));
    return (set->bits[word] & pattern);
}

int set_equals(BitSet set1, BitSet set2) {
    int i, words;
    assert(set1->capacity == set2->capacity);   
    words = WORDS(set1);
    for (i = 0; i < words; i++) {
        if (set1->bits[i] != set2->bits[i])
            return 0;
    }
    return 1;
}

int bitset_capacity(BitSet set) {
    return set->capacity;
}

/* Could optimise this */
void set_moveBit(BitSet set, int to, int from) {
    if (set_contains(set, from)) {
        set_add(set, to);    
        set_remove(set, from);    
    } else {
        set_remove(set, to);    
    }
}

void set_clear(BitSet set) {
    int i, words;
    assert(set);
    words = (set->capacity + BITS_PER_WORD - 1) / BITS_PER_WORD;
    for (i = 0; i < words; i++) {
        set->bits[i] = 0;    
    }
    CHECK(set);
}

void set_print(FILE *file, BitSet set) {
    int i;
    FOR_EACH(i, set) {
        fprintf(file, "%d ", i);
    }
}

static const int MultiplyDeBruijnBitPosition[32] = 
{
  0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 
  31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
};

#define TRAILING_ZEROES(v) MultiplyDeBruijnBitPosition[(((v) & -(v)) * 0x077CB531UL) >> 27]

int set_next(BitSet set, int from) {
    int words, result, offset;
    if (set->capacity == from) {
        return -1;    
    }
    assert(set->capacity > from);
    words = WORDS(set);
    offset = from & (BITS_PER_WORD - 1);
    int start_word = from / BITS_PER_WORD;
    unsigned x = set->bits[start_word] & (-1 << offset);
    if (x) {
        int index = TRAILING_ZEROES(x);
        return index + start_word * BITS_PER_WORD; 
    }
    for (int i = start_word + 1; i < words; i++) {
        x = set->bits[i];
        if (x) {
            int index = TRAILING_ZEROES(x);
            result = index + i * BITS_PER_WORD;
            assert(result >= from && result < set->capacity);
            return result;
            
        }
    }
    return -1;
}

int trailing_zeroes(int val) {
    if (val)
        return TRAILING_ZEROES(val);
    else
        return BITS_PER_WORD;
}

static int countBits( int x )
   {
   int count = 0;
   while ( x != 0 ) {
       x &= x - 1;
       count++;
   }
   return count;
}

int set_size(BitSet set) {
    int words, total, i;
    words = WORDS(set);
    total = 0;    
    for (i = 0; i < words; i++) {
        total += countBits(set->bits[i]);
    }
    return total;
}

int set_empty(BitSet set) {
    int i, words;
    words = WORDS(set);    
    for (i = 0; i < words; i++) {
        if (set->bits[i]) {
            return 0;
        }
    }
    return 1;
}

void set_addSet(BitSet set, const BitSet pattern) {
    int i, words;
    assert(set->capacity == pattern->capacity);
    words = WORDS(set);
    for (i = 0; i < words; i++) {
        set->bits[i] |= pattern->bits[i];    
    }    
    CHECK(set);
}

void set_invert(BitSet set) {
    int i, words;
    words = WORDS(set);
    for (i = 0; i < words - 1; i++) {
        set->bits[i] = ~set->bits[i];    
    }    
    int offset = set->capacity & (BITS_PER_WORD - 1);
    unsigned int pattern = 0xffffffffUL >> ((BITS_PER_WORD - offset) & (BITS_PER_WORD - 1));
    set->bits[WORDS(set) - 1] ^= pattern;
}

void set_removeSet(BitSet set, const BitSet pattern) {
    int i, words;
    assert(set->capacity == pattern->capacity);
    words = WORDS(set);
    for (i = 0; i < words; i++) {
        set->bits[i] &= (~(pattern->bits[i]));
    }
    CHECK(set);
}

