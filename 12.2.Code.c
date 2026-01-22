#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

char* pathprefix_abziehen(const char *string_laenger, const char *praefix) {
    size_t len2 = strlen(praefix);
    
    if (strncmp(string_laenger, praefix, len2) == 0) {
        return (char*)(string_laenger + len2);          //Einfach n Pointer auf die späterer Stelle. 
    }
    
    return NULL; // praefix ist kein Präfix
}

int copy_file(const char *source, const char *ziel) {       //Kopiert source-datei auf bzw ins ziel-datei
    int in = open(source, O_RDONLY);    //Quelle oeffnen
    if (in < 0) //Fehlerbehandlung
        return -1;

    struct stat st;
    fstat(in, &st);

    int out = open(ziel, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);  //ziel-öffnen
    if (out < 0) {
        close(in);
        return -1;
    }

    char buf[8192];
    ssize_t r;
    while ((r = read(in, buf, sizeof(buf))) > 0) {  //rüberschreiben
        if (write(out, buf, r) != r) {  //Fehlerbehandlung
            close(in);
            close(out);
            return -1;
        }
    }

    close(in);
    close(out);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <source> <oldbackup> <newbackup>\n", argv[0]);
        return 1;
    }       //Fehlerbehandlung input von user

    char* source_path[] = {argv[1], NULL};          //Argumente, also paths korrekt speicher für FTS, welches ein Array aus
    char* oldbackup_path[] = {argv[2], NULL};       //Stringpointern mit NULL am Ende benötigt.
    char* newbackup_path[] = {argv[3], NULL};

    FTS *source_handler = fts_open(source_path, 0, NULL);     //initialisiert den FTS (Path-of-Stringpointer, Optionen, NULL) => Pointer auf FTS-Stream als Rückgabe, NULL wenn fehlgeschlagen.
    FTSENT *try_source = fts_read(source_handler);              //liest den ersten Wert in der Suche

    FTS *oldbackup_handler = fts_open(oldbackup_path, 0, NULL);     
    FTSENT *try_oldbackup = fts_read(oldbackup_handler); 

    FTS *newbackup_handler = fts_open(newbackup_path, 0, NULL);     
    FTSENT *try_newbackup = fts_read(newbackup_handler);

    char *originaler_path_von_source = strdup(try_source -> fts_path);      //Path in Stringpointer bzw. Strings umwandeln
    char *originaler_path_von_oldbackup = strdup(try_oldbackup -> fts_path);
    char *originaler_path_von_newbackup = strdup(try_newbackup -> fts_path);

    fprintf(stderr, "Starte Backup-Erstellung:\n");  //Userinfo bzw unten zusaätzliche Userinfos

    while((try_source = fts_read(source_handler)) != NULL) {        //so lange FTS nicht fertig gesucht hat, laufe immer weiter
                                                                    //zum nächsten Knoten = directory oder Datei

        char *path_von_source = strdup(try_source -> fts_path);     //aktuellen Quellen-path-pointer erstellen, wie oben

        struct stat st;
        
        switch (try_source -> fts_info) {       //switch, was FTS liefert   

            case FTS_D:        // Verzeichnis (pre-order, also bevor Inhalt durchlaufen wird)
                //Nur das Ende des aktuellen paths, also alles was source_og auch beinhaltet entfernen, wie siehe oben
                char *name_neuer_path = pathprefix_abziehen(path_von_source, originaler_path_von_source);
                //Buffer 
                char neuer_path_in_newbackup[1028]; 
                //String von og_newbackup und Zusatz zusammenfügen
                snprintf(neuer_path_in_newbackup, sizeof(neuer_path_in_newbackup), "%s%s", originaler_path_von_newbackup, name_neuer_path);
                //Neues Directory erstellen
                mkdir(neuer_path_in_newbackup, 0777);
                fprintf(stderr, "neues Directory erstellt.\n");
                continue;
            case FTS_DOT:      // "." oder ".." Skippen
                continue;
            case FTS_DP:       // Verzeichnis (post-order, also nach Durchlaufen des Inhalts), kann man ignorieren, muss man ja nur einmal behandeln.
                continue;
            case FTS_F:        // Reguläre Datei
                //selbes Spiel wie bei neuem directory, diesmal für oldbackup und newbackup
                char *name_neuer_path2 = pathprefix_abziehen(path_von_source, originaler_path_von_source);
                char neuer_path_in_old_backup2[1024];
                snprintf(neuer_path_in_old_backup2, sizeof(neuer_path_in_old_backup2), "%s%s", originaler_path_von_oldbackup, name_neuer_path2);
                char neuer_path_in_newbackup2[1028]; 
                snprintf(neuer_path_in_newbackup2, sizeof(neuer_path_in_newbackup2), "%s%s", originaler_path_von_newbackup, name_neuer_path2);
                //Wenn es im alten backup schon gespeichert ist, dann sparen wir ein wenig Speicher und machen ein hardlink im neuen Backup auf das alte.
                //Wenn nicht einfach die Datei kopieren, siehe dazu oben
                if (lstat(neuer_path_in_old_backup2, &st) == 0) {
                    link(neuer_path_in_old_backup2, neuer_path_in_newbackup2);
                    fprintf(stderr, "Neuer Hardlink erstellt.\n");
                }
                else {
                    if (copy_file(path_von_source, neuer_path_in_newbackup2) == -1) {
                        perror("copy_file");
                    }
                    fprintf(stderr, "Datei kopiert.\n");
                }
                continue;
            case FTS_DC:       // Zyklus (Directory Cycle) - Verzeichnis erzeugt Schleife   
                fprintf(stderr, "Loop entstanden, überprüfe die verzeichnisse darauf. \n");
                break;
            case FTS_SL:       // Symbolischer Link, update ich noch, dass das auch gemacht wird
                fprintf(stderr, "Fehler, symbolischer Link. \n");
                break;
            case FTS_SLNONE:   // Symbolischer Link (defekt), update ich noch, dass das auch gemacht wird
                fprintf(stderr, "Fehler, symbolischer Link. \n");
                break;
            case FTS_DNR:      // Verzeichnis nicht lesbar (Permission denied)
                fprintf(stderr, "Verzeichnis nicht lesbar. \n");
                break;
            case FTS_ERR:      // Fehler generell
                fprintf(stderr, "universeller Fehler entstanden.\n");
                break;
            case FTS_NS:       // Stat-Informationen nicht verfügbar
                fprintf(stderr, "Stat-Informations-Fehler bei: %s\n", path_von_source);
                break;
            default:
                printf("Unbekannt: %s\n", path_von_source);
                break;
            }

        free(path_von_source);  //Speicherleks mögen wir nicht.
    }
    fts_close(source_handler);      //so, hinter Einem aufräumen, sind ja gut erzogen, nicht wahr?
    fts_close(newbackup_handler);
    fprintf(stderr, "Backup erstellen erfolgreich abgeschlossen.\n");
    return 0;
}   