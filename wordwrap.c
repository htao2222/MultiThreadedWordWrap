#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <pthread.h>

#define QUEUESIZE 128
#define BUFSIZE 40
#define DEFAULT S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH

typedef struct file_queue {
    char *data[QUEUESIZE];
    // plen includes /
    int plen[QUEUESIZE];
    int flen[QUEUESIZE];
    int start, stop;
    int full;
    int finish;
    int width;
    int success;
    pthread_mutex_t lock;
    pthread_cond_t enqueue_ready, dequeue_ready;
} fqueue;

typedef struct directory_node {
    char *path;
    struct directory_node *next;
} dnode;

// This is actually a stack lol
typedef struct directory_queue {
    struct directory_node *head;
    int active;
    pthread_mutex_t lock;
    pthread_cond_t dequeue_ready;
} dqueue;

typedef struct double_queue{
    struct directory_queue d;
    struct file_queue f;
} param;

typedef struct file_info{
    int plen;
    int flen;
} finfo;

int format_to(int fd, int wd, int width);
void *dthread(void *arg);
void *fthread(void *arg);
void denqueue(struct directory_queue *arg, char *newdir);
char *ddequeue(struct directory_queue *arg);
void fenqueue(struct file_queue *arg, char *newfile, int plen, int flen);
char *fdequeue(struct file_queue *arg, struct file_info *l);

int format_to(int fd, int wd, int width){
    // TODO: Modularize into function later - DONE

    int bytes, rlen = 0, start, next = 0, len = 0, extra_len = 0, pos, rem = 0, news = 0, space = 0, status;
    char last = ' ';
    char *word = NULL;
    //char *spacing = " \n\n";
    char buf[BUFSIZE];

    //int test = open("DEBUG", O_CREAT|O_TRUNC|O_WRONLY, DEFAULT);

    while((bytes = read(fd, buf, BUFSIZE)) > 0){
        start = 0;
        if(last != '\n'){
            last = ' ';
        }
        
        // TODO: Check that word isnt greater than width - DONE
        for(pos = 0; pos < bytes; pos++){
            // Check if next is active and append last word together
            // Set start depending on if last character was a space or not
            if(isspace(buf[pos])){

                
                // Stops pos-start check unless this is end of a word
                if(!next){
                    

                    // TODO: Trigger condition to stop double printing if over 2 newlines needed - DONE
                    if(last == '\n' && buf[pos] == '\n'){
                        news = 1;
                        len = 0;
                        space = 0;
                    }
                    last = buf[pos];
                    continue;
                }

                if(pos-start > width && len > 0){

                }
                
                // End word depending on if is space
                // Fix rlen to extra_len
                // Ugly code
                // FIX: If word too long won't print later half properly - DONE
                if(pos-start + extra_len > width){
                    if(len > 0){
                        write(wd, "\n", 1);
                    }
                    write(wd, word, extra_len);
                    write(wd, &buf[start], pos-start);
                    next = 0;
                    len = pos-start + extra_len;
                    extra_len = 0;
                    space = 0;
                    status = EXIT_FAILURE;
                    last = buf[pos];
                    continue;
                }
                if(len+(pos-start + extra_len) > width){
                    
                    len = 0;
                    write(wd, "\n", 1);
                    space = 0;
                }
                // Check if remnant here to print that out then - Obsolete
                // if(pos == 0 && rem){
                //     write(1, word, extra_len);
                //     write(1, spacing, 1);
                //     extra_len = 0;
                //     rem = 0;
                //     len += extra_len + 1;
                // }
                if(space){
                    write(wd, " ", 1);
                }
                if(rem){
                    write(wd, word, extra_len);
                    len += extra_len;
                    extra_len = 0;
                    rem = 0;
                }
                // TODO: Fix extra space at end of line - space variable set to 0 at start then write a space if true - DONE
                write(wd, &buf[start], pos-start);
                
                next = 0;
                space = 1;
                
                len += (pos-start) + 1;
                
                


            } else if (isspace(last)){
                if(news){
                    write(wd, "\n\n", 2);
                    news = 0;
                }
                start = pos;
                next = 1;
            }
            last = buf[pos];
        }
        // TODO: Write what is left in the buffer - DONE

        if(next){
            rlen = pos-start;
            word = realloc(word, extra_len + rlen);
            memcpy(&word[extra_len], &buf[start], rlen);


            extra_len += rlen;
            rem = 1;

            // write(test, word, extra_len);
            // write(test, "\n", 1);
            
        }

    }

    if(rem){
        if(space){
            write(wd, " ", 1);
        }
        write(wd, word, extra_len);
        write(wd, "\n", 1);
    } else {
        write(wd, "\n", 1);
    }

    free(word);

    if(bytes == -1){
        perror("read");
        return EXIT_FAILURE;
    }

    return status;
}

