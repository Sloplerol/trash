#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>

/*
* * * * * * * * * * * * * * * * * * * * * * * * * * * *
* * * * * * * * 简单数据传输工具的基本功能如下 * * * * * * *
* 1)显示页面内容
*   get http://xxx.xxx/xx.html
* 2)下载文件
*   download http://xx.xx.xx/aa.bb /home/xx/xx/aaa.bb 
* 3)上传文件到文件系统
*   copy /home/xx/xx/aaa.bb file:///home/xxx/upload/file
* 4)以POST方式上传文件到WEB服务器
*   upload /home/xx/xx/aaa.bb http://yy.yy/upload 
* * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/

#define MAX_INDEX 4
#define MAX_COMMAND 1024

const char *PROMPT = ">>> ";
const char *CMD_GET = "get";
const char *CMD_DOWNLOAD = "download";
const char *CMD_COPY = "copy";
const char *CMD_UPLOAD = "upload";


static int display_file(const char *url);
static int download_file(const char *url, const char *filename);
static int copy_file(const char *filename, const char *url);
static int upload_file(const char *filename, const char *url);



void exec_command(const char *command)
{

    int index = 0;
    char *cmd_array[MAX_INDEX] = {NULL};
    const char *delim = " ";

    char *cur = strtok(command, delim);
    while (cur != NULL)
    {

        cmd_array[index++] = cur;
        cur = strtok(NULL, delim);
    }

    pid_t pid = fork();
    if (pid > 0)
    {
        wait(NULL);
    }
    else if (pid == 0)
    {
        if (strcmp(cmd_array[0], CMD_GET) == 0)
        {
            display_file(cmd_array[1]);
        }
        else if (strcmp(cmd_array[0], CMD_DOWNLOAD) == 0)
        {
            download_file(cmd_array[1], cmd_array[2]);
        }
        else if (strcmp(cmd_array[0], CMD_COPY) == 0)
        {
            copy_file(cmd_array[1], cmd_array[2]);
        }
        else if (strcmp(cmd_array[0], CMD_UPLOAD) == 0)
        {
            upload_file(cmd_array[1], cmd_array[2]);
        }
        else
        {
            printf("无效的命令\n");
        }
        exit(0);
    }
}

uint display_cb(char *in, uint size, uint nmemb, void *out)
{
    uint length;
    length = size * nmemb;
    for (int i = 0; i < length; ++i)
    {
        printf("%c", in[i]);
    }
    return length;
}

uint download_cb(char *in, uint size, uint nmemb, char *filename)
{
    uint length = size * nmemb;

    int r = access(filename, F_OK);
    int fd;
    if(r != 0){

        fd = open(filename, O_WRONLY | O_CREAT, 0644);
    }else{

        fd = open(filename, O_WRONLY | O_APPEND);
    }

    if (fd > 0)
    {

        char * buffer = malloc(length);
        write(fd, in, length);
        close(fd);
        free(buffer);
    }
    else
    {
        perror("failed to open");
    }
    return length;
}

static int upload_file(const char *filename, const char *url)
{

    CURL *curl;
    CURLcode res;

    curl_mime *form = NULL;
    curl_mimepart *field = NULL;

    curl_global_init(CURL_GLOBAL_ALL);

    curl = curl_easy_init();
    if (curl)
    {
        form = curl_mime_init(curl);

        field = curl_mime_addpart(form);
        curl_mime_name(field, "file");
        curl_mime_filedata(field, filename);

        curl_easy_setopt(curl, CURLOPT_URL, url);

        curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));

        curl_easy_cleanup(curl);
        curl_mime_free(form);
    }
    return EXIT_SUCCESS;
}

static int copy_file(const char *filename, const char *path)
{

    char path_buffer[256] = {0};
    CURL *curl;
    CURLcode res;
    struct stat file_info;
    curl_off_t speed_upload, total_time;
    FILE *fd;

    fd = fopen(filename, "rb");
    if (!fd)
        return 1;

    if (fstat(fileno(fd), &file_info) != 0)
        return 1;

    curl = curl_easy_init();    
    if (curl != NULL)
    {

        sprintf(path_buffer, "%s%s", "file:///", path);
        curl_easy_setopt(curl, CURLOPT_URL, path_buffer);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl, CURLOPT_READDATA, fd);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)file_info.st_size);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));
        }
        else
        {
            curl_easy_getinfo(curl, CURLINFO_SPEED_UPLOAD_T, &speed_upload);
            curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME_T, &total_time);

            fprintf(stderr, "Speed: %" CURL_FORMAT_CURL_OFF_T " bytes/sec during %" CURL_FORMAT_CURL_OFF_T ".%06ld seconds\n",
                    speed_upload,
                    (total_time / 1000000), (long)(total_time % 1000000));
        }
        curl_easy_cleanup(curl);
    }
    fclose(fd);

    return EXIT_SUCCESS;
}

static int download_file(const char *url, const char *filename)
{

    CURL *curl;
    char curl_errbuf[CURL_ERROR_SIZE];
    int err;

    int size = strlen(filename);
    char * dir = malloc(size + 1);
    strcpy(dir, filename);
    int i;

    for(i = size - 1; i >= 0; --i){

        if(dir[i] == '/' && i > 0){

            dir[i] = 0;
            break;
        }
    }

    if(i > 0){
        int exist = access(dir, F_OK);
        if (exist != 0)
        {
            char cmd[256];
            sprintf(cmd, "mkdir -p %s", dir);
            system(cmd);
        }
    }


    free(dir);

    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_errbuf);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, download_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, filename);

    err = curl_easy_perform(curl);
    if (err)
    {
        fprintf(stderr, "%s\n", curl_errbuf);
    }

    curl_easy_cleanup(curl);

    return err;
}

static int display_file(const char *url)
{

    CURL *curl;
    char curl_errbuf[CURL_ERROR_SIZE];

    int err;

    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_errbuf);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, display_cb);

    err = curl_easy_perform(curl);
    if (err)
    {
        fprintf(stderr, "%s\n", curl_errbuf);
    }

    curl_easy_cleanup(curl);

    return err;
}

int main(int argc, char const *argv[])
{

    // printf("\033[32;5m_\033[0m\r\n");//

    // printf("\033[1A"); // Move up X lines;
    // printf("\033[1B"); // Move down X lines;
    // printf("\033[1C"); // Move right X column;
    // printf("\033[1D"); // Move left X column;
    // printf("\033[2J"); // Clear screen

    printf("\e[?25h"); //show cursor
    // printf("\033[?25l");//hide cursor

    char cl[MAX_COMMAND] = {0}, temp;



    printf("%s", PROMPT);
    scanf("%[^\n]", cl);
    scanf("%c", &temp);

    while (strcmp(cl, "exit") != 0)
    {

        exec_command(cl);        

        printf("%s",PROMPT);
        memset(cl, 0 , MAX_COMMAND);

        scanf("%[^\n]", cl);
        scanf("%c", &temp);
    }

    return EXIT_SUCCESS;
}