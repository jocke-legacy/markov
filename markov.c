#ifdef __unix__
#define _POSIX_C_SOURCE 200809L

#define timespec_diff(a, b) (struct timespec) { a.tv_sec  - b.tv_sec,    \
                                                a.tv_nsec - b.tv_nsec }

#define NANOSECS_IN_SEC 1000000000L

#define __INCLUDE_BENCHMARK__
#else
#undef  __INCLUDE_BENCHMARK__
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

#define FINITE_NEWLINE NULL

typedef struct {
   size_t length;
   size_t allocated;
   char **data;
} ArrayList;

typedef struct {
   uint8_t hashlength;
   size_t size;
   ArrayList **buckets;
} HashTable;

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
int arraylist_add(ArrayList *al, char *value);
int arraylist_add_smart(ArrayList *al, HashTable *h, char *value);
char *arraylist_str(ArrayList *al, char *delimiter);
void arraylist_free(ArrayList *al);
HashTable *hashtable_new(uint8_t hashlength);
int hashtable_add(HashTable *h, char *value);
int hashtable_find(HashTable *h, char *value, char **found);
uint32_t hashtable_jenkins(char *str);
void hashtable_free(HashTable *h); 
Finite *finite_load(char *filename);
int finite_filter(Finite *f);
char *finite_nextword(Finite *f, char *word);
int finite_prepare(Finite *f);
void finite_free(Finite *f);
char *markov_nextword(ArrayList *corpus, ArrayList *sentence, int pickiness);
char *markov(Finite *corpus, int pickiness, size_t length);

/* Benchmarking functions and their needs. */
#ifdef __INCLUDE_BENCHMARK__
double sum(double *arr, size_t length);
int compare(const void *p1, const void *p2);
void char_repeat(char c, size_t length);
double timespec2double(struct timespec *ts);
void markov_benchmark(int times, int pickiness, char *filename);
#endif


ArrayList *arraylist_new(void) {
   ArrayList *al;

   if ((al = malloc(sizeof(ArrayList))) == NULL) {
      return NULL;
   }
   memset(al, 0, sizeof(ArrayList));

   return al;
}

int arraylist_add(ArrayList *al, char *value) {
   char **data;

   if (al->length == al->allocated) {
      if ((data = realloc(al->data, sizeof(char *) * (al->allocated += 4))) == NULL) {
         return 0;
      }
      else {
         al->data = data;
      }
   }
   al->data[al->length++] = value;

   return 1;
}

/* This is an alternative way to add strings to our
 * list. It uses a hashtable to determine wheter
 * the same string already exists in the list. If so,
 * we use the same location for both.
 *
 * This gives us the oppertunity to compare strings
 * with pointers (which is fast) instead of using
 * `strcmp`.   */
int arraylist_add_smart(ArrayList *al, HashTable *h, char *value) {
   char *found;
   
   if (hashtable_find(h, value, &found)) {
      value = found;
   }
   else {
      hashtable_add(h, value);
   }

   return arraylist_add(al, value);
}


char *arraylist_str(ArrayList *al, char *delimiter) {
   size_t i, str_length;
   char *str;

   for (i = str_length = 0; i < al->length; i++) {
      str_length += strlen(al->data[i]);
   }
   str_length += al->length - 1;

   if ((str = malloc(sizeof(char) * str_length + strlen(delimiter) + 1)) == NULL) {
      return NULL;
   }
   for (i = 0, *str = '\0'; i < al->length; i++) {
      strcat(strcat(str, al->data[i]), delimiter);
   }
   str[str_length] = '\0'; /* no trailing delimiter */

   return str;
}

void arraylist_free(ArrayList *al) {
   if (al == NULL) {
      return;
   }

   free(al->data);
   free(al);
}


HashTable *hashtable_new(uint8_t hashlength) {
   HashTable *h;

   if ((h = malloc(sizeof(HashTable))) == NULL) {
      hashtable_free(h);
      return NULL;
   }

   h->size = 1 << hashlength;
   if ((h->buckets = malloc(sizeof(ArrayList *) * h->size)) == NULL) {
      hashtable_free(h);
      return NULL;
   }
   memset(h->buckets, 0, sizeof(ArrayList *) * h->size);

   h->hashlength = hashlength;

   return h;
}

int hashtable_add(HashTable *h, char *value) {
   uint32_t key;

   key = hashtable_jenkins(value) >> (32 - h->hashlength);

   if (h->buckets[key] == NULL) {
      h->buckets[key] = arraylist_new();
   }

   return arraylist_add(h->buckets[key], value);
}

int hashtable_find(HashTable *h, char *value, char **found) {
   size_t i;
   uint32_t key;
   char *element;

   key = hashtable_jenkins(value) >> (32 - h->hashlength);

   if (h->buckets[key] != NULL) {
      for (i = 0; i < h->buckets[key]->length; i++) {
         element = h->buckets[key]->data[i];
         if ((       element == value && value == NULL) ||
             (strcmp(element,   value)         == 0)) {
            *found = element;
            return 1;
         }
      }
   }

   return 0;
}

