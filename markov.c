#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
   size_t length;
   size_t allocated;
   char **data;
} ArrayList;

ArrayList *arraylist_new(void);
void arraylist_add(ArrayList *al, char *value);
char *arraylist_str(ArrayList *al, char *delimiter);
void arraylist_free(ArrayList *al);
char *file_load(const char *filename);
ArrayList *corpus_prepare(char *corpus_data);
ArrayList *markov_nextwords(ArrayList *c, ArrayList *sentence, size_t pickiness);
char *markov(ArrayList *c, size_t pickiness, size_t length);


ArrayList *arraylist_new(void) {
   ArrayList *al;

   if ((al = malloc(sizeof(ArrayList))) == NULL) {
      fprintf(stderr, "Cannot allocate list\n");
      exit(1);

   }

   memset(al, 0, sizeof(ArrayList));
   al->allocated = 512;

   return al;
}

void arraylist_add(ArrayList *al, char *value) {
   if (al->length == al->allocated || al->length == 0) {
      al->allocated *= 2;
      if ((al->data = realloc(al->data, sizeof(char *) * al->allocated))
            == NULL) {
         fprintf(stderr, "Cannot reallocate list\n");
         exit(1);
      }
   }
   al->data[al->length++] = value;
}

char *arraylist_str(ArrayList *al, char *delimiter) {
   size_t i, str_length;
   char *str;

   str_length = 0;
   for (i = 0; i < al->length; i++) {
      str_length += strlen(al->data[i]);
   }
   str_length += al->length - 1;

   str = malloc(sizeof(char) * str_length + 2);
   *str = '\0';
   for (i = 0; i < al->length; i++) {
      strcat(str, al->data[i]);
      strcat(str, delimiter);
   }
   *(str + str_length) = '\0';

   return str;
}

void arraylist_free(ArrayList *al) {
   free(al->data);
   al->data = NULL;
   free(al);
}


char *file_load(const char *filename) {
   size_t length;
   char *storage;
   FILE *fd;

   if ((fd = fopen(filename, "r")) == NULL) {
      fprintf(stderr, "Can't open file.\n");
      exit(1);
   }

   fseek(fd, 0, SEEK_END);
   length = ftell(fd);
   rewind(fd);

   if ((storage = malloc(sizeof(char) * length)) == NULL) {
      fprintf(stderr, "Can't allocate storage for file.\n");
      exit(1);
   }
   if (fread(storage, length, 1, fd) == 0) {
      fprintf(stderr, "Error while reading file\n");
      exit(1);
   }
   *(storage + length - 1) = '\0';

   fclose(fd);

   return storage;
}


ArrayList *corpus_prepare(char *corpus_data) {
   ArrayList *c;
   char *ptr;

   c = arraylist_new();

   ptr = strtok(corpus_data, " \n");
   while (ptr != NULL) {
      ptr = strtok(NULL, " \n");
      arraylist_add(c, ptr);
   }

   return c;
}

ArrayList *markov_nextwords(ArrayList *c, ArrayList *sentence, size_t pickiness) {
   ArrayList *words;
   size_t i, j, start;
          
   words = arraylist_new();
   start = (sentence->length - 1) - (pickiness - 1);
           
   for (i = 0; i < (c->length - 1) - pickiness; i++) {
      if (strcmp(sentence->data[start], c->data[i]) == 0) {
         for (j = 0; j < pickiness; j++) {
            if (strcmp(c->data[i + j], sentence->data[start + j]) != 0) {
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
   ArrayList *sentence, *nextwords;
   size_t i, randomword_index;
   char *str;

   sentence = arraylist_new();

   randomword_index = rand() % ((c->length - 1) - pickiness);

   for (i = 0; i < pickiness; i++) {
      arraylist_add(sentence, c->data[randomword_index + i]);
   }
   length -= pickiness;

   while (length--) {
      nextwords = markov_nextwords(c, sentence, pickiness);
      if (nextwords->length) {
         arraylist_add(sentence, nextwords->data[rand() % nextwords->length]);
      }
      else {
         length = 0;
      }
      arraylist_free(nextwords);
   }

   str = arraylist_str(sentence, " ");
   arraylist_free(sentence);

   return str;
}


void markov_timer(ArrayList *c) {
   char *sentence;
   clock_t start;
   double average;
   int i;

   average = 0.0f;

   for (i = 0; i < 100; i++) {
      start = clock();
      sentence = markov(c, 2, 16);
      average += ((double) clock() - start) / CLOCKS_PER_SEC;

      printf("%s\n", sentence);
      free(sentence);
   }
   average /= 100.0f;

   printf("\nAverage: %f s\n", average);
}


int main() {
   ArrayList *c;
   char *corpus_data;

   srand(time(NULL));

   corpus_data = file_load("corpus.txt");
   c = corpus_prepare(corpus_data);

   markov_timer(c);

   arraylist_free(c);
   free(corpus_data);
   
   return 0;
}

