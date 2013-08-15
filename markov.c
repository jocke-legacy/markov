#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "uthash.h"

#define tsdiff(a, b) (struct timespec) { a.tv_sec  - b.tv_sec,    \
                                         a.tv_nsec - b.tv_nsec }

#define ARRAYLIST_ALLOCATION_SIZE 1024 
#define FINITE_NEWLINE            NULL

typedef struct {
   char *key;
//   char *value;
   UT_hash_handle hh;
} HashTable;

typedef struct {
   size_t length;
   size_t allocated;
   char **data;
} ArrayList;

/* This structure represents a finite file in
 * the form of its size, data, words and lines
 * so that they are easily accessible.
 *
 * The data is altered as a part of the
 * preparation process. */
typedef struct {
   size_t size;
   char *data;
   ArrayList *words;
   ArrayList *lines;
   HashTable *h;
} Finite;


ArrayList *arraylist_new(void);
void arraylist_add(ArrayList *al, char *value);
void arraylist_add_smart(ArrayList *al, HashTable **h, char *value);
void hash_free(HashTable **h); 
char *arraylist_str(ArrayList *al, char *delimiter);
void arraylist_free(ArrayList *al);
Finite *finite_load(char *filename);
char *finite_nextword(Finite *f, char *word);
void finite_prepare(Finite *f);
void finite_free(Finite *f);
char *markov_nextword(ArrayList *corpus, ArrayList *sentence, unsigned int pickiness);
char *markov(Finite *corpus, unsigned int pickiness, size_t length);

/* Benchmarking functions and their needs. */
double sum(double *arr, size_t length);
int compare(const void *p1, const void *p2);
void printchr_iterate(char c, size_t length);
double ts2d(struct timespec *ts);
void markov_timer(int times);


ArrayList *arraylist_new(void) {
   ArrayList *al;

   if ((al = malloc(sizeof(ArrayList))) == NULL) {
      perror("Can't create list");
      exit(EXIT_FAILURE);
   }

   memset(al, 0, sizeof(ArrayList));
   al->allocated = 1;

   return al;
}

void arraylist_add(ArrayList *al, char *value) {
   if (al->length == al->allocated || al->allocated == 1) {
      al->allocated *= al->allocated == 1 ? ARRAYLIST_ALLOCATION_SIZE : 2;
      if ((al->data = realloc(al->data, sizeof(char *) * al->allocated))
            == NULL) {
         perror("Can't add element to list");
         exit(EXIT_FAILURE);
      }
   }

   al->data[al->length++] = value;
}

/* This is an alternative way to add strings to our
 * list. It uses a hashtable to determine wheter
 * the same string already exists in the list. If so,
 * we use the same location for both.
 *
 * This gives us the oppertunity to compare strings
 * with pointers (which is fast) instead of using
 * `strcmp`.   */
void arraylist_add_smart(ArrayList *al, HashTable **h, char *value) {
   char *key_ptr;
   HashTable *new, *found;

   key_ptr = value;

   HASH_FIND_STR(*h, key_ptr, found);
   if (found != NULL) {
      value = found->key;
   }
   else {
      if ((new = malloc(sizeof(HashTable))) == NULL) {
         perror("Cannot add field to hash table");
         exit(EXIT_FAILURE);
      }
      new->key = key_ptr;
      HASH_ADD_KEYPTR(hh, *h, key_ptr, strlen(key_ptr), new);
   }

   arraylist_add(al, value);
}

void hash_free(HashTable **h) {
   HashTable *curr, *tmp;

   HASH_ITER(hh, *h, curr, tmp) {
      HASH_DEL(*h, curr);
      free(curr);
   }
   *h = NULL;
}

char *arraylist_str(ArrayList *al, char *delimiter) {
   size_t i, str_length;
   char *str;

   for (i = str_length = 0; i < al->length; i++) {
      str_length += strlen(al->data[i]);
   }
   str_length += al->length - 1;

   if ((str = malloc(sizeof(char) * str_length + strlen(delimiter) + 1)) == NULL) {
      perror("Can't allocate space for string");
      exit(EXIT_FAILURE);
   }
   for (i = 0, *str = '\0'; i < al->length; i++) {
      strcat(strcat(str, al->data[i]), delimiter);
   }
   *(str + str_length) = '\0'; /* no trailing delimiter */

   return str;
}

