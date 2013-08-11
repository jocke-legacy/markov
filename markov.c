#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "uthash.h"

#define ARRAYLIST_ALLOCATION_SIZE 1024 


typedef struct {
   char *key;
   char *value;
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
   char newline;
   ArrayList *words;
   ArrayList *lines;
   HashTable *h;
} Finite;

void hash_add(HashTable **h, char *key, void *value);
HashTable *hash_find(HashTable **h, char *key);
void hash_free(HashTable **h);
ArrayList *arraylist_new(void);
void arraylist_add(ArrayList *al, char *value);
void arraylist_add_smart(ArrayList *al, HashTable *h, char *value);
char *arraylist_str(ArrayList *al, char *delimiter);
void arraylist_free(ArrayList *al);
Finite *finite_load(char *filename);
char *finite_nextword(Finite *f, char *word);
void finite_prepare(Finite *f);
void finite_free(Finite *f);
ArrayList *markov_nextwords(ArrayList *c, ArrayList *sentence, int pickiness);
char *markov(Finite *corpus, int pickiness, size_t length);

/* Benchmarking functions and their needs. */
double sum(double *arr, size_t length);
int compare(const void *p1, const void *p2);
void printchr_iterate(char c, size_t length);
void markov_timer(int times);


void hash_add(HashTable **h, char *key, void *value) {
   HashTable *new;

   if (hash_find(h, key) != NULL) {
      if ((new = malloc(sizeof(HashTable))) == NULL) {
         perror("Cannot add field to hash table");
         exit(EXIT_FAILURE);
      }
      new->key = key;
      new->value = value;
      HASH_ADD_KEYPTR(hh, *h, new->key, strlen(new->key), new);
   }
}

HashTable *hash_find(HashTable **h, char *key) {
   HashTable *found;

   HASH_FIND_STR(*h, key, found);

   return found;
}

void hash_free(HashTable **h) {
   HashTable *curr, *tmp;

   HASH_ITER(hh, *h, curr, tmp) {
      HASH_DEL(*h, curr);
      free(curr);
   }
   *h = NULL;
}

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
void arraylist_add_smart(ArrayList *al, HashTable *h, char *value) {
   HashTable *found;

   if ((found = hash_find(&h, value)) != NULL) {
      value = found->value;
   }
   else {
      hash_add(&h, value, value);
   }

   arraylist_add(al, value);
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
   f->newline = '\n';
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

/*   word = f->data;
   while ((word = strchr(word, ' ' )) != NULL) {
         hash_size++;
         word++;
   }*/
   /* As stated in the documentation for `hcreate`, the
    * implementation method leaves a possibility for
    * collisions in hash tables if populated more than 80%
    *
    * We create a hashtable which should have 20% left when
    * when considered fully populated, so that we can ensure
    * O(1) lookups. */
/*   if (hcreate(hash_size) == 0) {
      perror("Can't create hashtable");
      exit(EXIT_FAILURE);
   }*/
 
   word = strtok(f->data, " ");
   while (word != NULL) {
      arraylist_add_smart(f->words, f->h, word);

       /* This is a manual `strtok` for newlines. The reason
       * for this is because standard strtok doesn't give
       * us the control we need to distinguish between
       * given delimiters, so that we can do different
       * operations depending on what we'd hit. */
      if ((line = strchr(word, '\n')) != NULL) {
         *line = '\0';
         if (++line < f->data + f->size) {
            /* We do normal add here because the location
             * is important. */
            arraylist_add(f->lines, line);

            /* We also do a smart add as new lines also
             * should be considered a new words. */
            word = line;
            arraylist_add(f->words, &f->newline);
            arraylist_add_smart(f->words, f->h, word);
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


ArrayList *markov_nextwords(ArrayList *corpus, ArrayList *sentence, int pickiness) {
   size_t i, j, start;
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
         for (j = 0; j < (unsigned int) pickiness; j++) {
            if (sentence->data[start + j] != corpus->data[i + j]) {
               break;
            }
         }
         if (j == (unsigned int) pickiness) {
             i += pickiness;
             arraylist_add(words, corpus->data[i]);
         }
      }
   }

   return words;
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
   ArrayList *sentence, *nextwords;

   sentence = arraylist_new();
   word = corpus->lines->data[rand() % corpus->lines->length];

   /* To generate a sentence we first of all prepares `pickiness`
    * number of words so that we could go forth by choosing a
    * possible word that should follow.   */
   for (i = 0; i < pickiness; i++) {
      if (word != NULL && *word != '\n') {
         arraylist_add_smart(sentence, corpus->h, word);
         word = finite_nextword(corpus, word);
         length--;
      }
      else {
         break;
      }
   }

   while (length-- && i == pickiness && *word != '\n') {
      if ((nextwords = markov_nextwords(corpus->words, sentence, pickiness)
               )->length > 0) {
         /* Line breaks are also considered words. This will terminate
          * the sentence a natural way.   */
         if (*(word = nextwords->data[rand() % nextwords->length]) != '\n') {
            arraylist_add(sentence, word);
         }
            
         arraylist_free(nextwords);

      }
      else {
         arraylist_free(nextwords);
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

   if ((str = malloc(sizeof(char) * length)) == NULL) {
      perror("Can't allocate space");
      exit(EXIT_FAILURE);
   }
   memset(str, c, sizeof(char) * length);
   *(str + length) = '\0';

   printf("%s", str);

   free(str);
}

/* This function outputs some nice numbers to make our
 * lives easier when benchmarking. It measures
 * preparation and generation times, and then prints the
 * results into a nice graph.
 *
 * It is ported from kqr's `fast_markov.py` at:
 *
 * The argument specifies how many times we want to
 * generate a sentence.  */
void markov_timer(int times) {
   int i, quarter, len1, len2, len3, len4;
   double scaling, preptime, min, q1, q2, q3, max;
   clock_t start, end; 
   double *times_;
   char *sentence;
   Finite *corpus;

   start = clock();
   corpus = finite_load("corpus.txt");
   finite_prepare(corpus);
   end = clock();
   preptime = ((double) (end - start)) / CLOCKS_PER_SEC;
 
   if ((times_ = malloc(sizeof(double) * times)) == NULL) {
      perror("Can't allocate space");
      exit(EXIT_FAILURE);
   }
   for (i = 0; i < times; i++) {
      start = clock();
      sentence = markov(corpus, 3, 16);
      end = clock();

      times_[i] = ((double) (end - start)) / CLOCKS_PER_SEC;

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

   printf("\nPreparation time: %.2f seconds\n", preptime);
   printf("Generated %u, sentences in %.2f seconds. (average time was %.2f seconds)\n",
          (unsigned int) i,
          sum(times_, i),
          sum(times_, i) / i);
   printf("Total amount of time: %.2f seconds\n", sum(times_, i) + preptime);

   printf("<%.2f", min);
   printchr_iterate('-', len1);
   printf("[%.2f", q1);
   printchr_iterate('-', len2);
   printf("|%.2f|", q2);
   printchr_iterate('-', len3);
   printf("%.2f]", q3);
   printchr_iterate('-', len4);
   printf("%.2f>\n", max);

   free(times_);
   finite_free(corpus);
}


int main() {
   srand(time(NULL));

   markov_timer(10);
   
   return EXIT_SUCCESS;
}
