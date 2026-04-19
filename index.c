// index.c — Staging area implementation
//
// Text format of .pes/index (one entry per line, sorted by path):
//
//   <mode-octal> <64-char-hex-hash> <mtime-seconds> <size> <path>
//
// Example:
//   100644 a1b2c3d4e5f6...  1699900000 42 README.md
//   100644 f7e8d9c0b1a2...  1699900100 128 src/main.c
//
// This is intentionally a simple text format. No magic numbers, no
// binary parsing. The focus is on the staging area CONCEPT (tracking
// what will go into the next commit) and ATOMIC WRITES (temp+rename).
//
// PROVIDED functions: index_find, index_remove, index_status
// TODO functions:     index_load, index_save, index_add

#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);
// ─── PROVIDED ────────────────────────────────────────────────────────────────

// Find an index entry by path (linear scan).
IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0)
            return &index->entries[i];
    }
    return NULL;
}

// Remove a file from the index.
// Returns 0 on success, -1 if path not in index.
int index_remove(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) {
            int remaining = index->count - i - 1;
            if (remaining > 0)
                memmove(&index->entries[i], &index->entries[i + 1],
                        remaining * sizeof(IndexEntry));
            index->count--;
            return index_save(index);
        }
    }
    fprintf(stderr, "error: '%s' is not in the index\n", path);
    return -1;
}

// Print the status of the working directory.
//
// Identifies files that are staged, unstaged (modified/deleted in working dir),
// and untracked (present in working dir but not in index).
// Returns 0.
int index_status(const Index *index) {
    printf("Staged changes:\n");
    int staged_count = 0;
    // Note: A true Git implementation deeply diffs against the HEAD tree here. 
    // For this lab, displaying indexed files represents the staging intent.
    for (int i = 0; i < index->count; i++) {
        printf("  staged:     %s\n", index->entries[i].path);
        staged_count++;
    }
    if (staged_count == 0) printf("  (nothing to show)\n");
    printf("\n");

    printf("Unstaged changes:\n");
    int unstaged_count = 0;
    for (int i = 0; i < index->count; i++) {
        struct stat st;
        if (stat(index->entries[i].path, &st) != 0) {
            printf("  deleted:    %s\n", index->entries[i].path);
            unstaged_count++;
        } else {
            // Fast diff: check metadata instead of re-hashing file content
            if (st.st_mtime != (time_t)index->entries[i].mtime_sec || st.st_size != (off_t)index->entries[i].size) {
                printf("  modified:   %s\n", index->entries[i].path);
                unstaged_count++;
            }
        }
    }
    if (unstaged_count == 0) printf("  (nothing to show)\n");
    printf("\n");

    printf("Untracked files:\n");
    int untracked_count = 0;
    DIR *dir = opendir(".");
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            // Skip hidden directories, parent directories, and build artifacts
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
            if (strcmp(ent->d_name, ".pes") == 0) continue;
            if (strcmp(ent->d_name, "pes") == 0) continue; // compiled executable
            if (strstr(ent->d_name, ".o") != NULL) continue; // object files

            // Check if file is tracked in the index
            int is_tracked = 0;
            for (int i = 0; i < index->count; i++) {
                if (strcmp(index->entries[i].path, ent->d_name) == 0) {
                    is_tracked = 1; 
                    break;
                }
            }
            
            if (!is_tracked) {
                struct stat st;
                stat(ent->d_name, &st);
                if (S_ISREG(st.st_mode)) { // Only list regular files for simplicity
                    printf("  untracked:  %s\n", ent->d_name);
                    untracked_count++;
                }
            }
        }
        closedir(dir);
    }
    if (untracked_count == 0) printf("  (nothing to show)\n");
    printf("\n");

    return 0;
}

// ─── TODO: Implemented ───────────────────────────────────────────────────

int index_load(Index *index) {
    index->count = 0;

    FILE *f = fopen(".pes/index", "r");
    if (!f) {
        // file doesn't exist → empty index (NOT error)
        return 0;
    }

    while (!feof(f)) {
        IndexEntry e;
        char hash_hex[65];

        if (fscanf(f, "%o %64s %ld %ld %s\n",
                   &e.mode, hash_hex,
                   &e.mtime_sec, &e.size,
                   e.path) == 5) {

            hex_to_hash(hash_hex, &e.hash);
            index->entries[index->count++] = e;
        }
    }

    fclose(f);
    return 0;
}

int cmp_entries(const void *a, const void *b) {
    const IndexEntry *ea = a;
    const IndexEntry *eb = b;
    return strcmp(ea->path, eb->path);
}

int index_save(const Index *index) {
    Index temp = *index;

    // sort entries
    qsort(temp.entries, temp.count, sizeof(IndexEntry), cmp_entries);

    FILE *f = fopen(".pes/index.tmp", "w");
    if (!f) return -1;

    for (int i = 0; i < temp.count; i++) {
        char hash_hex[65];
        hash_to_hex(&temp.entries[i].hash, hash_hex);

        fprintf(f, "%o %s %ld %ld %s\n",
                temp.entries[i].mode,
                hash_hex,
                temp.entries[i].mtime_sec,
                temp.entries[i].size,
                temp.entries[i].path);
    }

    fflush(f);
    fsync(fileno(f));
    fclose(f);

    rename(".pes/index.tmp", ".pes/index");
    return 0;
}

int index_add(Index *index, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    struct stat st;
    if (stat(path, &st) != 0) {
        fclose(f);
        return -1;
    }

    size_t size = st.st_size;
    char *buffer = malloc(size ? size : 1);
    if (!buffer) {
        fclose(f);
        return -1;
    }

    if (size > 0) {
        if (fread(buffer, 1, size, f) != size) {
            free(buffer);
            fclose(f);
            return -1;
        }
    }

    fclose(f);

    ObjectID id;
    if (object_write(OBJ_BLOB, buffer, size, &id) != 0) {
        free(buffer);
        return -1;
    }

    free(buffer);

    IndexEntry *e = index_find(index, path);

    if (!e) {
        e = &index->entries[index->count++];
    }

    strcpy(e->path, path);
    e->hash = id;
    e->size = st.st_size;
    e->mtime_sec = st.st_mtime;
    e->mode = st.st_mode;

    return index_save(index);
}