uint32_t hashtable_jenkins(char *str) {
   uint32_t hash, i;

   if (str    == NULL ||
       str[0] == '\0') {
      return 0;
   }

   for (i = hash = 0; i < strlen(str); i++) {
      hash += str[i];
      hash += (hash << 10);
      hash ^= (hash >> 6);
   }
   hash += (hash << 3);
   hash ^= (hash >> 11);
   hash += (hash << 15);


   return hash;
}

void hashtable_free(HashTable *h) {
   size_t i;
   
   if (h == NULL) {
      return;
   }

   for (i = 0; i < h->size; i++) {
      arraylist_free(h->buckets[i]);
   }
   free(h->buckets);
   free(h);
}


Finite *finite_load(char *filename) {
   FILE *fd;
   Finite *f;

   f = malloc(sizeof(Finite));

   if ((fd = fopen(filename, "r")) == NULL) {
      finite_free(f);
      return NULL;
   }

   fseek(fd, 0, SEEK_END);
   f->size = ftell(fd);
   rewind(fd);

   if ((f->data = malloc(sizeof(char) * f->size)) == NULL) {
      finite_free(f);
      return NULL;
   }
   if (fread(f->data, f->size, 1, fd) != 1) {
      finite_free(f);
      return NULL;
   }
   f->data[f->size - 1] = '\0';
   
   fclose(fd);

   return f;
}

int finite_filter(Finite *f) {
   size_t offset;
   char *newline, *buf;

   offset = 0;
   newline = f->data;
   while ((buf = strchr(newline, '>')) != NULL) {
      buf += 2;
      if ((newline = strchr(buf, '\n')) != NULL) {
         memmove(&f->data[offset], buf, newline - buf + 1);

         offset += newline - buf + 1;
      }
      else {
         memmove(&f->data[offset], buf, strlen(buf) + 1);
         f->data = realloc(f->data, sizeof(char) *
                                    (size_t) &buf[strlen(buf) + 1] - (size_t) f->data);
         
         break;
      }
   }

   return 1;
}

char *finite_nextword(Finite *f, char *word) {
   word += strlen(word) + 1;
   return word < f->data + f->size - 1 ? word : NULL;
}

int finite_prepare(Finite *f) {
   char *word, *line;
   int i;

   if ((f->h      = hashtable_new(20)) == NULL ||
       (f->words  = arraylist_new())   == NULL ||
       (f->lines  = arraylist_new())   == NULL) {
      hashtable_free(f->h);
      arraylist_free(f->words);
      arraylist_free(f->lines);
       return 0;
   }

   finite_filter(f);

   word = strtok(f->data, " ");
   while (word != NULL) {
      line = word;
      for (i = 0; (line = strchr(line, '\n')) != NULL; i++) {
         *line++ = '\0';
      }
 
      arraylist_add_smart(f->words, f->h, word);

      /* This is a manual `strtok` for newlines. The reason
       * for this is because standard strtok doesn't give
       * us the control we need to distinguish between
       * given delimiters, so that we can do different
       * operations depending on what we'd hit. */
      if (i > 0) {
         line = word;
         while (i--) {
            line = finite_nextword(f, line);

            /* We do normal add here because the location
             * is important.  */
            arraylist_add(f->lines, line);
            arraylist_add(f->words, FINITE_NEWLINE);

            /* We also do a smart add as new lines also
             * should be considered a new words.   */
            word = line;
            arraylist_add_smart(f->words, f->h, word);
         }
      }

      word = strtok(NULL, " ");
   }

   return 1;
}

void finite_free(Finite *f) {
   if (f == NULL) {
      return;
   }

   hashtable_free(f->h);
   arraylist_free(f->lines);
   arraylist_free(f->words);
   free(f->data);
   free(f);
}