void *dthread(void *arg){

    printf("Directory thread started : %ld\n", pthread_self());

    struct double_queue *p = (struct double_queue *) arg;

    printf("Double queue received at: %p\n", p);
    printf("(DIRECTORY THREAD) File queue received at: %p\n", &p->f);

    char *path;

    while((path=ddequeue(&p->d)) != NULL){
        printf("Directory path found: %s\n", path);
        // Loop through directory dequeued
        DIR *dp;
        struct dirent *de;

        dp = opendir(path);
        
        while((de = readdir(dp)) != NULL){
            // FIX stat needs full path not just name - SEMI FIXED
            struct stat info;

            if(de->d_name[0] == '.'){
                continue;
            }
            char dest[6];
            strncpy(dest, de->d_name, 5);
            dest[5] = '\0';
            if(!strcmp(dest, "wrap.")){
                continue;
            }
            
            int plen = strlen(path);
            int flen = strlen(de->d_name);
            char *npath = malloc(plen + flen + 2);
            memcpy(npath, path, plen);
            npath[plen] = '/';
            memcpy(npath + plen + 1, de->d_name, flen + 1);

            if(stat(npath, &info)){
                perror(de->d_name);
                continue;
            }

            if(S_ISREG(info.st_mode)){
                printf("Enqueueing file: %s\n",npath);
                fenqueue(&p->f, npath, plen + 1, flen);
            } else if(S_ISDIR(info.st_mode)){
                printf("Enqueueing directory: %s\n",npath);
                denqueue(&p->d, npath);
            } else {
                free(npath);
                continue;
            }
        }

        closedir(dp);

        free(path);
    }

    pthread_mutex_lock(&p->f.lock);

    p->f.finish = 1;
    pthread_cond_broadcast(&p->f.dequeue_ready);

    pthread_mutex_unlock(&p->f.lock);

    printf("Terminating directory thread: %ld\n", pthread_self());
    return NULL;
}

void *fthread(void *arg){

    printf("File thread started : %ld\n", pthread_self());

    struct file_queue *f = (struct file_queue *) arg;

    printf("(FILE THREAD) File queue received at: %p\n", f);

    int width = f->width;
    int success = 0;
    char *path;
    finfo l;

    while((path = fdequeue(f, &l)) != NULL){
        printf("File path found: %s\n", path);
        int fd = open(path, O_RDONLY);
        if(fd == -1){
            perror(path);
            free(path);
            continue;
        }
        
        // Error overwriting malloced chunk metadata (???)
        path = realloc(path, l.plen + l.flen + 6);
        memmove(path + l.plen + 5, path + l.plen, l.flen + 1);
        memcpy(path + l.plen, "wrap.", 5);
        
        // char name[256];
        // strcpy(name, "wrap.");
        //printf("%s\n",name);
        //printf("Got here\File name: %s\n%ld\n", de->d_name, sizeof(de->d_name));

        int wd = open(path, O_CREAT|O_TRUNC|O_WRONLY, DEFAULT);
        if(wd == -1){
            perror("Write fail\n");
            return NULL;
        }
        
        success = format_to(fd, wd, width) || success;
        printf("Done writing: %s\n", path);
        close(fd);
        close(wd);

        free(path);
    }

    f->success = f->success || success;


    printf("Terminating file thread: %ld\n", pthread_self());
    return NULL;
}

