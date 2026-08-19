#include "dataparse.h"
#include "raylib.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int gProblems = 0;

/* ------------------------------------------------------------------------ */
/*  DIAGNOSTICA                                                             */
/* ------------------------------------------------------------------------ */

void DataProblem(const DataReader *r, const char *fmt, ...)
{
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    if (r != NULL) TraceLog(LOG_ERROR, "%s:%d: %s", r->path, r->line, msg);
    else           TraceLog(LOG_ERROR, "%s", msg);
    gProblems++;
}

void DataProblemAt(const DataReader *r, int line, const char *fmt, ...)
{
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    TraceLog(LOG_ERROR, "%s:%d: %s", r ? r->path : "?", line, msg);
    gProblems++;
}

int  DataProblemCount(void) { return gProblems; }
void DataProblemReset(void) { gProblems = 0; }

/* ------------------------------------------------------------------------ */
/*  LETTURA                                                                 */
/* ------------------------------------------------------------------------ */

bool DataOpen(DataReader *r, const char *path)
{
    memset(r, 0, sizeof(*r));
    TextCopy(r->path, path);
    r->line = 0;

    if (!FileExists(path)) {
        DataProblem(NULL, "%s: file mancante", path);
        return false;
    }
    r->text = LoadFileText(path);
    if (r->text == NULL) {
        DataProblem(NULL, "%s: non leggibile", path);
        return false;
    }
    r->cur = r->text;
    return true;
}

void DataClose(DataReader *r)
{
    if (r->text) UnloadFileText(r->text);
    r->text = NULL;
    r->cur  = NULL;
}

static void TrimTail(char *s)
{
    int n = (int)strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r'))
        s[--n] = '\0';
}

static char *SkipSpaces(char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

/* Isola la riga corrente terminandola con '\0' e lascia il cursore su quella
 * dopo. Ritorna NULL a fine file. */
static char *TakeLine(DataReader *r)
{
    if (r->cur == NULL || *r->cur == '\0') return NULL;

    char *start = r->cur;
    char *end   = start;
    while (*end && *end != '\n') end++;
    if (*end == '\n') { *end = '\0'; r->cur = end + 1; }
    else              { r->cur = end; }
    r->line++;
    TrimTail(start);
    return start;
}

/* Legge "[tipo id]" nei campi del lettore. false se la riga non e' valida. */
static bool ParseSectionLine(DataReader *r, char *s)
{
    s++;                                         /* oltre '[' */
    char *close = strchr(s, ']');
    if (close == NULL) {
        DataProblem(r, "sezione senza ']' di chiusura");
        return false;
    }
    *close = '\0';

    r->kind[0]  = '\0';
    r->id[0]    = '\0';
    r->kindLine = r->line;

    char *word = SkipSpaces(s);
    char *sp   = word;
    while (*sp && *sp != ' ' && *sp != '\t') sp++;
    if (*sp) { *sp = '\0'; sp++; }
    TextCopy(r->kind, word);
    char *rest = SkipSpaces(sp);
    TrimTail(rest);
    TextCopy(r->id, rest);

    if (r->kind[0] == '\0') {
        DataProblem(r, "sezione senza tipo");
        return false;
    }
    return true;
}

bool DataNextSection(DataReader *r)
{
    if (r->sectionReady) {          /* letta da DataNextField, ora si consuma */
        r->sectionReady = false;
        return true;
    }

    char *line;
    while ((line = TakeLine(r)) != NULL) {
        char *s = SkipSpaces(line);
        if (*s == '\0' || *s == '#') continue;

        if (*s != '[') {
            DataProblem(r, "attesa una sezione [tipo id], trovato \"%s\"", s);
            continue;
        }
        if (ParseSectionLine(r, s)) return true;
    }
    return false;
}

bool DataNextField(DataReader *r, char **key, char **value)
{
    for (;;) {
        char *line = TakeLine(r);
        if (line == NULL) return false;

        char *s = SkipSpaces(line);
        if (*s == '\0' || *s == '#') continue;

        if (*s == '[') {                    /* inizia la sezione successiva */
            r->sectionReady = ParseSectionLine(r, s);
            return false;                   /* la sezione corrente finisce qui */
        }

        char *eq = strchr(s, '=');
        if (eq == NULL) {
            DataProblem(r, "attesa \"chiave = valore\", trovato \"%s\"", s);
            continue;
        }
        *eq = '\0';
        TrimTail(s);
        char *v = SkipSpaces(eq + 1);
        TrimTail(v);

        if (*s == '\0') { DataProblem(r, "chiave vuota"); continue; }

        *key   = s;
        *value = v;
        return true;
    }
}

void DataSkipSection(DataReader *r)
{
    char *key, *value;
    while (DataNextField(r, &key, &value)) { /* letti e ignorati */ }
}

/* ------------------------------------------------------------------------ */
/*  CONVERSIONI                                                             */
/* ------------------------------------------------------------------------ */

bool DataAsInt(const DataReader *r, const char *key, const char *value,
               int lo, int hi, int *out)
{
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == value || (end && *SkipSpaces(end) != '\0')) {
        DataProblem(r, "%s: \"%s\" non e' un numero intero", key, value);
        return false;
    }
    if (v < lo || v > hi) {
        DataProblem(r, "%s: %ld fuori dall'intervallo ammesso [%d, %d]", key, v, lo, hi);
        return false;
    }
    *out = (int)v;
    return true;
}

bool DataAsFloat(const DataReader *r, const char *key, const char *value,
                 float lo, float hi, float *out)
{
    char *end = NULL;
    double v = strtod(value, &end);
    if (end == value || (end && *SkipSpaces(end) != '\0')) {
        DataProblem(r, "%s: \"%s\" non e' un numero", key, value);
        return false;
    }
    if (v < (double)lo || v > (double)hi) {
        DataProblem(r, "%s: %g fuori dall'intervallo ammesso [%g, %g]",
                    key, v, (double)lo, (double)hi);
        return false;
    }
    *out = (float)v;
    return true;
}

bool DataAsEnum(const DataReader *r, const char *key, const char *value,
                const char *const *names, int count, int *out)
{
    for (int i = 0; i < count; i++)
        if (names[i] && strcmp(names[i], value) == 0) { *out = i; return true; }

    /* Elenco dei valori ammessi. Costruito con snprintf e non con TextAppend
     * di raylib, che dereferenzia il parametro 'position' senza controllarlo. */
    char list[256];
    int  n = 0;
    list[0] = '\0';
    for (int i = 0; i < count; i++) {
        if (!names[i]) continue;
        int written = snprintf(list + n, sizeof(list) - (size_t)n, "%s%s",
                               (n > 0) ? ", " : "", names[i]);
        if (written < 0 || written >= (int)sizeof(list) - n) break;   /* pieno */
        n += written;
    }
    DataProblem(r, "%s: \"%s\" non ammesso; valori possibili: %s", key, value, list);
    return false;
}

bool DataAsText(const DataReader *r, const char *key, const char *value,
                char *dest, int destSize)
{
    int n = (int)strlen(value);
    if (n >= destSize) {
        DataProblem(r, "%s: testo troppo lungo (%d caratteri, il massimo e' %d)",
                    key, n, destSize - 1);
        return false;
    }
    memcpy(dest, value, (size_t)n + 1);
    return true;
}
