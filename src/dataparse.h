/* ============================================================================
 * dataparse.h - Lettore dei file di dati del gioco.
 *
 * Formato a sezioni, sulla riga di quello usato per gli agganci delle armi:
 *
 *     # le righe che iniziano con # sono commenti
 *     [oggetto iron_sword]
 *     nome    = Spada di ferro
 *     valore  = 90
 *
 *     [negozio]
 *     oggetto = potion_health      <- una chiave puo' ripetersi: chi legge
 *     oggetto = potion_mana           accumula, e diventa una lista
 *
 * Perche' non JSON: questo parser sono poche centinaia di righe senza
 * dipendenze, i file restano leggibili nei diff, la diagnostica indica file e
 * riga, e un editor li emette senza librerie.
 *
 * Regole: '#' apre un commento solo a inizio riga, cosi' un valore testuale
 * puo' contenere cancelletti; spazi attorno a chiave e valore sono ignorati;
 * il valore arriva fino a fine riga.
 *
 * Vedi docs/05-piano-dati-esterni-e-motore.md, fase 1.
 * ========================================================================== */
#ifndef DATAPARSE_H
#define DATAPARSE_H

#include <stdbool.h>

typedef struct {
    char  path[256];
    char *text;          /* buffer posseduto, modificato in loco */
    char *cur;
    int   line;

    char  kind[32];      /* prima parola della sezione: "oggetto", "negozio"  */
    char  id[64];        /* seconda parola, "" se assente                     */
    int   kindLine;

    /* Quando DataNextField incontra l'inizio della sezione successiva la legge
     * subito e la tiene qui: il cursore non si puo' riavvolgere, perche' la
     * lettura termina le righe con '\0' dentro al buffer. */
    bool  sectionReady;
} DataReader;

/* Apre il file. false se non esiste o non e' leggibile: in quel caso segnala
 * il problema, cosi' chi chiama puo' limitarsi a contare gli errori. */
bool DataOpen(DataReader *r, const char *path);
void DataClose(DataReader *r);

/* Porta il lettore alla prossima sezione. false a fine file. */
bool DataNextSection(DataReader *r);
/* Consuma e butta via i campi della sezione corrente. Si usa quando la sezione
 * viene respinta: senza, le sue righe verrebbero lette come sezioni malformate e
 * un errore ne genererebbe sei. */
void DataSkipSection(DataReader *r);
/* Prossima coppia chiave/valore della sezione corrente. false quando la
 * sezione finisce (fine file o inizio della successiva). */
bool DataNextField(DataReader *r, char **key, char **value);

/* ---- Diagnostica ------------------------------------------------------- */
/* Stampa "file:riga: messaggio" e incrementa il conteggio degli errori.
 * Nessun dato ha un valore di ripiego: chi carica accumula i problemi e alla
 * fine si rifiuta di avviare il gioco. */
void DataProblem(const DataReader *r, const char *fmt, ...);
/* Come DataProblem ma su una riga indicata: serve per i controlli che avvengono
 * a sezione conclusa, quando il lettore e' gia' andato oltre. */
void DataProblemAt(const DataReader *r, int line, const char *fmt, ...);
int  DataProblemCount(void);
void DataProblemReset(void);

/* ---- Conversioni con controllo di intervallo --------------------------- */
bool DataAsInt(const DataReader *r, const char *key, const char *value,
               int lo, int hi, int *out);
bool DataAsFloat(const DataReader *r, const char *key, const char *value,
                 float lo, float hi, float *out);
/* Valore fra quelli di 'names'. Il messaggio d'errore elenca gli ammessi. */
bool DataAsEnum(const DataReader *r, const char *key, const char *value,
                const char *const *names, int count, int *out);
/* Copia con troncamento segnalato: meglio un errore che un nome tagliato. */
bool DataAsText(const DataReader *r, const char *key, const char *value,
                char *dest, int destSize);

#endif /* DATAPARSE_H */
