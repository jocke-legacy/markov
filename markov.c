#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* POSIX */
#include <search.h>

#define ARRAYLIST_ALLOCATION_SIZE 1024


typedef struct {
   size_t length;
   size_t allocated;
   char **data;
} ArrayList;

typedef struct {
   size_t size;
   char *data;
} Finite;


ArrayList *arraylist_new(void);
void arraylist_add(ArrayList *al, char *value);
void arraylist_add_smart(ArrayList *al, char *value);
char *arraylist_str(ArrayList *al, char *delimiter);
void arraylist_free(ArrayList *al);
Finite *finite_file_load(const char *filename);
void finite_free(Finite *storage);
ArrayList *corpus_prepare(Finite *corpus_data);
ArrayList *markov_nextwords(ArrayList *c, ArrayList *sentence, size_t pickiness);
char *markov(ArrayList *c, size_t pickiness, size_t length);


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

   for (i = 0; i < al->length - 1; i++) {
      str_length += strlen(al->data[i]);
   }
   str_length += al->length - 1;

   if ((str = malloc(sizeof(char) * str_length + strlen(delimiter) + 1)) == NULL) {
      perror("Can't allocate space for string");
      exit(EXIT_FAILURE);
   }
   *str = '\0';
   for (i = 0; i < al->length - 1; i++) {
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


Finite *finite_file_load(const char *filename) {
   FILE *fd;
   Finite *f;

   f = malloc(sizeof(Finite));

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

   if (fread(f->data, f->size, 1, fd) == 0) {
      perror("Error while reading file");
      exit(EXIT_FAILURE);
   }
   f->data[f->size - 1] = '\0';

   fclose(fd);

   return f;
}

void finite_free(Finite *f) {
   free(f->data);
   free(f);
}


ArrayList *corpus_prepare(Finite *corpus_data) {
   char *ptr;
   ArrayList *c;

   c = arraylist_new();

   ptr = strtok(corpus_data->data, " ");
   while (ptr != NULL) {
      arraylist_add_smart(c, ptr);
      ptr = strtok(NULL, " ");
   }

   return c;
}


ArrayList *markov_nextwords(ArrayList *c, ArrayList *sentence, size_t pickiness) {
   size_t i, j, start;
   ArrayList *words;

   words = arraylist_new();
   start = (sentence->length - 1) - (pickiness - 1);
           
   for (i = 0; i < (c->length - 1) - pickiness; i++) {
      /* We only need to compare the string pointers becasue
       * `arraylist_add_smart` used in `corpus_prepare` maps
       * existing words to their first occurences. So if the
       * string is the same, the address is also the same */
      if (sentence->data[start] == c->data[i]) {
         for (j = 0; j < pickiness; j++) {
            if (sentence->data[start + j] != c->data[i + j]) {
               break;
            }
         }
         if (j == pickiness) {
             i += pickiness;
             arraylist_add(words, c->data[i]);
         }
      }
   }

   return words;
}

char *markov(ArrayList *c, size_t pickiness, size_t length) {
   size_t i, randomword_index;
   char *str, *word;
   ArrayList *sentence, *nextwords;

   sentence = arraylist_new();
   randomword_index = rand() % ((c->length - 1) - pickiness);

   for (i = 0; i < pickiness; i++) {
        arraylist_add(sentence, c->data[randomword_index + i]);
   }
   length -= pickiness;

   while (length--) {
      nextwords = markov_nextwords(c, sentence, pickiness);
      if (nextwords->length > 0) {
         word = nextwords->data[rand() % nextwords->length];
         arraylist_add(sentence, word);
         arraylist_free(nextwords);
         if (strchr(word, '\n') != NULL) {
            break;
         }
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


void markov_timer(ArrayList *c) {
   int i;
   double average;
   char *sentence;
   clock_t start;

   average = 0.0f;

   for (i = 0; i < 10; i++) {
      start = clock();
      sentence = markov(c, 3, 16);
      average += ((double) clock() - start) / CLOCKS_PER_SEC;

      printf("%s\n", sentence);
      free(sentence);
   }
   average /= 10.0f;

   printf("\nAverage generation time: %f s\n", average);
}


int main() {
   clock_t start;
   ArrayList *c;
   Finite *corpus_data;

   srand(time(NULL));

   start = clock();
   corpus_data = finite_file_load("corpus.txt");

   /* As stated in the documentation for `hcreate`, the
    * implementation method leaves a possibility for
    * collisions in hash tables if populated more than 80%
    *
    * We allocate twice the size to ensure O(1) lookups */
   if (hcreate(corpus_data->size * 2) == 0) {
      perror("Can't create hashtable");
      exit(EXIT_FAILURE);
   }
   c = corpus_prepare(corpus_data);
   start = clock() - start;

   markov_timer(c);
   printf("Prepearing time:\t %f s\n", ((double) start / CLOCKS_PER_SEC));


   hdestroy();
   arraylist_free(c);
   finite_free(corpus_data);
   
   return EXIT_SUCCESS;
}

