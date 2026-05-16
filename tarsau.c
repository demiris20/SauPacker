#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>

#define MAX_FILES        32
#define MAX_TOTAL_SIZE   (200LL * 1024 * 1024)
#define ORG_HEADER_LEN   10
#define DEFAULT_OUTPUT   "a.sau"

typedef struct {
    char path[PATH_MAX];    /* full path as given */
    char name[256];         /* basename only */
    mode_t permissions;
    long long size;
} FileEntry;

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static int is_text_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    int c;
    while ((c = fgetc(f)) != EOF) {
        /* Allow standard text characters: printable ASCII + common whitespace */
        if (c != '\t' && c != '\n' && c != '\r' && (c < 32 || c > 126)) {
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    return 1;
}

/* Recursively create directories (like mkdir -p) */
static int mkdir_recursive(const char *path) {
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

/* Build the organisation section string (without the 10-byte header).
   Caller must free the returned buffer. */
static char *build_org_section(FileEntry *entries, int count, long long *out_len) {
    /* Estimate size: |name,permissions,size| per entry */
    size_t buf_size = 1;
    for (int i = 0; i < count; i++)
        buf_size += strlen(entries[i].name) + 32; /* name + ",NNNN,LLLLLLLLLLL|" */

    char *buf = malloc(buf_size);
    if (!buf) { perror("malloc"); exit(1); }
    buf[0] = '\0';

    for (int i = 0; i < count; i++) {
        char record[512];
        snprintf(record, sizeof(record), "|%s,%o,%lld",
                 entries[i].name,
                 (unsigned int)(entries[i].permissions & 0777),
                 entries[i].size);
        strcat(buf, record);
    }
    /* Close last record */
    strcat(buf, "|");

    *out_len = (long long)strlen(buf);
    return buf;
}

/* ------------------------------------------------------------------ */
/* -b  Bundle mode                                                      */
/* ------------------------------------------------------------------ */

static void bundle_mode(int argc, char *argv[]) {
    char *output = DEFAULT_OUTPUT;
    FileEntry entries[MAX_FILES];
    int file_count = 0;
    long long total_size = 0;

    /* Parse arguments: tarsau -b file... [-o out.sau] */
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "-o parametresinden sonra dosya adı belirtilmelidir!\n");
                exit(1);
            }
            output = argv[++i];
        } else {
            if (file_count >= MAX_FILES) {
                fprintf(stderr, "Hata: En fazla %d giriş dosyası desteklenmektedir!\n", MAX_FILES);
                exit(1);
            }

            /* Validate: file exists */
            struct stat st;
            if (stat(argv[i], &st) != 0) {
                fprintf(stderr, "%s giriş dosyasının formatı uyumsuzdur!\n", argv[i]);
                exit(1);
            }

            /* Validate: regular file */
            if (!S_ISREG(st.st_mode)) {
                fprintf(stderr, "%s giriş dosyasının formatı uyumsuzdur!\n", argv[i]);
                exit(1);
            }

            /* Validate: text (ASCII) file */
            if (!is_text_file(argv[i])) {
                fprintf(stderr, "%s giriş dosyasının formatı uyumsuzdur!\n", argv[i]);
                exit(1);
            }

            /* Fill entry */
            strncpy(entries[file_count].path, argv[i], PATH_MAX - 1);
            entries[file_count].path[PATH_MAX - 1] = '\0';

            /* Store only basename in archive */
            char tmp[PATH_MAX];
            strncpy(tmp, argv[i], PATH_MAX - 1);
            tmp[PATH_MAX - 1] = '\0';
            strncpy(entries[file_count].name, basename(tmp), 255);
            entries[file_count].name[255] = '\0';

            entries[file_count].permissions = st.st_mode & 0777;
            entries[file_count].size = (long long)st.st_size;
            total_size += entries[file_count].size;
            file_count++;
        }
    }

    if (file_count == 0) {
        fprintf(stderr, "Hata: Arşivlenecek dosya belirtilmemiş!\n");
        exit(1);
    }

    if (total_size > MAX_TOTAL_SIZE) {
        fprintf(stderr, "Hata: Toplam dosya boyutu 200 MB'ı geçemez!\n");
        exit(1);
    }

    /* Build organisation section */
    long long org_len;
    char *org = build_org_section(entries, file_count, &org_len);

    /* Open output file */
    FILE *out = fopen(output, "wb");
    if (!out) {
        perror(output);
        free(org);
        exit(1);
    }

    /* Write 10-byte header: size of org section, zero-padded ASCII */
    char header[ORG_HEADER_LEN + 1];
    snprintf(header, sizeof(header), "%0*lld", ORG_HEADER_LEN, org_len);
    fwrite(header, 1, ORG_HEADER_LEN, out);

    /* Write organisation section */
    fwrite(org, 1, (size_t)org_len, out);
    free(org);

    /* Write file contents consecutively */
    for (int i = 0; i < file_count; i++) {
        FILE *in = fopen(entries[i].path, "rb");
        if (!in) {
            perror(entries[i].path);
            fclose(out);
            exit(1);
        }
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
            fwrite(buf, 1, n, out);
        fclose(in);
    }

    fclose(out);
    printf("Dosyalar birleştirildi.\n");
}

