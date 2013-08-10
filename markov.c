#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* GNU C Library */
#include <search.h>

#define ARRAYLIST_ALLOCATION_SIZE 1024 


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
} Finite;


ArrayList *arraylist_new(void);
void arraylist_add(ArrayList *al, char *value);
void arraylist_add_smart(ArrayList *al, char *value);
char *arraylist_str(ArrayList *al, char *delimiter);
void arraylist_free(ArrayList *al);
Finite *finite_load(const char *filename);
char *finite_nextword(Finite *f, char *word);
void finite_prepare(Finite *f);
void finite_free(Finite *f);
ArrayList *markov_nextwords(ArrayList *c, ArrayList *sentence, int pickiness);
char *markov(Finite *corpus, int pickiness, size_t length);


ArrayList *arraylist_new(void) {
   ArrayList *al;

   if ((al = malloc(sizeof(ArrayList))) == NULL) {
      perror("Can't create list");
      exit(EXIT_FAILURE);
   }

   memset(al, 0, sizeof(ArrayList));

   return al;
}

void arraylist_add(ArrayList *al, char *value) {
   if (al->length == al->allocated || al->allocated == 0) {
      if (al->allocated == 0) {
         al->allocated = ARRAYLIST_ALLOCATION_SIZE;
      }
      else {
         al->allocated *= 2;
      }

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
void arraylist_add_smart(ArrayList *al, char *value) {
   ENTRY item;
   ENTRY *found;

   item.key = value;
   if ((found = hsearch(item, FIND)) != NULL) {
      value = found->data;
   }
   else {
      item.data = value;
      hsearch(item, ENTER);
   }

   arraylist_add(al, value);
}
 
char *arraylist_str(ArrayList *al, char *delimiter) {
   size_t i, str_length;
   char *str;

   str_length = 0;

   for (i = 0; i < al->length; i++) {
      str_length += strlen(al->data[i]);
   }
   str_length += al->length - 1;

   if ((str = malloc(sizeof(char) * str_length + strlen(delimiter) + 1)) == NULL) {
      perror("Can't allocate space for string");
      exit(EXIT_FAILURE);
   }
   *str = '\0';
   for (i = 0; i < al->length; i++) {
      strcat(str, al->data[i]);
      strcat(str, delimiter);
   }
   *(str + str_length) = '\0'; /* no trailing delimiter */

   return str;
}

void arraylist_free(ArrayList *al) {
   free(al->data);
   free(al);
}


Finite *finite_load(const char *filename) {
   FILE *fd;
   Finite *f;

   f = malloc(sizeof(Finite));
   f->newline = '\n';

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

   f->words = arraylist_new();
   f->lines = arraylist_new();

   word = strtok(f->data, " ");
   while (word != NULL) {
      arraylist_add_smart(f->words, word);

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
            arraylist_add_smart(f->words, word);

         }
      }

      word = strtok(NULL, " ");
   }
}

void finite_free(Finite *f) {
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

char *markov(Finite *corpus, int pickiness, size_t length) {
   int i;
   char *str, *word;
   ArrayList *sentence, *nextwords;

   sentence = arraylist_new();
   word = corpus->lines->data[rand() % corpus->lines->length];

   for (i = 0; i < pickiness; i++) {
      if (word != NULL && *word != '\n') {
         arraylist_add_smart(sentence, word);
         word = finite_nextword(corpus, word);
         length--;
      }
      else {
         break;
      }
   }

   while (length-- && i == pickiness && *word != '\n') {
      nextwords = markov_nextwords(corpus->words, sentence, pickiness);
      if (nextwords->length > 0) {
         word = nextwords->data[rand() % nextwords->length];

         if (*word != '\n') {
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


void markov_timer(Finite *corpus) {
   int i;
   double average;
   char *sentence;
   clock_t start;

   average = 0.0f;

   for (i = 0; i < 10; i++) {
      start = clock();
      sentence = markov(corpus, 3, 16);
      average += ((double) clock() - start) / CLOCKS_PER_SEC;

      printf("%s\n", sentence); 
      free(sentence);
   }
   average /= i;

   printf("\nAverage generation time: %f s\n", average);
}


int main() {
   clock_t start;
   Finite *corpus;

   srand(time(NULL));

   start = clock();
   corpus = finite_load("corpus.txt");

   /* As stated in the documentation for `hcreate`, the
    * implementation method leaves a possibility for
    * collisions in hash tables if populated more than 80%
    *
    * We allocate 150% the size to ensure O(1) lookups */
   if (hcreate(corpus->size * 1.5f) == 0) {
      perror("Can't create hashtable");
      exit(EXIT_FAILURE);
   }
   finite_prepare(corpus);
   start = clock() - start;

   markov_timer(corpus);
   printf("Preparation time:\t%f s\n", ((double) start / CLOCKS_PER_SEC));

   hdestroy();
   finite_free(corpus);
   
   return EXIT_SUCCESS;
}

