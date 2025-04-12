void exit(int); 

int getchar(void); 

void* malloc(int); 

int putchar(int);

int main1(void);

int main() { return main1(); } 

char* my_realloc(char* old, int oldlen, int newlen) {
    char* new = malloc(newlen);
    int i = 0;
    while (i <= oldlen - 1) {
        new[i] = old[i];
        i = i + 1;
    }
    return new;
}