// Could possibly use single method for both directories and files?
void denqueue(struct directory_queue *arg, char *newdir){

    pthread_mutex_lock(&arg->lock);

    dnode *d = malloc(sizeof(struct directory_node));
    d->path = newdir;
    d->next = arg->head;
    arg->head = d;

    // struct directory_node *q = arg->head;
    // while(q != NULL){
    //     printf("%p\n", q->path);
    //     if(q->path != NULL) printf("%s\n", q->path);
    //     q = q->next;
    // }
    
    pthread_cond_signal(&arg->dequeue_ready);
    pthread_mutex_unlock(&arg->lock);
}

// Add signal all, add setting finished field for file queue
// FIX - Issue messing up links
char *ddequeue(struct directory_queue *arg){

    pthread_mutex_lock(&arg->lock);

    arg->active--;
    while(arg->head == NULL){
        
        if(arg->active <= 0){
            printf("Directory thread %ld kill switch active\n", pthread_self());
            pthread_cond_broadcast(&arg->dequeue_ready);
            pthread_mutex_unlock(&arg->lock);
            return NULL;
        }
        printf("Directory thread %ld dequeue waiting\n", pthread_self());
        pthread_cond_wait(&arg->dequeue_ready, &arg->lock);
    }
    
    arg->active++;

    char *path = arg->head->path;
    dnode *temp = arg->head->next;
    free(arg->head);
    arg->head = temp;
    
    pthread_mutex_unlock(&arg->lock);
    printf("Directory dequeued: %p\n", path);
    return path;
}

void fenqueue(struct file_queue *arg, char *newfile, int plen, int flen){

    printf("fenqueue triggered: %p\n", arg);

    pthread_mutex_lock(&arg->lock);

    while(arg->full){
        printf("File thread %ld enqueue waiting\n", pthread_self());
        pthread_cond_wait(&arg->enqueue_ready, &arg->lock);
    }

    arg->data[arg->stop] = newfile;
    arg->plen[arg->stop] = plen;
    arg->flen[arg->stop] = flen;
    arg->stop++;

    if(arg->stop == QUEUESIZE){
        arg->stop = 0;
    } 

    if(arg->stop == arg->start){
        arg->full = 1;
    }
    
    printf("File thread %ld signaling dequeue ready\n", pthread_self());
    printf("Signaling: %p\n", &arg->dequeue_ready);
    pthread_cond_signal(&arg->dequeue_ready);
    pthread_mutex_unlock(&arg->lock);
}

// Possibly return struct instead to resolve new file name issue
char *fdequeue(struct file_queue *arg, struct file_info *l){

    printf("fdequeue triggered: %p\n", arg);

    pthread_mutex_lock(&arg->lock);

    while(arg->start == arg->stop && !arg->full){
        
        if(arg->finish){
            printf("File thread %ld hit finish\n", pthread_self());
            pthread_cond_broadcast(&arg->dequeue_ready);
            pthread_mutex_unlock(&arg->lock);
            return NULL;
        }
        printf("File thread %ld dequeue waiting\n", pthread_self());
        printf("Waiting: %p\n", &arg->dequeue_ready);
        pthread_cond_wait(&arg->dequeue_ready, &arg->lock);
        printf("File dequeue signal received\n");
    }

    char *path = arg->data[arg->start];
    l->plen = arg->plen[arg->start];
    l->flen = arg->flen[arg->start];
    arg->start++;

    if(arg->start == QUEUESIZE){
        arg->start = 0;
    }

    arg->full = 0;


    pthread_cond_signal(&arg->enqueue_ready);
    pthread_mutex_unlock(&arg->lock);
    
    return path;
}