/* ------------------------------------------------------------------ */
/* -a  Extract mode                                                     */
/* ------------------------------------------------------------------ */

static void extract_mode(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Arşiv dosyası uygunsuz veya bozuk!\n");
        exit(1);
    }

    if (argc > 4) {
        fprintf(stderr, "Arşiv dosyası uygunsuz veya bozuk!\n");
        exit(1);
    }

    const char *archive_path = argv[2];
    const char *dest_dir = (argc >= 4) ? argv[3] : ".";

    /* Validate archive extension */
    const char *ext = strrchr(archive_path, '.');
    if (!ext || strcmp(ext, ".sau") != 0) {
        fprintf(stderr, "Arşiv dosyası uygunsuz veya bozuk!\n");
        exit(1);
    }

    FILE *arc = fopen(archive_path, "rb");
    if (!arc) {
        fprintf(stderr, "Arşiv dosyası uygunsuz veya bozuk!\n");
        exit(1);
    }

    /* Read 10-byte header */
    char header[ORG_HEADER_LEN + 1];
    if (fread(header, 1, ORG_HEADER_LEN, arc) != ORG_HEADER_LEN) {
        fprintf(stderr, "Arşiv dosyası uygunsuz veya bozuk!\n");
        fclose(arc);
        exit(1);
    }
    header[ORG_HEADER_LEN] = '\0';

    long long org_len = atoll(header);
    if (org_len <= 0) {
        fprintf(stderr, "Arşiv dosyası uygunsuz veya bozuk!\n");
        fclose(arc);
        exit(1);
    }

    /* Read organisation section */
    char *org = malloc((size_t)org_len + 1);
    if (!org) { perror("malloc"); fclose(arc); exit(1); }

    if (fread(org, 1, (size_t)org_len, arc) != (size_t)org_len) {
        fprintf(stderr, "Arşiv dosyası uygunsuz veya bozuk!\n");
        free(org);
        fclose(arc);
        exit(1);
    }
    org[org_len] = '\0';

    /* Parse organisation section: |name,perms,size|name,perms,size|... */
    FileEntry entries[MAX_FILES];
    int count = 0;

    char *ptr = org;
    while (*ptr == '|' && count < MAX_FILES) {
        ptr++; /* skip '|' */

        char *end = strchr(ptr, '|');
        if (!end) break;

        /* Copy record to a local buffer so strtok doesn't corrupt 'org' */
        char record[512];
        size_t rlen = (size_t)(end - ptr);
        if (rlen >= sizeof(record)) {
            fprintf(stderr, "Arşiv dosyası uygunsuz veya bozuk!\n");
            free(org); fclose(arc); exit(1);
        }
        memcpy(record, ptr, rlen);
        record[rlen] = '\0';

        char *name  = strtok(record, ",");
        char *perms = strtok(NULL, ",");
        char *sz    = strtok(NULL, ",");

        if (!name || !perms || !sz) {
            fprintf(stderr, "Arşiv dosyası uygunsuz veya bozuk!\n");
            free(org); fclose(arc); exit(1);
        }

        strncpy(entries[count].name, name, 255);
        entries[count].name[255] = '\0';
        entries[count].permissions = (mode_t)strtol(perms, NULL, 8);
        entries[count].size = atoll(sz);
        count++;

        ptr = end; /* point at the closing '|' → next iteration skips it */
    }
    free(org);

    if (count == 0) {
        fprintf(stderr, "Arşiv dosyası uygunsuz veya bozuk!\n");
        fclose(arc);
        exit(1);
    }

    /* Create destination directory if needed */
    if (strcmp(dest_dir, ".") != 0) {
        struct stat st;
        if (stat(dest_dir, &st) != 0) {
            if (mkdir_recursive(dest_dir) != 0) {
                fprintf(stderr, "Dizin oluşturulamadı: %s\n", dest_dir);
                fclose(arc);
                exit(1);
            }
        } else if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "'%s' bir dizin değil!\n", dest_dir);
            fclose(arc);
            exit(1);
        }
    }

    /* Extract files — data starts right after org section (already at correct position) */
    for (int i = 0; i < count; i++) {
        char out_path[PATH_MAX];
        snprintf(out_path, sizeof(out_path), "%s/%s", dest_dir, entries[i].name);

        FILE *out = fopen(out_path, "wb");
        if (!out) {
            perror(out_path);
            fclose(arc);
            exit(1);
        }

        long long remaining = entries[i].size;
        char buf[4096];
        while (remaining > 0) {
            size_t to_read = (remaining > (long long)sizeof(buf))
                             ? sizeof(buf) : (size_t)remaining;
            size_t n = fread(buf, 1, to_read, arc);
            if (n == 0) {
                fprintf(stderr, "Arşiv dosyası uygunsuz veya bozuk!\n");
                fclose(out);
                fclose(arc);
                exit(1);
            }
            fwrite(buf, 1, n, out);
            remaining -= (long long)n;
        }
        fclose(out);

        /* Restore permissions */
        chmod(out_path, entries[i].permissions);
    }

    fclose(arc);

    /* Print success: "d1 dizininde t1, t2, t3 ve t4.txt dosyaları açıldı." */
    printf("%s dizininde ", dest_dir);
    for (int i = 0; i < count; i++) {
        if (i == count - 1 && count > 1)
            printf("ve %s ", entries[i].name);
        else if (i == count - 1)
            printf("%s ", entries[i].name);
        else if (i == count - 2)
            printf("%s ", entries[i].name);
        else
            printf("%s, ", entries[i].name);
    }
    printf("dosyaları açıldı.\n");
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Kullanım:\n");
        fprintf(stderr, "  %s -b dosya1 dosya2 ... [-o arşiv.sau]\n", argv[0]);
        fprintf(stderr, "  %s -a arşiv.sau [dizin]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-b") == 0) {
        bundle_mode(argc, argv);
    } else if (strcmp(argv[1], "-a") == 0) {
        extract_mode(argc, argv);
    } else {
        fprintf(stderr, "Bilinmeyen parametre: %s\n", argv[1]);
        fprintf(stderr, "Kullanım:\n");
        fprintf(stderr, "  %s -b dosya1 dosya2 ... [-o arşiv.sau]\n", argv[0]);
        fprintf(stderr, "  %s -a arşiv.sau [dizin]\n", argv[0]);
        return 1;
    }

    return 0;
}