void arraylist_free(ArrayList *al) {
   free(al->data);
   free(al);
}


Finite *finite_load(char *filename) {
   FILE *fd;
   Finite *f;

   f = malloc(sizeof(Finite));
   f->h = NULL;

   if ((fd = fopen(filename, "r")) == NULL) {
      perror("Can't open file");
      exit(EXIT_FAILURE);
   }

   fseek(fd, 0, SEEK_END);
   f->size = ftell(fd);
   rewind(fd);

   if ((f->data = malloc(sizeof(char) * f->size)) == NULL) {
      perror("Can't allocate space for file");
      exit(EXIT_FAILURE);
   }

   if (fread(f->data, f->size, 1, fd) != 1) {
      perror("Error while reading file");
      exit(EXIT_FAILURE);
   }
   f->data[f->size - 1] = '\0';

   fclose(fd);

   return f;
}

char *finite_nextword(Finite *f, char *word) {
   char *nextword;

   nextword = word + strlen(word) + 1;

   return nextword < f->data + f->size - 1 ? nextword : NULL;
}

void finite_prepare(Finite *f) {
   char *word;
   char *line;

   f->words  = arraylist_new();
   f->lines  = arraylist_new();
 
   word = strtok(f->data, " ");
   while (word != NULL) {
      arraylist_add_smart(f->words, &f->h, word);

       /* This is a manual `strtok` for newlines. The reason
       * for this is because standard strtok doesn't give
       * us the control we need to distinguish between
       * given delimiters, so that we can do different
       * operations depending on what we'd hit. */
      if ((line = strchr(word, '\n')) != NULL) {
         *line = '\0';
         if (++line < f->data + f->size) {
            /* We do normal add here because the location
             * is important.  */
            arraylist_add(f->lines, line);

            arraylist_add(f->words, FINITE_NEWLINE);

            /* We also do a smart add as new lines also
             * should be considered a new words.   */
            word = line;
            arraylist_add_smart(f->words, &f->h, word);
         }
      }

      word = strtok(NULL, " ");
   }
}

void finite_free(Finite *f) {
   hash_free(&f->h);
   arraylist_free(f->lines);
   arraylist_free(f->words);
   free(f->data);
   free(f);
}


char *markov_nextword(ArrayList *corpus, ArrayList *sentence, unsigned int pickiness) {
   size_t i, j, start;
   char *word;
   ArrayList *words;

   words = arraylist_new();
   start = (sentence->length - 1) - (pickiness - 1);

   /* The selection of possible words should satisfy the `pickiness`
    * last words in the incomplete sentence. */
   for (i = 0; i < corpus->length - pickiness; i++) {
      /* We only need to compare the string pointers becasue
       * `arraylist_add_smart` used in `finite_prepare` and
       * `markov` maps existing words to their first
       * occurences. So if the string is the same, the
       * address is also the same   */
      if (sentence->data[start] == corpus->data[i]) {
         for (j = 0; j < pickiness; j++) {
            if (sentence->data[start + j] != corpus->data[i + j]) {
               break;
            }
         }
         if (j == pickiness) {
             i += pickiness;
             arraylist_add(words, corpus->data[i]);
         }
      }
   }

   word = words->length > 0 ? words->data[rand() % words->length] : NULL;
   arraylist_free(words);

   return word;
}

/* The heart of this program. It generates a sentence based
 * on the words found in corpus.
 *
 * Apart from the corpus, it takes two additional arguments:
 *    * `pickiness`, which specifies how strict we will
 *      genereate the sentence.
 *    * `length`, which is simply the maximum number of
 *      words that we will have in our sentence.   */