int main(int argc, char **argv){
    if(argc < 2 || argc > 4){
        printf("Error: Incorrect arguments\n");
        return EXIT_FAILURE;
    }
    int fd, success = EXIT_SUCCESS;
    // char last = '\0';
    // char *saved, *line = NULL;
    int width;
    struct stat info;
    if(argc <= 3){
        width = strtol(argv[1], NULL, 10);
        if(stat(argv[2], &info)){
            perror(argv[2]);
            return EXIT_FAILURE;
        }
    }

    if(argc == 4){
        width = strtol(argv[2], NULL, 10);
        if(stat(argv[3], &info)){
            perror(argv[3]);
            return EXIT_FAILURE;
        }
    }

    if (argc == 4){
        if(argv[1][0] != '-' || argv[1][1] != 'r'){
            printf("Error: Incorrect arguments\n");
            return EXIT_FAILURE;
        }
        int n = 1;
        int m = 1;
        char *nums = NULL;
        if(strlen(argv[1]) > 2){
            nums = strtok(argv[1]+2, ",");
            n = strtol(nums, NULL, 10);
            nums = strtok(NULL, ",");
        } 
        
        if(nums != NULL){
            m = n;
            n = strtol(nums, NULL, 10);
        }

        if(!S_ISDIR(info.st_mode)){
            printf("Error: Not directory\n");
            return EXIT_FAILURE;
        }

        

        struct directory_queue q;
        struct file_queue f;
        struct double_queue p;
        struct directory_node *node = malloc(sizeof(struct directory_node));
        // node->path = argv[3];
        int len = strlen(argv[3]);
        char *temp = malloc(sizeof(char) * (len+1));
        memcpy(temp, argv[3], len+1);
        node->path = temp;
        node->next = NULL;
        q.head = node;
        q.active = 0;
        
        f.start = 0;
        f.stop = 0;
        f.full = 0;
        f.finish = 0;
        f.width = width;
        f.success = 0;

        // Change to pointers maybe
        p.d = q;
        p.f = f;

        pthread_mutex_init(&p.d.lock, NULL);
        pthread_cond_init(&p.d.dequeue_ready, NULL); 

        pthread_mutex_init(&p.f.lock, NULL);
        pthread_cond_init(&p.f.enqueue_ready, NULL);
        pthread_cond_init(&p.f.dequeue_ready, NULL); 

        printf("Double queue at: %p\n", &p);
        printf("File queue at: %p\n", &p.f);
        printf("File dequeue lock: %p\n", &p.f.dequeue_ready);

        pthread_t *dtid = malloc(sizeof(pthread_t) * m), *ftid = malloc(sizeof(pthread_t) * n);
        for(int i = 0; i < m; i++){
            pthread_create(&dtid[i], NULL, dthread, &p);
        }

        for(int i = 0; i < n; i++){
            pthread_create(&ftid[i], NULL, fthread, &p.f);
        }

        printf("GOT HERE: %d %d\n", m,n);

        for(int i = 0; i < m; i++){
            pthread_join(dtid[i], NULL);
        }

        for(int i = 0; i < n; i++){
            pthread_join(ftid[i], NULL);
        }

        free(dtid);
        free(ftid);

        success = p.f.success;

    } else if (argc < 3){
        fd = 0;
        success = format_to(fd, 1, width);
    } else if(S_ISREG(info.st_mode)){
        fd = open(argv[2], O_RDONLY);
        if(fd == -1){
            perror(argv[2]);
            return EXIT_FAILURE;
        }
        
        success = format_to(fd, 1, width);

        close(fd);
    } else if(S_ISDIR(info.st_mode)){
        
        DIR *dp;
        struct dirent *de;

        dp = opendir(argv[2]);
        
        while((de = readdir(dp)) != NULL){

            chdir(argv[2]);

            // printf("%s\n", de->d_name);

            if(de->d_name[0] == '.'){
                continue;
            }
            char dest[6];
            strncpy(dest, de->d_name, 5);
            dest[5] = '\0';
            if(!strcmp(dest, "wrap.")){
                continue;
            }
            
            
            if(stat(de->d_name, &info)){
                perror(de->d_name);
                return EXIT_FAILURE;
            }
            if(!S_ISREG(info.st_mode)){
                continue;
            }

            fd = open(de->d_name, O_RDONLY);
            if(fd == -1){
                perror(de->d_name);
                return EXIT_FAILURE;
            }
            
            char name[256];
            strcpy(name, "wrap.");
            //printf("%s\n",name);
            //printf("Got here\File name: %s\n%ld\n", de->d_name, sizeof(de->d_name));

            int wd = open(strcat(name, de->d_name), O_CREAT|O_TRUNC|O_WRONLY, DEFAULT);
            if(wd == -1){
                perror("Write fail\n");
                return EXIT_FAILURE;
            }
            
            success = format_to(fd, wd, width) || success;
            close(fd);
            close(wd);
            //free(name);
        }

        closedir(dp);
        
    } else {
        printf("Error: Invalid file\n");
        return EXIT_FAILURE;
    }

    return success;
}
