#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MCH_OK 0
#define MCH_UNMATCHED -1
#define MCH_NOT_FOUND -2
#define MCH_BUF_OVERFLOW -100

#define MCH_BUFSIZE 16384 // 16KB
#define MCH_BUFSIZE_MAX 1048576 // 1MB

#define MCH_HELP_MSG "mch -i input_pattern -o output_pattern\n"

#define MCH_IS_EOL(ch) (ch == '\0' || ch == '\n')
#define MCH_IS_DIGIT(ch) (ch >= '0' && ch <= '9')

#define MCH_DIE(msg)                            \
        do {                                    \
                fprintf(stderr, msg); exit(1);  \
        } while (0)

#define MCH_DIE1(fmt, v)                                \
        do {                                            \
                fprintf(stderr, fmt, v); exit(1);       \
        } while (0)


typedef struct val_t {
        char *src;
        int len;
        struct val_t *next;
} val_t;

typedef struct vlist_t {
        val_t *head;
        val_t *tail;
        int len;
} vlist_t;

typedef struct vartbl_t {
        val_t *vars;
        int len;
} vartbl_t;

int val_init(val_t *val) {
        val->src = NULL;
        val->len = 0;
        val->next = NULL;

        return MCH_OK;
}

int vlist_init(vlist_t *vlist) {
        vlist->head = NULL;
        vlist->tail = NULL;
        vlist->len = 0;

        return MCH_OK;
}

int vlist_toa(vlist_t *vlist, char *buf, int len) {
        val_t *val;
        int l;

        l = 0;
        for (val = vlist->head; val != NULL; val = val->next) {
                l += val->len;
                if (l > len) {
                        return MCH_BUF_OVERFLOW;
                }

                if (val->len > 0) {
                        memcpy(buf, val->src, val->len);
                        buf += val->len;
                }
        }

        *buf = '\0';

        return MCH_OK;
}

int vlist_append(vlist_t *vlist, val_t *val) {
        if (vlist->head == NULL) {
                vlist->head = vlist->tail = val;
        } else {
                val->next = NULL;
                vlist->tail->next = val;
                vlist->tail = val;
        }

        vlist->len++;

        return MCH_OK;
}

int vartbl_init(vartbl_t *vartbl) {
        vartbl->vars = NULL;
        vartbl->len = 0;

        return MCH_OK;
}

int vartbl_get(vartbl_t *vartbl, int id, val_t **vp) {
        if (id < 0 || id > vartbl->len - 1) {
                return MCH_NOT_FOUND;
        }

        *vp = &vartbl->vars[id];

        return MCH_OK;
}

// matches the line against the input pattern and updates vartbl;
int match(char *line, char *patin, vartbl_t *vartbl) {
        int id;
        val_t *val;
        char *ch;
        char *p;
        char *d;
        char *s;

        ch = line;
        id = 1;
        for (p = patin; !MCH_IS_EOL(*p); p++) {
                if (*p != '$') {
                        if (*p != *ch) {
                                return MCH_UNMATCHED;
                        }

                        ch++;
                        continue;
                }

                s = ch;
                for (d = p + 1; *ch != *d && !MCH_IS_EOL(*ch); ch++) {
                }

                if (vartbl_get(vartbl, id, &val) != MCH_OK) {
                        MCH_DIE1("BUG. invalid variable $%d\n", id);
                }
                val->src = s;
                val->len = ch - s;
                id++;
        }

        // not fully matched
        if (!MCH_IS_EOL(*ch)) {
                return MCH_UNMATCHED;
        }

        // $0
        if (vartbl_get(vartbl, 0, &val) != MCH_OK) {
                MCH_DIE1("BUG. invalid variable $%d\n", 0);
        }
        val->src = line;
        val->len = ch - line;

        return MCH_OK;
}

// setup vartbl
int parsein(char *patin, vartbl_t *vartbl) {
        int c;
        int i;
        char *ch;
        char *last;

        c = 1;
        last = NULL;
        for (ch = patin; *ch; ch++) {
                if (*ch == '$') {
                        if (last != NULL && *last == '$') {
                                MCH_DIE("invalid input pattern: '$$'\n");
                        }
                        c++;
                }
                last = ch;
        }

        vartbl->len = c;
        vartbl->vars = malloc(c * sizeof(val_t));
        if (vartbl->vars == NULL) {
                MCH_DIE("failed to malloc\n");
        }

        for (i = 0; i < c; i++) {
                val_init(&vartbl->vars[i]);
        }

        return MCH_OK;
}

// setup vlist
int parseout(char *patout, vartbl_t *vartbl, vlist_t *vlist) {
        char *ch;
        val_t *val;
        int vi;
        char *s;
        int len;

        s = NULL;
        len = 0;
        for (ch = patout; ; ch++) {
                if (*ch != '$' && !MCH_IS_EOL(*ch)) {
                        if (s == NULL) {
                                s = ch;
                                len = 1;
                        } else {
                                len++;
                        }

                        continue;
                }

                if (s != NULL) {
                        val = malloc(sizeof(val_t));
                        if (val == NULL) {
                                MCH_DIE("failed to malloc\n");
                        }
                        val_init(val);
                        val->src = s;
                        val->len = len;
                        vlist_append(vlist, val);

                        s = NULL;
                        len = 0;
                }

                if (MCH_IS_EOL(*ch)) {
                        break;
                }

                vi = 0;
                ch++;
                if (!MCH_IS_DIGIT(*ch)) {
                        MCH_DIE("'$' must be followed by a number in the output pattern\n");
                }

                for (; MCH_IS_DIGIT(*ch); ch++) {
                        vi = vi * 10 + *ch - '0';
                }

                if (vartbl_get(vartbl, vi, &val) != MCH_OK) {
                        MCH_DIE1("invalid variable $%d\n", vi);
                }

                vlist_append(vlist, val);
                ch--;
        }

        return MCH_OK;
}

int main(int argc, char **argv) {
        int c;
        char *patin;
        char *patout;
        vartbl_t vartbl;
        vlist_t vlist;

        char *buf;
        int bufsize;
        char *line;
        size_t len;
        ssize_t read;

        while ((c = getopt(argc, argv, "i:o:h")) != -1) {
                switch (c) {
                case 'i':
                        patin = optarg;
                        break;
                case 'o':
                        patout = optarg;
                        break;
                default:
                        MCH_DIE(MCH_HELP_MSG);
                }
        }

        if (patin == NULL || patout == NULL) {
                MCH_DIE(MCH_HELP_MSG);
        }

        vartbl_init(&vartbl);
        vlist_init(&vlist);
        parsein(patin, &vartbl);
        parseout(patout, &vartbl, &vlist);

        bufsize = MCH_BUFSIZE;
        buf = malloc(bufsize);
        line = NULL;
        len = 0;
        read = 0;
        while ((read = getline(&line, &len, stdin)) != -1) {
                if (match(line, patin, &vartbl) != MCH_OK) {
                        continue;
                }

                while (vlist_toa(&vlist, buf, bufsize) == MCH_BUF_OVERFLOW) {
                        bufsize *= 2;
                        if (bufsize > MCH_BUFSIZE_MAX) {
                                MCH_DIE("the current line is too long\n");
                        }

                        buf = realloc(buf, bufsize);
                        if (buf == NULL) {
                                MCH_DIE("failed to realloc\n");
                        }
                }

                fprintf(stdout, "%s\n", buf);
        }

        return 0;
}