char *markov(Finite *corpus, unsigned int pickiness, size_t length) {
   unsigned int i;
   char *str, *word;
   ArrayList *sentence;

   sentence = arraylist_new();
   word = corpus->lines->data[rand() % corpus->lines->length];

   /* To generate a sentence we first of all prepares `pickiness`
    * number of words so that we could go forth by choosing a
    * possible word that should follow.   */
   for (i = 0; i < pickiness; i++) {
      /* Yes, this basically says "if word is not NULL and word is not NULL"
       * The reason though, even if FINITE_NEWLINE expands to NULL, the code
       * is more readable this way. It won't affect any performance either as
       * the compiler will optimise it away. */
      if (word != NULL && word != FINITE_NEWLINE) {
         arraylist_add_smart(sentence, &corpus->h, word);
         word = finite_nextword(corpus, word);
         length--;
      }
      else {
         break;
      }
   }

   while (length-- && i == pickiness) {
      /* Line breaks are also considered words. This will terminate
       * the sentence a natural way.
       *
       * As a side effect, this will also terminate the sentence if
       * there are no matching next word, because `markov_nextword`
       * returns NULL for both cases.
       *
       * See commentary above.   */
      word = markov_nextword(corpus->words, sentence, pickiness);
      if (word != NULL && word != FINITE_NEWLINE) {
         arraylist_add(sentence, word);
      }
      else {
         break;
      }
   }

   str = arraylist_str(sentence, " ");
   arraylist_free(sentence);

   return str;
}


/* Benchmarking functions and their needs. */

double sum(double *arr, size_t length) {
   double sum_;
   size_t i;

   for (i = 0, sum_ = 0; i < length; i++) {
      sum_ += arr[i];
   }

   return sum_;
}

int compare(const void *p1, const void *p2) {
   double p1_, p2_;

   p1_ = *(double *) p1;
   p2_ = *(double *) p2;

   if (p1_ < p2_) {
      return -1;
   }
   else if (p1_ == p2_) {
      return 0;
   }
   else {
      return 1;
   }
}

void printchr_iterate(char c, size_t length) {
   char *str;

   if (length > 0) {
      if ((str = malloc(sizeof(char) * length + 1)) == NULL) {
         perror(NULL);
         exit(EXIT_FAILURE);
      }
      memset(str, c, length);
      str[length] = '\0';

      printf("%s", str);

      free(str);
   }
}

double ts2d(struct timespec *ts) {
   return ts->tv_sec + (long double) ts->tv_nsec / 1000000000L;
}

/* This function outputs some nice numbers to make our
 * lives easier when benchmarking. It measures
 * preparation and generation times, and then prints the
 * results into a nice graph.
 *
 * The argument specifies how many times we want to
 * generate a sentence.  */
void markov_timer(int times) {
   int i, quarter, len1, len2, len3, len4;
   double scaling, preptime, min, q1, q2, q3, max;
   double *times_;
   char *sentence;
   struct timespec start, end;
   Finite *corpus;

   clock_gettime(CLOCK_MONOTONIC, &start);
   corpus = finite_load("corpus.txt");
   finite_prepare(corpus);
   clock_gettime(CLOCK_MONOTONIC, &end);

   preptime = ts2d(&tsdiff(end, start));

   if ((times_ = malloc(sizeof(double) * times)) == NULL) {
      perror("Can't allocate space");
      exit(EXIT_FAILURE);
   }
   for (i = 0; i < times; i++) {
      clock_gettime(CLOCK_MONOTONIC, &start);
      sentence = markov(corpus, 2, 16);
      clock_gettime(CLOCK_MONOTONIC, &end);

      times_[i] = ts2d(&tsdiff(end, start));

      printf("%s\n", sentence); 

      free(sentence);
   }

   qsort(times_, i, sizeof(double), compare);
   quarter = i / 4;

   min = times_[0];
   q1  = times_[1 * quarter];
   q2  = times_[2 * quarter];
   q3  = times_[3 * quarter];
   max = times_[i - 1];

   scaling = 50 / (max - min);
   len1    = (q1 - min) * scaling;
   len2    = (q2 - q1)  * scaling;
   len3    = (q3 - q2)  * scaling;
   len4    = (max - q3) * scaling;

   printf("\nPreparation time: %.3f seconds\n", preptime);
   printf("Generated %u sentences in %.3f seconds. (average time was %.3f seconds)\n",
          (unsigned int) i,
          sum(times_, i),
          sum(times_, i) / i);
   printf("Total amount of time: %.3f seconds\n", sum(times_, i) + preptime);

   printf("<%.3f", min);
   printchr_iterate('-', len1);
   printf("[%.3f", q1);
   printchr_iterate('-', len2);
   printf("|%.3f|", q2);
   printchr_iterate('-', len3);
   printf("%.3f]", q3);
   printchr_iterate('-', len4);
   printf("%.3f>\n", max);

   free(times_);
   finite_free(corpus);
}


int main() {
   srand(time(NULL));

   markov_timer(100);
   
   return EXIT_SUCCESS;
}