char *markov_nextword(ArrayList *corpus, ArrayList *sentence, int pickiness) {
   size_t i, j, start;
   char *word;
   ArrayList *words;

   if (pickiness == 0) {
      return NULL;
   }

   if ((words = arraylist_new()) == NULL) {
      return NULL;
   }
   start = (sentence->length - 1) - (pickiness - 1);

   /* The selection of possible words should satisfy the `pickiness`
    * last words in the incomplete sentence. */
   for (i = 0; i < corpus->length - pickiness; i++) {
      /* We only need to compare the string pointers becasue
       * `arraylist_add_smart` used in `finite_prepare` and
       * `markov` maps existing words to their first
       * occurences. So if the string is the same, the
       * address is also the same.  */
      if (sentence->data[start] == corpus->data[i]) {
         for (j = 0; j < (size_t) pickiness; j++) {
            if (sentence->data[start + j] != corpus->data[i + j]) {
               break;
            }
         }
         if (j == (size_t) pickiness) {
             i += (size_t) pickiness;
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
char *markov(Finite *corpus, int pickiness, size_t length) {
   int i;
   char *str, *word;
   ArrayList *sentence;

   if ((sentence = arraylist_new()) == NULL) {
      return NULL;
   }
   word = corpus->lines->data[rand() % corpus->lines->length];

   /* To generate a sentence we first of all prepares `pickiness`
    * number of words so that we could go forth by choosing a
    * possible word that should follow.   */
   for (i = 0; i < pickiness; i++) {
      /* FINITE_NEWLINE represents a newline in corpus.
       * This basically says "if word is not NULL and word is not NULL".
       * The reason though is, even if FINITE_NEWLINE expands to NULL,
       * the code is more readable this way. */
      if (word != NULL && word != FINITE_NEWLINE) {
         arraylist_add_smart(sentence, corpus->h, word);
         word = finite_nextword(corpus, word);
         length--;
      }
      else {
         break;
      }
   }

   while (length-- && i == pickiness) {
      /* Line breaks are also considered words. This will terminate
       * the sentence in a natural way.
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
#ifdef __INCLUDE_BENCHMARK__
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

/* This function returns a string with a repeated character.
 * Note that the `buf` argument must be NULL or an allocated
 * adress on the heap.  */
void char_repeat(char c, size_t length) {
   while (length--) {
      putchar(c);
   }
}

double timespec2double(struct timespec *ts) {
   return ts->tv_sec + (long double) ts->tv_nsec / NANOSECS_IN_SEC;
}

/* This function outputs some nice numbers to make our
 * lives easier when benchmarking. It measures
 * preparation and generation times, and then prints the
 * results into a nice graph.
 *
 * The arguments specifices the number of sentences to generate,
 * the pickiness and the filename of the corpus to use.  */
void markov_benchmark(int n, int pickiness, char *filename) {
   int i, quarter, len1, len2, len3, len4;
   double scaling, preptime, min, q1, q2, q3, max;
   double *times;
   char *sentence;
   struct timespec start, end;
   Finite *corpus;

   clock_gettime(CLOCK_MONOTONIC, &start);
   corpus = finite_load(filename);
   finite_prepare(corpus);
   clock_gettime(CLOCK_MONOTONIC, &end);

   preptime = timespec2double(&timespec_diff(end, start));

   if ((times = malloc(sizeof(double) * n)) == NULL) {
      perror(NULL);
      exit(EXIT_FAILURE);
   }
   for (i = 0; i < n; i++) {
      clock_gettime(CLOCK_MONOTONIC, &start);
      sentence = markov(corpus, pickiness, 16);
      clock_gettime(CLOCK_MONOTONIC, &end);

      times[i] = timespec2double(&timespec_diff(end, start));

      printf("%s\n", sentence); 

      free(sentence);
   }
   finite_free(corpus);

   qsort(times, i, sizeof(double), compare);
   quarter = i / 4;

   min = times[0];
   q1  = times[1 * quarter];
   q2  = times[2 * quarter];
   q3  = times[3 * quarter];
   max = times[i - 1];

   scaling = 50 / (max - min);
   len1    = (q1 - min) * scaling;
   len2    = (q2 - q1)  * scaling;
   len3    = (q3 - q2)  * scaling;
   len4    = (max - q3) * scaling;

   printf("\nPreparation time: %.3f seconds\n", preptime);
   printf("Generated %d sentences in %.3f seconds. (average time was %.3f seconds)\n",
          i,
          sum(times, i),
          sum(times, i) / i);
   printf("Total amount of time: %.3f seconds\n", sum(times, i) + preptime);

   printf("<%.3f" , min); char_repeat('-', len1);
   printf("[%.3f" , q1 ); char_repeat('-', len2);
   printf("|%.3f|", q2 ); char_repeat('-', len3);
   printf("%.3f]" , q3 ); char_repeat('-', len4);
   printf("%.3f>\n" , max);

   free(times);
}
#endif


int main(int argc, char **argv) {
   int i;
   char *sentence;
   Finite *corpus;

   srand(time(NULL));

   switch (argc) {
#ifdef __INCLUDE_BENCHMARK__
      case 5:
         if (strcmp(argv[1], "benchmark") == 0 && isdigit(argv[2][0])
             && isdigit(argv[3][0])) {
            markov_benchmark(atoi(argv[2]), atoi(argv[3]), argv[4]);
         }
         else {
            return EXIT_FAILURE;
         }

         return EXIT_SUCCESS;
#endif

      case 4:
         if (!isdigit(argv[1][0]) || !isdigit(argv[2][0])) {
            return EXIT_FAILURE;
         }

         break;

      default:
         return EXIT_FAILURE;
   }

   if ((corpus = finite_load(argv[3])) == NULL ||
       !finite_prepare(corpus)) {
      finite_free(corpus);
      return EXIT_FAILURE;
   }

   for (i = 0; i < atoi(argv[1]); i++) {
      sentence = markov(corpus, atoi(argv[2]), 16);

      printf("%s\n", sentence); 

      free(sentence);
   }

   finite_free(corpus);

   return EXIT_SUCCESS;
}
