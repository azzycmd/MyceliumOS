#include "lib.h"

int strdif(char* s1, char* s2) {
    int i = 0;
    while (s1[i] == s2[i]) {
        if (s1[i] == '\0') return 0;
        i++;
    }
    return s1[i] - s2[i];
}

int strdifb(char* s1, char* s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

void mtom(char* s) {
    if (!s) return;
    for (int i = 0; s[i] != '\0'; i++) {
        if (s[i] >= 'A' && s[i] <= 'Z') {
            s[i] = s[i] + 32; // Na tabela ASCII, 'a' é 'A' + 32
        }
    }
}

void itoa(int n, char* str) {
    int i = 0;
    int temp = n;
    
    if (n == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    while (temp > 0) {
        str[i++] = (temp % 10) + '0';
        temp /= 10;
    }
    str[i] = '\0';

    for (int j = 0; j < i / 2; j++) {
        char aux = str[j];
        str[j] = str[i - j - 1];
        str[i - j - 1] = aux;
    }
}

int atoi(char* str) {
    int res = 0;
    int i = 0;

    while (str[i] == ' ') i++;

    for (; str[i] != '\0' && str[i] >= '0' && str[i] <= '9'; ++i) {
        res = res * 10 + str[i] - '0';
    }
    return res;
}