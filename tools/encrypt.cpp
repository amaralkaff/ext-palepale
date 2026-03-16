#include <cstdio>
#include <cstdlib>
#include <cstring>

static const char KEY[] = "pAlEpAlE_2026_sEcReT_kEy_X9Z3!";
static const size_t KEY_LEN = sizeof(KEY) - 1; // 31 bytes

int main(int argc, char* argv[])
{
    if (argc != 3) {
        printf("Usage: encrypt.exe <input> <output>\n");
        printf("  XOR encrypts a file with the palepale key.\n");
        printf("  Example: encrypt.exe client.exe client.enc\n");
        return 1;
    }

    FILE* fin = fopen(argv[1], "rb");
    if (!fin) {
        printf("[!] Cannot open input: %s\n", argv[1]);
        return 1;
    }

    fseek(fin, 0, SEEK_END);
    long size = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    if (size <= 0) {
        printf("[!] Input file is empty or unreadable.\n");
        fclose(fin);
        return 1;
    }

    unsigned char* buf = (unsigned char*)malloc(size);
    if (!buf) {
        printf("[!] Allocation failed.\n");
        fclose(fin);
        return 1;
    }

    fread(buf, 1, size, fin);
    fclose(fin);

    // XOR encrypt
    for (long i = 0; i < size; i++) {
        buf[i] ^= (unsigned char)KEY[i % KEY_LEN];
    }

    FILE* fout = fopen(argv[2], "wb");
    if (!fout) {
        printf("[!] Cannot open output: %s\n", argv[2]);
        free(buf);
        return 1;
    }

    fwrite(buf, 1, size, fout);
    fclose(fout);
    free(buf);

    printf("[+] Encrypted %s -> %s (%ld bytes)\n", argv[1], argv[2], size);
    return 0;
}
